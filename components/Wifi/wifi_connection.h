/*
 * wifi_connection.h
 *
 *  Created on: 16-May-2026
 *      Author: jayesh
 */

#ifndef WIFI_CONNECTION_H_
#define WIFI_CONNECTION_H_

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

// Event group bits for wifi connection status
#define WIFI_STA_CONNECTED       BIT0
#define WIFI_STA_IPV4_OBTAINED   BIT1
#define WIFI_STA_IPV6_OBTAINED   BIT2

// Function prototypes
esp_err_t wifi_init(EventGroupHandle_t event_grp);
esp_err_t wifi_sta_stop(void);
esp_err_t wifi_reconnect(void);

#endif /* WIFI_CONNECTION_H_ */