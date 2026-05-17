/*
 * wifi_connection.c
 *
 *  Created on: 16-May-2026
 *      Author: jayesh
 */
 
#include "wifi_connection.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_netif_types.h"
#include "esp_private/wifi.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_netif.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"

static const char *TAG = "ESP_WIFI";

// Global variables for wifi
static esp_netif_t *wifi_netif = NULL;
static EventGroupHandle_t wifi_event_grp = NULL;
static wifi_netif_driver_t wifi_driver = NULL;

// Callbacks
static void on_wifi_event(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
                        
static void wifi_start(void *esp_netif, esp_event_base_t event_base,
                       int32_t event_id, void *event_data);

static void on_wifi_event(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    switch(event_id) {
        case WIFI_EVENT_STA_START:
            if(wifi_netif != NULL) {
                wifi_start(wifi_netif, event_base, event_id, event_data);
            }
            break;
            
        case WIFI_EVENT_STA_STOP:
            if(wifi_netif != NULL) {
                esp_netif_action_stop(wifi_netif, event_base, 
                                      event_id, event_data);
            }
            break;
            
        case WIFI_EVENT_STA_CONNECTED:
            if(wifi_netif == NULL) {
                ESP_LOGE(TAG, "Wifi not started: interface is NULL");
                return;
            }
            wifi_event_sta_connected_t *event_sta_connected = 
                (wifi_event_sta_connected_t *)event_data;
                
            ESP_LOGI(TAG, "Connected to AP");
            ESP_LOGI(TAG, "SSID: %s", (char *)event_sta_connected->ssid);
            ESP_LOGI(TAG, "Channel: %d", event_sta_connected->channel);
            ESP_LOGI(TAG, "Auth mode: %d", event_sta_connected->authmode);
            ESP_LOGI(TAG, "AID: %d", event_sta_connected->aid);
            
            // Register interface callback
            wifi_netif_driver_t driver = esp_netif_get_io_driver(wifi_netif);
            if(!esp_wifi_is_if_ready_when_started(driver)) {
                esp_err_t esp_ret = esp_wifi_register_if_rxcb(driver, esp_netif_receive, 
                                                               wifi_netif);
                if (esp_ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register wifi rx callback");
                    return;
                }
            }
            
            // Setup wifi interface and start DHCP process
            esp_netif_action_connected(wifi_netif, event_base, event_id, event_data);
            
            // Set Wifi connected bit
            xEventGroupSetBits(wifi_event_grp, WIFI_STA_CONNECTED);
            
#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
            // Request ipv6 address
            esp_ret = esp_netif_create_ip6_linklocal(wifi_netif);
            if(esp_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create IPv6 link-local address");
            }
#endif
            break; 
            
        case WIFI_EVENT_STA_DISCONNECTED:
            if(wifi_netif != NULL) {
                esp_netif_action_disconnected(wifi_netif,
                                              event_base, event_id, event_data);
            }
            xEventGroupClearBits(wifi_event_grp, WIFI_STA_CONNECTED);
#if CONFIG_WIFI_STA_AUTO_RECONNECT
            ESP_LOGI(TAG, "Attempting to reconnect...");
            wifi_reconnect();
#endif
            break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    esp_err_t esp_ret;
    switch(event_id) {
#if CONFIG_WIFI_STA_CONNECT_IPV4 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
        case IP_EVENT_STA_GOT_IP:
            if(wifi_netif == NULL) {
                ESP_LOGE(TAG, "On Obtain IPv4 addr: Interface handle is NULL");
                return;
            }
            // Notify the wifi driver that we got the IP addr
            esp_ret = esp_wifi_internal_set_sta_ip();
            if(esp_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to notify wifi driver of IP");
                return;
            }
            xEventGroupSetBits(wifi_event_grp, WIFI_STA_IPV4_OBTAINED);
            
            // Print IP
            ip_event_got_ip_t *event_ip = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Wifi IPv4 address obtained");
            ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event_ip->ip_info.ip));
            ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&event_ip->ip_info.gw));
            break;
#endif
 
#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
        case IP_EVENT_GOT_IP6:
            if(wifi_netif == NULL) {
                ESP_LOGE(TAG, "On Obtain IPv6 addr: Interface handle is NULL");
                return;
            }
            // Notify the wifi driver that we got the IP addr
            esp_ret = esp_wifi_internal_set_sta_ip();
            if(esp_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to notify wifi driver of IP");
                return;
            }
            xEventGroupSetBits(wifi_event_grp, WIFI_STA_IPV6_OBTAINED);
            
            // Print IP
            ip_event_got_ip6_t *event_ipv6 = (ip_event_got_ip6_t *)event_data;
            ESP_LOGI(TAG, "Wifi IPv6 address obtained");
            ESP_LOGI(TAG, "IP: " IPV6STR, IPV62STR(event_ipv6->ip6_info.ip));
            break;
