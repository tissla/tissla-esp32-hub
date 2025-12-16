#include "led_api.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "driver/ledc.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_RESOLUTION LEDC_TIMER_8_BIT // 0–255
#define LEDC_FREQ_HZ 5000

static QueueHandle_t s_led_q;
static int s_led_gpio = -1;

// set brightness func
static void led_set_brightness(uint8_t level) {
  // level: 0–255
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, level);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// init pwm-settings
static void led_pwm_init(int gpio) {
  ledc_timer_config_t timer = {.speed_mode = LEDC_MODE,
                               .timer_num = LEDC_TIMER,
                               .duty_resolution = LEDC_RESOLUTION,
                               .freq_hz = LEDC_FREQ_HZ,
                               .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&timer);

  ledc_channel_config_t ch = {.gpio_num = gpio,
                              .speed_mode = LEDC_MODE,
                              .channel = LEDC_CHANNEL,
                              .timer_sel = LEDC_TIMER,
                              .duty = 0,
                              .hpoint = 0};
  ledc_channel_config(&ch);
}

static void led_task(void *arg) {
  led_cmd_t cmd;

  static const char *LTAG = "led";

  ESP_LOGI(LTAG, "cmd=%d brightness=%u count=%u on=%u off=%u", cmd.type,
           cmd.brightness, cmd.count, cmd.on_ms, cmd.off_ms);

  while (1) {
    if (xQueueReceive(s_led_q, &cmd, portMAX_DELAY) == pdTRUE) {

      // input
      switch (cmd.type) {

        // turn on LED
      case LED_CMD_ON:
        led_set_brightness(255);
        break;

        // turn off LED
      case LED_CMD_OFF:
        led_set_brightness(0);
        break;

        // set brightness
      case LED_CMD_SET_BRIGHTNESS:
        led_set_brightness(cmd.brightness);
        break;

        // Blink
      case LED_CMD_BLINK: {
        uint16_t n = cmd.count ? cmd.count : 1;
        uint16_t on_ms = cmd.on_ms ? cmd.on_ms : 150;
        uint16_t off_ms = cmd.off_ms ? cmd.off_ms : 150;
        uint8_t level = cmd.brightness ? cmd.brightness : 255;

        for (uint16_t i = 0; i < n; i++) {
          led_set_brightness(level);
          vTaskDelay(pdMS_TO_TICKS(on_ms));
          led_set_brightness(0);
          if (i + 1 < n)
            vTaskDelay(pdMS_TO_TICKS(off_ms));
        }
        break;
      }
      default:
        break;
      }
    }
  }
}

// init the api
void led_api_init(int gpio_num) {
  s_led_gpio = gpio_num;

  led_pwm_init(s_led_gpio);
  //
  // gpio_reset_pin(s_led_gpio);
  // gpio_set_direction(s_led_gpio, GPIO_MODE_OUTPUT);
  // gpio_set_level(s_led_gpio, 0);

  led_set_brightness(100);

  s_led_q = xQueueCreate(8, sizeof(led_cmd_t));
  // Stack/prio kan tweakas senare
  xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
}

// dynamic api function
bool led_api_send(const led_cmd_t *cmd) {
  if (!s_led_q)
    return false;
  return xQueueSend(s_led_q, cmd, 0) == pdTRUE;
}
