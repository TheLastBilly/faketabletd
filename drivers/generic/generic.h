#ifndef FAKETABLETD_DRIVERS_GENERIC_H__
#define FAKETABLETD_DRIVERS_GENERIC_H__

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>
#include <linux/uinput.h>

#include "utilities.h"
#include "faketabletd.h"

int generic_create_virtual_pad(struct input_id *id, const char *name);
int generic_create_virtual_pen(struct input_id *id, const char *name);

#endif