#endif
            
        case IP_EVENT_STA_LOST_IP:
            xEventGroupClearBits(wifi_event_grp, WIFI_STA_IPV4_OBTAINED);
            xEventGroupClearBits(wifi_event_grp, WIFI_STA_IPV6_OBTAINED);
            ESP_LOGI(TAG, "Wifi IP address Lost");
            break;
            
        default:
            ESP_LOGI(TAG, "Unhandled IP event: %li", event_id);
            break;
    }
}

static void wifi_start(void *esp_netif, esp_event_base_t event_base,
                       int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_err_t esp_ret;
    
    // Get driver handle
    wifi_netif_driver_t driver = esp_netif_get_io_driver(esp_netif);
    if(driver == NULL) {
        ESP_LOGE(TAG, "Failed to get Wifi Driver handle");
        return;
    }
    
    esp_ret = esp_wifi_get_if_mac(driver, mac_addr);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get Mac Addr");
        return;
    }
    ESP_LOGI(TAG, "Wifi MAC Addr: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], 
             mac_addr[3], mac_addr[4], mac_addr[5]);
    
    // Register net stack buffer reference and free CB
    esp_ret = esp_wifi_internal_reg_netstack_buf_cb(esp_netif_netstack_buf_ref,
                                                      esp_netif_netstack_buf_free);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Netstack CB Registration failed");
    }
    
    // Set mac addr of the wifi interface
    esp_ret = esp_netif_set_mac(esp_netif, mac_addr);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set MAC address");
        return;
    }
    
    // Start WIFI interface
    esp_netif_action_start(esp_netif, event_base, event_id, event_data);
    
    // Connect to wifi
    ESP_LOGI(TAG, "Connecting to %s...", CONFIG_WIFI_STA_SSID);
    esp_ret = esp_wifi_connect();
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to Connect to %s", CONFIG_WIFI_STA_SSID);
        return; 
    }
}

/**
 * Public functions which can be used by users
 */

// Initialize Wifi in STA MODE
esp_err_t wifi_init(EventGroupHandle_t event_grp)
{
    esp_err_t esp_ret;
    
    ESP_LOGI(TAG, "Starting Wifi in STA mode");
    
    if(event_grp == NULL) {
        ESP_LOGE(TAG, "Event Group handle is NULL");
        return ESP_FAIL;
    }
    
    wifi_event_grp = event_grp;
    
    // Create default wifi network
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    wifi_netif = esp_netif_new(&netif_cfg);
    if(wifi_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create Wifi network handle");
        return ESP_FAIL;
    }
    
    // Create a wifi driver
    wifi_driver = esp_wifi_create_if_driver(WIFI_IF_STA);
    if(wifi_driver == NULL) {
        ESP_LOGE(TAG, "Failed to create wifi driver");
        return ESP_FAIL;
    }
    
    // Connect wifi driver to network interface 
    esp_ret = esp_netif_attach(wifi_netif, wifi_driver);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect wifi driver to network interface");
        return ESP_FAIL;
    }
    
    // Register wifi event: station start
    esp_ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START,
                                         &on_wifi_event, NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to Register WIFI STA start event");
        return ESP_FAIL;
    }
    
    // Register wifi event: station stop
    esp_ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_STOP,
                                         &on_wifi_event, NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to Register WIFI STA stop event");
        return ESP_FAIL;
    }
    
    // Register wifi event: station connected
    esp_ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                         &on_wifi_event, NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to Register WIFI STA connected event");
        return ESP_FAIL;
    }
    
    // Register wifi event: station disconnected
    esp_ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                         &on_wifi_event, NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to Register WIFI STA disconnected event");
        return ESP_FAIL;
    }
    
#if CONFIG_WIFI_STA_CONNECT_IPV4 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
    // Register IP event: station got IPv4 address
    esp_ret = esp_event_handler_register(IP_EVENT,
                                         IP_EVENT_STA_GOT_IP,
                                         &on_ip_event,
                                         NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IPv4 event handler");
        return ESP_FAIL;
    }
#endif

#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
    // Register IP event: station got IPv6 address
    esp_ret = esp_event_handler_register(IP_EVENT,
                                         IP_EVENT_GOT_IP6,
                                         &on_ip_event,
                                         NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IPv6 event handler");
        return ESP_FAIL;
    }
#endif

    // Register IP event: station lost IP address
    esp_ret = esp_event_handler_register(IP_EVENT, 
                                         IP_EVENT_STA_LOST_IP, 
                                         &on_ip_event,
                                         NULL);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register lost IP event");
        return ESP_FAIL;
    }
    
    // Register shutdown handler 
    esp_ret = esp_register_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register shutdown event");
        return ESP_FAIL;
    }
    
    // Initialize wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&cfg);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI Initialization failed");
        return ESP_FAIL;
    }
    
    // Set Wifi station mode 
    esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI mode setup failed");
        return ESP_FAIL;
    }
    
    // Configure Wifi Auth mode
