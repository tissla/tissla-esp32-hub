idf_component_register(
    SRCS "wifi_connect.c"
    INCLUDE_DIRS "."
	PRIV_REQUIRES esp_event esp_netif esp_wifi nvs_flash log
)
