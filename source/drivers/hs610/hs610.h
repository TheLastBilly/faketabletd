#ifndef FAKETABLETD_DRIVERS_HS610_H__
#define FAKETABLETD_DRIVERS_HS610_H__

#include "drivers/generic/generic.h"

int hs610_process_raw_input(const struct raw_input_data_t *data);

struct input_id *hs610_get_device_id();
const char *hs610_get_pad_name();
const char *hs610_get_pen_name();

const char *hs610_get_device_name();

#endif