#include "esp_http_server.h"
#include "esp_log.h"
#include "http_web.h"
#include "led_api.h"
#include <stdlib.h>

static const char *TAG = "http";

static esp_err_t blink_handler(httpd_req_t *req) {
  led_cmd_t cmd = {
      .type = LED_CMD_BLINK, .count = 3, .on_ms = 150, .off_ms = 150};

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char v[16];
    if (httpd_query_key_value(query, "count", v, sizeof(v)) == ESP_OK)
      cmd.count = (uint16_t)atoi(v);
    if (httpd_query_key_value(query, "on", v, sizeof(v)) == ESP_OK)
      cmd.on_ms = (uint16_t)atoi(v);
    if (httpd_query_key_value(query, "off", v, sizeof(v)) == ESP_OK)
      cmd.off_ms = (uint16_t)atoi(v);
  }

  bool ok = led_api_send(&cmd);
  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, ok ? "esp32: queued\n" : "queue full\n");
  return ESP_OK;
}

static esp_err_t on_handler(httpd_req_t *req) {
  led_cmd_t cmd = {.type = LED_CMD_ON};
  bool ok = led_api_send(&cmd);
  httpd_resp_sendstr(req, ok ? "esp32: queued\n" : "queue full\n");
  return ESP_OK;
}

static esp_err_t off_handler(httpd_req_t *req) {
  led_cmd_t cmd = {.type = LED_CMD_OFF};
  bool ok = led_api_send(&cmd);
  httpd_resp_sendstr(req, ok ? "esp32: queued\n" : "queue full\n");
  return ESP_OK;
}

static esp_err_t brightness_handler(httpd_req_t *req) {
  led_cmd_t cmd = {.type = LED_CMD_SET_BRIGHTNESS, .brightness = 200};
  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char v[16];
    if (httpd_query_key_value(query, "brightness", v, sizeof(v)) == ESP_OK)
      cmd.brightness = (uint16_t)atoi(v);
  }
  bool ok = led_api_send(&cmd);
  httpd_resp_sendstr(req, ok ? "esp32: queued\n" : "queue full\n");
  return ESP_OK;
}

// register endpoints
httpd_handle_t http_api_start(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  httpd_handle_t server = NULL;
  ESP_ERROR_CHECK(httpd_start(&server, &config));

  httpd_uri_t index_uri = {
      .uri = "/", .method = HTTP_GET, .handler = index_handler};
  httpd_uri_t brightness_uri = {
      .uri = "/brightness", .method = HTTP_GET, .handler = brightness_handler};
  httpd_uri_t blink_uri = {
      .uri = "/blink", .method = HTTP_GET, .handler = blink_handler};
  httpd_uri_t on_uri = {
      .uri = "/on", .method = HTTP_GET, .handler = on_handler};
  httpd_uri_t off_uri = {
      .uri = "/off", .method = HTTP_GET, .handler = off_handler};

  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &brightness_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &blink_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &on_uri));
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &off_uri));

  ESP_LOGI(TAG, "HTTP API started");
  return server;
}
