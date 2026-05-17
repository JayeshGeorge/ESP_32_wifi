#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"          // FIX: Added missing include for ESP_LOGI/ESP_LOGE
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"       
#include "portmacro.h"
#include "wifi_connection.h"

#define HEARBEAT GPIO_NUM_2
#define TAG "main"            // FIX: Added missing TAG definition

void app_main(void)
{
	gpio_set_direction(HEARBEAT,GPIO_MODE_OUTPUT);
	
    esp_err_t esp_ret;
    EventGroupHandle_t network_event_grp;
    EventBits_t network_eventBits;

    // FIX: EventGroup must be created before use
    network_event_grp = xEventGroupCreate();
    esp_ret = nvs_flash_init();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%d): Failed to initialize NVS", esp_ret);
        abort();
    }

    // Initialize TCP/IP network interface
    esp_ret = esp_netif_init();
    if (esp_ret != ESP_OK) {  // FIX: Was missing the "if" keyword
        ESP_LOGE(TAG, "Error (%d): Failed to initialize network interface", esp_ret);
        abort();
    }

    esp_ret = esp_event_loop_create_default();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop");
        abort();
    }

    // Initialize network connection
    esp_ret = wifi_init(network_event_grp);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize wifi");
        abort();
    }

    // Wait for network to connect
    ESP_LOGI(TAG, "Waiting for network to connect..");
    network_eventBits = xEventGroupWaitBits(network_event_grp, WIFI_STA_CONNECTED,
                                            pdFALSE,           // FIX: pdFalse → pdFALSE
                                            pdTRUE,            // FIX: pdTrue  → pdTRUE
                                            pdMS_TO_TICKS(10000));

    if (network_eventBits & WIFI_STA_CONNECTED) {
        ESP_LOGI(TAG, "Wifi Connected");  // FIX: Added missing semicolon
    } else {                              // FIX: Removed stray underscores around }
        ESP_LOGI(TAG, "Failed to connect to wifi");
        abort();
    }

    network_eventBits = xEventGroupWaitBits(network_event_grp,
                                            WIFI_STA_IPV4_OBTAINED | WIFI_STA_IPV6_OBTAINED,
                                            pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));

    if (network_eventBits & WIFI_STA_IPV4_OBTAINED) {
        ESP_LOGI(TAG, "Obtained IPv4 address");
    } else if (network_eventBits & WIFI_STA_IPV6_OBTAINED) {
        ESP_LOGI(TAG, "Obtained IPv6 address");
    } else {
        ESP_LOGE(TAG, "Failed to obtain address");
        abort();
    }

    while (true) {
        // FIX: Refresh eventBits each iteration — stale bits from before the loop
        //      would make the condition never update during runtime
        network_eventBits = xEventGroupGetBits(network_event_grp);

        if (network_eventBits & WIFI_STA_CONNECTED) {
            ESP_LOGI(TAG, "Still connected to Wifi");
            vTaskDelay(10);
            gpio_set_level(HEARBEAT, 1);
            vTaskDelay(10);
            gpio_set_level(HEARBEAT, 0);
            vTaskDelay(10);
        } else {
            ESP_LOGI(TAG, "Wifi connection lost");
            gpio_set_level(HEARBEAT, 0);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}