#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include "esp_netif.h"
#include "esp_wifi.h"

#include "wifi_connect.h"

// NOTE: Available to this file
char * get_wifi_disconnection_reason_string(wifi_err_reason_t wifi_err_reason);

#define TAG "WIFI"

static esp_netif_t * esp_netif;

static EventGroupHandle_t wifi_event_group_handle;
static int CONNECTED = BIT0;
static int DISCONNECTED = BIT1;

static int disconnection_error_count = 0;
static bool attempt_reconnect = false;

void event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");

            esp_wifi_connect();

            break;
        }
        case WIFI_EVENT_STA_CONNECTED:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");

            disconnection_error_count = 0;

            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t * wifi_event_sta_disconnected = event_data;

            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED: reason code: %d, reason string: %s", 
                wifi_event_sta_disconnected->reason, get_wifi_disconnection_reason_string(wifi_event_sta_disconnected->reason));

            if (attempt_reconnect) 
            {
                // TODO: Change this error handling mechanism, test these error codes.
                if (wifi_event_sta_disconnected->reason == WIFI_REASON_NO_AP_FOUND
                    || wifi_event_sta_disconnected->reason == WIFI_REASON_ASSOC_LEAVE
                    || wifi_event_sta_disconnected->reason == WIFI_REASON_AUTH_EXPIRE
                    || wifi_event_sta_disconnected->reason == WIFI_REASON_UNSPECIFIED
                    || wifi_event_sta_disconnected->reason == WIFI_REASON_AUTH_LEAVE)
                {
                    if (disconnection_error_count++ < 5)
                    {
                        vTaskDelay(5000 / portTICK_PERIOD_MS);
                        esp_wifi_connect();
                        break;
                    }
                }
            }
            

            xEventGroupSetBits(wifi_event_group_handle, DISCONNECTED);

            break;
        }
        case IP_EVENT_STA_GOT_IP:
        {
            ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");

            xEventGroupSetBits(wifi_event_group_handle, CONNECTED);

            break;
        }
        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t * wifi_event_ap_staconnected = event_data;

            char mac_str[18];
            snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                wifi_event_ap_staconnected->mac[0], wifi_event_ap_staconnected->mac[1], wifi_event_ap_staconnected->mac[2], 
                wifi_event_ap_staconnected->mac[3], wifi_event_ap_staconnected->mac[4], wifi_event_ap_staconnected->mac[5]);

            ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED: client mac id: %s", mac_str);

            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");

            break;
        }
        default:
        {
            break;
        }
    }
}

void wifi_connect_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
}

esp_err_t wifi_connect_sta(char * ssid, char * password, int timeout)
{
    attempt_reconnect = true;
    wifi_event_group_handle = xEventGroupCreate();
    esp_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t result = xEventGroupWaitBits(wifi_event_group_handle, CONNECTED | DISCONNECTED, true, false, pdMS_TO_TICKS(timeout));
    if (result == CONNECTED)
    {
        return ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }
}

void wifi_connect_ap(const char * ssid, const char * password)
{
    esp_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    // NOTE: Max connection count
    wifi_config.ap.max_connection = 1;
    wifi_config.ap.beacon_interval = 100;
    // NOTE: 2.4GHz wifi channel between 
    wifi_config.ap.channel = 1;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_disconnect(void)
{
    attempt_reconnect = false;
    esp_wifi_stop();
    esp_netif_destroy(esp_netif);
}