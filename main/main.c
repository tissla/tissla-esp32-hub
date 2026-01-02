#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "http_web.h"
#include "led_api.h"
#include "nvs_flash.h"
#include "wifi_connect.h"

// main loop
void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
  }
  ESP_LOGI("main", "=== LED TEST START ===");
  ws2812_init(27, 12);

  gpio_set_drive_capability(GPIO_NUM_27, GPIO_DRIVE_CAP_3);

  for (int i = 0; i < 5; i++) {
    ESP_LOGI("main", "Test %d: RED", i);
    ws2812_set_all(200, 0, 0);
    ws2812_show();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI("main", "Test %d: GREEN", i);
    ws2812_set_all(0, 200, 0);
    ws2812_show();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI("main", "Test %d: BLUE", i);
    ws2812_set_all(0, 0, 200);
    ws2812_show();
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI("main", "Test %d: OFF", i);
    ws2812_clear();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  ESP_LOGI("main", "=== LED TEST DONE ===");

  // try to connect or start AP mode
  wifi_connect();
  // wait for connect (returns false if AP-mode)
  wifi_wait_until_connected(10000);

  // start API
  http_api_start();
  // log
  wifi_print_status();
}
