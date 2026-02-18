#include "screen_instances.h"
#include "ui.h"
#include "theme.h"
#include <stdio.h>

// Session details screen (repurposed from instance list)
// Shows: cost, messages, model distribution, remaining time

static lv_obj_t *s_cost_label;
static lv_obj_t *s_messages_label;
static lv_obj_t *s_remaining_label;
static lv_obj_t *s_models_label;
static lv_obj_t *s_plan_label;
static lv_obj_t *s_no_data_label;

static lv_obj_t *create_row(lv_obj_t *parent, const char *heading, int y)
{
    lv_obj_t *hdr = lv_label_create(parent);
    lv_label_set_text(hdr, heading);
    lv_obj_set_style_text_color(hdr, THEME_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(hdr, 8, y);

    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, THEME_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(val, 120, y);

    return val;
}

void screen_instances_init(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 0, 0);

    s_no_data_label = lv_label_create(parent);
    lv_label_set_text(s_no_data_label, "Waiting for data...");
    lv_obj_set_style_text_color(s_no_data_label, THEME_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(s_no_data_label, &lv_font_montserrat_16, 0);
    lv_obj_center(s_no_data_label);

    s_plan_label      = create_row(parent, "Plan:",      8);
    s_cost_label      = create_row(parent, "Session $:", 30);
    s_messages_label  = create_row(parent, "Messages:",  52);
    s_remaining_label = create_row(parent, "Remaining:", 74);
    s_models_label    = create_row(parent, "Models:",    96);

    // Allow model label to wrap (long text)
    lv_obj_set_width(s_models_label, 180);
    lv_label_set_long_mode(s_models_label, LV_LABEL_LONG_WRAP);
}

void screen_instances_update(const status_data_t *status)
{
    if (!status || !status->valid) {
        return;
    }

    lv_obj_add_flag(s_no_data_label, LV_OBJ_FLAG_HIDDEN);

    // Plan
    if (status->plan[0]) {
        label_set_text_if_changed(s_plan_label, status->plan);
    }

    // Session cost
    char buf[64];
    snprintf(buf, sizeof(buf), "$%.2f", status->session_cost_usd);
    label_set_text_if_changed(s_cost_label, buf);

    // Messages
    snprintf(buf, sizeof(buf), "%d", status->session_message_count);
    label_set_text_if_changed(s_messages_label, buf);

    // Remaining time
    int64_t rem = status->session_remaining_seconds;
    if (rem > 0) {
        int hrs  = (int)(rem / 3600);
        int mins = (int)((rem % 3600) / 60);
        snprintf(buf, sizeof(buf), "%dh %dm (%.0f%%)", hrs, mins, status->session_remaining_pct);
    } else {
        snprintf(buf, sizeof(buf), "Expired");
    }
    label_set_text_if_changed(s_remaining_label, buf);

    // Model distribution
    if (status->model_count > 0) {
        char models_buf[128] = "";
        int offset = 0;
        for (int i = 0; i < status->model_count && i < MAX_MODELS; i++) {
            if (i > 0) {
                offset += snprintf(models_buf + offset, sizeof(models_buf) - offset, ", ");
            }
            offset += snprintf(models_buf + offset, sizeof(models_buf) - offset,
                               "%s %.0f%%", status->models[i].model, status->models[i].cost_pct);
        }
        label_set_text_if_changed(s_models_label, models_buf);
    }
}
