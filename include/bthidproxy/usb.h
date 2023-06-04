#pragma once

#include <zephyr/types.h>

/**
 * @brief BOOT HID keyboard report descriptor.
 */
#define HID_BOOT_KEYBOARD_REPORT_DESC() {				\
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),				\
	HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),			\
	HID_COLLECTION(HID_COLLECTION_APPLICATION),			\
		HID_REPORT_SIZE(1),                       		\
		HID_REPORT_COUNT(8),							\
		HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD),   \
		HID_USAGE_MIN8(224), 							\
		HID_USAGE_MAX8(231),							\
		HID_LOGICAL_MIN8(0),							\
		HID_LOGICAL_MAX8(1),							\
		HID_INPUT(0x02),								\
		HID_REPORT_COUNT(1),							\
		HID_REPORT_SIZE(8),								\
		HID_INPUT(0x03),								\
		HID_REPORT_COUNT(6),							\
		HID_REPORT_SIZE(8),								\
		HID_LOGICAL_MIN8(0),							\
		HID_LOGICAL_MAX8(255),							\
		HID_USAGE_MIN8(0),								\
		HID_USAGE_MAX8(255),							\
		HID_INPUT(0x00),								\
	HID_END_COLLECTION,									\
}

int usb_hid_write(const uint8_t *data, uint32_t data_len);
int usb_init();
void handle_usb();