#include "drivers/generic/generic.h"

#define SET_ABS_PROPERTY(_code, _value, _min, _max, _res)   \
    abs_setup = (struct uinput_abs_setup){};                \
    abs_setup.code = _code;                                 \
    abs_setup.absinfo.value = _value;                       \
    abs_setup.absinfo.minimum = _min;                       \
    abs_setup.absinfo.maximum = _max;                       \
    abs_setup.absinfo.resolution = _res;                    \
    ret = ioctl(fd, UI_ABS_SETUP, &abs_setup);              \

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

static const uint8_t abs_codes[] =
{
    ABS_X,
    ABS_Y,
    ABS_Z,
    ABS_RZ,
    ABS_THROTTLE,
    ABS_WHEEL,
    ABS_PRESSURE,
    ABS_DISTANCE,
    ABS_TILT_X,
    ABS_TILT_Y,
    ABS_MISC
};

int generic_create_virtual_pen(struct input_id *id, const char *name)
{
    int fd = -1, ret = 0;
    size_t size = 0, i = 0;
    struct uinput_setup uinput_setup;
    struct uinput_abs_setup abs_setup;

    VALIDATE(id != NULL, "cannot use invalid id for pen");
    do
    {
        fd = open(FAKETABLETD_UINPUT_PATH, FAKETABLETD_UINTPUT_OFLAGS);
        if(fd < 0) { ret = fd; break; }

        // Enable events
        size = GET_LEN(evt_codes);
        for(i = 0; i < size && ret >= 0; i++)
            ret = ioctl(fd, UI_SET_EVBIT, evt_codes[i]);
        if(ret < 0) break;
        
        // Enable buttons
        size = GET_LEN(btn_codes);
        for(i = 0; i < size && ret >= 0; i++)
            ret = ioctl(fd, UI_SET_KEYBIT, btn_codes[i]);
        if(ret < 0) break;
        
        // Enable absolute values
        size = GET_LEN(abs_codes);
        for(i = 0; i < size && ret >= 0; i++)
            ret = ioctl(fd, UI_SET_ABSBIT, abs_codes[i]);
        if(ret < 0) break;

        // Enable remaining devices
        ret = ioctl(fd, UI_SET_ABSBIT, REL_WHEEL);      if(ret < 0) break;
        ret = ioctl(fd, UI_SET_MSCBIT, MSC_SERIAL);     if(ret < 0) break;
        
        // Setup absolute values
        SET_ABS_PROPERTY(ABS_X, 0, 0, 50800, 200);      if(ret < 0) break;
        SET_ABS_PROPERTY(ABS_Y, 0, 0, 31750, 200);      if(ret < 0) break;
        SET_ABS_PROPERTY(ABS_PRESSURE, 0, 0, 8191, 0);  if(ret < 0) break;
        SET_ABS_PROPERTY(ABS_TILT_X, 0, -60, 60, 0);    if(ret < 0) break;
        SET_ABS_PROPERTY(ABS_TILT_Y, 0, -60, 60, 0);    if(ret < 0) break;
        
        // Make sure the name is not too long for the setup
        size  = strlen(name);
        size = size > GET_LEN(uinput_setup.name) ? GET_LEN(uinput_setup.name) : size;

        // Assing id and name to virtual device
        uinput_setup = (struct uinput_setup){};
        memcpy(uinput_setup.name, name, size);
        memcpy(&uinput_setup.id, id, sizeof(struct input_id));
        ret = ioctl(fd, UI_DEV_SETUP, &uinput_setup); if(ret < 0) break;

        // Create the virtual device
        ret = ioctl(fd, UI_DEV_CREATE);                 if(ret < 0) break;

        return fd;
    }while(0);

    if(ret < 0)
        close(fd);
    __STD_CATCHER_CRITICAL(ret, "cannot create pad device");
    
    return ret;
}