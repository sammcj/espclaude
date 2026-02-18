#include "http_client.h"
#include "config.h"
#include "wifi.h"

// Default API_TOKEN to empty if not defined in config.h
#ifndef API_TOKEN
#define API_TOKEN ""
#endif

#include <string.h>
#include <time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "http_client";

#define HTTP_BUF_SIZE 4096

static status_data_t s_status = {0};
static status_data_t s_status_copy = {0};
static SemaphoreHandle_t s_status_mutex;
static bool s_got_first_response = false;
static time_t s_last_success_time = 0;
static bool s_polling_paused = false;
static char s_http_buf[HTTP_BUF_SIZE];
static int s_http_buf_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (s_http_buf_len + evt->data_len < HTTP_BUF_SIZE - 1) {
            memcpy(s_http_buf + s_http_buf_len, evt->data, evt->data_len);
            s_http_buf_len += evt->data_len;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

// Parse a usage tier from a JSON object with utilisation_pct, resets_at, resets_in_seconds
static void parse_tier(cJSON *obj, usage_tier_t *tier)
{
    if (!obj || cJSON_IsNull(obj)) {
        tier->present = false;
        return;
    }
    tier->present = true;

    cJSON *util = cJSON_GetObjectItem(obj, "utilisation_pct");
    if (cJSON_IsNumber(util)) {
        tier->utilisation = (float)util->valuedouble;
    }

    cJSON *resets = cJSON_GetObjectItem(obj, "resets_at");
    if (cJSON_IsString(resets) && resets->valuestring) {
        strncpy(tier->resets_at, resets->valuestring, sizeof(tier->resets_at) - 1);
    }

    cJSON *secs = cJSON_GetObjectItem(obj, "resets_in_seconds");
    if (cJSON_IsNumber(secs)) {
        tier->resets_in_seconds = (int64_t)secs->valuedouble;
    }
}

static void parse_status_response(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return;
    }

    status_data_t new_status = {0};
    new_status.valid = true;

    // Parse session (5-hour)
    cJSON *session = cJSON_GetObjectItem(root, "session");
    if (session && !cJSON_IsNull(session)) {
        parse_tier(session, &new_status.session);

        cJSON *cost = cJSON_GetObjectItem(session, "cost_usd");
        if (cJSON_IsNumber(cost)) {
            new_status.session_cost_usd = (float)cost->valuedouble;
        }

        cJSON *msgs = cJSON_GetObjectItem(session, "message_count");
        if (cJSON_IsNumber(msgs)) {
            new_status.session_message_count = (int)msgs->valuedouble;
        }

        cJSON *rem_secs = cJSON_GetObjectItem(session, "remaining_seconds");
        if (cJSON_IsNumber(rem_secs)) {
            new_status.session_remaining_seconds = (int64_t)rem_secs->valuedouble;
        }

        cJSON *rem_pct = cJSON_GetObjectItem(session, "remaining_pct");
        if (cJSON_IsNumber(rem_pct)) {
            new_status.session_remaining_pct = (float)rem_pct->valuedouble;
        }

        // Parse model distribution
        cJSON *dist = cJSON_GetObjectItem(session, "model_distribution");
        if (cJSON_IsArray(dist)) {
            int count = cJSON_GetArraySize(dist);
            if (count > MAX_MODELS) count = MAX_MODELS;
            new_status.model_count = count;

            for (int i = 0; i < count; i++) {
                cJSON *item = cJSON_GetArrayItem(dist, i);
                cJSON *model = cJSON_GetObjectItem(item, "model");
                if (cJSON_IsString(model) && model->valuestring) {
                    strncpy(new_status.models[i].model, model->valuestring,
                            sizeof(new_status.models[i].model) - 1);
                }
                cJSON *pct = cJSON_GetObjectItem(item, "cost_pct");
                if (cJSON_IsNumber(pct)) {
                    new_status.models[i].cost_pct = (float)pct->valuedouble;
                }
            }
        }
    }

    // Parse weekly tiers
    cJSON *weekly = cJSON_GetObjectItem(root, "weekly");
    if (weekly && !cJSON_IsNull(weekly)) {
        parse_tier(cJSON_GetObjectItem(weekly, "all_models"), &new_status.weekly_all);
        parse_tier(cJSON_GetObjectItem(weekly, "sonnet"), &new_status.weekly_sonnet);
        parse_tier(cJSON_GetObjectItem(weekly, "opus"), &new_status.weekly_opus);
    }

    // Parse burn rate
    cJSON *burn = cJSON_GetObjectItem(root, "burn_rate");
    if (burn && !cJSON_IsNull(burn)) {
        new_status.burn_rate_present = true;
        cJSON *tpm = cJSON_GetObjectItem(burn, "tokens_per_min");
        if (cJSON_IsNumber(tpm)) {
            new_status.burn_tokens_per_min = (float)tpm->valuedouble;
        }
        cJSON *cph = cJSON_GetObjectItem(burn, "cost_per_hour_usd");
        if (cJSON_IsNumber(cph)) {
            new_status.burn_cost_per_hour = (float)cph->valuedouble;
        }
    }

    // Parse prediction
    cJSON *pred = cJSON_GetObjectItem(root, "prediction");
    if (pred && !cJSON_IsNull(pred)) {
        new_status.prediction_present = true;

        cJSON *swhl = cJSON_GetObjectItem(pred, "session_will_hit_limit");
        new_status.session_will_hit_limit = cJSON_IsTrue(swhl);

        cJSON *slis = cJSON_GetObjectItem(pred, "session_limit_in_seconds");
        if (cJSON_IsNumber(slis)) {
            new_status.session_limit_in_seconds = (int64_t)slis->valuedouble;
        }

        cJSON *wwhl = cJSON_GetObjectItem(pred, "weekly_will_hit_limit");
        new_status.weekly_will_hit_limit = cJSON_IsTrue(wwhl);

        cJSON *wlis = cJSON_GetObjectItem(pred, "weekly_limit_in_seconds");
        if (cJSON_IsNumber(wlis)) {
            new_status.weekly_limit_in_seconds = (int64_t)wlis->valuedouble;
        }
    }

    // Parse meta
    cJSON *st = cJSON_GetObjectItem(root, "server_time");
    if (cJSON_IsString(st) && st->valuestring) {
        strncpy(new_status.server_time, st->valuestring, sizeof(new_status.server_time) - 1);
    }

    cJSON *age = cJSON_GetObjectItem(root, "data_age_seconds");
    if (cJSON_IsNumber(age)) {
        new_status.data_age_seconds = (int)age->valuedouble;
    }

    cJSON *plan = cJSON_GetObjectItem(root, "plan");
    if (cJSON_IsString(plan) && plan->valuestring) {
        strncpy(new_status.plan, plan->valuestring, sizeof(new_status.plan) - 1);
    }

    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    s_status = new_status;
    s_got_first_response = true;
    s_last_success_time = time(NULL);
    xSemaphoreGive(s_status_mutex);

    ESP_LOGI(TAG, "status: session=%.0f%% weekly=%.0f%% burn=$%.1f/hr",
             new_status.session.utilisation,
             new_status.weekly_all.utilisation,
             new_status.burn_cost_per_hour);

    cJSON_Delete(root);
}

