#include "esp_err.h"
#include "http_api.h"
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

  wifi_connect();
  wifi_wait_until_connected();

  led_api_init(2);
  http_api_start();
}
