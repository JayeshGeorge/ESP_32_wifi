#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"          // FIX: Added missing include for ESP_LOGI/ESP_LOGE
#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "http_init.h"
#include "nvs_flash.h"       
#include "portmacro.h"
#include "wifi_connection.h"
#include "time.h"
#include "mqtt_init.h"



#define HEARBEAT GPIO_NUM_2
#define TAG "main"            // FIX: Added missing TAG definition



void get_time(void);
time_t now = 0;
struct tm timeinfo = { 0 };

const uint32_t sleep_time_ms = 5000;	//5seconds



void app_main(void)
{
	gpio_set_direction(HEARBEAT,GPIO_MODE_OUTPUT);
	
	/*	for wifi */
    esp_err_t  esp_ret;
    EventGroupHandle_t network_event_grp;
    EventBits_t network_eventBits;
	
	esp_ret = nvs_flash_init();
	if (esp_ret != ESP_OK) {
	        ESP_LOGE(TAG, "Error (%d): Failed to initialize NVS", esp_ret);
//	        abort();
	    }
	
    // FIX: EventGroup must be created before use
    network_event_grp = xEventGroupCreate();
    

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
	network_eventBits = xEventGroupWaitBits(
	    network_event_grp,
	    WIFI_STA_CONNECTED,
	    pdFALSE,          // don't clear bits
	    pdFALSE,          // wait for any bit (not all)
	    pdMS_TO_TICKS(10000)
	);
	network_eventBits = xEventGroupWaitBits(
		    network_event_grp,
		    WIFI_STA_IPV4_OBTAINED,
		    pdFALSE,          // don't clear bits
		    pdFALSE,          // wait for any bit (not all)
		    pdMS_TO_TICKS(10000)
		);
	if(network_eventBits & WIFI_STA_IPV4_OBTAINED){
		ESP_LOGI(TAG,"Starting MQTT");
		mqtt_app_start();
	}
    while (true) {
		

        if (network_eventBits & WIFI_STA_CONNECTED) {
            vTaskDelay(5);
            gpio_set_level(HEARBEAT, 1);
            vTaskDelay(5);
            gpio_set_level(HEARBEAT, 0);
            vTaskDelay(2);
        } else {
            ESP_LOGI(TAG, "Wifi connection lost");
            gpio_set_level(HEARBEAT, 0);
        }
	  
	  vTaskDelay(sleep_time_ms/portTICK_PERIOD_MS);
    }
}



void get_time(){
	esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
	esp_sntp_setservername(0, "pool.ntp.org");
	esp_sntp_init();
	
	
	
	while (timeinfo.tm_year < (2025 - 1900)) {
//	    printf("Waiting for system time to be set...\n");
	    vTaskDelay(2000 / portTICK_PERIOD_MS);
//	    time(&now);
//	    localtime_r(&now, &timeinfo);
	}
}