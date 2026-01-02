#include "led_api.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// WS2812 led constants
// High for 0-signal
#define WS2812_T0H_NS 300
// low for 0-signal
#define WS2812_T0L_NS 875
// high for 1-signal
#define WS2812_T1H_NS 875
// low for 1-signal
#define WS2812_T1L_NS 300
// rest in microseconds
#define WS2812_RESET_US 280

// built in led (pin 2)
#define BUILTIN_LED_GPIO 2

// containerof error fix
#ifndef __containerof
#define __containerof(ptr, type, member)                                       \
  ((type *)((char *)(ptr) - offsetof(type, member)))
#endif
static const char *TAG = "ws2812";
static rmt_channel_handle_t s_channel = NULL;
static rmt_encoder_handle_t s_encoder = NULL;
static rgb_t *s_pixels = NULL;
static int s_num_leds = 0;

// Bytes encoder callback
static size_t ws2812_encode(rmt_encoder_t *encoder,
                            rmt_channel_handle_t channel, const void *data,
                            size_t size, rmt_encode_state_t *state);

typedef struct {
  rmt_encoder_t base;
  rmt_encoder_t *bytes_encoder;
  rmt_encoder_t *copy_encoder;
  rmt_symbol_word_t reset_code;
  int state;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder,
                            rmt_channel_handle_t channel, const void *data,
                            size_t size, rmt_encode_state_t *state) {
  ws2812_encoder_t *ws_enc = __containerof(encoder, ws2812_encoder_t, base);
  rmt_encode_state_t session_state = RMT_ENCODING_RESET;
  size_t encoded_symbols = 0;

  switch (ws_enc->state) {
  case 0: // send RGB data
    encoded_symbols += ws_enc->bytes_encoder->encode(
        ws_enc->bytes_encoder, channel, data, size, &session_state);
    if (session_state & RMT_ENCODING_COMPLETE) {
      ws_enc->state = 1;
    }
    if (session_state & RMT_ENCODING_MEM_FULL) {
      *state |= RMT_ENCODING_MEM_FULL;
      return encoded_symbols;
    }
    // fall through
  case 1: // send reset code
    encoded_symbols += ws_enc->copy_encoder->encode(
        ws_enc->copy_encoder, channel, &ws_enc->reset_code,
        sizeof(ws_enc->reset_code), &session_state);
    if (session_state & RMT_ENCODING_COMPLETE) {
      ws_enc->state = 0;
      *state |= RMT_ENCODING_COMPLETE;
    }
    if (session_state & RMT_ENCODING_MEM_FULL) {
      *state |= RMT_ENCODING_MEM_FULL;
    }
    break;
  }
  return encoded_symbols;
}

static esp_err_t ws2812_encoder_reset(rmt_encoder_t *encoder) {
  ws2812_encoder_t *ws_enc = __containerof(encoder, ws2812_encoder_t, base);
  rmt_encoder_reset(ws_enc->bytes_encoder);
  rmt_encoder_reset(ws_enc->copy_encoder);
  ws_enc->state = 0;
  return ESP_OK;
}

static esp_err_t ws2812_encoder_del(rmt_encoder_t *encoder) {
  ws2812_encoder_t *ws_enc = __containerof(encoder, ws2812_encoder_t, base);
  rmt_del_encoder(ws_enc->bytes_encoder);
  rmt_del_encoder(ws_enc->copy_encoder);
  free(ws_enc);
  return ESP_OK;
}

