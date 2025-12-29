#include "http_web.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "led_api.h"
#include "led_effects.h"
#include "wifi_connect.h"
#include <ctype.h>
#include <stdlib.h>

static const char *TAG = "http";

static const char *INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WS2812 LED-control</title>
  <style>
    body { font-family: sans-serif; max-width: 500px; margin: 2rem auto; padding: 1rem; }
    button { width: 100%; padding: 1rem; margin: 0.5rem 0; font-size: 1.2rem; cursor: pointer; }
    input { width: 100%; padding: 0.5rem; margin: 0.5rem 0; box-sizing: border-box; }
    .red { background: #f44336; color: white; }
    .green { background: #4CAF50; color: white; }
    .blue { background: #2196F3; color: white; }
    .rainbow { background: linear-gradient(90deg, red, orange, yellow, green, blue, indigo, violet); color: white; }
    .off { background: #333; color: white; }
    .color-inputs { display: flex; gap: 0.5rem; }
    .color-inputs input { flex: 1; }
  </style>
</head>
<body>
  <h1>🌈 WS2812 LED-control</h1>
  
  <h3>Colors</h3>
  <button class="red" onclick="setColor(255,0,0)">Red</button>
  <button class="green" onclick="setColor(0,255,0)">Green</button>
  <button class="blue" onclick="setColor(0,0,255)">Blue</button>
  <button onclick="setColor(255,255,255)">White</button>
  <button class="off" onclick="fetch('/off')">Off</button>
  
  <h3>Custom colors</h3>
  <div class="color-inputs">
    <input type="number" id="r" placeholder="R" min="0" max="255" value="255">
    <input type="number" id="g" placeholder="G" min="0" max="255" value="0">
    <input type="number" id="b" placeholder="B" min="0" max="255" value="0">
  </div>
  <button onclick="setCustomColor()">Set color</button>
  
  <h3>Effects</h3>
  <button class="rainbow" onclick="rainbow()">Rainbow Chase (5s)</button>
  <button class="rainbow" onclick="cycle()">Rainbow Cycle (3s)</button>
  <button class="blue" onclick="bounce()">Bounce</button>
  <button onclick="wipe()">Color Wipe</button>
  
  <h3>Effect-settings</h3>
  <label>Speed (1-10): <input type="number" id="speed" value="5" min="1" max="10"></label>
  <label>Duration (ms): <input type="number" id="duration" value="5000" min="1000" max="30000"></label>
  
  <script>
    function setColor(r, g, b) {
      fetch(`/color?r=${r}&g=${g}&b=${b}`);
    }
    
    function setCustomColor() {
      const r = document.getElementById('r').value;
      const g = document.getElementById('g').value;
      const b = document.getElementById('b').value;
      setColor(r, g, b);
    }
    
    function rainbow() {
      const speed = document.getElementById('speed').value;
      const duration = document.getElementById('duration').value;
      fetch(`/rainbow?speed=${speed}&duration=${duration}`);
    }
    
    function cycle() {
      const speed = document.getElementById('speed').value;
      const duration = document.getElementById('duration').value;
      fetch(`/cycle?speed=${speed}&duration=${duration}`);
    }
    
    function bounce() {
      const speed = document.getElementById('speed').value;
      fetch(`/bounce?r=0&g=0&b=255&speed=${speed}`);
    }
    
    function wipe() {
      const r = document.getElementById('r').value;
      const g = document.getElementById('g').value;
      const b = document.getElementById('b').value;
      fetch(`/wipe?r=${r}&g=${g}&b=${b}&delay=50`);
    }
  </script>
</body>
</html>
)rawliteral";

static const char *SETUP_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 WiFi Setup</title>
  <style>
    body { font-family: sans-serif; max-width: 400px; margin: 2rem auto; padding: 1rem; }
    input { width: 100%; padding: 0.8rem; margin: 0.5rem 0; box-sizing: border-box; font-size: 1rem; }
    button { width: 100%; padding: 1rem; font-size: 1.2rem; background: #4CAF50; color: white; border: none; cursor: pointer; }
    button:hover { background: #45a049; }
  </style>
</head>
<body>
  <h1>🛜 WiFi Setup</h1>
  <p>Wifi-SSID:</p>
  
  <form action="/setup" method="POST">
    <input type="text" name="ssid" placeholder="WiFi SSID" required autocomplete="off">
    <input type="password" name="password" placeholder="Password" autocomplete="new-password">
    <button type="submit">Connect</button>
  </form>
  <script>
    // empty fields on load
    window.onload = function() {
      document.querySelector('input[name="ssid"]').value = '';
      document.querySelector('input[name="password"]').value = '';
    };
</body>
</html>
)rawliteral";

// GET / - show setup or control mode
static esp_err_t index_handler(httpd_req_t *req) {
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);

  httpd_resp_set_type(req, "text/html");

  if (mode == WIFI_MODE_AP) {
    // AccesPoint-mode, show setup form
    httpd_resp_sendstr(req, SETUP_HTML);
  } else {
    // STA-mode, normal control mode
    httpd_resp_sendstr(req, INDEX_HTML);
  }
  return ESP_OK;
}

// Parse RGB values from query string
static void parse_rgb(const char *query, uint8_t *r, uint8_t *g, uint8_t *b) {
  char v[16];
  if (httpd_query_key_value(query, "r", v, sizeof(v)) == ESP_OK)
    *r = (uint8_t)atoi(v);
  if (httpd_query_key_value(query, "g", v, sizeof(v)) == ESP_OK)
    *g = (uint8_t)atoi(v);
  if (httpd_query_key_value(query, "b", v, sizeof(v)) == ESP_OK)
    *b = (uint8_t)atoi(v);
}

// GET /color?r=255&g=0&b=0
static esp_err_t color_handler(httpd_req_t *req) {
  builtin_led_blink(3, 100);

  uint8_t r = 255, g = 255, b = 255;

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    parse_rgb(query, &r, &g, &b);
  }

  ws2812_set_all(r, g, b);
  ws2812_show();

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "OK\n");
  return ESP_OK;
}

// GET /rainbow?speed=5&duration=5000
static esp_err_t rainbow_handler(httpd_req_t *req) {
  builtin_led_blink(3, 100);

  uint8_t speed = 5;
  uint32_t duration = 5000;

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char v[16];
    if (httpd_query_key_value(query, "speed", v, sizeof(v)) == ESP_OK)
      speed = (uint8_t)atoi(v);
    if (httpd_query_key_value(query, "duration", v, sizeof(v)) == ESP_OK)
      duration = (uint32_t)atoi(v);
  }

  led_rainbow_chase(speed, duration);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "OK\n");
  return ESP_OK;
}

