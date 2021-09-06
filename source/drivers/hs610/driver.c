#include "drivers/hs610/hs610.h"

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

#define WHEEL_STEP                  1

#define FORM_24BIT(a, b, c)         ((int32_t)(a) << 16 | (int32_t)(b) << 8 | (int32_t)(c))
#define FORM_16BIT(a, b)            ((uint16_t)(a) << 8 | (uint16_t)(b))

// tl;dr, the scroll wheel goes from 0x00 to 0x0d (12 positions) and
// the wacom driver can only take values from 0 to 71. The comparison
// at the start is just to orientate the wheel from left to right
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
    ev.time.tv_sec = 0;                                 \
    ev.time.tv_usec = 0;                                \
    __STD_CATCHER(                                      \
        ret = write(_fd, &ev, sizeof(ev)),              \
        "cannot send event data"                        \
    );                                                  \
    if(ret < 0) return -1;                              \
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

int hs610_process_raw_input(const struct raw_input_data_t *data)
{
    int ret = 0;
    size_t i = 0;
    uint8_t report_type = 0;
    struct input_event ev = (struct input_event){};

    static int32_t scroll_wheel_buffer = 0;

    VALIDATE(data->data != NULL, "cannot process NULL data");
    VALIDATE(data->pad_device >= 0, "invalid virtual pad device");
    VALIDATE(data->pen_device >= 0, "invalid virtual pen device");

    if(data->size < REPORT_SIZE || data->data[0] != REPORT_LEADING_BYTE) return 0;
    report_type = data->data[1];

    // If you received a pen report...
    if(CHECK_MASK(report_type, REPORT_PEN_MASK))
    {
        // If it's in range, send its coordinates and status to the virtual pen
        if(report_type & REPORT_PEN_IN_RANGE_MASK)
        {
            // Send X and Y coordinates value
            SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_X, FORM_24BIT(data->data[8], data->data[3], data->data[2]));
            SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_Y, FORM_24BIT(data->data[9], data->data[5], data->data[4]));

            // Send pressure readings
            SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_PRESSURE, FORM_24BIT(0, data->data[7], data->data[6]));

            // Send tilt readinds
            SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_TILT_X, (int8_t)data->data[10]);
            SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_TILT_X, -(int8_t)data->data[11]);    // Y axis needs to be inverted so it point would
                                                                                    // point to the opposite direction
            
            // Let the virtual pen know the real pen is present
            SEND_INPUT_EVENT(data->pen_device, EV_KEY, BTN_TOOL_PEN, 1);

            // Update the driver
            SEND_INPUT_EVENT(data->pen_device, EV_SYN, SYN_REPORT, 1);

            // Send button data
            SEND_INPUT_EVENT(data->pen_device, EV_KEY, BTN_TOUCH, ((report_type & REPORT_PEN_TOUCH_MASK) != 0));
            SEND_INPUT_EVENT(data->pen_device, EV_KEY, BTN_STYLUS, ((report_type & REPORT_PEN_BTN_STYLUS) != 0));
            SEND_INPUT_EVENT(data->pen_device, EV_KEY, BTN_STYLUS2, ((report_type & REPORT_PEN_BTN_STYLUS2) != 0));

            // Update the driver
            SEND_INPUT_EVENT(data->pen_device, EV_SYN, SYN_REPORT, 1);
        }
        // Otherwise, let the virtual pen know we are not touching the frame
        else
            SEND_INPUT_EVENT(data->pen_device, EV_KEY, BTN_TOOL_PEN, 0);

        // A serial number is required when sending button press
        // events from a pen device. Or somthing like that...
        SEND_INPUT_EVENT(data->pen_device, EV_MSC, MSC_SERIAL, 1098942556);
        SEND_INPUT_EVENT(data->pen_device, EV_SYN, SYN_REPORT, 1);
    }
    else
    {
        switch (report_type)
        {
        case REPORT_FRAME_ID:
        {
            // Each bit in btn_pressed represents the state of a
            // button, thus why we use 16 buttons in this case
            uint16_t btns_pressed = FORM_16BIT(data->data[5], data->data[4]);

            // I don't know what this is for, but I guess that it
            // tells the virtual device a button has been pressed?
            SEND_INPUT_EVENT(data->pad_device, EV_ABS, ABS_MISC, btns_pressed ? 15 : 0)

            // Go through all the bits of btns_pressed
            for(i = 0; i < (sizeof(btns_pressed) *8); btns_pressed >>=1, i++)
                SEND_INPUT_EVENT(data->pad_device, EV_KEY, btn_codes[i], btns_pressed & 0x01);
            
            // Also dunno what this is for
            SEND_INPUT_EVENT(data->pad_device, EV_SYN, SYN_REPORT, 1);
            break;
        }
        case REPORT_DIAL_ID:
        {
            // The scrolling wheel goes from 0x00 to 0x00d
            int32_t dial_value = data->data[5];
            if(data->mouse_device < 0)
            {    
                if(dial_value != 0)
                    dial_value = CONVERT_RAW_DIAL(dial_value);

                // https://github.com/DIGImend/digimend-kernel-drivers/issues/275#issuecomment-667822380
                SEND_INPUT_EVENT(data->pad_device, EV_ABS, ABS_MISC, dial_value > 0 ? 15 : 0);
                SEND_INPUT_EVENT(data->pad_device, EV_ABS, ABS_WHEEL, dial_value);
                SEND_INPUT_EVENT(data->pad_device, EV_SYN, SYN_REPORT, 1);
            }
            else if (dial_value != 0)
            {
                int step = WHEEL_STEP;
                if(dial_value < scroll_wheel_buffer || (scroll_wheel_buffer == 1 && dial_value == 0x0d))
                    step = -step;
                
                SEND_INPUT_EVENT(data->mouse_device, EV_REL, REL_WHEEL, step);
                SEND_INPUT_EVENT(data->mouse_device, EV_SYN, SYN_REPORT, 0);
                scroll_wheel_buffer = dial_value;
            }
            break;
        }
        
        default:
            return 0;
        }
    }

    return 0;
}