#include "screen_settings.h"
#include "ui.h"
#include "theme.h"
#include "wifi.h"
#include "config.h"
#include <stdio.h>

static lv_obj_t *s_wifi_status;
static lv_obj_t *s_wifi_ip;
static lv_obj_t *s_wifi_rssi;
static lv_obj_t *s_server_url;
static lv_obj_t *s_poll_interval;
static lv_obj_t *s_sleep_timeout;
static lv_obj_t *s_sleep_remaining;

static lv_obj_t *create_setting_row(lv_obj_t *parent, const char *label, int y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, THEME_TEXT_SECONDARY, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl, 8, y);

    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, THEME_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(val, 120, y);

    return val;
}

void screen_settings_init(lv_obj_t *parent)
{
    lv_obj_set_style_pad_all(parent, 4, 0);

    s_wifi_status  = create_setting_row(parent, "WiFi:", 4);
    s_wifi_ip      = create_setting_row(parent, "IP:", 28);
    s_wifi_rssi    = create_setting_row(parent, "Signal:", 52);
    s_server_url   = create_setting_row(parent, "Server:", 84);
    s_poll_interval = create_setting_row(parent, "Poll:", 108);

    s_sleep_timeout   = create_setting_row(parent, "Sleep:", 132);
    s_sleep_remaining = create_setting_row(parent, "Sleep in:", 156);

    // Static values
    lv_label_set_text(s_server_url, SERVER_URL);

    char buf[16];
    snprintf(buf, sizeof(buf), "%ds", POLL_INTERVAL_MS / 1000);
    lv_label_set_text(s_poll_interval, buf);

    int sleep_hrs = SLEEP_AFTER_MS / 3600000;
    int sleep_mins = (SLEEP_AFTER_MS % 3600000) / 60000;
    if (sleep_hrs > 0 && sleep_mins > 0) {
        snprintf(buf, sizeof(buf), "%dh %dm", sleep_hrs, sleep_mins);
    } else if (sleep_hrs > 0) {
        snprintf(buf, sizeof(buf), "%dh", sleep_hrs);
    } else {
        snprintf(buf, sizeof(buf), "%dm", sleep_mins);
    }
    lv_label_set_text(s_sleep_timeout, buf);
}

void screen_settings_update(void)
{
    static bool s_last_connected = false;
    bool connected = wifi_is_connected();

    if (connected) {
        label_set_text_if_changed(s_wifi_status, WIFI_SSID);
        if (!s_last_connected) {
            lv_obj_set_style_text_color(s_wifi_status, THEME_GREEN, 0);
        }

        label_set_text_if_changed(s_wifi_ip, wifi_get_ip());

        char rssi_buf[16];
        int8_t rssi = wifi_get_rssi();
        snprintf(rssi_buf, sizeof(rssi_buf), "%d dBm", rssi);
        label_set_text_if_changed(s_wifi_rssi, rssi_buf);
    } else {
        label_set_text_if_changed(s_wifi_status, "Disconnected");
        if (s_last_connected) {
            lv_obj_set_style_text_color(s_wifi_status, THEME_RED, 0);
        }
        label_set_text_if_changed(s_wifi_ip, "--");
        label_set_text_if_changed(s_wifi_rssi, "--");
    }

    s_last_connected = connected;

    // Sleep countdown
    if (ui_is_sleeping()) {
        label_set_text_if_changed(s_sleep_remaining, "Sleeping");
        lv_obj_set_style_text_color(s_sleep_remaining, THEME_TEXT_DIM, 0);
    } else {
        uint32_t remain_ms = ui_sleep_remaining_ms();
        uint32_t remain_s = remain_ms / 1000;
        int hrs = (int)(remain_s / 3600);
        int mins = (int)((remain_s % 3600) / 60);
        char buf[16];
        if (hrs > 0) {
            snprintf(buf, sizeof(buf), "%dh %dm", hrs, mins);
        } else {
            snprintf(buf, sizeof(buf), "%dm", mins);
        }
        label_set_text_if_changed(s_sleep_remaining, buf);
        lv_obj_set_style_text_color(s_sleep_remaining, THEME_TEXT_PRIMARY, 0);
    }
}
