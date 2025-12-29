#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include <stdbool.h>

void wifi_connect(void);
bool wifi_wait_until_connected(uint32_t timeout_ms);
esp_err_t wifi_save_credentials(const char *ssid, const char *password);
esp_err_t wifi_clear_credentials(void);
esp_err_t wifi_reconfigure(const char *ssid, const char *password);
void wifi_print_status(void);

#endif
