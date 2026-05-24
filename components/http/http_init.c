/*
 * http_init.c
 *
 *  Created on: 19-May-2026
 *      Author: jayes
 */
#include "http_init.h"
#include "esp_log.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdio.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG= "HTTP_init";

void http_get_start(void *arg){
	/*	for http	*/
	int ret;
	struct addrinfo *dns_res;
	int sock;
	char recv_buf[RX_BUF_SIZE];
	uint32_t recv_total;
	ssize_t recv_len;
	
	//hints for DNS lookup
	struct addrinfo hints={
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	
	struct timeval sock_timeout ={
		.tv_sec = SOCKET_TIMEOUT_SEC,
		.tv_usec = 0,
	};
	ESP_LOGI(TAG,"Starting HTTP GET request demo");
	
	while(1){
		//perform DNS lookup
		ret= getaddrinfo(WEB_HOST, (const char*)WEB_PORT, &hints, &dns_res);
		if(ret != 0 || dns_res == NULL){
			ESP_LOGI(TAG,"Error (%d) DNS lookup failed",ret);
        	vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;	
		}
		//print resolved IP address 
		ESP_LOGI(TAG,"DNS Lookup suceess");
		for (struct addrinfo *addr= dns_res ; addr != NULL; addr=addr->ai_next){
			if(addr->ai_family == AF_INET){
				struct in_addr *ip = &((struct sockaddr_in *)addr->ai_addr)->sin_addr;
				inet_ntop(AF_INET, ip, recv_buf, INET_ADDRSTRLEN);
				ESP_LOGI(TAG, "  IPv4: %s", recv_buf);
			}
		}
		// Create a socket
       sock = socket(dns_res->ai_family, dns_res->ai_socktype, dns_res->ai_protocol);
       if (sock < 0) {
           ESP_LOGE(TAG, "Failed to create socket (%d): %s", errno, strerror(errno));
           vTaskDelay(1000 / portTICK_PERIOD_MS);
           continue;
       }
	   // Set socket send timeout
	   ret = setsockopt(sock, SOL_SOCKET,SO_SNDTIMEO,&sock_timeout, sizeof(sock_timeout));
       if(ret < 0){
			ESP_LOGE(TAG,"Failed to set socket timeout");
			close(sock);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			continue;
	   }
	   //set received timeout 
	   ret = setsockopt(sock, SOL_SOCKET,SO_RCVTIMEO,&sock_timeout, sizeof(sock_timeout));
	         if(ret < 0){
	   		ESP_LOGE(TAG,"Failed to set socket timeout");
	   		close(sock);
	   		vTaskDelay(1000 / portTICK_PERIOD_MS);
	   		continue;
		}
	  // Connect to server
      ret = connect(sock, dns_res->ai_addr, dns_res->ai_addrlen);
      if (ret < 0) {
          ESP_LOGE(TAG, "Failed to connect to server (%d): %s", errno, strerror(errno));
          close(sock);
          vTaskDelay(1000 / portTICK_PERIOD_MS);
          continue;
      }
	  
	  freeaddrinfo(dns_res);
	  
	  // Send HTTP GET request
      ESP_LOGI(TAG, "Sending HTTP GET request...");
//      ret = send(sock, REQUEST, strlen(REQUEST), 0);
      if (ret < 0) {
          ESP_LOGE(TAG, "Failed to send HTTP GET request (%d): %s", errno, strerror(errno));
          close(sock);
          vTaskDelay(1000 / portTICK_PERIOD_MS);
          continue;
      }
	  //print the HTTP request
	  ESP_LOGI(TAG,"Printing the HTTP request");
	  recv_total=0;
	  while(1){
		//recv data from the socket
		recv_len = recv(sock, recv_buf,sizeof(recv_buf)- 1, 0);
		
		if(recv_len < 0){
			ESP_LOGE(TAG,"Error (%d) Failed to receive data %s",errno,strerror(errno));
			break; 
		}
		
		if(recv_len == 0) break;
		
		// Null-terminate the received data and print it
        recv_buf[recv_len] = '\0';
        printf("%s\n", recv_buf);
        recv_total += (uint32_t)recv_len;			
	  }
	  
	  close(sock);
	  
	  printf("Total bytes received: %"PRIu32"\n",recv_total);
	vTaskDelay(sleep_time_ms/portTICK_PERIOD_MS);
	}
 }

 http_get_err_t http_get_request(const char *host,uint16_t port, const char *path,
                                  char *resp_buf,size_t buf_len)
{
	static const char *TAG = "http_get";
	
	struct addrinfo hints = {
	        .ai_family   = AF_INET,       // IPv4
	        .ai_socktype = SOCK_STREAM,   // TCP
    };
	
	struct addrinfo *res = NULL;
	
	char port_str[6];
	snprintf(port_str,sizeof(port),"%"PRIu16"",port);
	// getaddrinfo() is the standard POSIX DNS lookup function.
	int err= getaddrinfo(host, port_str, &hints, &res);
	if(err != 0 || res == NULL){
		ESP_LOGE(TAG,"DNS Lookup function failed, err=%d",err);
		return HTTP_GET_DNS_FAIL;
	}
	// ── Step 2: Create a TCP socket ───────────────────────────
    // A socket is just a file descriptor for network I/O.
    // AF_INET = IPv4, SOCK_STREAM = TCP (reliable, ordered)
	int sock = socket(res->ai_family,res->ai_socktype,0);
	if(sock < 0){
		ESP_LOGE(TAG, "Failed to create socket");
        freeaddrinfo(res);
        return HTTP_GET_SOCK_FAIL;
	}
	
	// ── Step 3: Connect to the server ────────────────────────
    // TCP three-way handshake happens here (SYN → SYN-ACK → ACK).
    ESP_LOGI(TAG, "Connecting to %s:%u ...", host, port);
	
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "Socket connect failed");
        close(sock);
        freeaddrinfo(res);
        return HTTP_GET_SOCK_FAIL;
    }
    freeaddrinfo(res);  // no longer needed after connect
    ESP_LOGI(TAG, "Connected!");
	
	// ── Step 4: Build and send the HTTP GET request ──────────
    char request[512];
    int req_len = snprintf(request, sizeof(request),
                           HTTP_REQUEST_FMT, path, host);
	 
    // Print what we're sending so you can see the raw format
    ESP_LOGI(TAG, "Sending HTTP request:\n%s", request);
 
    int sent = send(sock, request, req_len, 0);
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send request");
        close(sock);
        return HTTP_GET_SEND_FAIL;
    }
    ESP_LOGI(TAG, "Sent %d bytes", sent);
		
	// ── Step 5: Receive the HTTP response ────────────────────
    // The server sends back: status line + headers + blank line + body.
    // We read in chunks (4096 bytes at a time) until the server closes
    // the connection (recv returns 0) or our buffer is full.
    memset(resp_buf, 0, buf_len);
    size_t total_received = 0;
    int    chunk;
 
    ESP_LOGI(TAG, "Waiting for response...");
 
    while (total_received < buf_len - 1) {
    	chunk = recv(sock,
                     resp_buf + total_received,
                     buf_len - 1 - total_received,
                     0);
 
        if (chunk < 0) {
            ESP_LOGE(TAG, "recv() error");
            close(sock);
            return HTTP_GET_RECV_FAIL;
        }
        if (chunk == 0) {
            // Server closed the connection — response is complete
            break;
        }
	        total_received += chunk;
	    }
	 
	    resp_buf[total_received] = '\0'; // null-terminate for safe string use
	    ESP_LOGI(TAG, "Received %zu bytes total", total_received);
	 
    // ── Step 6: Close the socket ─────────────────────────────
    close(sock);
		
	return HTTP_GET_OK;
}
 					
