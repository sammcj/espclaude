#include "ui.h"
#include "theme.h"
#include "screen_dashboard.h"
#include "screen_instances.h"
#include "screen_settings.h"
#include "http_client.h"
#include "config.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "ui";

static lv_obj_t *s_tabview = NULL;
static lv_obj_t *s_status_banner = NULL;
static lv_obj_t *s_status_banner_label = NULL;
static uint32_t s_last_activity_tick = 0;
static bool s_sleeping = false;

static void enter_sleep(void)
{
    if (s_sleeping) return;
    s_sleeping = true;
    ESP_LOGI(TAG, "entering sleep mode");
    http_client_pause_polling();
    bsp_display_enter_sleep();
}

static void exit_sleep(void)
{
    if (!s_sleeping) return;
    s_sleeping = false;
    ESP_LOGI(TAG, "waking from sleep");
    bsp_display_exit_sleep();
    http_client_resume_polling();
    s_last_activity_tick = lv_tick_get();
}

static void screen_touch_cb(lv_event_t *e)
{
    (void)e;
    ui_notify_activity();
}

void ui_init(void)
{
    theme_init();

    // Set screen background
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, THEME_BG_COLOUR, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Create tabview
    s_tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_size(s_tabview, THEME_TAB_HEIGHT);
    lv_obj_set_size(s_tabview, LCD_WIDTH, LCD_HEIGHT);

    // Style the tabview
    lv_obj_set_style_bg_color(s_tabview, THEME_BG_COLOUR, 0);
    lv_obj_set_style_bg_opa(s_tabview, LV_OPA_COVER, 0);

    // Style tab bar
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(s_tabview);
    lv_obj_set_style_bg_color(tab_bar, THEME_PANEL_COLOUR, 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tab_bar, THEME_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);

    // Create tabs
    lv_obj_t *tab_dash = lv_tabview_add_tab(s_tabview, "Dashboard");
    lv_obj_t *tab_inst = lv_tabview_add_tab(s_tabview, "Sessions");
    lv_obj_t *tab_sett = lv_tabview_add_tab(s_tabview, "Settings");

    // Style tab buttons: themed text and indicator
    uint32_t btn_count = lv_obj_get_child_count(tab_bar);
    for (uint32_t i = 0; i < btn_count; i++) {
        lv_obj_t *btn = lv_obj_get_child(tab_bar, i);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(btn, THEME_TEXT_SECONDARY, 0);
        lv_obj_set_style_text_color(btn, THEME_ACCENT, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, THEME_BAR_BG, LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, THEME_ACCENT, LV_STATE_CHECKED);
    }

    // Set tab content backgrounds
    lv_obj_set_style_bg_color(tab_dash, THEME_BG_COLOUR, 0);
    lv_obj_set_style_bg_color(tab_inst, THEME_BG_COLOUR, 0);
    lv_obj_set_style_bg_color(tab_sett, THEME_BG_COLOUR, 0);

    // Initialise screen contents
    screen_dashboard_init(tab_dash);
    screen_instances_init(tab_inst);
    screen_settings_init(tab_sett);

    // Status banner (info on startup, warning when data goes stale)
    s_status_banner = lv_obj_create(scr);
    lv_obj_remove_style_all(s_status_banner);
    lv_obj_set_size(s_status_banner, LCD_WIDTH, 28);
    lv_obj_set_pos(s_status_banner, 0, LCD_HEIGHT - 28);
    lv_obj_set_style_bg_opa(s_status_banner, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_status_banner, LV_OBJ_FLAG_SCROLLABLE);

    s_status_banner_label = lv_label_create(s_status_banner);
    lv_obj_set_style_text_color(s_status_banner_label, THEME_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(s_status_banner_label, &lv_font_montserrat_14, 0);
    lv_obj_center(s_status_banner_label);

    // Start with info-style "fetching" message
    lv_obj_set_style_bg_color(s_status_banner, THEME_PANEL_COLOUR, 0);
    lv_label_set_text(s_status_banner_label, "Fetching data from server...");
    lv_obj_set_style_text_color(s_status_banner_label, THEME_ACCENT, 0);

    // Sleep timer: register touch handler on the active screen
    s_last_activity_tick = lv_tick_get();
    lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_PRESSED, NULL);
}

void ui_update(void)
{
    // Check sleep timeout
    uint32_t idle_ms = lv_tick_elaps(s_last_activity_tick);
    if (!s_sleeping && idle_ms >= SLEEP_AFTER_MS) {
        enter_sleep();
    }

    // Skip UI updates while sleeping
    if (s_sleeping) {
        return;
    }

    const status_data_t *status = http_client_get_status();
    screen_dashboard_update(status);
    screen_instances_update(status);
    screen_settings_update();

    // Status banner: info while fetching, warning when stale, hidden otherwise
    time_t last = http_client_last_success_time();
    if (last == 0) {
        // Never received data -- show info-style banner
        lv_obj_set_style_bg_color(s_status_banner, THEME_PANEL_COLOUR, 0);
        lv_obj_set_style_text_color(s_status_banner_label, THEME_ACCENT, 0);
        lv_label_set_text(s_status_banner_label, "Fetching data from server...");
        lv_obj_clear_flag(s_status_banner, LV_OBJ_FLAG_HIDDEN);
    } else {
        time_t now = time(NULL);
        bool stale = (now - last) > STALE_DATA_SECONDS;
        if (stale) {
            // Had data but it's gone stale -- show warning
            lv_obj_set_style_bg_color(s_status_banner, THEME_RED, 0);
            lv_obj_set_style_text_color(s_status_banner_label, THEME_TEXT_PRIMARY, 0);
            lv_label_set_text(s_status_banner_label, "No data from server");
            lv_obj_clear_flag(s_status_banner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_status_banner, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

lv_obj_t *ui_get_tabview(void)
{
    return s_tabview;
}

bool ui_is_sleeping(void)
{
    return s_sleeping;
}

void ui_notify_activity(void)
{
    s_last_activity_tick = lv_tick_get();
    if (s_sleeping) {
        exit_sleep();
    }
}

uint32_t ui_sleep_remaining_ms(void)
{
    if (s_sleeping) return 0;
    uint32_t elapsed = lv_tick_elaps(s_last_activity_tick);
    if (elapsed >= SLEEP_AFTER_MS) return 0;
    return SLEEP_AFTER_MS - elapsed;
}
