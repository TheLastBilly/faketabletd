#ifndef FAKETABLETD_UTILITIES_H__
#define FAKETABLETD_UTILITIES_H__

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Loggers
#define __FTD_LOG(_file, _fmt, _args...)    fprintf(_file, _fmt "\n", ##_args)
#define __INFO(_fmt, _args...)              __FTD_LOG(stdout, _fmt, ##_args)
#define __WARNING(_fmt, _args...)           __FTD_LOG(stdout, "warning: " _fmt, ##_args)
#define __ERROR(_fmt, _args...)             __FTD_LOG(stderr, "error [%s:%d]: " _fmt, __FILENAME__, __LINE__, ##_args)

// Catchers
#define __BASE_CATCHER(_expr, _fmt, extra, _args...)                        \
{                                                                           \
    int ret = _expr;                                                        \
    if(ret < 0)                                                             \
        __WARNING(_fmt " %s", ##_args, extra);                              \
}
#define __BASE_CATCHER_CRITICAL(_expr, _fmt, extra, _args...)               \
{                                                                           \
    int ret = _expr;                                                        \
    if(ret < 0)                                                             \
    {                                                                       \
        __ERROR(_fmt " %s", ##_args, extra);                                \
        exit(1);                                                            \
    }                                                                       \
}

// Generic catchers
#define __CATCHER(_expr, _fmt, _args...)                                    \
    __BASE_CATCHER(_expr, _fmt, "", ##_args)
#define __CATCHER_CRITICAL(_expr, _fmt, _args...)                           \
    __BASE_CATCHER_CRITICAL(_expr, _fmt, "", ##_args)


// libusb catchers
#define __USB_CATCHER(_expr, _fmt, _args...)                                \
    __BASE_CATCHER(_expr, _fmt ":", libusb_strerror(ret), ##_args)
#define __USB_CATCHER_CRITICAL(_expr, _fmt, _args...)                       \
    __BASE_CATCHER_CRITICAL(_expr, _fmt ":", libusb_strerror(ret), ##_args)

// stderr catches
#define __STD_CATCHER(_expr, _fmt, _args...)                                \
    __BASE_CATCHER(_expr, _fmt ":", strerror(ret), ##_args)
#define __STD_CATCHER_CRITICAL(_expr, _fmt, _args...)                       \
    __BASE_CATCHER_CRITICAL(_expr, _fmt ":", strerror(ret), ##_args)

#endif

#define VALIDATE(_expr, _fmt, _args...) __CATCHER_CRITICAL((_expr) ? 0 : -1, _fmt, ##_args);