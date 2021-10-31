#include "drivers/generic/generic.h"

#define SET_ABS_PROPERTY(_code, _min, _max, _res)       \
    libevdev_set_abs_minimum(device, _code, _min);      \
    libevdev_set_abs_maximum(device, _code, _max);      \
    libevdev_set_abs_resolution(device, _code, _res);

static const uint8_t evt_codes[] =
{
    EV_SYN,
    EV_KEY,
    EV_REL,
    EV_ABS,
    EV_MSC
};

static const uint32_t btn_codes[] = 
{
    BTN_LEFT,
    BTN_RIGHT,
    BTN_MIDDLE,
    BTN_SIDE,
    BTN_EXTRA,
    BTN_TOOL_PEN,
    BTN_TOOL_RUBBER,
    BTN_TOOL_BRUSH,
    BTN_TOOL_PENCIL,
    BTN_TOOL_AIRBRUSH,
    BTN_TOOL_MOUSE,
    BTN_TOOL_LENS,
    BTN_TOUCH,
    BTN_STYLUS,
    BTN_STYLUS2
};

struct libevdev_uinput *generic_create_virtual_pen(struct input_id *id, const char *name)
{
    int ret = 0;
    size_t i = 0;
    struct libevdev *device = NULL;
    struct libevdev_uinput *udev = NULL;

    VALIDATE(id != NULL, "cannot use invalid id for pen");
    do
    {
        device = libevdev_new();

        // Enable events
        for(i = 0; i < GET_LEN(evt_codes) && ret >= 0; i++)
            ret = libevdev_enable_event_type(device, evt_codes[i]);
        if(ret < 0) break;

        if((ret = libevdev_enable_event_code(device, EV_SYN, SYN_REPORT, NULL)) < 0) break;

        for(i = 0; i < GET_LEN(btn_codes) && ret >= 0; i++)
            ret = libevdev_enable_event_code(device, EV_KEY, btn_codes[i], NULL);
        if(ret < 0) break;

        // Enable remaining devices
        ret = libevdev_enable_event_code(device, EV_REL, REL_WHEEL, NULL);      if(ret < 0) break;
        ret = libevdev_enable_event_code(device, EV_MSC, MSC_SERIAL, NULL);     if(ret < 0) break;
        
        // Setup absolute values
        SET_ABS_PROPERTY(ABS_X, 0, 50800, 200);
        SET_ABS_PROPERTY(ABS_Y, 0, 31750, 200);
        SET_ABS_PROPERTY(ABS_PRESSURE, 0, 8191, 0);
        SET_ABS_PROPERTY(ABS_TILT_X, -60, 60, 0);
        SET_ABS_PROPERTY(ABS_TILT_Y, -60, 60, 0);
        
        // Assing id and name to virtual device
        libevdev_set_name(device, name);
        libevdev_set_id_product(device, id->product);
        libevdev_set_id_vendor(device, id->vendor);

        ret = libevdev_uinput_create_from_device(device, LIBEVDEV_UINPUT_OPEN_MANAGED, &udev);
        if(ret < 0) break;

        libevdev_free(device);
        return udev;
    }while(0);

    if(ret < 0)
    {
        if(udev != NULL)
            libevdev_uinput_destroy(udev);
        
        else if(device != NULL)
            libevdev_free(device);
    }
    __EVDEV_CATCHER_CRITICAL(ret, "cannot create pad device");
    
    return udev;
}