
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>

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

static struct libusb_context *usb_context;
static struct libusb_device  **device_list, *device;
static struct libusb_device_handle *device_handle;
static struct libusb_device_descriptor descriptor;
static struct libusb_transfer *device_transfer;
static uint8_t *transfer_buffer;
static volatile int pen_device, pad_device, mouse_device;
static size_t devices_detected;

// Device info callbacks
get_name_callback_t get_pad_name_callback;
get_name_callback_t get_pen_name_callback;
get_input_id_callback_t get_input_id_callback;

// Device setup callbacks
create_virtual_device_callback_t create_virtual_pad_callback;
create_virtual_device_callback_t create_virtual_pen_callback;

// Parsing callbacks
process_raw_input_callback_t process_raw_input_callback;

REGISTER_MUTEX_VARIABLE(bool, should_close);
REGISTER_MUTEX_VARIABLE(bool, should_reset);

struct interface_status_t
{
    int number;
    bool claimed;
    bool detached_from_kernel;
};
static struct interface_status_t interface_0, interface_1;

// Callback handlers
static inline struct input_id * get_input_id()
{ USE_RETURNING_CALLBACK(get_input_id_callback); }
static inline const char *get_pad_name()
{ USE_RETURNING_CALLBACK(get_pad_name_callback); }
static inline const char *get_pen_name()
{ USE_RETURNING_CALLBACK(get_pen_name_callback); }

static inline int create_virtual_pad(struct input_id *id, const char *name)
{ USE_RETURNING_CALLBACK(create_virtual_pad_callback, id, name); }
static inline int create_virtual_pen(struct input_id *id, const char *name)
{ USE_RETURNING_CALLBACK(create_virtual_pen_callback, id, name); }

static inline int process_raw_input(const struct raw_input_data_t *data)
{ USE_RETURNING_CALLBACK(process_raw_input_callback, data); }

// Signal handlers
static void sigint_handler(int sig)
{
    __INFO("SIGINT detected, terminating...");
    exit(0);
}
static inline void sigterm_handler(int sig)
{
    exit(0);
}

// Device name for the specified vendor and product id. Return NULL if
// the specified device is not supported
static const char * setup_device(uint16_t vendor_id, uint16_t product_id)
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
                get_input_id_callback = &hs610_get_device_id;
                get_pad_name_callback = &hs610_get_pad_name;
                get_pen_name_callback = &hs610_get_pen_name;

                create_virtual_pad_callback = &hs610_create_virtual_pad;
                create_virtual_pen_callback = &hs610_create_virtual_pen;
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
        input_setup.id.vendor = 0x1234;
        input_setup.id.product = 0x5678;
        strcpy(input_setup.name, "Example device");

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

static void claim_interface_for_handle(struct libusb_device_handle *handle, struct interface_status_t *interface)
{
    int ret = 0;
    // Detach interface from the kernel and claim it for ourselves
    ret  = libusb_detach_kernel_driver(handle, interface->number);
    if(ret < 0 && ret != LIBUSB_ERROR_NOT_FOUND)
        __USB_CATCHER_CRITICAL(ret, "cannot detach kernel from interface %d", interface->number);
    interface->detached_from_kernel = true;

    __CATCHER_CRITICAL(libusb_claim_interface(handle, interface->number), "cannot claim interface %d", interface->number);
    interface->claimed = true;

    // This part is a little trickier to understand. Basically, we are going to setup the
    // handle to work with a HID device on this interface. You can actually understand a lot
    // of what's going on by reading this spec
    // https://www.usb.org/sites/default/files/hid1_11.pdf
    
    // Set the device's protocol to report
    __USB_CATCHER_CRITICAL(libusb_control_transfer(
        handle, 
        HID_SET_REQUEST_TYPE, 
        HID_SET_PROTOCOL, 
        HID_SET_PROTOCOL_REPORT,
        interface->number,
        NULL, 0,
        HID_TIMEOUT
    ), "cannot set protocol on interface %d", interface->number);
    
    // Make sure idle is set to infinity
    __USB_CATCHER_CRITICAL(libusb_control_transfer(
        handle, 
        HID_SET_REQUEST_TYPE, 
        HID_SET_IDLE, 

        // This makes sure the 8th bit of the word (wValue) is 0, 
        // making the idle duration undefined (it's on the pdf too)
        0 << 8,
        
        interface->number,
        NULL, 0,
        HID_TIMEOUT
    ), "cannot set protocol on interface %d", interface->number);
}

