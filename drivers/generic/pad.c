#include "drivers/generic/generic.h"

#define SET_ABS_PROPERTY(_code, _min, _max)         \
    libevdev_set_abs_minimum(device, _code, _min);  \
    libevdev_set_abs_maximum(device, _code, _max);

static const uint8_t evt_codes[] =
{
    EV_SYN,
    EV_KEY,
    EV_ABS,
};

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
    BTN_STYLUS
};

struct libevdev_uinput *generic_create_virtual_pad(struct input_id *id, const char *name)
{
    int ret = 0;
    size_t i = 0;
    struct libevdev *device = NULL;
    struct libevdev_uinput *udev = NULL;

    VALIDATE(id != NULL, "cannot use invalid id for pad");
    do
    {
        // Create uinput device
        device = libevdev_new();

        // Enable events
        for(i = 0; i < GET_LEN(evt_codes) && ret >= 0; i++)
            ret = libevdev_enable_event_type(device, evt_codes[i]);
        if(ret < 0) break;

        if((ret = libevdev_enable_event_code(device, EV_SYN, SYN_REPORT, NULL)) < 0) break;

        for(i = 0; i < GET_LEN(btn_codes) && ret >= 0; i++)
            ret = libevdev_enable_event_code(device, EV_KEY, btn_codes[i], NULL);
        if(ret < 0) break;
        
        // Setup absolute values
        SET_ABS_PROPERTY(ABS_X, 0, 1);
        SET_ABS_PROPERTY(ABS_Y, 0, 1);
        SET_ABS_PROPERTY(ABS_WHEEL, 0, 71);
        SET_ABS_PROPERTY(ABS_MISC, 0, 0);

        // Assing id and name to virtual device
        libevdev_set_name(device, name);
        libevdev_set_id_product(device, id->product);
        libevdev_set_id_vendor(device, id->vendor);

        // Create evdev device
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