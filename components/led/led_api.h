#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} rgb_t;

void ws2812_init(int gpio, int num_leds);
void ws2812_set_pixel(int index, uint8_t r, uint8_t g, uint8_t b);
void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b);
void ws2812_show(void);
void ws2812_clear(void);

int ws2812_get_num_leds(void);
rgb_t ws2812_get_pixel(int index);

// builtin led for troubleshooting
void builtin_led_blink(int count, int delay_ms);
