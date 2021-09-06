#include <stdio.h>
#include <unistd.h>

#include <linux/uinput.h>

#include "faketabletd.h"
#include "utilities.h"

// This is the size of the data that we are expecting to receive.
// You can actually observe it in realtime by using the usbhid-dump command
// ie: sudo usbhid-dump -es -m [your device's VID]
// More info: https://digimend.github.io/support/howto/trbl/diagnostics/
#define REPORT_SIZE                 12

#define REPORT_LEADING_BYTE         0x08

#define REPORT_PEN_MASK             0x70
#define REPORT_PEN_IN_RANGE_MASK    0x80
#define REPORT_PEN_TOUCH_MASK       0x01
#define REPORT_PEN_BTN_STYLUS       0x02
#define REPORT_PEN_BTN_STYLUS2      0x04

#define REPORT_FRAME_ID             0xe0
#define REPORT_DIAL_ID              0xf0

#define FORM_24BIT(a, b, c)         ((int32_t)(a) << 16 | (int32_t)(b) << 8 | (int32_t)(c))
#define FORM_16BIT(a, b)            ((uint16_t)(a) << 8 | (uint16_t)(b))

#define CONVERT_RAW_DIAL(_val)      (_val > 6 ? (19 - _val) : (7 - _val)) * 71 / 12

#ifdef VALIDATE
#undef VALIDATE
#define VALIDATE(_expr, _fmt, _args...)                 \
{                                                       \
    if(!(_expr))                                        \
    {                                                   \
        __ERROR(_fmt, ##_args);                         \
        return -1;                                      \
    }                                                   \
}
#endif

// Send event to device using fd
#define SEND_INPUT_EVENT(_fd, _type, _code, _value)     \
{                                                       \
    ev.type = _type;                                    \
    ev.code = _code;                                    \
    ev.value = _value;                                  \
    __STD_CATCHER(                                      \
        ret = write(_fd, &ev, sizeof(ev)),              \
        "cannot send event data"                        \
    );                                                  \
    if(ret < 0) return 0;                               \
}

static const uint32_t btn_codes[] = 
{
    BTN_0,
    BTN_1,
    BTN_2,
    BTN_3,
    BTN_4,
    BTN_5,
    BTN_6,
    BTN_7,
    BTN_8,
    BTN_9,
    BTN_A,
    BTN_B,
    BTN_C,
    BTN_X, 
    BTN_Y, 
    BTN_Z
};

int process_raw_input(const uint8_t *data, size_t size, int pad_device, int pen_device)
{
    int ret = 0;
    size_t i = 0;
    uint8_t report_type = 0;
    struct input_event ev;

    VALIDATE(data != NULL, "cannot process NULL data");
    VALIDATE(pad_device >= 0, "invalid virtual pad device");
    VALIDATE(pen_device >= 0, "invalid virtual pen device");

    if(size < REPORT_SIZE || data[0] != REPORT_LEADING_BYTE) return 0;
    report_type = data[1];

    // If you received a pen report...
    if(CHECK_MASK(report_type, REPORT_PEN_MASK))
    {
        // If it's in range, send its coordinates and status to the virtual pen
        if(report_type & REPORT_PEN_IN_RANGE_MASK)
        {
            // Send X and Y coordinates value
            SEND_INPUT_EVENT(pen_device, EV_ABS, ABS_X, FORM_24BIT(data[8], data[3], data[2]));
            SEND_INPUT_EVENT(pen_device, EV_ABS, ABS_Y, FORM_24BIT(data[9], data[5], data[4]));

            // Send pressure readings
            SEND_INPUT_EVENT(pen_device, EV_ABS, ABS_PRESSURE, FORM_24BIT(0, data[7], data[6]));

            // Send tilt readinds
            SEND_INPUT_EVENT(pen_device, EV_ABS, ABS_TILT_X, (int8_t)data[10]);
            SEND_INPUT_EVENT(pen_device, EV_ABS, ABS_TILT_X, -(int8_t)data[11]);    // Y axis needs to be inverted so it point would
                                                                                    // point to the opposite direction
            
            // Let the virtual pen know the real pen is present
            SEND_INPUT_EVENT(pen_device, EV_KEY, BTN_TOOL_PEN, 1);

            // Send button data
            SEND_INPUT_EVENT(pen_device, EV_KEY, BTN_TOUCH, ((report_type & REPORT_PEN_TOUCH_MASK) != 0));
            SEND_INPUT_EVENT(pen_device, EV_KEY, BTN_STYLUS, ((report_type & REPORT_PEN_BTN_STYLUS) != 0));
            SEND_INPUT_EVENT(pen_device, EV_KEY, BTN_STYLUS2, ((report_type & REPORT_PEN_BTN_STYLUS2) != 0));
        }
        // Otherwise, let the virtual pen know we are not touching the frame
        else
            SEND_INPUT_EVENT(pen_device, EV_KEY, BTN_TOOL_PEN, 0);

        // Don't know what these two are for, but they seem to be needed,
        // there is some info on them here tho
        // https://github.com/linuxwacom/input-wacom/wiki/Kernel-Input-Event-Overview
        SEND_INPUT_EVENT(pen_device, EV_MSC, MSC_SERIAL, 1098942556);
        SEND_INPUT_EVENT(pen_device, EV_SYN, SYN_REPORT, 1);
    }
    else
    {
        switch (report_type)
        {
        case REPORT_FRAME_ID:
        {
            // Each bit in btn_pressed represents the state of a
            // button, thus why we use 16 buttons in this case
            uint16_t btns_pressed = FORM_16BIT(data[5], data[4]);

            // I don't know what this is for, but I guess that it
            // tells the virtual device a button has been pressed?
            SEND_INPUT_EVENT(pad_device, EV_ABS, ABS_MISC, btns_pressed ? 15 : 0)

            // Go through all the bits of btns_pressed
            for(i = 0; i < (sizeof(btns_pressed) *8); btns_pressed >>=1, i++)
                SEND_INPUT_EVENT(pad_device, EV_KEY, btn_codes[i], btns_pressed & 0x01);
            
            // Also dunno what this is for
            SEND_INPUT_EVENT(pad_device, EV_SYN, SYN_REPORT, 1);
            break;
        }
        
        case REPORT_DIAL_ID:
        {
            int32_t dial_value = data[5];
            if(dial_value != 0)
                dial_value = CONVERT_RAW_DIAL(dial_value);
            
            SEND_INPUT_EVENT(pad_device, EV_ABS, ABS_MISC, dial_value ? 15 : 0);

            SEND_INPUT_EVENT(pad_device, EV_ABS, ABS_WHEEL, dial_value);
            SEND_INPUT_EVENT(pad_device, EV_SYN, SYN_REPORT, 1);
            break;
        }
        
        default:
            return 0;
        }
    }

    return 0;
}