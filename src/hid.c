#include <bthidproxy/hid.h>

#include <zephyr/types.h>

#include <bthidproxy/usb.h>

int hid_write(const uint8_t *data, uint32_t data_len){
    return usb_hid_write(data, data_len);
}