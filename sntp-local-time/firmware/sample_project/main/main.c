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

#define TAG "NTP TIME"

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

void app_main(void)
{
    got_time_semaphore = xSemaphoreCreateBinary();

    setenv("TZ", "<+03>-3", 1);
    tzset();
    print_time();

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    example_connect();

    esp_sntp_init();
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(on_got_time);

    xSemaphoreTake(got_time_semaphore, portMAX_DELAY);

    esp_http_client_config_t esp_http_client_config = {
        .url = "https://jsonplaceholder.typicode.com/todos/1",
        .event_handler = http_client_event_handler,
    };

    esp_http_client_handle_t esp_http_client_handle = esp_http_client_init(&esp_http_client_config);
    esp_http_client_perform(esp_http_client_handle);
    esp_http_client_cleanup(esp_http_client_handle);

    for (uint8_t i = 0; i < 5; i++) 
    {
        print_time();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
