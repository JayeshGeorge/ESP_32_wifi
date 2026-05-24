/*
 * mqtt_init.c
 *
 *  Created on: 21-May-2026
 *      Author: jayes
 */

#include "mqtt_init.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"

const char *TAG = "MQTT";
  
esp_mqtt_client_handle_t MQTTclient;
cJSON *pub_msg;
char json_payload[256];

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
								int32_t event_id, void *event_data) {

ESP_LOGI(TAG,"Event dispatched from event loop base=%s, event_id=%" PRIi32,
			  base, event_id);
	 esp_mqtt_event_handle_t event = event_data;
	 esp_mqtt_client_handle_t client = event->client;
	 int msg_id;

	 switch ((esp_mqtt_event_id_t)event_id) {
	 case MQTT_EVENT_CONNECTED:
		 ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
//		 msg_id = esp_mqtt_client_subscribe(client, MQTT_CLIENT_TOPIC, 0);
//		 ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
//		xTaskCreate(mqtt_publish_msg, "mqtt_publish", 4096, NULL, 5, NULL);

		int msg_id = esp_mqtt_client_publish(MQTTclient, MQTT_CLIENT_TOPIC,json_payload,0,0,0);
		if(msg_id == 0){
			ESP_LOGI(TAG,"Sent Data : %s",json_payload);
		}else{
			ESP_LOGI(TAG,"Error: msg_id: %d, when sending data",msg_id);
		}
		cJSON_free(pub_msg);
		cJSON_Delete(pub_msg);
		 break;

	 case MQTT_EVENT_DISCONNECTED:
		 ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		 break;

	 case MQTT_EVENT_SUBSCRIBED:
		 ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ",
				  event->msg_id, (uint8_t)*event->data);
		 break;

	 case MQTT_EVENT_UNSUBSCRIBED:
		 ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		 break;

	 case MQTT_EVENT_PUBLISHED:
		 ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		 break;

	 case MQTT_EVENT_DATA:
		 ESP_LOGI(TAG, "MQTT_EVENT_DATA");
		 printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
		 printf("DATA=%.*s\r\n", event->data_len, event->data);
		 break;

	 case MQTT_EVENT_ERROR:
		 ESP_LOGI(TAG, "MQTT_EVENT_ERROR");

		 if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
			 ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x",
					  event->error_handle->esp_tls_last_esp_err);
			 ESP_LOGI(TAG, "Last tls stack error number: 0x%x",
					  event->error_handle->esp_tls_stack_err);
			 ESP_LOGI(TAG, "Last captured errno : %d (%s)",
					  event->error_handle->esp_transport_sock_errno,
					  strerror(event->error_handle->esp_transport_sock_errno));
		 } else if (event->error_handle->error_type ==
					MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
			 ESP_LOGI(TAG, "Connection refused error: 0x%x",
					  event->error_handle->connect_return_code);
		 } else {
			 ESP_LOGW(TAG, "Unknown error type: 0x%x",
					  event->error_handle->error_type);
		 }

		 break;

	 default:
		 ESP_LOGI(TAG, "Other event id:%d", event->event_id);
		 break;
	 }
}

void mqtt_publish_msg(void *arg){
	char dataSend[5];
	while(1){
		int val = random() % 100;
		sprintf(dataSend,"%d",val);	
		int msg_id = esp_mqtt_client_publish(MQTTclient, MQTT_CLIENT_TOPIC,dataSend,sizeof(dataSend),0,0);
		if(msg_id == 0){
			ESP_LOGI(TAG,"Sent Data : %d",val);
		}else{
			ESP_LOGI(TAG,"Error: msg_id: %d, when sending data");
		}
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
}
 
 
void mqtt_app_start(void)
{
 const esp_mqtt_client_config_t mqtt_cfg = {
     .broker = {
         .address.uri = CONFIG_BROKER_URI,
		 .address.port = 1883,
     },
 };
 ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
 esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
 MQTTclient=client;
 /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
 esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
 esp_mqtt_client_start(client);
 
 cJSON *pub_msg= cJSON_CreateObject();
 cJSON_AddStringToObject(pub_msg, "Name", "Jayesh");
 cJSON_AddNumberToObject(pub_msg, "Age", 23);
 cJSON_AddStringToObject(pub_msg, "Email", "jayeshgeorge55@gmail.com");
 
 char *temp = cJSON_PrintUnformatted(pub_msg);
 strncpy(json_payload,temp,sizeof(json_payload)-1);
 cJSON_free(temp);
}