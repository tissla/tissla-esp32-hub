#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  LED_CMD_ON,
  LED_CMD_OFF,
  LED_CMD_BLINK,
  LED_CMD_SET_BRIGHTNESS
} led_cmd_type_t;

typedef struct {
  led_cmd_type_t type;
  uint8_t brightness; // 0-255
  uint16_t count;
  uint16_t on_ms;
  uint16_t off_ms;
} led_cmd_t;

void led_api_init(int gpio_num);
bool led_api_send(const led_cmd_t *cmd);
