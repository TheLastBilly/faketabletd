#ifndef FAKETABLETD_DRIVERS_HS610_H__
#define FAKETABLETD_DRIVERS_HS610_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <fcntl.h>
#include <linux/uinput.h>

#include "faketabletd.h"
#include "utilities.h"

int hs610_process_raw_input(const uint8_t *data, size_t size, int pad_device, int pen_device);
int hs610_create_virtual_pad(struct input_id *id, const char *name);
int hs610_create_virtual_pen(struct input_id *id, const char *name);

#endif