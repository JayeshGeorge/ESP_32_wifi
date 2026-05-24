set(srcs)


if(CONFIG_WIFI_STA_CONNECT)
	list(APPEND srcs
		"http_init.c")
endif()

idf_component_register(SRCS "http_init.c"
                       INCLUDE_DIRS "."
                       REQUIRES lwip)