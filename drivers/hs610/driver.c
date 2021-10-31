#include "drivers/hs610/hs610.h"

#define DEVICE_NAME "HS610"

// This is the size of the data that we are expecting to receive.
// You can actually observe it in realtime by using the usbhid-dump command
// ie: sudo usbhid-dump -es -m [your device's VID]
// More info: https://digimend.github.io/support/howto/trbl/diagnostics/
#define REPORT_SIZE                 12

#define REPORT_PEN_MASK             0x08
#define REPORT_PEN_IN_RANGE_MASK    0x80
#define REPORT_PEN_TOUCH_MASK       0x01
#define REPORT_PEN_BTN_STYLUS       0x02
#define REPORT_PEN_BTN_STYLUS2      0x04

#define REPORT_FRAME_ID             0xe0
#define REPORT_DIAL_ID              0xf0

#define WHEEL_STEP                  1

#define FORM_24BIT(a, b, c)         ((int32_t)(a) << 16 | (int32_t)(b) << 8 | (int32_t)(c))
#define FORM_16BIT(a, b)            ((uint16_t)(a) << 8 | (uint16_t)(b))

#define MAX_POS                     51000

// tl;dr, the scroll wheel goes from 0x00 to 0x0d (12 positions) and
// the wacom driver can only take values from 0 to 71. The comparison
// at the start is just to orientate the wheel from left to right
#define CONVERT_RAW_DIAL(_val)      (_val > 6 ? (19 - _val) : (7 - _val)) * 71 / 12

