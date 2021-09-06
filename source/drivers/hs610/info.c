#include "drivers/hs610/hs610.h"

// This are the properties that this driver will give to
// the virtual devices
#define DEVICE_NAME "HS610"
#define VENDOR_ID   0x056a
#define PRODUCT_ID  0x0314
#define VERSION     0x0110
#define NAME        "Wacom Intuos Pro S"

struct input_id *hs610_get_device_id()
{
    static const struct input_id id = (const struct input_id){
        .bustype = BUS_USB,
        .vendor = VENDOR_ID,
        .product = PRODUCT_ID,
        .version = VERSION,
    };

    return (struct input_id *)&id;
}

const char *hs610_get_pad_name()
{
    return NAME " Pad";
}
const char *hs610_get_pen_name()
{
    return NAME " Pen";
}

const char *hs610_get_device_name()
{
    return DEVICE_NAME;
}