static void fetch_status(void)
{
    if (!wifi_is_connected()) {
        return;
    }

    char url[128];
    snprintf(url, sizeof(url), "%s%s", SERVER_URL, API_STATUS_PATH);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    s_http_buf_len = 0;

    // Send Bearer token if configured
    if (sizeof(API_TOKEN) > 1) {  // non-empty string
        char auth_header[128];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", API_TOKEN);
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status == 200) {
            s_http_buf[s_http_buf_len] = '\0';
            parse_status_response(s_http_buf);
        } else {
            ESP_LOGW(TAG, "HTTP %d", status);
        }
    } else {
        ESP_LOGW(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void http_poll_task(void *arg)
{
    // Wait a moment for WiFi to stabilise
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        if (!s_polling_paused) {
            fetch_status();
        }
        TickType_t delay = pdMS_TO_TICKS(
            s_got_first_response ? POLL_INTERVAL_MS : POLL_FAST_INTERVAL_MS);
        vTaskDelay(delay);
    }
}

const status_data_t *http_client_get_status(void)
{
    if (!s_status_mutex) {
        return &s_status_copy;
    }
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    s_status_copy = s_status;
    xSemaphoreGive(s_status_mutex);
    return &s_status_copy;
}

time_t http_client_last_success_time(void)
{
    if (!s_status_mutex) {
        return 0;
    }
    xSemaphoreTake(s_status_mutex, portMAX_DELAY);
    time_t t = s_last_success_time;
    xSemaphoreGive(s_status_mutex);
    return t;
}

void http_client_pause_polling(void)
{
    s_polling_paused = true;
}

void http_client_resume_polling(void)
{
    s_polling_paused = false;
}

void http_client_init(void)
{
    s_status_mutex = xSemaphoreCreateMutex();
}

void http_client_start(void)
{
    if (!s_status_mutex) {
        http_client_init();
    }
    xTaskCreate(http_poll_task, "http_poll", 8192, NULL, 5, NULL);
}
