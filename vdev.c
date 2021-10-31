#include <linux/uinput.h>

#include "faketabletd.h"
#include "utilities.h"
#include "libevdev/libevdev-uinput.h"

// Send event to device using fd
#define SEND_INPUT_EVENT(_dev, _type, _code, _value)                \
{                                                                   \
    ret = libevdev_uinput_write_event(_dev, _type, _code, _value);  \
    if(ret < 0) return ret;                                         \
}

#define SEND_REPORT(_fd)                                            \
    SEND_INPUT_EVENT(_fd, EV_SYN, SYN_REPORT, 0)

static int get_key_code(char c)
{
    int key = 0;

#define CATCH_KEY(_c, _C)                                           \
    case _c:                                                        \
        key = KEY_##_C;                                             \
        break;

    switch (c)
    {
    // Letters
    CATCH_KEY('a', A);
    CATCH_KEY('b', B);
    CATCH_KEY('c', C);
    CATCH_KEY('d', D);
    CATCH_KEY('e', E);
    CATCH_KEY('f', F);
    CATCH_KEY('g', G);
    CATCH_KEY('h', H);
    CATCH_KEY('i', I);
    CATCH_KEY('j', J);
    CATCH_KEY('k', K);
    CATCH_KEY('l', L);
    CATCH_KEY('m', M);
    CATCH_KEY('n', N);
    CATCH_KEY('o', O);
    CATCH_KEY('p', P);
    CATCH_KEY('q', Q);
    CATCH_KEY('r', R);
    CATCH_KEY('s', S);
    CATCH_KEY('t', T);
    CATCH_KEY('u', U);
    CATCH_KEY('v', V);
    CATCH_KEY('w', W);
    CATCH_KEY('x', X);
    CATCH_KEY('y', Y);
    CATCH_KEY('z', Z);

    // Numbers
    CATCH_KEY('0', 0);
    CATCH_KEY('1', 1);
    CATCH_KEY('2', 2);
    CATCH_KEY('3', 3);
    CATCH_KEY('4', 4);
    CATCH_KEY('5', 5);
    CATCH_KEY('6', 6);
    CATCH_KEY('7', 7);
    CATCH_KEY('8', 8);
    CATCH_KEY('9', 9);
    
    // Modifiers
    CATCH_KEY('S', LEFTSHIFT);
    CATCH_KEY('C', LEFTCTRL);

    default:
        return -1;
    }
#undef CATCH_KEY

    return key;
}

bool validate_key_presses(const char keys[INI_STRING_SIZE])
{
    for(int i = 0 ; i < INI_STRING_SIZE; i++)
        if(get_key_code(i) == -1) return false;
    
    return true;
}

int simulate_key_presses(struct libevdev_uinput *fd, const char keys[INI_STRING_SIZE])
{
    int ret = 0, size = 0;

    size = 0;
    while(size < INI_STRING_SIZE && keys[size] != '\0')
        SEND_INPUT_EVENT(fd, EV_KEY, get_key_code(keys[size++]), 1);
    SEND_REPORT(fd);

    size = 0;
    while(size < INI_STRING_SIZE && keys[size] != '\0')
        SEND_INPUT_EVENT(fd, EV_KEY, get_key_code(keys[size++]), 0);
    SEND_REPORT(fd);


    return 0;
}