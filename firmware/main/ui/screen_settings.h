#pragma once

#include "lvgl.h"

// Initialise the settings screen widgets.
void screen_settings_init(lv_obj_t *parent);

// Update settings display (WiFi status, etc.).
void screen_settings_update(void);
