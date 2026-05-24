set(srcs)


if(CONFIG_WIFI_STA_CONNECT)
	list(APPEND srcs
		"wifi_connection.c")
endif()

idf_component_register(SRCS "wifi_connection.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_wifi esp_event nvs_flash)