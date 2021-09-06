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

int hs610_create_virtual_pad(struct input_id *id, const char *name);
int hs610_create_virtual_pen(struct input_id *id, const char *name);

int hs610_process_raw_input(const uint8_t *data, size_t size, int pad_device, int pen_device);

struct input_id *hs610_get_device_id();
const char *hs610_get_pad_name();
const char *hs610_get_pen_name();

const char *hs610_get_device_name();

#endif