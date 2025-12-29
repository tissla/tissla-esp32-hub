#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

#define MAX_RETRY 5
#define NVS_NAMESPACE "wifi_config"
#define AP_SSID "ESP32-Setup"
#define AP_PASSWORD "12345678" // 8 chars

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi";
static int s_retry_num = 0;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

// Spara WiFi credentials
esp_err_t wifi_save_credentials(const char *ssid, const char *password) {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK)
    return err;

  nvs_set_str(nvs_handle, "ssid", ssid);
  nvs_set_str(nvs_handle, "password", password);
  nvs_commit(nvs_handle);
  nvs_close(nvs_handle);

  ESP_LOGI(TAG, "WiFi credentials saved");
  return ESP_OK;
}

// Läs WiFi credentials
esp_err_t wifi_load_credentials(char *ssid, size_t ssid_len, char *password,
                                size_t pass_len) {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (err != ESP_OK)
    return err;

  size_t actual_ssid_len = ssid_len;
  err = nvs_get_str(nvs_handle, "ssid", ssid, &actual_ssid_len);
  if (err != ESP_OK) {
    nvs_close(nvs_handle);
    return err;
  }

  size_t actual_pass_len = pass_len;
  err = nvs_get_str(nvs_handle, "password", password, &actual_pass_len);
  nvs_close(nvs_handle);
  return err;
}

// Radera sparade credentials (för reset)
esp_err_t wifi_clear_credentials(void) {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK)
    return err;

  nvs_erase_key(nvs_handle, "ssid");
  nvs_erase_key(nvs_handle, "password");
  nvs_commit(nvs_handle);
  nvs_close(nvs_handle);

  ESP_LOGI(TAG, "WiFi credentials cleared");
  return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
    ESP_LOGI(TAG, "Connecting to WiFi...");
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < MAX_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGW(TAG, "Retry %d/%d", s_retry_num, MAX_RETRY);
    } else {
      ESP_LOGE(TAG, "Failed to connect, clearing credentials and restarting");
      wifi_clear_credentials();
      vTaskDelay(pdMS_TO_TICKS(2000));
      esp_restart();
    }
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event =
        (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "Station connected to AP, MAC: " MACSTR, MAC2STR(event->mac));
  }
}

// Starta i AP-läge (Setup-mode)
static void wifi_start_ap_mode(void) {
  ESP_LOGI(TAG, "Starting AP mode: %s", AP_SSID);

  s_ap_netif = esp_netif_create_default_wifi_ap();

  wifi_config_t ap_config = {
      .ap =
          {
              .ssid = AP_SSID,
              .ssid_len = strlen(AP_SSID),
              .password = AP_PASSWORD,
              .max_connection = 4,
              .authmode = WIFI_AUTH_WPA2_PSK,
          },
  };

  // Om inget lösenord, använd öppet nätverk
  if (strlen(AP_PASSWORD) == 0) {
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
  }

  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();

  ESP_LOGI(TAG, "AP started. Connect to '%s' and go to http://192.168.4.1",
           AP_SSID);
}

// Starta i Station-läge (Normal drift)
static void wifi_start_sta_mode(const char *ssid, const char *password) {
  ESP_LOGI(TAG, "Starting STA mode, connecting to: %s with password %s", ssid,
           password);

  s_sta_netif = esp_netif_create_default_wifi_sta();

  wifi_config_t sta_config = {0};
  strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
  strncpy((char *)sta_config.sta.password, password,
          sizeof(sta_config.sta.password) - 1);

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &sta_config);
  esp_wifi_start();
}

void wifi_connect(void) {
  s_wifi_event_group = xEventGroupCreate();

  // Initiera TCP/IP stack
  esp_netif_init();
  esp_event_loop_create_default();

  // WiFi init
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  // Registrera event handlers
  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &wifi_event_handler, NULL, NULL);
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      &wifi_event_handler, NULL, NULL);

  // Försök läsa sparade credentials
  char saved_ssid[32] = {0};
  char saved_pass[64] = {0};

  if (wifi_load_credentials(saved_ssid, sizeof(saved_ssid), saved_pass,
                            sizeof(saved_pass)) == ESP_OK) {
    // Har sparat WiFi, försök ansluta
    ESP_LOGI(TAG, "Found saved WiFi credentials");
    wifi_start_sta_mode(saved_ssid, saved_pass);
  } else {
    // Inget sparat WiFi, starta i AP-mode
    ESP_LOGW(TAG, "No saved WiFi, starting setup mode");
    wifi_start_ap_mode();
  }
}

bool wifi_wait_until_connected(uint32_t timeout_ms) {
  EventBits_t bits = xEventGroupWaitBits(
      s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
      pdMS_TO_TICKS(timeout_ms));

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "WiFi connected successfully");
    return true;
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGW(TAG, "WiFi connection failed, switching to AP mode");
    wifi_start_ap_mode();
    return false;
  } else {
    ESP_LOGW(TAG, "WiFi connection timeout");
    return false;
  }
}

// Byt WiFi och starta om
esp_err_t wifi_reconfigure(const char *ssid, const char *password) {
  ESP_LOGI(TAG, "Reconfiguring WiFi to: %s", ssid);

  // Spara nya credentials
  wifi_save_credentials(ssid, password);

  // Starta om ESP32
  ESP_LOGI(TAG, "Rebooting in 2 seconds...");
  vTaskDelay(pdMS_TO_TICKS(2000));
  esp_restart();

  return ESP_OK;
}

// Visa WiFi-status
void wifi_print_status(void) {
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);

  if (mode == WIFI_MODE_AP) {
    ESP_LOGI(TAG, "Mode: Access Point (Setup)");
    ESP_LOGI(TAG, "SSID: %s", AP_SSID);
    ESP_LOGI(TAG, "IP: 192.168.4.1");
  } else if (mode == WIFI_MODE_STA) {
    ESP_LOGI(TAG, "Mode: Station (Connected)");
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(s_sta_netif, &ip_info);
    ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ip_info.ip));
  }
}
