#pragma once

#include "lvgl.h"
#include <string.h>

// Dirty-checking helpers: only update LVGL widgets when values actually change.
// This avoids full widget invalidation and prevents visible screen flicker.
static inline void label_set_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *current = lv_label_get_text(label);
    if (!current || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static inline void bar_set_value_if_changed(lv_obj_t *bar, int32_t value, bool animate)
{
    if (lv_bar_get_value(bar) != value) {
        lv_bar_set_value(bar, value, animate ? LV_ANIM_ON : LV_ANIM_OFF);
    }
}

typedef enum {
    SCREEN_DASHBOARD = 0,
    SCREEN_INSTANCES,
    SCREEN_SETTINGS,
    SCREEN_COUNT
} screen_id_t;

// Initialise the UI (creates tabview, all screens). Call after LVGL + display init.
void ui_init(void);

// Update all screens with latest data. Called from a timer or task.
void ui_update(void);

// Get the LVGL tabview object (for tab content access).
lv_obj_t *ui_get_tabview(void);

// Returns true if the display is in sleep mode.
bool ui_is_sleeping(void);

// Record touch activity (resets the sleep timer, wakes if sleeping).
// Must be called with the LVGL/display lock held.
void ui_notify_activity(void);

// Returns milliseconds remaining until the display enters sleep mode.
// Returns 0 if already sleeping.
uint32_t ui_sleep_remaining_ms(void);
