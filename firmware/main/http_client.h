#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Maximum number of models in distribution
#define MAX_MODELS 4

// Usage tier data (shared by session and weekly tiers)
typedef struct {
    float   utilisation;       // 0-100+ (session can exceed 100)
    char    resets_at[32];
    int64_t resets_in_seconds;
    bool    present;
} usage_tier_t;

// Model distribution entry
typedef struct {
    char  model[24];
    float cost_pct;
} model_dist_t;

// Full status response (matches CCU API schema)
typedef struct {
    // Dashboard usage tiers
    usage_tier_t session;          // 5-hour session
    usage_tier_t weekly_all;       // 7-day all models
    usage_tier_t weekly_sonnet;    // 7-day sonnet
    usage_tier_t weekly_opus;      // 7-day opus

    // Session details
    float   session_cost_usd;
    int     session_message_count;
    int64_t session_remaining_seconds;
    float   session_remaining_pct;

    // Model distribution (cost-weighted)
    model_dist_t models[MAX_MODELS];
    int          model_count;

    // Burn rate
    float burn_tokens_per_min;
    float burn_cost_per_hour;
    bool  burn_rate_present;

    // Prediction
    bool prediction_present;
    bool session_will_hit_limit;
    int64_t session_limit_in_seconds;
    bool weekly_will_hit_limit;
    int64_t weekly_limit_in_seconds;

    // Meta
    char server_time[32];
    int  data_age_seconds;
    char plan[16];
    bool valid;
} status_data_t;

// Get a pointer to the current status data (thread-safe read).
// Safe to call before http_client_start() -- returns zeroed data with valid=false.
const status_data_t *http_client_get_status(void);

// Create the status mutex. Call before any LVGL timers that use http_client_get_status().
void http_client_init(void);

// Start the HTTP polling task. Polls /api/status every POLL_INTERVAL_MS.
// Calls http_client_init() if not already called.
void http_client_start(void);

// Returns the time of the last successful API response (0 if none yet).
time_t http_client_last_success_time(void);

// Pause and resume HTTP polling (for sleep mode).
void http_client_pause_polling(void);
void http_client_resume_polling(void);
