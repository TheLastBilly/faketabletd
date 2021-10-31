
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>

#include <hidapi/hidapi.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <signal.h>
#include <linux/uinput.h>
#include <libusb-1.0/libusb.h>

#include "faketabletd.h"
#include "utilities.h"

#include "drivers/hs610/hs610.h"

#define USE_RETURNING_CALLBACK(_cb, ...)                        \
{                                                               \
    VALIDATE(                                                   \
        _cb != NULL,                                            \
        "cannot call empty callback \"" #_cb "\""               \
    );                                                          \
    return _cb(__VA_ARGS__);                                    \
}

#define CLOSE_UINPUT_DEVICE(fd)                                 \
{                                                               \
    if(fd >= 0)                                                 \
    {                                                           \
        ioctl(fd, UI_DEV_DESTROY);                              \
        close(fd);                                              \
        fd = -1;                                                \
    }                                                           \
}

static hid_device *handle;
static struct hid_device_info *device_info, *current_device;
static unsigned char hid_buffer[HID_BUFFER_SIZE];

static volatile int pen_device, pad_device, mouse_device, keyboard_device;

static size_t devices_detected;
static uint8_t *transfer_buffer;

static int cursor_speed;
static bool use_virtual_cursor;
static bool use_virtual_wheel;

// Device setup callbacks
create_virtual_device_callback_t create_virtual_pad_callback;
create_virtual_device_callback_t create_virtual_pen_callback;

// Parsing callbacks
process_raw_input_callback_t process_raw_input_callback;

REGISTER_MUTEX_VARIABLE(bool, should_close);
REGISTER_MUTEX_VARIABLE(bool, should_reset);
REGISTER_MUTEX_VARIABLE(bool, should_use_config);

// Callback handlers
static inline int create_virtual_pad(struct input_id *id, const char *name)
{ USE_RETURNING_CALLBACK(create_virtual_pad_callback, id, name); }
static inline int create_virtual_pen(struct input_id *id, const char *name)
{ USE_RETURNING_CALLBACK(create_virtual_pen_callback, id, name); }

static inline int process_raw_input(const struct raw_input_data_t *data)
{ USE_RETURNING_CALLBACK(process_raw_input_callback, data); }

// faketablet id
static const struct input_id faketabletd_id = (const struct input_id)
{
    .bustype    = BUS_USB,
    .vendor     = FAKETABLETD_VID,
    .product    = FAKETABLETD_PID,
    .version    = FAKETABLETD_VERSION,
};
static const struct input_id wacom_id = (const struct input_id)
{
    .bustype    = BUS_USB,
    .vendor     = 0x056a,
    .product    = 0x0314,
    .version    = 0x0110,
};

// Signal handlers
static void signal_handler(int sig)
{
    __INFO("termination signal detected!");

    set_should_close(true);
    set_should_reset(false);
}

// Device name for the specified vendor and product id. Return NULL if
// the specified device is not supported
static const char *setup_device(uint16_t vendor_id, uint16_t product_id)
{
    switch (vendor_id)
    {
    // For HUION devices
    case USB_VENDOR_ID_HUION:
        {
            switch (product_id)
            {
            case USB_DEVICE_ID_HUION_TABLET:
            case USB_DEVICE_ID_HUION_HS610:
                create_virtual_pad_callback = &generic_create_virtual_pad;
                create_virtual_pen_callback = &generic_create_virtual_pen;
                process_raw_input_callback = &hs610_process_raw_input;
                return hs610_get_device_name();
            default:
                break;
            }
        }
        break;
    
    default:
        break;
    }

    return NULL;
}

// We use this to simulate mouse scroll events for the scroll wheel
static int create_virtual_mouse()
{
    int ret = 0;
    struct uinput_setup input_setup = (struct uinput_setup){};

    __STD_CATCHER_CRITICAL(
        mouse_device = open(FAKETABLETD_UINPUT_PATH, FAKETABLETD_UINTPUT_OFLAGS),
        "cannot open virtual mouse file"
    );

#define __IOCTL( ...) ret = ioctl(mouse_device, __VA_ARGS__); if(ret < 0) break;
    do
    {
        // Setup mouse events, buttons, scroll wheel and movement
        __IOCTL(UI_SET_EVBIT, EV_KEY);
        __IOCTL(UI_SET_KEYBIT, BTN_LEFT);
        __IOCTL(UI_SET_KEYBIT, BTN_RIGHT);
        __IOCTL(UI_SET_KEYBIT, BTN_MIDDLE);
        __IOCTL(UI_SET_EVBIT, EV_REL);
        __IOCTL(UI_SET_RELBIT, REL_X);
        __IOCTL(UI_SET_RELBIT, REL_Y);
        __IOCTL(UI_SET_RELBIT, REL_WHEEL);
        __IOCTL(UI_SET_RELBIT, REL_WHEEL_HI_RES);

        input_setup.id.bustype = BUS_USB;
        input_setup.id.vendor = 0x1233;
        input_setup.id.product = FAKETABLETD_VID;
        strcpy(input_setup.name, FAKETABLETD_NAME " Mouse");

        __IOCTL(UI_DEV_SETUP, &input_setup);
        __IOCTL(UI_DEV_CREATE);

    } while (0);
#undef __IOCTL
    if(ret < 0)
    {
        close(mouse_device);
        __STD_CATCHER_CRITICAL(ret, "error creating virtual mouse");
    }
    
    return mouse_device;
}

