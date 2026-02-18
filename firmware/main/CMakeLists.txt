idf_component_register(
    SRCS
        "main.c"
        "wifi.c"
        "http_client.c"

        "ui/ui.c"
        "ui/screen_dashboard.c"
        "ui/screen_instances.c"
        "ui/screen_settings.c"
        "ui/theme.c"
    INCLUDE_DIRS
        "."
        "ui"
    PRIV_REQUIRES
        esp_wifi
        esp_http_client
        esp_netif
        nvs_flash
        lwip
        json

)
