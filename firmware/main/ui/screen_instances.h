#pragma once

#include "lvgl.h"
#include "http_client.h"

// Initialise the instances screen widgets.
void screen_instances_init(lv_obj_t *parent);

// Update the instances list with new status data.
void screen_instances_update(const status_data_t *status);
