/*
 * http_init.h
 *
 *  Created on: 19-May-2026
 *      Author: jayes
 */

#ifndef COMPONENTS_HTTP_HTTP_INIT_H_
#define COMPONENTS_HTTP_HTTP_INIT_H_


#include <stdint.h>
#define WEB_HOST "httpbin.org"
#define WEB_PORT 80
#define WEB_PATH "/get"

//define timeout
#define CONNECTION_TIMEOUT_SEC 10		//set delay to wait for connection
#define SOCKET_TIMEOUT_SEC 5
#define RX_BUF_SIZE	 8192

extern const uint32_t sleep_time_ms;


/**
*	@brief HTTP Request format
*	  GET <path> HTTP/1.1\r\n        ← request line
*	  Host: <host>\r\n               ← required in HTTP/1.1
*	  Connection: close\r\n          ← tells server to close after reply
*	  \r\n                           ← blank line = end of headers
*
**/

#define HTTP_REQUEST_FMT \
    "GET %s HTTP/1.1\r\n"           \
    "Host: %s\r\n"                  \
    "User-Agent: ESP32/1.0\r\n"     \
    "Accept: */*\r\n"               \
    "Connection: close\r\n"         \
    "\r\n"                          // <── blank line is MANDATORY
	

//static const char * REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
//	"Host: " WEB_HOST ":" WEB_PORT "\r\n"
//	"User-Agent: esp-idf/1.0 esp32\r\n"
//	"Accept: */*\r\n"
//	"Connection: close\r\n"
//	"\r\n";
	
typedef enum {
    HTTP_GET_OK        =  0,   // success
    HTTP_GET_DNS_FAIL  = -1,   // could not resolve hostname
    HTTP_GET_SOCK_FAIL = -2,   // socket creation or connect failed
    HTTP_GET_SEND_FAIL = -3,   // failed to send the request
    HTTP_GET_RECV_FAIL = -4,   // failed to receive the response
} http_get_err_t;
 
http_get_err_t http_get_request(const char *host,
                                 uint16_t    port,
                                 const char *path,
                                 char       *resp_buf,
                                 size_t      buf_len);
								 
void http_get_start(void *arg);

#endif /* COMPONENTS_HTTP_HTTP_INIT_H_ */
