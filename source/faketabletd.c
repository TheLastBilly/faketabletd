
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <signal.h>
#include <linux/uinput.h>
#include <libusb-1.0/libusb.h>

#include "faketabletd.h"
#include "utilities.h"

#include "drivers/hs610/hs610.h"

#define CLOSE_UINPUT_DEVICE(fd)     \
{                                   \
    if(fd >= 0)                     \
    {                               \
        ioctl(fd, UI_DEV_DESTROY);  \
        close(fd);                  \
        fd = -1;                    \
    }                               \
}

static struct libusb_context *usb_context;
static struct libusb_device  **device_list, *device;
static struct libusb_device_handle *device_handle;
static struct libusb_device_descriptor descriptor;
static struct libusb_transfer *device_transfer;
static uint8_t *transfer_buffer;
static volatile int pen_device, pad_device;
static size_t devices_detected;

create_virtual_device_callback_t create_virtual_pad_callback;
create_virtual_device_callback_t create_virtual_pen_callback;
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
static inline int create_virtual_pad(struct input_id *id, const char *name)
{
    VALIDATE(create_virtual_pad_callback != NULL, "cannot call empty callback");
    return create_virtual_pad_callback(id, name);
}
static inline int create_virtual_pen(struct input_id *id, const char *name)
{
    VALIDATE(create_virtual_pen_callback != NULL, "cannot call empty callback");
    return create_virtual_pen_callback(id, name);
}

static inline int process_raw_input(const uint8_t *data, size_t size, int pad_device, int pen_device)
{
    VALIDATE(process_raw_input_callback != NULL, "cannot call empty callback");
    return process_raw_input_callback(data, size, pad_device, pen_device);
}

// Global signal handler
static void signal_handler(int sig)
{
    __INFO("SIGINT detected, terminating...");
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
                create_virtual_pad_callback = &hs610_create_virtual_pad;
                create_virtual_pen_callback = &hs610_create_virtual_pen;
                process_raw_input_callback = &hs610_process_raw_input;
                return "HS610";
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
    VALIDATE(transfer != NULL, "cannot process null transfer");
    
    switch (transfer->status)
    {
    case LIBUSB_TRANSFER_COMPLETED:
        ret = process_raw_input(transfer->buffer, transfer->actual_length, pad_device, pen_device);
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
        set_should_reset(true);
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
    set_should_reset(true);

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
    if(interface_0.detached_from_kernel)
    {
        libusb_attach_kernel_driver(device_handle, interface_0.number);
        interface_0.detached_from_kernel = false;
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

int main(int argc, char const *argv[])
{
    int ret = 0;

    // Initialize locals
    usb_context         = NULL;
    device_list         = NULL;
    device              = NULL;
    device_handle       = NULL;
    device_transfer     = NULL;
    transfer_buffer     = NULL;

    create_virtual_pad_callback = NULL;
    create_virtual_pen_callback = NULL;
    process_raw_input_callback = NULL;

    pen_device          = -1;
    pad_device          = -1;

    descriptor = (struct libusb_device_descriptor){};

    should_close = false;
    should_reset = true;

    struct input_id virtual_device_id = {};
    const char* device_name = NULL;

    // Make sure we catch Ctrl-C when asked to terminate
    signal(SIGINT, signal_handler);

    // Make sure we clean our mess before we leave
    atexit(cleannup);

    while(1)
    {
        // Initialize libusb context
        __USB_CATCHER_CRITICAL(libusb_init(&usb_context), "cannot create libusb context");

        __INFO("looking for compatible devices...");
        while(!get_should_close() && !look_for_devices(&device_name)) SLEEP_FOR_US(100);

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
                cleannup();
                continue;
            }
        }
        __INFO("connected!");

        __INFO("configuring device...");
        // Claim interfaces 0 and 1 (dunno yet why the two of them but it works so...)
        claim_interface_for_handle(device_handle, &interface_0);
        claim_interface_for_handle(device_handle, &interface_1);

        // Create virtual pen and pad
        virtual_device_id = (struct input_id){
            .bustype = BUS_USB,
            .vendor = FAKETABLETD_FAKE_VENDOR_ID,
            .product = FAKETABLETD_FAKE_PRODUCT_ID,
            .version = FAKETABLETD_FAKE_VERSION,
        };
        pad_device = create_virtual_pad(&virtual_device_id, FAKETABLETD_FAKE_NAME "pen");
        pen_device = create_virtual_pen(&virtual_device_id, FAKETABLETD_FAKE_NAME "pad");

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

        if(get_should_reset())
            cleannup();
        else
            break;
    }

    __INFO("terminating...");
    return 0;
}