static void interrupt_transfer_callback(struct libusb_transfer *transfer)
{
    int ret = 0;
    bool error_found = true;
    struct raw_input_data_t raw_input_data;
    
    if(transfer == NULL || get_should_close()) return;
    
    switch (transfer->status)
    {
    case LIBUSB_TRANSFER_COMPLETED:
        raw_input_data = (struct raw_input_data_t){
            .data = transfer->buffer,
            .size = transfer->actual_length,

            .pad_device = pad_device,
            .pen_device = pen_device,

            .mouse_device = mouse_device
        };
        ret = process_raw_input(&raw_input_data);
        if(!(error_found = ret < 0))
        {
            libusb_submit_transfer(transfer);
            __USB_CATCHER(ret, "cannot resubmit event transfer");
            error_found = ret < 0;
        }
        break;
    
    // Taken from https://github.com/DIGImend/digimend-userspace-drivers/blob/main/src/dud-translate.c
#define MAP(_name, _desc)                       \
    case LIBUSB_TRANSFER_##_name:               \
        __ERROR(_desc);                         \
        break

        MAP(ERROR,      "interrupt transfer failed");
        MAP(TIMED_OUT,  "interrupt transfer timed out");
        MAP(STALL,      "interrupt transfer halted (endpoint stalled)");
        MAP(OVERFLOW,   "interrupt transfer overflowed "
                        "(device sent more data than requested)");
#undef MAP

    case LIBUSB_TRANSFER_NO_DEVICE:
        __INFO("device has been disconnected!");
        break;

    case LIBUSB_TRANSFER_CANCELLED:
        break;
    
    default:
        __ERROR("Uknown transfer error: %d", transfer->status);
        break;
    }

    if(error_found)
        set_should_close(true);
}

// Free allocated objects and deinitialize libusb
static void cleannup(void)
{
    devices_detected = 0;

    set_should_close(false);

    CLOSE_UINPUT_DEVICE(mouse_device);
    CLOSE_UINPUT_DEVICE(pen_device);
    CLOSE_UINPUT_DEVICE(pad_device);

    if(device_transfer != NULL)
    {
        libusb_free_transfer(device_transfer);
        device_transfer = NULL;
    }

    if(transfer_buffer != NULL)
    {
        free(transfer_buffer);
        transfer_buffer = NULL;
    }

    if(interface_0.claimed)
    {
        libusb_release_interface(device_handle, interface_0.number);
        interface_0.claimed = false;
    }
    if(interface_1.claimed)
    {
        libusb_release_interface(device_handle, interface_1.number);
        interface_1.claimed = false;
    }

    if(interface_1.detached_from_kernel)
    {
        libusb_attach_kernel_driver(device_handle, interface_1.number);
        interface_1.detached_from_kernel = false;
    }
    if(interface_0.detached_from_kernel)
    {
        libusb_attach_kernel_driver(device_handle, interface_0.number);
        interface_0.detached_from_kernel = false;
    }

    if(device_handle != NULL)
    {
        libusb_close(device_handle);
        device_handle = NULL;
    }

    if(device_list != NULL)
    {
        libusb_free_device_list(device_list, true);
        device_list = NULL;
    }

    if(usb_context != NULL)
    {
        libusb_exit(usb_context);
        usb_context = NULL;
    }

    get_pad_name_callback = NULL;
    get_pen_name_callback = NULL;
    get_input_id_callback = NULL;

    create_virtual_pad_callback = NULL;
    create_virtual_pen_callback = NULL;
    process_raw_input_callback = NULL;
}

static bool look_for_devices(const char **device_name)
{
    size_t i = 0;
    devices_detected = 0;

    if(device_list != NULL) libusb_free_device_list(device_list, true);

    // Get device list
    __USB_CATCHER_CRITICAL(
        devices_detected = libusb_get_device_list(usb_context, &device_list), 
        "cannot retreive connected devices"
    );

    // Find compatible devices
    for(i = 0; i < devices_detected; i++)
    {
        device = device_list[i];

        // See if current device on the list is supported
        __USB_CATCHER(libusb_get_device_descriptor(device, &descriptor), "cannot get device descriptor");
        if((*device_name = setup_device(descriptor.idVendor, descriptor.idProduct)) != NULL) return true;
    }
    
    return false;
}

