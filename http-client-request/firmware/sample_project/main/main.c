#include <stdio.h>

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "protocol_examples_common.h"

#include "esp_http_client.h"

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

void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    example_connect();

    esp_http_client_config_t esp_http_client_config = {
        .url = "https://jsonplaceholder.typicode.com/todos/1",
        .event_handler = http_client_event_handler,
    };

    esp_http_client_handle_t esp_http_client_handle = esp_http_client_init(&esp_http_client_config);
    esp_http_client_perform(esp_http_client_handle);
    esp_http_client_cleanup(esp_http_client_handle);
}
