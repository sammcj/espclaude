#pragma once

#include "lvgl.h"

// Theme IDs
#define THEME_DEFAULT   0
#define THEME_ANTHROPIC 1

// All colours that make up a theme
typedef struct {
    lv_color_t bg;
    lv_color_t panel;
    lv_color_t text_primary;
    lv_color_t text_secondary;
    lv_color_t text_dim;
    lv_color_t accent;
    lv_color_t green;
    lv_color_t yellow;
    lv_color_t orange;
    lv_color_t red;
    lv_color_t bar_bg;
    lv_color_t model_opus;
    lv_color_t model_sonnet;
    lv_color_t model_haiku;
    lv_color_t model_other;
} theme_palette_t;

// Active palette (set by theme_init, readable everywhere)
extern const theme_palette_t *theme;

// Accessor macros (drop-in replacements for the old #defines)
#define THEME_BG_COLOUR       (theme->bg)
#define THEME_PANEL_COLOUR    (theme->panel)
#define THEME_TEXT_PRIMARY    (theme->text_primary)
#define THEME_TEXT_SECONDARY  (theme->text_secondary)
#define THEME_TEXT_DIM        (theme->text_dim)
#define THEME_ACCENT          (theme->accent)
#define THEME_GREEN           (theme->green)
#define THEME_YELLOW          (theme->yellow)
#define THEME_ORANGE          (theme->orange)
#define THEME_RED             (theme->red)
#define THEME_BAR_BG          (theme->bar_bg)
#define THEME_MODEL_OPUS      (theme->model_opus)
#define THEME_MODEL_SONNET    (theme->model_sonnet)
#define THEME_MODEL_HAIKU     (theme->model_haiku)
#define THEME_MODEL_OTHER     (theme->model_other)

// Tab bar height
#define THEME_TAB_HEIGHT 36

// Colour thresholds for usage bars
// green < 50%, yellow 50-79%, orange 80-89%, red >= 90%
lv_color_t theme_usage_colour(float pct);

// Initialise theme styles (call once after LVGL init).
// Uses THEME_ID from config.h to select the palette.
void theme_init(void);

// Switch to a different theme at runtime. Re-init is needed to apply fully.
void theme_set(int theme_id);

// Get the bar background style.
lv_style_t *theme_get_bar_bg_style(void);
