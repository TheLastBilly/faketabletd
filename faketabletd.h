#ifndef FAKETABLETD_H__
#define FAKETABLETD_H__

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <linux/hid.h>

#include "ini.h"

// Fake device info
#define FAKETABLETD_VID             0x5FE1
#define FAKETABLETD_PID             0x1234
#define FAKETABLETD_NAME            "FaketabletD"
#define FAKETABLETD_VERSION         0x0001

// Taken from https://github.com/DIGImend/digimend-kernel-drivers/blob/master/hid-ids.h
#define USB_VENDOR_ID_HUION		    0x256c
#define USB_DEVICE_ID_HUION_TABLET	0x006e
#define USB_DEVICE_ID_HUION_HS610	0x006d

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

#define DEFAULT_CURSOR_SPEED        5000

#ifndef FAKETABLETD_UINPUT_PATH
#define FAKETABLETD_UINPUT_PATH     "/dev/uinput"
#endif

#define CONFIG_FILE_NAME            "faketabletd.conf"
#define ETC_CONFIG_PATH             ("/etc/" CONFIG_FILE_NAME)

#define FAKETABLETD_UINTPUT_OFLAGS  (O_WRONLY | O_NONBLOCK)

#define INI_BUTTON_1_INDEX          0
#define INI_BUTTON_2_INDEX          1
#define INI_BUTTON_3_INDEX          2
#define INI_BUTTON_4_INDEX          3
#define INI_BUTTON_5_INDEX          4
#define INI_BUTTON_6_INDEX          5
#define INI_BUTTON_7_INDEX          6
#define INI_BUTTON_8_INDEX          7
#define INI_BUTTON_9_INDEX          8
#define INI_BUTTON_10_INDEX         9
#define INI_BUTTON_11_INDEX         10
#define INI_BUTTON_12_INDEX         11
#define INI_BUTTON_13_INDEX         12
#define INI_BUTTON_14_INDEX         13
#define INI_BUTTON_15_INDEX         14
#define INI_BUTTON_16_INDEX         15

#define INI_CURSOR_SPEED            16
#define INI_BUTTON_MAX     (INI_CURSOR_SPEED + 1)

// Object that we pass to the drivers
struct raw_input_data_t
{
    const uint8_t *data;
    size_t size;

    int pad_device;
    int pen_device;

    int mouse_device;
    int keyboard_device;

    int cursor_speed;
    bool use_virtual_cursor;
    bool use_virtual_wheel;

    bool config_available;
};

typedef int (*create_virtual_device_callback_t)(struct input_id *id, const char *name);
typedef int (*process_raw_input_callback_t)(const struct raw_input_data_t *raw_input_data);

bool validate_key_presses(const char keys[INI_STRING_SIZE]);
int simulate_key_presses(int fd, const char keys[INI_STRING_SIZE]);

#endif