idf_component_register(
    SRCS "led_api.c"
    INCLUDE_DIRS "."
	PRIV_REQUIRES esp_driver_gpio esp_driver_ledc freertos
)
