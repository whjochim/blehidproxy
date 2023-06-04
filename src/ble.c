#include <bthidproxy/ble.h>
#include <bthidproxy/hid.h>

#include <stdio.h>
#include <stdlib.h>
#include <zephyr/types.h>
#include <sys/byteorder.h>
#include <sys/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>


#define LOG_LEVEL CONFIG_BTHP_BLE_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bthp_ble);

static struct bt_conn *default_conn;

static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;
static struct bt_gatt_read_params read_params;
static struct bt_gatt_write_params write_params;

static uint16_t hid_service_start_handle;
static uint16_t hid_service_end_handle;
static uint16_t hid_protocol_mode_handle;

static bt_security_t security_level = BT_SECURITY_L2;

static char ZERO = 0x00;

void start_scan();

static uint8_t read_protocol_mode(struct bt_conn *conn,
                 uint8_t err,
				 struct bt_gatt_read_params *params,
				 const void *data,
				 uint16_t length)
{
	if (data == NULL) {
		LOG_DBG("[PROTOCOL_MODE] reading finished");
		return BT_GATT_ITER_CONTINUE;
	}
	if (err) {
		LOG_WRN("[PROTOCOL_MODE] read failed (err %d)", err);
		return BT_GATT_ITER_CONTINUE;
	}
	LOG_DBG("[PROTOCOL_MODE] data %x length %u", *(char *) data, length);

	return BT_GATT_ITER_CONTINUE;
}

static void write_protocol_mode(struct bt_conn *conn,
				 uint8_t err,
				 struct bt_gatt_write_params *params) {
	if (err) {
		LOG_WRN("Failed to write protocol mode (err %d)", err);
	} else {
	LOG_INF("Changed to boot mode successfully");
		read_params.func = read_protocol_mode;
		read_params.handle_count = 1;
		read_params.single.handle = hid_protocol_mode_handle; 
		read_params.single.offset = 0;
		bt_gatt_read(conn, &read_params);

		memcpy(&uuid, BT_UUID_HIDS_BOOT_KB_IN_REPORT, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = hid_service_start_handle;
		discover_params.end_handle = hid_service_end_handle;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_WRN("Discover failed (err %d)", err);
		}
	}
}

static uint8_t read_keys(struct bt_conn *conn,
		struct bt_gatt_subscribe_params *params,
				 const void *data, uint16_t length) {
	if (!data) {
		LOG_INF("[UNSUBSCRIBED]");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}
	char *keys = (char *) data;

	hid_write(keys, sizeof(keys));

	LOG_HEXDUMP_DBG(keys, length, "");

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	int err;

	if (!attr) {
		LOG_INF("Discover complete");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	LOG_DBG("[ATTRIBUTE] handle %u", attr->handle);

	if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HIDS)) {

		LOG_DBG("Found HIDS Gatt Service");
		
		hid_service_start_handle = attr->handle;

		struct bt_gatt_service_val *service_val = (struct bt_gatt_service_val *) attr->user_data;
		hid_service_end_handle = service_val->end_handle;

		LOG_DBG("service end handle %u", hid_service_end_handle);

		memcpy(&uuid, BT_UUID_HIDS_PROTOCOL_MODE, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = hid_service_start_handle;
		discover_params.end_handle = hid_service_end_handle;
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_WRN("Discover failed (err %d)", err);
		}
	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_HIDS_PROTOCOL_MODE)) {
		LOG_DBG("Found HIDS Protocol Mode Characteristic");

		hid_protocol_mode_handle = bt_gatt_attr_value_handle(attr);

		read_params.func = read_protocol_mode;
		read_params.handle_count = 1;
		read_params.single.handle = hid_protocol_mode_handle; 
		read_params.single.offset = 0;
		bt_gatt_read(conn, &read_params);

		write_params.func = write_protocol_mode;
		write_params.handle = hid_protocol_mode_handle;
		write_params.data = &ZERO;
		write_params.length = 1;
		bt_gatt_write(conn, &write_params);
	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_HIDS_BOOT_KB_IN_REPORT)) {
	LOG_DBG("Found HIDS Boot Input Characteristic");
		memcpy(&uuid, BT_UUID_GATT_CCC, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.start_handle = attr->handle + 2;
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);

		err = bt_gatt_discover(conn, &discover_params);
		if (err) {
			LOG_WRN("Discover failed (err %d)", err);
		}

	} else if (!bt_uuid_cmp(discover_params.uuid,
				BT_UUID_GATT_CCC)) {
		LOG_DBG("Found GATT Client Characteristic Configuration");
		subscribe_params.notify = read_keys;
		subscribe_params.value = BT_GATT_CCC_NOTIFY;
		subscribe_params.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_params);
		if (err && err != -EALREADY) {
			LOG_WRN("Subscribe failed (err %d)", err);
		} else {
			LOG_INF("[SUBSCRIBED]");
		}

		return BT_GATT_ITER_CONTINUE;
	}else {
		LOG_WRN("attribute with non matching uuid found");
		return BT_GATT_ITER_STOP;
	}
	return BT_GATT_ITER_STOP;
}

