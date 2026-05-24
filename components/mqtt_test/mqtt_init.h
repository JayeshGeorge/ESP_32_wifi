/*
 * mqtt_init.h
 *
 *  Created on: 21-May-2026
 *      Author: jayes
 */

#ifndef COMPONENTS_MQTT_TEST_MQTT_INIT_H_
#define COMPONENTS_MQTT_TEST_MQTT_INIT_H_

#include <stdint.h>
#include "esp_event.h"


// ═════════════════════════════════════════════════════════════
// MQTT Security Configuration
// Edit ONLY this file to switch between security levels.
// ═════════════════════════════════════════════════════════════
 
// ── Level selector ────────────────────────────────────────────
// 0 = No security  (plain mqtt://, port 1883)
// 1 = Basic Auth   (plain mqtt://, port 1883 + username/password)	pass auth
// 2 = TLS          (mqtts://,      port 8883 + CA verification)	tls
// 3 = TLS + Auth   (mqtts://,      port 8883 + CA + credentials)	mtls
#define MQTT_SECURITY_LEVEL  3

#if (MQTT_SECURITY_LEVEL >=2)
	#define CONFIG_BROKER_URI "mqtts://broker.hivemq.com"
#else
	#define CONFIG_BROKER_URI "mqtt://broker.hivemq.com"
#endif

#define MQTT_CLIENT_TOPIC SECRET_CLIENT_TOPIC
	
#define MQTT_USERNAME SECRET_MQTT_USERNAME
#define	MQTT_PASSWORK SECRET_MQTT_PASSWORK


/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
 							int32_t event_id, void *event_data);

void mqtt_publish_msg(void *arg);
void mqtt_app_start(void);

#endif /* COMPONENTS_MQTT_TEST_MQTT_INIT_H_ */
