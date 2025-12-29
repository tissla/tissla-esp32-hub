#include "led_effects.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_api.h"
#include <math.h>

// Konvertera HSV (Hue, Saturation, Value) till RGB
void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g,
                uint8_t *b) {
  h %= 360; // Hue är 0-359 grader
  uint8_t region = h / 60;
  uint8_t remainder = (h - (region * 60)) * 6;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
  case 0:
    *r = v;
    *g = t;
    *b = p;
    break;
  case 1:
    *r = q;
    *g = v;
    *b = p;
    break;
  case 2:
    *r = p;
    *g = v;
    *b = t;
    break;
  case 3:
    *r = p;
    *g = q;
    *b = v;
    break;
  case 4:
    *r = t;
    *g = p;
    *b = v;
    break;
  default:
    *r = v;
    *g = p;
    *b = q;
    break;
  }
}

// Rainbow chase - färgregnbåge som rör sig längs stripen
void led_rainbow_chase(uint8_t speed, uint32_t duration_ms) {
  int num_leds = ws2812_get_num_leds();
  uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  uint16_t hue_offset = 0;

  while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_time) <
         duration_ms) {
    for (int i = 0; i < num_leds; i++) {
      // Varje LED får en färg baserat på position + offset
      uint16_t hue = ((i * 360 / num_leds) + hue_offset) % 360;
      uint8_t r, g, b;
      hsv_to_rgb(hue, 255, 255, &r, &g, &b);
      ws2812_set_pixel(i, r, g, b);
    }
    ws2812_show();

    hue_offset += speed;
    if (hue_offset >= 360)
      hue_offset = 0;

    vTaskDelay(pdMS_TO_TICKS(20)); // ~50 FPS
  }
}

// Rainbow cycle - alla LEDs ändrar färg synkront
void led_rainbow_cycle(uint8_t speed, uint32_t duration_ms) {
  uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  uint16_t hue = 0;

  while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_time) <
         duration_ms) {
    uint8_t r, g, b;
    hsv_to_rgb(hue, 255, 255, &r, &g, &b);
    ws2812_set_all(r, g, b);
    ws2812_show();

    hue = (hue + speed) % 360;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// Studsande boll-effekt
void led_bouncing_ball(uint8_t r, uint8_t g, uint8_t b, uint8_t speed) {
  int num_leds = ws2812_get_num_leds();
  int position = 0;
  int direction = 1; // 1 = framåt, -1 = bakåt

  for (int i = 0; i < 100; i++) { // 100 studsar
    ws2812_clear();

    // Rita "bollen" med trailing effect
    ws2812_set_pixel(position, r, g, b);
    if (position > 0) {
      ws2812_set_pixel(position - 1, r / 4, g / 4, b / 4);
    }
    if (position < num_leds - 1) {
      ws2812_set_pixel(position + 1, r / 4, g / 4, b / 4);
    }

    ws2812_show();

    position += direction;

    // Studsa vid ändarna
    if (position >= num_leds - 1 || position <= 0) {
      direction *= -1;
    }

    vTaskDelay(pdMS_TO_TICKS(50 / speed));
  }
}

// Color wipe - fyller stripen från början till slut
void led_color_wipe(uint8_t r, uint8_t g, uint8_t b, uint16_t delay_ms) {
  int num_leds = ws2812_get_num_leds();

  for (int i = 0; i < num_leds; i++) {
    ws2812_set_pixel(i, r, g, b);
    ws2812_show();
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}
