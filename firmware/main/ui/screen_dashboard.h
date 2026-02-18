#pragma once

#include "lvgl.h"
#include "http_client.h"

// Initialise the dashboard screen widgets inside the given parent tab.
void screen_dashboard_init(lv_obj_t *parent);

// Update the dashboard with new status data.
void screen_dashboard_update(const status_data_t *status);