void ws2812_init(int gpio, int num_leds) {
  ESP_LOGI(TAG, "=== WS2812 INIT START ===");
  ESP_LOGI(TAG, "GPIO: %d, LEDs: %d", gpio, num_leds);

  s_num_leds = num_leds;
  s_pixels = calloc(num_leds, sizeof(rgb_t));

  if (!s_pixels) {
    ESP_LOGE(TAG, "Failed to allocate pixel memory!");
    return;
  }
  ESP_LOGI(TAG, "Pixel buffer allocated: %d bytes", num_leds * sizeof(rgb_t));

  // RMT TX channel config
  rmt_tx_channel_config_t tx_cfg = {
      .gpio_num = gpio,
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10000000, // 10MHz = 100ns per tick
      .mem_block_symbols = 64,
      .trans_queue_depth = 4,
  };

  ESP_LOGI(TAG, "Creating RMT TX channel...");
  esp_err_t err = rmt_new_tx_channel(&tx_cfg, &s_channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ RMT channel creation FAILED: %s (0x%x)",
             esp_err_to_name(err), err);
    return;
  }
  ESP_LOGI(TAG, "✅ RMT TX channel created");

  // Create encoder
  ws2812_encoder_t *ws_enc = calloc(1, sizeof(ws2812_encoder_t));
  if (!ws_enc) {
    ESP_LOGE(TAG, "Failed to allocate encoder!");
    return;
  }

  ws_enc->base.encode = ws2812_encode;
  ws_enc->base.reset = ws2812_encoder_reset;
  ws_enc->base.del = ws2812_encoder_del;

  // Bytes encoder for RGB data
  rmt_bytes_encoder_config_t bytes_cfg = {
      .bit0 = {.duration0 = 3,
               .level0 = 1,
               .duration1 = 9,
               .level1 = 0}, // T0H=300ns, T0L=900ns
      .bit1 = {.duration0 = 9,
               .level0 = 1,
               .duration1 = 3,
               .level1 = 0}, // T1H=900ns, T1L=300ns
      .flags.msb_first = 1,
  };

  ESP_LOGI(TAG, "Creating bytes encoder...");
  err = rmt_new_bytes_encoder(&bytes_cfg, &ws_enc->bytes_encoder);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ Bytes encoder creation FAILED: %s", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "✅ Bytes encoder created");

  // Copy encoder for reset code
  rmt_copy_encoder_config_t copy_cfg = {};
  err = rmt_new_copy_encoder(&copy_cfg, &ws_enc->copy_encoder);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ Copy encoder creation FAILED: %s", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "✅ Copy encoder created");

  // Reset code: low for 280us
  ws_enc->reset_code = (rmt_symbol_word_t){
      .duration0 = 2800, .level0 = 0, .duration1 = 0, .level1 = 0};

  s_encoder = &ws_enc->base;

  err = rmt_enable(s_channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ RMT enable FAILED: %s", esp_err_to_name(err));
    return;
  }

  ESP_LOGI(TAG, "✅✅✅ WS2812D init SUCCESS on GPIO %d ✅✅✅", gpio);
}

void ws2812_set_pixel(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index >= 0 && index < s_num_leds) {
    s_pixels[index] = (rgb_t){r, g, b};
  }
}

void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < s_num_leds; i++) {
    s_pixels[i] = (rgb_t){r, g, b};
  }
}

void ws2812_clear(void) {
  ws2812_set_all(0, 0, 0);
  ws2812_show();
}

void ws2812_show(void) {
  ESP_LOGI(TAG, "=== ws2812_show() called ===");

  if (!s_channel) {
    ESP_LOGE(TAG, "❌ RMT channel is NULL!");
    return;
  }

  if (!s_encoder) {
    ESP_LOGE(TAG, "❌ Encoder is NULL!");
    return;
  }

  // WS2812 expects GRB order
  uint8_t *grb_data = malloc(s_num_leds * 3);
  if (!grb_data) {
    ESP_LOGE(TAG, "❌ Failed to allocate GRB buffer!");
    return;
  }

  for (int i = 0; i < s_num_leds; i++) {
    grb_data[i * 3 + 0] = s_pixels[i].g;
    grb_data[i * 3 + 1] = s_pixels[i].r;
    grb_data[i * 3 + 2] = s_pixels[i].b;

    ESP_LOGI(TAG, "LED %d: R=%d G=%d B=%d -> GRB: %02x %02x %02x", i,
             s_pixels[i].r, s_pixels[i].g, s_pixels[i].b, grb_data[i * 3 + 0],
             grb_data[i * 3 + 1], grb_data[i * 3 + 2]);
  }

  rmt_transmit_config_t tx_config = {.loop_count = 0};

  ESP_LOGI(TAG, "Transmitting %d bytes...", s_num_leds * 3);
  esp_err_t err =
      rmt_transmit(s_channel, s_encoder, grb_data, s_num_leds * 3, &tx_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ rmt_transmit FAILED: %s", esp_err_to_name(err));
    free(grb_data);
    return;
  }
  ESP_LOGI(TAG, "Waiting for transmission...");

  err = rmt_tx_wait_all_done(s_channel, portMAX_DELAY);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ rmt_tx_wait_all_done FAILED: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "✅ Transmission complete!");
  }

  free(grb_data);
  ESP_LOGI(TAG, "=== ws2812_show() done ===");
}

// builtin led blink
void builtin_led_blink(int count, int delay_ms) {
  // configure if not done
  static bool gpio_initialized = false;
  if (!gpio_initialized) {
    gpio_set_direction(BUILTIN_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_initialized = true;
  }

  for (int i = 0; i < count; i++) {
    gpio_set_level(BUILTIN_LED_GPIO, 1); // on
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    gpio_set_level(BUILTIN_LED_GPIO, 0); // off
    if (i < count - 1) {                 // pause
      vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
  }
}

int ws2812_get_num_leds(void) { return s_num_leds; }

rgb_t ws2812_get_pixel(int index) {
  if (index >= 0 && index < s_num_leds) {
    return s_pixels[index];
  }
  return (rgb_t){0, 0, 0};
}
