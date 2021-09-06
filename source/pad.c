#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>

#include <fcntl.h>
#include <linux/uinput.h>

#include "faketabletd.h"
#include "utilities.h"

#define SET_ABS_PROPERTY(_code, _value, _min, _max) \
    abs_setup = (struct uinput_abs_setup){};        \
    abs_setup.code = _code;                         \
    abs_setup.absinfo.value = _value;               \
    abs_setup.absinfo.minimum = _min;               \
    abs_setup.absinfo.maximum = _max;               \
    ret = ioctl(fd, UI_ABS_SETUP, &abs_setup);      \

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

static const int abs_codes[] =
{
    ABS_X,
    ABS_Y,
    ABS_WHEEL,
    ABS_MISC
};

int create_virtual_pad(struct input_id *id, const char *name)
{
    int fd = -1, ret = 0;
    size_t size = 0, i = 0;
    struct uinput_setup uinput_setup;
    struct uinput_abs_setup abs_setup;

    VALIDATE(id != NULL, "cannot use invalid id for pad");
    do
    {
        fd = open(FAKETABLETD_UINPUT_PATH, FAKETABLETD_UINTPUT_OFLAGS);
        if(fd < 0) { ret = fd; break; }

        // Enable events
        for(i = 0; i < GET_LEN(evt_codes) && ret >= 0; i++)
            ret = ioctl(fd, UI_SET_EVBIT, evt_codes[i]);
        if(ret < 0) break;
        
        // Enable buttons
        for(i = 0; i < GET_LEN(btn_codes) && ret >= 0; i++)
            ret = ioctl(fd, UI_SET_KEYBIT, btn_codes[i]);
        if(ret < 0) break;
        
        // Enable absolute values
        for(i = 0; i < GET_LEN(abs_codes) && ret >= 0; i++)
            ret = ioctl(fd, UI_SET_ABSBIT, abs_codes[i]);
        if(ret < 0) break;
        
        // Setup absolute values
        SET_ABS_PROPERTY(ABS_X, 0, 0, 1);       if(ret < 0) break;
        SET_ABS_PROPERTY(ABS_Y, 0, 0, 1);       if(ret < 0) break;
        SET_ABS_PROPERTY(ABS_WHEEL, 0, 0, 71);  if(ret < 0) break;
        SET_ABS_PROPERTY(ABS_MISC, 0, 0, 0);    if(ret < 0) break;
        
        // Make sure the name is not too long for the setup
        size = strlen(name);
        size = size > GET_LEN(uinput_setup.name) ? GET_LEN(uinput_setup.name) : size;

        // Assing id and name to virtual device
        uinput_setup = (struct uinput_setup){};
        memcpy(uinput_setup.name, name, size);
        memcpy(&uinput_setup.id, id, sizeof(struct input_id));
        ret = ioctl(fd, UI_DEV_SETUP, &uinput_setup); if(ret < 0) break;

        // Create the virtual device
        ret = ioctl(fd, UI_DEV_CREATE);

        return fd;
    }while(0);

    if(ret < 0)
        close(fd);
    __STD_CATCHER_CRITICAL(ret, "cannot create pad device");
    
    return ret;
}