// GET /cycle?speed=3&duration=3000
static esp_err_t cycle_handler(httpd_req_t *req) {
  builtin_led_blink(3, 100);

  uint8_t speed = 3;
  uint32_t duration = 3000;

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char v[16];
    if (httpd_query_key_value(query, "speed", v, sizeof(v)) == ESP_OK)
      speed = (uint8_t)atoi(v);
    if (httpd_query_key_value(query, "duration", v, sizeof(v)) == ESP_OK)
      duration = (uint32_t)atoi(v);
  }

  led_rainbow_cycle(speed, duration);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "OK\n");
  return ESP_OK;
}

// GET /bounce?r=0&g=0&b=255&speed=3
static esp_err_t bounce_handler(httpd_req_t *req) {
  builtin_led_blink(3, 100);

  uint8_t r = 0, g = 0, b = 255;
  uint8_t speed = 3;

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char v[16];
    parse_rgb(query, &r, &g, &b);
    if (httpd_query_key_value(query, "speed", v, sizeof(v)) == ESP_OK)
      speed = (uint8_t)atoi(v);
  }

  led_bouncing_ball(r, g, b, speed);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "OK\n");
  return ESP_OK;
}

// GET /wipe?r=255&g=0&b=0&delay=50
static esp_err_t wipe_handler(httpd_req_t *req) {
  builtin_led_blink(3, 100);

  uint8_t r = 255, g = 0, b = 0;
  uint16_t delay = 50;

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char v[16];
    parse_rgb(query, &r, &g, &b);
    if (httpd_query_key_value(query, "delay", v, sizeof(v)) == ESP_OK)
      delay = (uint16_t)atoi(v);
  }

  led_color_wipe(r, g, b, delay);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "OK\n");
  return ESP_OK;
}

