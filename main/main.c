#include "esp_err.h"
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

  // init leds
  ws2812_init(14, 50);
  // try to connect or start AP mode
  wifi_connect();
  // wait for connect (returns false if AP-mode)
  wifi_wait_until_connected(10000);

  // start API
  http_api_start();
  // log
  wifi_print_status();
}