#if CONFIG_WIFI_STA_AUTH_OPEN
    const wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;
#elif CONFIG_WIFI_STA_AUTH_WEP
    const wifi_auth_mode_t auth_mode = WIFI_AUTH_WEP;
#elif CONFIG_WIFI_STA_AUTH_WPA_PSK
    const wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA_PSK;
#elif CONFIG_WIFI_STA_AUTH_WPA2_PSK
    const wifi_auth_mode_t auth_mode = WIFI_AUTH_WPA2_PSK;
#else 
    const wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;
#endif
    
    // Configure Wifi connection
    // Note: You can save these configs in NVS too 
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_STA_SSID,
            .password = CONFIG_WIFI_STA_PASSWORD,
            .threshold.authmode = auth_mode,
        },
    };
    
    esp_ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI configuration failed");
        return ESP_FAIL;
    }
    
    // Start the wifi driver
    esp_ret = esp_wifi_start();
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wifi");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// Stop Wifi 
esp_err_t wifi_sta_stop(void)
{
    esp_err_t esp_ret; 
    ESP_LOGI(TAG, "Stopping Wifi.....");
    
    // Unregister events
    esp_ret = esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, 
                                           &on_wifi_event);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI Start unregister event failed");
        return ESP_FAIL;
    }
    
    esp_ret = esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_STOP, 
                                           &on_wifi_event);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI Stop unregister events failed");
        return ESP_FAIL;
    }
    
    esp_ret = esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, 
                                           &on_wifi_event);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI connected events failed");
        return ESP_FAIL;
    }
    
    esp_ret = esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, 
                                           &on_wifi_event);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "WIFI disconnected events failed");
        return ESP_FAIL;
    }
    
#if CONFIG_WIFI_STA_CONNECT_IPV4 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
    esp_ret = esp_event_handler_unregister(IP_EVENT,
                                           IP_EVENT_STA_GOT_IP,
                                           &on_ip_event);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister IPv4 event handler");
        return ESP_FAIL;
    }
#endif

#if CONFIG_WIFI_STA_CONNECT_IPV6 || CONFIG_WIFI_STA_CONNECT_UNSPECIFIED
    esp_ret = esp_event_handler_unregister(IP_EVENT,
                                           IP_EVENT_GOT_IP6,
                                           &on_ip_event);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister IPv6 event handler");
        return ESP_FAIL;
    }
#endif 

    esp_ret = esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                           &on_ip_event);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister lost IP event");
        return ESP_FAIL;
    }
    
    // Unregister shutdown handler
    esp_ret = esp_unregister_shutdown_handler((shutdown_handler_t)esp_wifi_stop);
    if(esp_ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Shutdown handler already unregistered");
    } else if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unregister shutdown handler");
        return ESP_FAIL;
    }
    
    // Disconnect the wifi driver
    esp_ret = esp_wifi_disconnect();
    if((esp_ret == ESP_ERR_WIFI_NOT_INIT) || 
       (esp_ret == ESP_ERR_WIFI_NOT_STARTED)) {
        ESP_LOGE(TAG, "Wifi already disconnected");
    } else if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect Wifi");
        return ESP_FAIL;
    }
    
    esp_ret = esp_wifi_stop();
    if(esp_ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "Wifi already stopped");
    } else if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop Wifi");
        return ESP_FAIL;
    }
    
    esp_ret = esp_wifi_deinit();
    if(esp_ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "Wifi already deinitialized");
    } else if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinitialize Wifi");
        return ESP_FAIL;
    }
    
    // Detach wifi driver
    if(wifi_driver != NULL) {
        esp_wifi_destroy_if_driver(wifi_driver);
        wifi_driver = NULL;
    }
    
    if(wifi_netif != NULL) {
        esp_netif_destroy(wifi_netif);
    }
    
    if(wifi_event_grp != NULL) {
        xEventGroupClearBits(wifi_event_grp, WIFI_STA_CONNECTED);
        xEventGroupClearBits(wifi_event_grp, WIFI_STA_IPV4_OBTAINED);
        xEventGroupClearBits(wifi_event_grp, WIFI_STA_IPV6_OBTAINED);
    }
    
    wifi_netif = NULL;
    ESP_LOGI(TAG, "Wifi Stopped");
    
    return ESP_OK;
}

// Attempt to reconnect
esp_err_t wifi_reconnect(void)
{
    esp_err_t esp_ret;
    
    // Stop wifi
    esp_ret = wifi_sta_stop();
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop wifi during reconnect");
        return esp_ret;
    }
    
    // Restart wifi 
    esp_ret = wifi_init(wifi_event_grp);
    if(esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize wifi during reconnect");
        return esp_ret; 
    }
    
    return ESP_OK;
}