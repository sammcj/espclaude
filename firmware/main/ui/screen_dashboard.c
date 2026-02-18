#include "screen_dashboard.h"
#include "ui.h"
#include "theme.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Per-tier UI widgets
typedef struct {
    lv_obj_t *label_name;
    lv_obj_t *label_pct;
    lv_obj_t *label_countdown;
    lv_obj_t *bar;
} tier_widgets_t;

static tier_widgets_t s_session;
static tier_widgets_t s_weekly_all;
static tier_widgets_t s_weekly_sonnet;
static lv_obj_t *s_prediction_label;
static lv_obj_t *s_burn_label;

// Model distribution widgets
static lv_obj_t *s_dashboard_parent;

static lv_obj_t *s_model_bar_bg;
static lv_obj_t *s_model_bar_segs[MAX_MODELS];
static lv_obj_t *s_legend_swatches[MAX_MODELS];
static lv_obj_t *s_legend_labels[MAX_MODELS];
static int s_legend_y_offset;

// Cached countdowns: base values from server, decremented locally each second
static int64_t s_session_remaining = 0;
static int64_t s_weekly_all_remaining = 0;
static int64_t s_weekly_sonnet_remaining = 0;
static time_t  s_last_fetch_time = 0;
static char    s_last_resets_at[32] = "";