// We use this to simulate keyboard presses
static int create_virtual_keyboard()
{
    int ret = 0;
    struct uinput_setup input_setup = (struct uinput_setup){};

    __STD_CATCHER_CRITICAL(
        keyboard_device = open(FAKETABLETD_UINPUT_PATH, FAKETABLETD_UINTPUT_OFLAGS),
        "cannot open virtual keyboard file"
    );

#define __IOCTL( ...) ret = ioctl(keyboard_device, __VA_ARGS__); if(ret < 0) break;
    do
    {
        // Setup mouse events, buttons, scroll wheel and movement
        __IOCTL(UI_SET_EVBIT, EV_KEY);
        for(int i = KEY_RESERVED; i < KEY_F24; i++)
        { __IOCTL(UI_SET_KEYBIT, i); }

        input_setup.id.bustype = BUS_USB;
        input_setup.id.vendor = 0x1232;
        input_setup.id.product = FAKETABLETD_VID;
        strcpy(input_setup.name, FAKETABLETD_NAME " Keyboard");

        __IOCTL(UI_DEV_SETUP, &input_setup);
        __IOCTL(UI_DEV_CREATE);

    } while (0);
#undef __IOCTL

    if(ret < 0)
    {
        close(keyboard_device);
        __STD_CATCHER_CRITICAL(ret, "error creating virtual keyboard");
    }
    
    return keyboard_device;
}

static void process_raw_report(const unsigned char *data)
{
    struct raw_input_data_t raw_input_data;

    if(data == NULL || get_should_close())
        return;


#ifndef NDEBUG
    __INFO("report: ");
    for(size_t s = 0; s < HID_BUFFER_SIZE; s++)
        printf("%02X ", data[s]);
    printf("\n");
#endif

    raw_input_data = (struct raw_input_data_t){
        .data = data,
        .size = HID_BUFFER_SIZE,

        .pad_device = pad_device,
        .pen_device = pen_device,

        .mouse_device = mouse_device,
        .keyboard_device = keyboard_device,

        .cursor_speed = cursor_speed,

        .use_virtual_cursor = use_virtual_cursor,
        .use_virtual_wheel = use_virtual_wheel,

        .config_available = get_should_use_config()
    };

    if(process_raw_input(&raw_input_data) < 0)
        set_should_close(true);
}

// Free allocated objects and deinitialize libusb
static void cleannup(void)
{
    set_should_close(false);

    CLOSE_UINPUT_DEVICE(keyboard_device);
    CLOSE_UINPUT_DEVICE(mouse_device);
    CLOSE_UINPUT_DEVICE(pen_device);
    CLOSE_UINPUT_DEVICE(pad_device);

    if(handle != NULL)
    {
        hid_close(handle);
        handle = NULL;
    }

    if(device_info != NULL)
    {
        hid_free_enumeration(device_info);
        device_info = NULL;
        current_device = NULL;
    }

    hid_exit();

    create_virtual_pad_callback = NULL;
    create_virtual_pen_callback = NULL;
    process_raw_input_callback = NULL;
}

static bool look_for_devices(const char **device_name)
{
    struct hid_device_info *info;
    
    devices_detected = 0;

    if(device_info != NULL) 
        hid_free_enumeration(device_info);

    // Get device list
    info = device_info = hid_enumerate(0,0);
    __HIDAPI_CATCHER_CRITICAL(
        handle,
        info != NULL ? 1 : -1, 
        "cannot retreive connected devices"
    );

    // Find compatible devices
    while(info != NULL)
    {
        // See if current device on the list is supported
        if((*device_name = setup_device(info->vendor_id, info->product_id)) != NULL)
        {
            current_device = info;
            return true;
        }

        info = info->next;
    }
    
    return false;
}

static const char *get_home_config_file()
{
    static char path[60] = {0};

    snprintf(path, 60, "%s/." CONFIG_FILE_NAME, getpwuid(getuid())->pw_dir);
    return path;
}

static void read_config()
{
    int i = 0;
    char label[INI_STRING_SIZE] = {0};
    const char *str = NULL;

    // Make sure config file data is empty, and register the expected items
    ini_clear_items();

    // Register buttons
    for(i = INI_BUTTON_1_INDEX; i < INI_BUTTON_MAX; i++)
    {
        snprintf(label, INI_STRING_SIZE, "pad_button_%d", i+1);
        ini_register_item(i, INI_TYPE_STRING, label);
    }

    snprintf(label, INI_STRING_SIZE, "cursor_speed");
    ini_register_item(INI_CURSOR_SPEED, INI_TYPE_INT, label);

    // Look for a directory where a config file might be, and parse it if you found it
    str = get_home_config_file();
    const char *config_paths[] = { str, ETC_CONFIG_PATH };
    if((str = check_paths(config_paths, 2)) == NULL)
        return;
    
    __INFO("detected configuration file on \"%s\"", str);
    set_should_use_config(false);
    __CATCHER_CRITICAL(ini_parse_file(str), "cannot parse file \"%s\"", str);
    __INFO("loaded configuration from \"%s\" successfuly", str);

    for(i = INI_BUTTON_1_INDEX; i < INI_BUTTON_MAX; i++)
    {
        if(!ini_item_is_populated(i)) continue;

        str = ini_get_item(i, const char*);
        __CATCHER_CRITICAL(validate_key_presses(str), "invalid binding on a config file \"%s\"", str);
    }

    if(ini_item_is_populated(INI_CURSOR_SPEED))
        cursor_speed = ini_get_item(INI_CURSOR_SPEED, int);
    
    set_should_use_config(true);
}

