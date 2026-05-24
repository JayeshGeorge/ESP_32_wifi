set(srcs)

list(APPEND srcs
		"mqtt_init.c")
		
idf_component_register(SRCS "mqtt_init.c"
                       INCLUDE_DIRS "."
                       REQUIRES mqtt esp_event json)