static bool eir_found(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;
	int i;

	LOG_DBG("[AD]: %u data_len %u", data->type, data->data_len);

	switch (data->type) {
	case BT_DATA_UUID16_SOME:
	case BT_DATA_UUID16_ALL:
		if (data->data_len % sizeof(uint16_t) != 0U) {
			LOG_WRN("AD malformed");
			return true;
		}

		for (i = 0; i < data->data_len; i += sizeof(uint16_t)) {
			struct bt_uuid *uuid;
			uint16_t u16;
			int err;

			memcpy(&u16, &data->data[i], sizeof(u16));
			uuid = BT_UUID_DECLARE_16(sys_le16_to_cpu(u16));
			if (bt_uuid_cmp(uuid, BT_UUID_HIDS)) {
				LOG_INF("HID device found");
				continue;
			}

			err = bt_le_scan_stop();
			if (err) {
				LOG_ERR("Stop LE scan failed (err %d)", err);
				continue;
			}

			err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
						BT_LE_CONN_PARAM_DEFAULT,
						&default_conn);
			if (err) {
				LOG_ERR("Create connection failed (err %d)",
				       err);
				start_scan();
			}

			return false;
		}
	}

	return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	if (default_conn) {
		return;
	}

	/* We're only interested in connectable events */
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		return;
	}


	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	LOG_DBG("Device found: %s (RSSI %d)", log_strdup(addr_str), rssi);

	bt_data_parse(ad, eir_found, (void *)addr);
}

void start_scan()
{
	int err;

	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return;
	}

	LOG_DBG("Scanning successfully started");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_WRN("Failed to connect to %s (%u)", log_strdup(addr), err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	if (conn != default_conn) {
		return;
	}

	LOG_INF("Connected: %s", log_strdup(addr));
	LOG_DBG("Security Level: %u", bt_conn_get_security(conn));

	int secerr;
	secerr = bt_conn_set_security(conn, security_level);

	if(secerr) {
		LOG_INF("Failed to secure connection to %s (%d)", log_strdup(addr), secerr);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason 0x%02x)", log_strdup(addr), reason);

	bt_conn_unref(default_conn);
	default_conn = NULL;

	start_scan();
}

static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err) {

	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Security for %s changed: Security: %u, err: %d", log_strdup(addr) ,level, err);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
};

static void cancel(struct bt_conn *conn) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("authentication canceled for %s", log_strdup(addr));
}

static void pairing_confirm(struct bt_conn *conn) {
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	// just accept all pairings
	int err;
	err = bt_conn_auth_pairing_confirm(conn);
	if (err) {
		LOG_INF("pairing confirmation failed for %s, err code: %d", log_strdup(addr), err);
        bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
		return;
	}
	LOG_INF("pairing confirmed for %s", log_strdup(addr));
}

static void pairing_complete(struct bt_conn *conn, bool bonded) {

	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing complete for %s", addr);
	LOG_DBG("was bonding information send?: %d", bonded);

	// Start the gatt discovery
	int err;

	if (conn == default_conn) {
		memcpy(&uuid, BT_UUID_HIDS, sizeof(uuid));
		discover_params.uuid = &uuid.uuid;
		discover_params.func = discover_func;
		discover_params.start_handle = BT_ATT_FIRST_ATTTRIBUTE_HANDLE;
		discover_params.end_handle = BT_ATT_LAST_ATTTRIBUTE_HANDLE;
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;

		err = bt_gatt_discover(default_conn, &discover_params);
		if (err) {
			LOG_WRN("Discover failed(err %d)", err);
			return;
		}
	}
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err err) {

	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	printk("Pairing failed for %s, err: %d", addr, err);

}

static void bond_deleted(uint8_t id, const bt_addr_le_t *peer) {
	LOG_INF("Bond deleted, id: %u, peer: %p", id, peer);
}

static void passkey_display(struct bt_conn *conn, unsigned int passkey) {
	printk("\n[PASSKEY]  : %06u\n\n", passkey);	
}

static struct bt_conn_auth_cb auth_callbacks = {
	.passkey_display = passkey_display,
	.passkey_entry = NULL,
	.passkey_confirm = NULL,
	.oob_data_request = NULL,
	.cancel = cancel,
	.pairing_confirm = pairing_confirm,
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
	.bond_deleted = bond_deleted,
};

int ble_init(){
	int err = 0;

	err = bt_conn_auth_cb_register(&auth_callbacks);

	if (err) {
		LOG_ERR("couldn't register authentication callbacks. Err Code: %d", err);
		return -1;
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -1;
	}
	LOG_DBG("Bluetooth initialized");

    return err;
}

void handle_ble() {
	start_scan();
}