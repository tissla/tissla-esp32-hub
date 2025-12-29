idf_component_register(
    SRCS "led_api.c" "led_effects.c"
    INCLUDE_DIRS "."
	PRIV_REQUIRES esp_driver_gpio esp_driver_ledc freertos esp_driver_rmt
)
