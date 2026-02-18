#include "theme.h"
#include "config.h"

// Default theme (cyber blue)
static const theme_palette_t s_palette_default = {
    .bg             = {.red = 0x1A, .green = 0x1A, .blue = 0x2E},
    .panel          = {.red = 0x16, .green = 0x21, .blue = 0x3E},
    .text_primary   = {.red = 0xE0, .green = 0xE0, .blue = 0xE0},
    .text_secondary = {.red = 0x9E, .green = 0x9E, .blue = 0x9E},
    .text_dim       = {.red = 0x66, .green = 0x66, .blue = 0x66},
    .accent         = {.red = 0x64, .green = 0xB5, .blue = 0xF6},
    .green          = {.red = 0x4C, .green = 0xAF, .blue = 0x50},
    .yellow         = {.red = 0xFF, .green = 0xC1, .blue = 0x07},
    .orange         = {.red = 0xFF, .green = 0x98, .blue = 0x00},
    .red            = {.red = 0xF4, .green = 0x43, .blue = 0x36},
    .bar_bg         = {.red = 0x2A, .green = 0x2A, .blue = 0x4A},
    .model_opus     = {.red = 0xAB, .green = 0x47, .blue = 0xBC},
    .model_sonnet   = {.red = 0x42, .green = 0xA5, .blue = 0xF5},
    .model_haiku    = {.red = 0x66, .green = 0xBB, .blue = 0x6A},
    .model_other    = {.red = 0x78, .green = 0x90, .blue = 0x9C},
};

// Anthropic brand theme (warm dark)
static const theme_palette_t s_palette_anthropic = {
    .bg             = {.red = 0x14, .green = 0x14, .blue = 0x13},  // #141413
    .panel          = {.red = 0x1E, .green = 0x1E, .blue = 0x1C},  // slightly lighter
    .text_primary   = {.red = 0xFA, .green = 0xF9, .blue = 0xF5},  // #faf9f5
    .text_secondary = {.red = 0xB0, .green = 0xAE, .blue = 0xA5},  // #b0aea5
    .text_dim       = {.red = 0x7A, .green = 0x79, .blue = 0x70},  // warm mid
    .accent         = {.red = 0xD9, .green = 0x77, .blue = 0x57},  // #d97757
    .green          = {.red = 0x78, .green = 0x8C, .blue = 0x5D},  // #788c5d
    .yellow         = {.red = 0xD4, .green = 0xA8, .blue = 0x53},  // warm amber
    .orange         = {.red = 0xD9, .green = 0x77, .blue = 0x57},  // #d97757
    .red            = {.red = 0xC4, .green = 0x45, .blue = 0x3A},  // warm red
    .bar_bg         = {.red = 0x2A, .green = 0x29, .blue = 0x25},  // warm dark
    .model_opus     = {.red = 0xD9, .green = 0x77, .blue = 0x57},  // orange (premium)
    .model_sonnet   = {.red = 0x6A, .green = 0x9B, .blue = 0xCC},  // #6a9bcc
    .model_haiku    = {.red = 0x78, .green = 0x8C, .blue = 0x5D},  // #788c5d
    .model_other    = {.red = 0xB0, .green = 0xAE, .blue = 0xA5},  // #b0aea5
};

const theme_palette_t *theme = &s_palette_default;

static lv_style_t s_bar_bg_style;

void theme_set(int theme_id)
{
    switch (theme_id) {
    case THEME_ANTHROPIC:
        theme = &s_palette_anthropic;
        break;
    default:
        theme = &s_palette_default;
        break;
    }
}

lv_color_t theme_usage_colour(float pct)
{
    if (pct < 50.0f)  return THEME_GREEN;
    if (pct < 80.0f)  return THEME_YELLOW;
    if (pct < 90.0f)  return THEME_ORANGE;
    return THEME_RED;
}

void theme_init(void)
{
    theme_set(THEME_ID);

    lv_style_init(&s_bar_bg_style);
    lv_style_set_bg_color(&s_bar_bg_style, THEME_BAR_BG);
    lv_style_set_bg_opa(&s_bar_bg_style, LV_OPA_COVER);
    lv_style_set_radius(&s_bar_bg_style, 4);
}

lv_style_t *theme_get_bar_bg_style(void)
{
    return &s_bar_bg_style;
}
