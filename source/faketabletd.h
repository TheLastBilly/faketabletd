#ifndef FAKETABLETD_H__
#define FAKETABLETD_H__

#include <stdlib.h>
#include <stdint.h>
#include <linux/hid.h>

// Taken from https://github.com/DIGImend/digimend-kernel-drivers/blob/master/hid-ids.h
#define USB_VENDOR_ID_HUION		    0x256c
#define USB_DEVICE_ID_HUION_TABLET	0x006e
#define USB_DEVICE_ID_HUION_HS610	0x006d

#define FAKETABLETD_FAKE_VENDOR_ID  0x056a
#define FAKETABLETD_FAKE_PRODUCT_ID 0x0314
#define FAKETABLETD_FAKE_VERSION    0x0110
#define FAKETABLETD_FAKE_NAME       "Wacom Intuos Pro S"

// Used for HID devices. You can read more on this
// by going into the following link and looking for "00100001"
// https://www.usb.org/sites/default/files/hid1_11.pdf
#define HID_SET_REQUEST_TYPE        0x21

// Also defined in the document described above
#define HID_SET_IDLE                0x0a
#define HID_SET_PROTOCOL            0x0a
#define     HID_SET_PROTOCOL_BOOT   0
#define     HID_SET_PROTOCOL_REPORT 1
#define HID_TIMEOUT                 1000
#define HID_BUFFER_SIZE             0x40
#define HID_ENDPOINT                0x81

#ifndef FAKETABLETD_UINPUT_PATH
#define FAKETABLETD_UINPUT_PATH     "/dev/uinput"
#endif

#define FAKETABLETD_UINTPUT_OFLAGS  (O_WRONLY | O_NONBLOCK)

int create_virtual_pad(struct input_id *id, const char *name);
int create_virtual_pen(struct input_id *id, const char *name);

int process_raw_input(const uint8_t *data, size_t size, int pad_device, int pen_device);

#endif