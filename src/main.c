#include <zephyr/types.h>
#include <sys/printk.h>

#include <bthidproxy/usb.h>
#include <bthidproxy/ble.h>

void main(void)
{
	int ret = 0;
	ret = usb_init();
	if (ret) {
		return;
	}
	ret = ble_init();
	if (ret) {
		return;
	}

	handle_ble();

	handle_usb();
}
