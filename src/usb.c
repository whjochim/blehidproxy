#include <bthidproxy/usb.h>

#include <zephyr/types.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>
#include <sys/printk.h>

#define LOG_LEVEL CONFIG_BTHP_USB_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(bthp_usb);

static const uint8_t hid_report_desc[] = HID_BOOT_KEYBOARD_REPORT_DESC();
static enum usb_dc_status_code usb_status;
static const struct device *hid_dev;

static uint8_t usb_buf[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

K_MSGQ_DEFINE(usb_msgq, sizeof(usb_buf), 30, 4);

void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	usb_status = status;
}

int usb_hid_write(const uint8_t *data, uint32_t data_len){
    int ret = 0;
	while(k_msgq_put(&usb_msgq, data, K_NO_WAIT) != 0) {
		k_msgq_purge(&usb_msgq);
	}
    return ret;
}

int usb_init() {
	int ret = 0;

	hid_dev = device_get_binding("HID_0");
	if (hid_dev == NULL) {
		LOG_ERR("Cannot get USB HID Device");
		return -1;
	}
	ret = usb_hid_set_proto_code(hid_dev, HID_BOOT_IFACE_CODE_KEYBOARD);
	if (ret) {
		LOG_ERR("Couldn't change to boot mode %d\n", ret);
	}

	usb_hid_register_device(hid_dev,
				hid_report_desc, sizeof(hid_report_desc),
				NULL);

	usb_hid_init(hid_dev);

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}
    return ret;
}

void handle_usb() {
	while (true) {
		k_msgq_get(&usb_msgq, (void *) usb_buf, K_FOREVER);
		int ret;
		ret = hid_int_ep_write(hid_dev, usb_buf, sizeof(usb_buf), NULL);
		if (ret) {
			LOG_WRN("HID write error, %d", ret);
			int i;
			for (i = 0; ret == -EAGAIN && i < CONFIG_BTHP_USB_RETRIES; i++) {
				LOG_WRN("retry");
				ret = hid_int_ep_write(hid_dev, usb_buf, sizeof(usb_buf), NULL);
	    	}
			if (ret == -EAGAIN && i >= CONFIG_BTHP_USB_RETRIES){
				LOG_ERR("dropped usb retries");
			}
		}
	}
}