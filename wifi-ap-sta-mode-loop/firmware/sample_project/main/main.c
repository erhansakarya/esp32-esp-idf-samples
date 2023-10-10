#include <stdio.h>

// NOTE: Freertos includes
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// NOTE: Wifi connection includes
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "protocol_examples_common.h"

// NOTE: HTTP Client includes
#include "esp_http_client.h"

// NOTE: Logging includes
#include "esp_log.h"

// NOTE: NTP includes
#include <time.h>
#include "esp_sntp.h"

// NOTE: Wifi includes as component library
#include "wifi_connect.h"

#define RUN_SNTP    (0)
#define RUN_HTTP_CLIENT     (0)
#define RUN_WIFI_EXAMPLE_CONNECT    (0)
#define RUN_WIFI_SCANNER    (0)
#define RUN_WIFI_LIBRARY    (0)
#define RUN_WIFI_LIBRARY_AP_MODE    (0)
#define RUN_AP_TO_STA_LOOP_MODE     (1)

#define TAG "NTP TIME"

#define MAX_AP_COUNT    (20)

SemaphoreHandle_t got_time_semaphore;

void print_time()
{
    time_t now = 0;
    time(&now);

    struct tm * time_info = localtime(&now);

    char time_buffer[50];
    strftime(time_buffer, sizeof(time_buffer), "%c", time_info);

    ESP_LOGI(TAG, "**** %s ****", time_buffer);
}

esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    esp_err_t esp_err = ESP_OK;

    switch (evt->event_id)
    {
        case HTTP_EVENT_ON_DATA:
        {
            printf("HTTP_EVENT_ON_DATA: %.*s", evt->data_len, (char *)evt->data);
            break;
        }
        default:
        {
            printf("http event unhandled!");
            break;
        }
    }

    return esp_err;
}

void on_got_time(struct timeval *tv)
{
    printf("on got callback: %lld\n", tv->tv_sec);
    print_time();

    xSemaphoreGive(got_time_semaphore);
}

char * get_auth_mode_name(wifi_auth_mode_t wifi_auth_mode)
{
    switch(wifi_auth_mode)
    {
        case WIFI_AUTH_OPEN:
        {
            return "WIFI_AUTH_OPEN";
        }
        case WIFI_AUTH_WEP:
        {
            return "WIFI_AUTH_WEP";
        }
        case WIFI_AUTH_WPA_PSK:
        {
            return "WIFI_AUTH_WPA_PSK";
        }
        case WIFI_AUTH_WPA2_PSK:
        {
            return "WIFI_AUTH_WPA2_PSK";
        }
        case WIFI_AUTH_WPA_WPA2_PSK:
        {
            return "WIFI_AUTH_WPA_WPA2_PSK";
        }
        case WIFI_AUTH_WPA2_ENTERPRISE:
        {
            return "WIFI_AUTH_WPA2_ENTERPRISE";
        }
        case WIFI_AUTH_WPA3_PSK:
        {
            return "WIFI_AUTH_WPA3_PSK";
        }
        case WIFI_AUTH_WPA2_WPA3_PSK:
        {
            return "WIFI_AUTH_WPA2_WPA3_PSK";
        }
        case WIFI_AUTH_WAPI_PSK:
        {
            return "WIFI_AUTH_WAPI_PSK";
        }
        case WIFI_AUTH_OWE:
        {
            return "WIFI_AUTH_OWE";
        }
        case WIFI_AUTH_MAX:
        {
            return "WIFI_AUTH_MAX";
        }
        default:
        {
            return "UNKNOWN_AUTH_MODE";
        }
    }
}

void app_main(void)
{
    nvs_flash_init();

#if RUN_SNTP
    got_time_semaphore = xSemaphoreCreateBinary();

    setenv("TZ", "<+03>-3", 1);
    tzset();
    print_time();
#endif

#if RUN_WIFI_EXAMPLE_CONNECT
    esp_netif_init();
    esp_event_loop_create_default();

    example_connect();
#endif

#if RUN_SNTP
    esp_sntp_init();
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(on_got_time);

    xSemaphoreTake(got_time_semaphore, portMAX_DELAY);
#endif

#if RUN_HTTP_CLIENT
    esp_http_client_config_t esp_http_client_config = {
        .url = "https://jsonplaceholder.typicode.com/todos/1",
        .event_handler = http_client_event_handler,
    };

    esp_http_client_handle_t esp_http_client_handle = esp_http_client_init(&esp_http_client_config);
    esp_http_client_perform(esp_http_client_handle);
    esp_http_client_cleanup(esp_http_client_handle);
#endif

#if RUN_SNTP
    for (uint8_t i = 0; i < 5; i++) 
    {
        print_time();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

#if RUN_WIFI_SCANNER
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_init_config);
    esp_wifi_start();

    wifi_scan_config_t wifi_scan_config = {
        .show_hidden = true,
    };
    esp_wifi_scan_start(&wifi_scan_config, true);

    wifi_ap_record_t wifi_ap_records[MAX_AP_COUNT];
    uint16_t max_record = MAX_AP_COUNT;

    esp_wifi_scan_get_ap_records(&max_record, wifi_ap_records);

    printf("Found %d access points:\n", max_record);
    printf("\n");
    printf("SSID | Channel | RSSI | Auth Mode\n");
    for (uint8_t i = 0; i < max_record; i++)
    {
        printf("%32s | %7d | %4d | %12s\n", (char *)wifi_ap_records[i].ssid, wifi_ap_records[i].primary, wifi_ap_records[i].rssi, get_auth_mode_name(wifi_ap_records[i].authmode));
    }
#endif

#if RUN_WIFI_LIBRARY
    wifi_connect_init();
    esp_err_t esp_err = wifi_connect_sta("erhan", "1963erhan", 10000);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
#endif

#if RUN_WIFI_LIBRARY_AP_MODE
    wifi_connect_init();
    wifi_connect_ap("myEsp32AP", "password");
#endif

#if RUN_AP_TO_STA_LOOP_MODE
    wifi_connect_init();

    for (;;)
    {
        wifi_connect_ap("myEsp32AP", "password");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        wifi_disconnect();

        wifi_connect_sta("erhan", "1963erhan", 10000);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        wifi_disconnect();
    }

#endif
}
