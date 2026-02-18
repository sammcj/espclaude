#include <stdio.h>
#include <time.h>

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include "config.h"
#include "wifi.h"
#include "http_client.h"

#include "ui/ui.h"

static const char *TAG = "espclaude";

static void sntp_init_time(void)
{
    ESP_LOGI(TAG, "initialising SNTP");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER_1);
    esp_sntp_setservername(1, NTP_SERVER_2);
    esp_sntp_init();

    // Wait for time to be set (timeout after ~10s)
    int retry = 0;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[32];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "current time: %s", strftime_buf);
}

static void ui_update_timer_cb(lv_timer_t *timer)
{
    ui_update();
}

void app_main(void)
{
    // Initialise NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialise BSP display
    bsp_display_start();
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "display initialised");

    // Create the HTTP client mutex BEFORE any LVGL timers that call http_client_get_status().
    // Without this, the timer callback would xSemaphoreTake(NULL) and trigger a panic.
    http_client_init();

    // Lock LVGL mutex for UI setup
    bsp_display_lock(0);

    // Create UI
    ui_init();

    // Create an LVGL timer to update the UI every second
    lv_timer_create(ui_update_timer_cb, UI_COUNTDOWN_MS, NULL);

    bsp_display_unlock();

    // Connect to WiFi (non-blocking: times out after 15s but keeps retrying)
    ESP_LOGI(TAG, "connecting to WiFi...");
    ret = wifi_init_sta();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected");
    } else {
        ESP_LOGW(TAG, "WiFi not yet connected, will keep retrying in background");
    }

    // Start NTP and HTTP regardless -- both tolerate no connectivity and
    // will begin working once WiFi connects (even if that's later).
    sntp_init_time();
    http_client_start();

    ESP_LOGI(TAG, "espclaude firmware running");
}
