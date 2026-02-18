#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Initialise WiFi in STA mode and connect to the configured AP.
// Blocks until connected or max retries exceeded.
esp_err_t wifi_init_sta(void);

// Returns true if WiFi is connected.
bool wifi_is_connected(void);

// Get the current IP address as a string. Returns empty string if not connected.
const char *wifi_get_ip(void);

// Get the current RSSI (signal strength). Returns 0 if not connected.
int8_t wifi_get_rssi(void);
