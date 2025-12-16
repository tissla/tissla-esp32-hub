#include "esp_http_server.h"

static const char *INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LED-styrning</title>
  <style>
    body { font-family: sans-serif; max-width: 400px; margin: 2rem auto; padding: 1rem; }
    button { width: 100%; padding: 1rem; margin: 0.5rem 0; font-size: 1.2rem; }
    input { width: 100%; padding: 0.5rem; margin: 0.5rem 0; }
    .on { background: #4CAF50; color: white; }
    .off { background: #f44336; color: white; }
  </style>
</head>
<body>
  <h1>💡 LED-styrning</h1>
  
  <button class="on" onclick="fetch('/on')">Tänd</button>
  <button class="off" onclick="fetch('/off')">Släck</button>
  <button onclick="fetch('/blink')">Blinka 3x</button>
  
  <h3>Ljusstyrka</h3>
  <input type="range" min="0" max="255" value="200" 
         oninput="fetch('/brightness?brightness='+this.value)">
  
  <h3>Blink-inställningar</h3>
  <label>Antal: <input type="number" id="count" value="3" min="1" max="20"></label>
  <label>På (ms): <input type="number" id="on" value="150"></label>
  <label>Av (ms): <input type="number" id="off" value="150"></label>
  <button onclick="customBlink()">Blinka</button>
  
  <script>
    function customBlink() {
      const c = document.getElementById('count').value;
      const on = document.getElementById('on').value;
      const off = document.getElementById('off').value;
      fetch(`/blink?count=${c}&on=${on}&off=${off}`);
    }
  </script>
</body>
</html>
)rawliteral";

esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_sendstr(req, INDEX_HTML);
  return ESP_OK;
}