#ifdef VALIDATE
#undef VALIDATE
#define VALIDATE(_expr, _fmt, _args...)                             \
{                                                                   \
    if(!(_expr))                                                    \
    {                                                               \
        __ERROR(_fmt, ##_args);                                     \
        return -1;                                                  \
    }                                                               \
}
#endif

// Send event to device using fd
#define SEND_INPUT_EVENT(_dev, _type, _code, _value)                \
{                                                                   \
    ret = libevdev_uinput_write_event(_dev, _type, _code, _value);  \
    if(ret < 0)                                                     \
    {                                                               \
        __EVDEV_CATCHER(ret, "canno send input event");             \
        return -1;                                                  \
    }                                                               \
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

const char *hs610_get_device_name()
{
    return DEVICE_NAME;
}

int hs610_process_raw_input(const struct raw_input_data_t *data)
{
    int ret = 0;
    bool pen_present = false;
    size_t i = 0, x = 0;
    uint8_t status = 0, report_type = 0;
    int32_t x_pos = 0, y_pos = 0;
    static int32_t x_opos = 0, y_opos = 0, pres = 0;

    static int32_t scroll_wheel_buffer = 0;

    VALIDATE(data->data != NULL, "cannot process NULL data");
    VALIDATE(data->pad_device >= 0, "invalid virtual pad device");
    VALIDATE(data->pen_device >= 0, "invalid virtual pen device");

    if(data->size < REPORT_SIZE) return 0;
    status = data->data[0];
    report_type = data->data[1];

    // If you received a pen report...
    if(CHECK_MASK(status, REPORT_PEN_MASK))
    {
#ifndef NDEBUG
        __INFO("recevied pen report (%02X)", status);
#endif
        pen_present = report_type & REPORT_PEN_IN_RANGE_MASK;
        x_pos = FORM_24BIT(0, data->data[3], data->data[2]);
        y_pos = FORM_24BIT(0, data->data[6], data->data[5]);
        pres = FORM_24BIT(0, data->data[9], data->data[8]);

        // https://01.org/linuxgraphics/gfx-docs/drm/input/uinput.html
        if(data->use_virtual_cursor && data->mouse_device != NULL && pen_present)
        {
            SEND_INPUT_EVENT(data->mouse_device, EV_REL, REL_X, (int)(data->cursor_speed*((float)(x_pos - x_opos)/MAX_POS)));
            SEND_INPUT_EVENT(data->mouse_device, EV_REL, REL_Y, (int)(data->cursor_speed*((float)(y_pos - y_opos)/MAX_POS)));
            SEND_INPUT_EVENT(data->mouse_device, EV_SYN, SYN_REPORT, 1);

            SEND_INPUT_EVENT(data->mouse_device, EV_KEY, BTN_LEFT, ((report_type & REPORT_PEN_TOUCH_MASK) != 0));
            SEND_INPUT_EVENT(data->mouse_device, EV_KEY, BTN_RIGHT, ((report_type & REPORT_PEN_BTN_STYLUS) != 0));
            SEND_INPUT_EVENT(data->mouse_device, EV_KEY, BTN_MIDDLE, ((report_type & REPORT_PEN_BTN_STYLUS2) != 0));
            SEND_INPUT_EVENT(data->mouse_device, EV_SYN, SYN_REPORT, 1);

            x_opos = x_pos;
            y_opos = y_pos;
        }
        else
        {
            // If it's in range, send its coordinates and report_type to the virtual pen
            if(pen_present)
            {
                // Send X and Y coordinates value
                SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_X, x_pos);
                SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_Y, y_pos);

                // Send pressure readings
                SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_PRESSURE, pres);

                // Send tilt readinds
                SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_TILT_X, (int8_t)data->data[10]);
                SEND_INPUT_EVENT(data->pen_device, EV_ABS, ABS_TILT_X, -(int8_t)data->data[11]);    // Y axis needs to be inverted so it point would
                                                                                                    // point to the opposite direction
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
    }
    else
    {
        switch (report_type)
        {
        case REPORT_FRAME_ID:
        {
#ifndef NDEBUG
            __INFO("recevied frame report (%02X)", report_type);
#endif
            // Each bit in btn_pressed represents the state of a
            // button, thus why we use 16 buttons in this case
            uint16_t btns_pressed = FORM_16BIT(data->data[5], data->data[4]);

            // I don't know what this is for, but I guess that it
            // tells the virtual device a button has been pressed?
            SEND_INPUT_EVENT(data->pad_device, EV_ABS, ABS_MISC, btns_pressed ? 15 : 0)

            // Go through all the bits of btns_pressed
            for(i = 0; i < (sizeof(btns_pressed) *8); btns_pressed >>=1, i++)
                SEND_INPUT_EVENT(data->pad_device, EV_KEY, btn_codes[i], btns_pressed & 0x01);
            
            if(data->config_available && data->keyboard_device != NULL)
            {
                btns_pressed = FORM_16BIT(data->data[5], data->data[4]);

                for(i = 0; i < (sizeof(btns_pressed) *8); btns_pressed >>=1, i++)
                {
                    if(!(btns_pressed & 0x01)) continue;

                #define CATCH_BUTTON(_id, _b)                   \
                    case _id:                                   \
                        x = _b;                                 \
                        break;
                    switch (i)
                    {
                        CATCH_BUTTON(0, INI_BUTTON_1_INDEX);
                        CATCH_BUTTON(1, INI_BUTTON_2_INDEX);
                        CATCH_BUTTON(2, INI_BUTTON_3_INDEX);
                        CATCH_BUTTON(3, INI_BUTTON_4_INDEX);
                        CATCH_BUTTON(4, INI_BUTTON_5_INDEX);
                        CATCH_BUTTON(5, INI_BUTTON_6_INDEX);
                        CATCH_BUTTON(6, INI_BUTTON_7_INDEX);
                        CATCH_BUTTON(7, INI_BUTTON_8_INDEX);
                        CATCH_BUTTON(8, INI_BUTTON_9_INDEX);
                        CATCH_BUTTON(9, INI_BUTTON_10_INDEX);
                        CATCH_BUTTON(10, INI_BUTTON_11_INDEX);
                        CATCH_BUTTON(11, INI_BUTTON_12_INDEX);
                        CATCH_BUTTON(12, INI_BUTTON_13_INDEX);
                        CATCH_BUTTON(13, INI_BUTTON_14_INDEX);
                        CATCH_BUTTON(14, INI_BUTTON_15_INDEX);
                        CATCH_BUTTON(15, INI_BUTTON_16_INDEX);
                    default:
                        break;
                    }
                #undef CATCH_BUTTON

                    if(ini_item_is_populated(x) && (ret = simulate_key_presses(data->keyboard_device, 
                        ini_get_item(x, const char *)
                    )) < 0) 
                    {
                        __EVDEV_CATCHER(ret,  "cannot simulate key press");
                        return ret;
                    }
                }
            }
            
            // Also dunno what this is for
            SEND_INPUT_EVENT(data->pad_device, EV_SYN, SYN_REPORT, 1);
            break;
        }
        case REPORT_DIAL_ID:
        {
#ifndef NDEBUG
        __INFO("recevied dial report (%02X)", status);
#endif
            // The scrolling wheel goes from 0x00 to 0x00d
            int32_t dial_value = data->data[5];
            if(!data->use_virtual_wheel)
            {    
                if(dial_value != 0)
                    dial_value = CONVERT_RAW_DIAL(dial_value);

                // https://github.com/DIGImend/digimend-kernel-drivers/issues/275#issuecomment-667822380
                SEND_INPUT_EVENT(data->pad_device, EV_ABS, ABS_MISC, dial_value > 0 ? 15 : 0);
                SEND_INPUT_EVENT(data->pad_device, EV_ABS, ABS_WHEEL, dial_value);
                SEND_INPUT_EVENT(data->pad_device, EV_SYN, SYN_REPORT, 1);
            }
            else if (dial_value != 0 && data->mouse_device != NULL)
            {
                int step = WHEEL_STEP;
                if(dial_value < scroll_wheel_buffer || (scroll_wheel_buffer == 0x01 && dial_value == 0x0d) || (scroll_wheel_buffer == 0x0d && dial_value == 0x01))
                    step = -step;
                SEND_INPUT_EVENT(data->mouse_device, EV_REL, REL_WHEEL, step);
                SEND_INPUT_EVENT(data->mouse_device, EV_SYN, SYN_REPORT, 1);
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