static inline void print_help()
{
    printf(
        "Usage: faketabletd [OPTION]\n"
        "User space driver for drawing tablets\n\n"
        
        "Options\n"
        "  -w\t\t\tEnables wacom tablet simulation support\n"
        "  -c\t\t\tEnables virtual mouse cursor emulation\n"
        "  -s\t\t\tEnables virtual mouse scrolling wheel emulation\n"
        "  -k\t\t\tEnables virtual keyboard emulation\n"
        "  -r\t\t\tResets the program back to the scanning phase on disconnect (experimental)\n\n"

        "Examples:\n"
        "  faketabletd -m\tRuns driver with virtual mouse emulation\n"
        "  faketabletd -mr\tRuns driver with virtual mouse emulation. Will no exit on disconnect\n"
    );
}

int main(int argc, char const **argv)
{
    // Local variables
    int ret = 0;
    bool 
        use_virtual_mouse = false, 
        use_virtual_keyboard = false,
        use_wacom = false;
    struct input_id *input_id;

    // Initialize locals
    transfer_buffer     = NULL;

    create_virtual_pad_callback = NULL;
    create_virtual_pen_callback = NULL;
    process_raw_input_callback = NULL;

    pen_device          = -1;
    pad_device          = -1;
    mouse_device        = -1;

    cursor_speed        = DEFAULT_CURSOR_SPEED;

    should_close = false;
    should_reset = false;

    const char* device_name = NULL;

    // Make sure we catch Ctrl-C when asked to terminate
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Make sure we clean our mess before we leave
    atexit(cleannup);

    // Get argument options
    while((ret = getopt(argc, (char* const*)argv, "scrhkw")) != -1)
    {
        switch (ret)
        {
        case 'w':
            use_wacom = true;
            break;
        case 's':
            use_virtual_wheel = true;
            break;
        case 'c':
            use_virtual_cursor = true;
            break;
        case 'k':
            use_virtual_keyboard = true;
            break;
        case 'r':
            set_should_reset(true);
            __WARNING("-r has been set, this is an experimental feature and is known to cause problems");
            break;
        case 'h':
            print_help();
            exit(0);
            break;
        default:
            print_help();
            exit(1);
            break;
        }
    }
    use_virtual_mouse = use_virtual_cursor || use_virtual_wheel;

    // Read config from config file
    read_config();

    while(1)
    {
        // Make sure we are clear to go on every cycle
        cleannup();

        // Initialize hidapi and look for devices
        __CATCHER_CRITICAL(hid_init(), "cannot initialize hidapi");
        __INFO("looking for compatible devices...");
        while(!get_should_close() && !look_for_devices(&device_name)) 
            SLEEP_FOR_US(100);
        if(get_should_close())
            break;

        // Let us know what you've found
        __INFO("found supported device: %s (%04x:%04x)", device_name, current_device->vendor_id, current_device->product_id);

        // Open device
        __INFO("connecting to device");
        handle = hid_open(current_device->vendor_id, current_device->product_id, NULL);
        __HIDAPI_CATCHER_CRITICAL(
            NULL,
            handle == NULL ? -1 : 1,
            "cannot open device!"
        );
        
        __INFO("connected!");

        __INFO("preparing device...");
        // Create virtual pen and pad
        if(use_wacom)
            input_id = (struct input_id *)&wacom_id;
        else
            input_id = (struct input_id *)&faketabletd_id;
        
        pad_device = create_virtual_pad(input_id, FAKETABLETD_NAME " Pad");
        pen_device = create_virtual_pen(input_id, FAKETABLETD_NAME " Pen");
    
        if(use_virtual_mouse)
            mouse_device = create_virtual_mouse();
        if(use_virtual_keyboard)
            keyboard_device = create_virtual_keyboard();

        __HIDAPI_CATCHER(
            handle,
            hid_set_nonblocking(handle, 1),
            "cannot set device to nonblocking"
        );
        __INFO("done configuring device!");

        __INFO("ready!");
        while(!get_should_close())
        {
            __HIDAPI_CATCHER_CRITICAL(
                handle,
                !get_should_close() ?
                    ret = hid_read(handle, hid_buffer, HID_BUFFER_SIZE): 0,
                "cannot read data from the device"
            );
            if(ret > 0)
                process_raw_report(hid_buffer);
        }

        if(!get_should_reset())
            break;
    }

    __INFO("terminating...");
    return 0;
}