static void create_tier_row(lv_obj_t *parent, tier_widgets_t *tw, const char *name, int y_offset)
{
    // Name label (left)
    tw->label_name = lv_label_create(parent);
    lv_label_set_text(tw->label_name, name);
    lv_obj_set_style_text_color(tw->label_name, THEME_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(tw->label_name, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(tw->label_name, 8, y_offset);

    // Percentage label (right of name)
    tw->label_pct = lv_label_create(parent);
    lv_label_set_text(tw->label_pct, "-- %");
    lv_obj_set_style_text_color(tw->label_pct, THEME_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(tw->label_pct, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(tw->label_pct, 160, y_offset);

    // Countdown label (far right)
    tw->label_countdown = lv_label_create(parent);
    lv_label_set_text(tw->label_countdown, "--:--");
    lv_obj_set_style_text_color(tw->label_countdown, THEME_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(tw->label_countdown, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(tw->label_countdown, 225, y_offset);

    // Progress bar
    tw->bar = lv_bar_create(parent);
    lv_obj_set_size(tw->bar, 296, 16);
    lv_obj_set_pos(tw->bar, 8, y_offset + 20);
    lv_bar_set_range(tw->bar, 0, 100);
    lv_bar_set_value(tw->bar, 0, LV_ANIM_OFF);

    // Bar background style
    lv_obj_add_style(tw->bar, theme_get_bar_bg_style(), LV_PART_MAIN);

    // Bar indicator style (colour set dynamically)
    lv_obj_set_style_bg_color(tw->bar, THEME_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(tw->bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(tw->bar, 4, LV_PART_INDICATOR);
}

static void update_tier(tier_widgets_t *tw, const usage_tier_t *tier, int64_t remaining)
{
    if (!tier->present) {
        lv_obj_add_flag(tw->label_name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tw->label_pct, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tw->label_countdown, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(tw->bar, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(tw->label_name, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(tw->label_pct, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(tw->label_countdown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(tw->bar, LV_OBJ_FLAG_HIDDEN);

    // Update percentage
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f%%", tier->utilisation);
    label_set_text_if_changed(tw->label_pct, buf);

    // Update bar value and colour
    int val = (int)tier->utilisation;
    if (val > 100) val = 100;
    if (val < 0) val = 0;
    bar_set_value_if_changed(tw->bar, val, true);

    // Only update bar colour if it actually changed
    lv_color_t new_colour = theme_usage_colour(tier->utilisation);
    lv_color_t cur_colour = lv_obj_get_style_bg_color(tw->bar, LV_PART_INDICATOR);
    if (cur_colour.red != new_colour.red || cur_colour.green != new_colour.green || cur_colour.blue != new_colour.blue) {
        lv_obj_set_style_bg_color(tw->bar, new_colour, LV_PART_INDICATOR);
    }

    // Update countdown
    if (remaining > 0) {
        int days = (int)(remaining / 86400);
        int hrs  = (int)((remaining % 86400) / 3600);
        int mins = (int)((remaining % 3600) / 60);

        if (days > 0) {
            snprintf(buf, sizeof(buf), "%dd %dh", days, hrs);
        } else if (hrs > 0) {
            snprintf(buf, sizeof(buf), "%dh %dm", hrs, mins);
        } else {
            snprintf(buf, sizeof(buf), "%dm", mins);
        }
        label_set_text_if_changed(tw->label_countdown, buf);
    } else {
        label_set_text_if_changed(tw->label_countdown, "now");
    }
}

// Convert raw model ID to short display name.
// e.g. "claude-opus-4-6" -> "O 4.6", "claude-sonnet-4-20250514" -> "S 4"
static void short_model_name(const char *model, char *buf, size_t buf_len)
{
    char prefix = '?';
    const char *after = NULL;

    const char *p;
    if ((p = strstr(model, "opus")) || (p = strstr(model, "Opus"))) {
        prefix = 'O';
        after = p + 4;
    } else if ((p = strstr(model, "sonnet")) || (p = strstr(model, "Sonnet"))) {
        prefix = 'S';
        after = p + 6;
    } else if ((p = strstr(model, "haiku")) || (p = strstr(model, "Haiku"))) {
        prefix = 'H';
        after = p + 5;
    } else {
        snprintf(buf, buf_len, "%.6s", model);
        return;
    }

    // Skip separator between family name and version
    if (*after == '-' || *after == ' ' || *after == '_') after++;

    int major = 0, minor = -1;
    if (*after >= '0' && *after <= '9') {
        major = (*after - '0') % 10;
        after++;
        if (*after == '-' || *after == '.') {
            after++;
            const char *ds = after;
            int num = 0;
            while (*after >= '0' && *after <= '9') {
                num = num * 10 + (*after - '0');
                after++;
            }
            // 1-2 digit number = version minor; longer = date suffix, ignore
            if (after - ds >= 1 && after - ds <= 2) {
                minor = num % 100;
            }
        }
    }

    if (minor >= 0) {
        snprintf(buf, buf_len, "%c %d.%d", prefix, major, minor);
    } else if (major > 0) {
        snprintf(buf, buf_len, "%c %d", prefix, major);
    } else {
        snprintf(buf, buf_len, "%c", prefix);
    }
}

static lv_color_t model_colour(const char *model)
{
    if (strstr(model, "Opus") || strstr(model, "opus"))
        return THEME_MODEL_OPUS;
    if (strstr(model, "Sonnet") || strstr(model, "sonnet"))
        return THEME_MODEL_SONNET;
    if (strstr(model, "Haiku") || strstr(model, "haiku"))
        return THEME_MODEL_HAIKU;
    return THEME_MODEL_OTHER;
}

static void create_model_dist(lv_obj_t *parent, int y_offset)
{
    s_model_bar_bg = lv_obj_create(parent);
    lv_obj_remove_style_all(s_model_bar_bg);
    lv_obj_set_size(s_model_bar_bg, 296, 10);
    lv_obj_set_pos(s_model_bar_bg, 8, y_offset);
    lv_obj_set_style_bg_color(s_model_bar_bg, THEME_BAR_BG, 0);
    lv_obj_set_style_bg_opa(s_model_bar_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_model_bar_bg, 4, 0);
    lv_obj_set_style_pad_all(s_model_bar_bg, 0, 0);
    lv_obj_clear_flag(s_model_bar_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_model_bar_bg, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < MAX_MODELS; i++) {
        s_model_bar_segs[i] = lv_obj_create(s_model_bar_bg);
        lv_obj_remove_style_all(s_model_bar_segs[i]);
        lv_obj_set_size(s_model_bar_segs[i], 0, 10);
        lv_obj_set_pos(s_model_bar_segs[i], 0, 0);
        lv_obj_set_style_bg_opa(s_model_bar_segs[i], LV_OPA_COVER, 0);
        lv_obj_add_flag(s_model_bar_segs[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_model_legend(lv_obj_t *parent, int y_offset)
{
    s_legend_y_offset = y_offset;

    for (int i = 0; i < MAX_MODELS; i++) {
        s_legend_swatches[i] = lv_obj_create(parent);
        lv_obj_remove_style_all(s_legend_swatches[i]);
        lv_obj_set_size(s_legend_swatches[i], 8, 8);
        lv_obj_set_pos(s_legend_swatches[i], 0, y_offset + 3);
        lv_obj_set_style_bg_opa(s_legend_swatches[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_legend_swatches[i], 2, 0);
        lv_obj_add_flag(s_legend_swatches[i], LV_OBJ_FLAG_HIDDEN);

        s_legend_labels[i] = lv_label_create(parent);
        lv_label_set_text(s_legend_labels[i], "");
        lv_obj_set_style_text_color(s_legend_labels[i], THEME_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_font(s_legend_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_pos(s_legend_labels[i], 0, y_offset);
        lv_obj_add_flag(s_legend_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void update_model_dist(const status_data_t *status)
{
    if (status->model_count == 0) {
        lv_obj_add_flag(s_model_bar_bg, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < MAX_MODELS; i++) {
            lv_obj_add_flag(s_legend_swatches[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_legend_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    lv_obj_clear_flag(s_model_bar_bg, LV_OBJ_FLAG_HIDDEN);

    int bar_w = 296;
    int seg_x = 0;
    int legend_x = 8;

    for (int i = 0; i < MAX_MODELS; i++) {
        if (i < status->model_count) {
            lv_color_t col = model_colour(status->models[i].model);

            // Bar segment
            int seg_w;
            if (i == status->model_count - 1) {
                seg_w = bar_w - seg_x;
            } else {
                seg_w = (int)(status->models[i].cost_pct / 100.0f * bar_w);
            }
            if (seg_w < 2) seg_w = 2;
            if (seg_x + seg_w > bar_w) seg_w = bar_w - seg_x;
            if (seg_w <= 0) {
                lv_obj_add_flag(s_model_bar_segs[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_set_size(s_model_bar_segs[i], seg_w, 10);
                lv_obj_set_pos(s_model_bar_segs[i], seg_x, 0);
                lv_obj_set_style_bg_color(s_model_bar_segs[i], col, 0);
                lv_obj_clear_flag(s_model_bar_segs[i], LV_OBJ_FLAG_HIDDEN);
                seg_x += seg_w;
            }

            // Legend swatch
            lv_obj_set_pos(s_legend_swatches[i], legend_x, s_legend_y_offset + 3);
            lv_obj_set_style_bg_color(s_legend_swatches[i], col, 0);
            lv_obj_clear_flag(s_legend_swatches[i], LV_OBJ_FLAG_HIDDEN);

            // Legend label (short name)
            char short_name[8];
            short_model_name(status->models[i].model, short_name, sizeof(short_name));
            label_set_text_if_changed(s_legend_labels[i], short_name);
            lv_obj_set_pos(s_legend_labels[i], legend_x + 12, s_legend_y_offset);
            lv_obj_clear_flag(s_legend_labels[i], LV_OBJ_FLAG_HIDDEN);

            int text_w = (int)strlen(short_name) * 9;
            legend_x += 12 + text_w + 10;
        } else {
            lv_obj_add_flag(s_model_bar_segs[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_legend_swatches[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_legend_labels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void screen_dashboard_init(lv_obj_t *parent)
{
    s_dashboard_parent = parent;
    lv_obj_set_style_pad_all(parent, 0, 0);

    create_tier_row(parent, &s_session,       "Session",     4);
    create_tier_row(parent, &s_weekly_all,    "Weekly",      50);
    create_tier_row(parent, &s_weekly_sonnet, "Sonnet (7d)", 96);

    // Halve the Sonnet bar height to make room for model distribution
    lv_obj_set_size(s_weekly_sonnet.bar, 296, 8);

    // Model distribution bar (colour-coded segments per model)
    create_model_dist(parent, 136);

    // Model legend (swatch + name per model, horizontal)
    create_model_legend(parent, 150);

    // Burn rate label (left side of combined status line)
    s_burn_label = lv_label_create(parent);
    lv_label_set_text(s_burn_label, "");
    lv_obj_set_style_text_color(s_burn_label, THEME_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_burn_label, &lv_font_montserrat_14, 0);
    int status_row_y = LCD_HEIGHT - THEME_TAB_HEIGHT - 18;
    lv_obj_set_pos(s_burn_label, 8, status_row_y);

    // Prediction label (right-aligned on the same line)
    s_prediction_label = lv_label_create(parent);
    lv_label_set_text(s_prediction_label, "");
    lv_obj_set_style_text_color(s_prediction_label, THEME_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_prediction_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_prediction_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(s_prediction_label, 140);
    lv_obj_set_pos(s_prediction_label, 164, status_row_y);

}

void screen_dashboard_update(const status_data_t *status)
{
    if (!status || !status->valid) {
        return;
    }

    // Calculate locally decremented countdowns
    time_t now = time(NULL);
    int64_t elapsed = 0;
    if (s_last_fetch_time > 0 && now > s_last_fetch_time) {
        elapsed = now - s_last_fetch_time;
    }

    // Detect new data from server by comparing the session resets_at string.
    if (strcmp(status->session.resets_at, s_last_resets_at) != 0) {
        strncpy(s_last_resets_at, status->session.resets_at, sizeof(s_last_resets_at) - 1);
        s_session_remaining = status->session.resets_in_seconds;
        s_weekly_all_remaining = status->weekly_all.resets_in_seconds;
        s_weekly_sonnet_remaining = status->weekly_sonnet.resets_in_seconds;
        s_last_fetch_time = now;
        elapsed = 0;
    }

    int64_t sess_rem = s_session_remaining - elapsed;
    int64_t wa_rem = s_weekly_all_remaining - elapsed;
    int64_t ws_rem = s_weekly_sonnet_remaining - elapsed;
    if (sess_rem < 0) sess_rem = 0;
    if (wa_rem < 0) wa_rem = 0;
    if (ws_rem < 0) ws_rem = 0;

    update_tier(&s_session,       &status->session,        sess_rem);
    update_tier(&s_weekly_all,    &status->weekly_all,     wa_rem);
    update_tier(&s_weekly_sonnet, &status->weekly_sonnet,  ws_rem);

    // Model distribution
    update_model_dist(status);

    // Burn rate (left side of combined line)
    if (status->burn_rate_present) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.0f tok/m | $%.1f/hr",
                 status->burn_tokens_per_min, status->burn_cost_per_hour);
        label_set_text_if_changed(s_burn_label, buf);
    }

    // Prediction (right side of same line)
    if (status->prediction_present) {
        char buf[32];
        if (status->session_will_hit_limit) {
            int64_t secs = status->session_limit_in_seconds;
            if (secs > 0) {
                int hrs = (int)(secs / 3600);
                int mins = (int)((secs % 3600) / 60);
                if (hrs > 0) {
                    snprintf(buf, sizeof(buf), "Limit %dh %dm", hrs, mins);
                } else {
                    snprintf(buf, sizeof(buf), "Limit %dm", mins);
                }
            } else {
                snprintf(buf, sizeof(buf), "Limit hit!");
            }
            lv_obj_set_style_text_color(s_prediction_label, THEME_RED, 0);
        } else if (status->weekly_will_hit_limit) {
            snprintf(buf, sizeof(buf), "Weekly at risk");
            lv_obj_set_style_text_color(s_prediction_label, THEME_ORANGE, 0);
        } else {
            snprintf(buf, sizeof(buf), "Usage OK");
            lv_obj_set_style_text_color(s_prediction_label, THEME_GREEN, 0);
        }
        label_set_text_if_changed(s_prediction_label, buf);
    }


    // Alert background: red when any tier exceeds 80% utilisation
    bool alert = false;
    if (status->session.present && status->session.utilisation >= 80.0f)
        alert = true;
    if (status->weekly_all.present && status->weekly_all.utilisation >= 80.0f)
        alert = true;
    if (status->weekly_sonnet.present && status->weekly_sonnet.utilisation >= 80.0f)
        alert = true;
    if (status->weekly_opus.present && status->weekly_opus.utilisation >= 80.0f)
        alert = true;

    lv_color_t target_bg = alert ? THEME_RED : THEME_BG_COLOUR;
    lv_color_t current_bg = lv_obj_get_style_bg_color(s_dashboard_parent, 0);
    if (current_bg.red != target_bg.red || current_bg.green != target_bg.green || current_bg.blue != target_bg.blue) {
        lv_obj_set_style_bg_color(s_dashboard_parent, target_bg, 0);
    }
}