// GET /off
static esp_err_t off_handler(httpd_req_t *req) {
  builtin_led_blink(3, 100);

  ws2812_clear();

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_sendstr(req, "OK\n");
  return ESP_OK;
}

// decoder for wifi symbols
static void url_decode(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
        (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}
// GET /setup?ssid=MyWiFi&password=MyPass
static esp_err_t setup_handler(httpd_req_t *req) {
  char content[256];
  char ssid_encoded[32] = {0};
  char password_encoded[64] = {0};
  char ssid[32] = {0};
  char password[64] = {0};

  int ret = httpd_req_recv(req, content, sizeof(content));
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }
  content[ret] = '\0';

  if (httpd_query_key_value(content, "ssid", ssid_encoded,
                            sizeof(ssid_encoded)) != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "Missing SSID");
    return ESP_OK;
  }

  httpd_query_key_value(content, "password", password_encoded,
                        sizeof(password_encoded));

  // URL-dekoda SSID och password
  url_decode(ssid, ssid_encoded);
  url_decode(password, password_encoded);

  ESP_LOGI(TAG, "Setup WiFi - SSID: '%s'", ssid); // Debug-logg

  // Spara och starta om
  wifi_reconfigure(ssid, password);

  const char *response =
      "<html><body><h1>Configuration done!</h1><p>ESP32 restarts and connects "
      "to your WiFi...</p></body></html>";
  httpd_resp_set_type(req, "text/html");
  httpd_resp_sendstr(req, response);

  return ESP_OK;
}

// GET /reset - Delete wifi-config and restart in AccesPoint-mode
static esp_err_t reset_handler(httpd_req_t *req) {
  wifi_clear_credentials();

  const char *response = "<html><body><h1>Reset!</h1><p>WiFi-settings deleted. "
                         "ESP32 starting in setup-mode...</p></body></html>";
  httpd_resp_set_type(req, "text/html");
  httpd_resp_sendstr(req, response);

  vTaskDelay(pdMS_TO_TICKS(2000));
  esp_restart();

  return ESP_OK;
}

// Register endpoints
httpd_handle_t http_api_start(void) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;
  // increase handlers from default (8)
  config.max_uri_handlers = 12;
  httpd_handle_t server = NULL;

  ESP_ERROR_CHECK(httpd_start(&server, &config));

  httpd_uri_t uris[] = {
      {.uri = "/", .method = HTTP_GET, .handler = index_handler},
      {.uri = "/color", .method = HTTP_GET, .handler = color_handler},
      {.uri = "/rainbow", .method = HTTP_GET, .handler = rainbow_handler},
      {.uri = "/cycle", .method = HTTP_GET, .handler = cycle_handler},
      {.uri = "/bounce", .method = HTTP_GET, .handler = bounce_handler},
      {.uri = "/wipe", .method = HTTP_GET, .handler = wipe_handler},
      {.uri = "/off", .method = HTTP_GET, .handler = off_handler},
      {.uri = "/setup", .method = HTTP_POST, .handler = setup_handler},
      {.uri = "/reset", .method = HTTP_GET, .handler = reset_handler},
  };

  for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uris[i]));
  }

  ESP_LOGI(TAG, "HTTP API started with %d endpoints",
           sizeof(uris) / sizeof(uris[0]));
  return server;
}
