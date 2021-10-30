#ifndef FAKETABLETD_UTILITIES_H__
#define FAKETABLETD_UTILITIES_H__

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

#ifndef uint
#define uint unsigned int
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Loggers
#define __FTD_LOG(_file, _fmt, _args...)    fprintf(_file, "[%s:%d] " _fmt "\n", __FILENAME__, __LINE__, ##_args)
#define __INFO(_fmt, _args...)              __FTD_LOG(stdout, "[info]: "_fmt, ##_args)
#define __WARNING(_fmt, _args...)           __FTD_LOG(stdout, "[warning]: " _fmt, ##_args)
#define __ERROR(_fmt, _args...)             __FTD_LOG(stderr, "[error]: " _fmt, ##_args)

// Catchers
#define __BASE_CATCHER(_expr, _fmt, extra, _args...)                        \
{                                                                           \
    int _ret = _expr;                                                       \
    if(_ret < 0)                                                            \
        __WARNING(_fmt " %s", ##_args, extra);                              \
}
#define __BASE_CATCHER_CRITICAL(_expr, _fmt, extra, _args...)               \
{                                                                           \
    int _ret = _expr;                                                       \
    if(_ret < 0)                                                            \
    {                                                                       \
        __ERROR(_fmt " %s", ##_args, extra);                                \
        exit(_ret);                                                         \
    }                                                                       \
}

// Generic catchers
#define __CATCHER(_expr, _fmt, _args...)                                    \
    __BASE_CATCHER(_expr, _fmt, "", ##_args)
#define __CATCHER_CRITICAL(_expr, _fmt, _args...)                           \
    __BASE_CATCHER_CRITICAL(_expr, _fmt, "", ##_args)


// libusb catchers
#define __USB_CATCHER(_expr, _fmt, _args...)                                \
    __BASE_CATCHER(_expr, _fmt ":", libusb_strerror(_ret), ##_args)
#define __USB_CATCHER_CRITICAL(_expr, _fmt, _args...)                       \
    __BASE_CATCHER_CRITICAL(_expr, _fmt ":", libusb_strerror(_ret), ##_args)

// stderr catches
#define __STD_CATCHER(_expr, _fmt, _args...)                                \
    __BASE_CATCHER(_expr, _fmt ":", strerror(_ret), ##_args)
#define __STD_CATCHER_CRITICAL(_expr, _fmt, _args...)                       \
    __BASE_CATCHER_CRITICAL(_expr, _fmt ":", strerror(_ret), ##_args)

#define VALIDATE(_expr, _fmt, _args...) __CATCHER_CRITICAL((_expr) ? 0 : -1, _fmt, ##_args);

extern int errno;

// Misc
#define GET_LEN(_arr)           (sizeof(_arr)/sizeof(_arr[0]))
#define SLEEP_FOR_US(_us)                                                   \
{                                                                           \
    struct timespec req = {                                                 \
        .tv_nsec = (long)(_us)                                              \
    };                                                                      \
    while(nanosleep(&req, &req) == -1 && errno == EINTR);                   \
}

// Mutex helpers
#define REGISTER_MUTEX_VARIABLE(_type, _name)                               \
    static pthread_mutex_t _name##_mutex;                                   \
    static _type _name;                                                     \
    static inline void lock_##_name()                                       \
    { pthread_mutex_lock(&_name##_mutex); }                                 \
    static inline void unlock_##_name()                                     \
    { pthread_mutex_unlock(&_name##_mutex); }                               \
    static void set_##_name(_type val)                                      \
    {                                                                       \
        lock_##_name();                                                     \
        _name = val;                                                        \
        unlock_##_name();                                                   \
    }                                                                       \
    static _type get_##_name()                                              \
    {                                                                       \
        _type _v;                                                           \
        lock_##_name();                                                     \
        _v = _name;                                                         \
        unlock_##_name();                                                   \
        return _v;                                                          \
    }

#define CHECK_MASK(_value, _mask) (((_value) & (_mask)) == 0)

#define MAX(a, b) ((a) >= (b) ? (a): (b))
#define MIN(a, b) ((a) <= (b) ? (a): (b))

bool path_exits(const char *path);
bool path_is_dir(const char *path);
const char *check_paths(const char *paths[], int len);

#endif