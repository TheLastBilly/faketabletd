#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

#include <fcntl.h>
#include <linux/uinput.h>

#include "faketabletd.h"
#include "utilities.h"

static const uint8_t evt_codes[] =
{
    EV_SYN,
    EV_KEY,
    EV_REL,
    EV_ABS,
    EV_MSC
};

static const uint16_t btn_codes[] = 
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
    
};

int create_virtual_pad(struct input_id *id)
{
    int fd = -1;
    size_t size = 0;
    struct uinput_abs_setup abs_setup;
    struct uinput_setup input_setup;

    if(id == NULL)
        return -1;

    __STD_CATCHER_CRITICAL(
        fd = open(FAKETABLETD_UINPUT_PATH, FAKETABLETD_UINTPUT_OFLAGS),
        "cannot open \"%s\"", FAKETABLETD_UINPUT_PATH
    );



    return 0;
}