static inline void print_help()
{
    printf(
        "Usage: faketabletd [OPTION]\n"
        "User space driver for drawing tablets\n\n"
        
        "Options\n"
        "  -m\t\t\tEnables virtual mouse emulation\n"
        "  -s\t\t\tDisables reset on disconnect\n\n"

        "Examples:\n"
        "  faketabletd -m\tRuns driver with virtual mouse emulation\n"
        "  faketabletd -ms\tRuns driver with virtual mouse emulation. Will exit on disconnect\n"
    );
}

int main(int argc, char const **argv)
{
    // Local variables
    int ret = 0;
    bool use_virtual_mouse = false;

    // Initialize locals
    usb_context         = NULL;
    device_list         = NULL;
    device              = NULL;
    device_handle       = NULL;
    device_transfer     = NULL;
    transfer_buffer     = NULL;

    get_pad_name_callback = NULL;
    get_pen_name_callback = NULL;
    get_input_id_callback = NULL;

    create_virtual_pad_callback = NULL;
    create_virtual_pen_callback = NULL;
    process_raw_input_callback = NULL;

    pen_device          = -1;
    pad_device          = -1;
    mouse_device        = -1;

    descriptor = (struct libusb_device_descriptor){};

    should_close = false;
    should_reset = true;

    const char* device_name = NULL;

    // Make sure we catch Ctrl-C when asked to terminate
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigterm_handler);

    // Make sure we clean our mess before we leave
    atexit(cleannup);

    // Get argument options
    while((ret = getopt(argc, (char* const*)argv, "msh")) != -1)
    {
        switch (ret)
        {
        case 'm':
            use_virtual_mouse = true;
            break;
        case 's':
            set_should_reset(false);
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

    while(1)
    {
        // Make sure we are clear to go on every cycle
        cleannup();

        // Initialize libusb context
        __USB_CATCHER_CRITICAL(libusb_init(&usb_context), "cannot create libusb context");

        __INFO("looking for compatible devices...");
        while(!get_should_close() && !look_for_devices(&device_name)) SLEEP_FOR_US(100);
        if(get_should_close())
            break;

        // Let us know what you've found
        __INFO("found supported device: %s (%04x:%04x)", device_name, descriptor.idVendor, descriptor.idProduct);

        // Open device
        __INFO("connecting to device");
        __USB_CATCHER(
            ret = libusb_open(device, &device_handle), 
            "cannot open device with current handle"
        );
        if(ret < 0)
        {
            switch (ret)
            {
            case LIBUSB_ERROR_ACCESS:
                __WARNING("you need to be running as root in order to use this software");
                exit(1);
                break;
            
            default:
                continue;
            }
        }
        __INFO("connected!");

        __INFO("configuring device...");
        // Claim interfaces 0 and 1 (dunno yet why the two of them but it works so...)
        claim_interface_for_handle(device_handle, &interface_0);
        claim_interface_for_handle(device_handle, &interface_1);

        // Create virtual pen and pad
        pad_device = create_virtual_pad(get_input_id(), get_pad_name());
        pen_device = create_virtual_pen(get_input_id(), get_pen_name());
    
        if(use_virtual_mouse)
            mouse_device = create_virtual_mouse();

        // Allocate space for tranfer callback
        transfer_buffer = calloc(HID_BUFFER_SIZE, sizeof(uint8_t));
        __CATCHER_CRITICAL(transfer_buffer == NULL ? -1 : 0, "cannot allocate memory for transfer buffer");

        device_transfer = libusb_alloc_transfer(0);
        __CATCHER_CRITICAL(device_transfer == NULL ? -1 : 0, "cannot allocate libusb transfer");

        // Register transfer callback
        libusb_fill_interrupt_transfer(device_transfer, 
            device_handle, HID_ENDPOINT, 
            transfer_buffer, HID_BUFFER_SIZE,

            // Do keep in mind that this function will be handled from another
            // thread, so terminating the program using exit from 
            // here isn't an option
            interrupt_transfer_callback,
            
            NULL, 0
        );
        __USB_CATCHER_CRITICAL(libusb_submit_transfer(device_transfer), "cannot submit device transfer");
        __INFO("done configuring device!");

        __INFO("ready!");
        while(!get_should_close())
        {
            int ret = libusb_handle_events(usb_context);
            if(ret < 0 && ret != LIBUSB_ERROR_INTERRUPTED)
                __USB_CATCHER_CRITICAL(ret, "usb event handling error: ");
        }

        if(!get_should_reset())
            break;
    }

    __INFO("terminating...");
    return 0;
}
