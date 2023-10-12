#include <stdio.h>
#include <string.h>

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

// NOTE: HTTP client includes
#include "esp_http_client.h"

// NOTE: HTTP server includes
#include "esp_http_server.h"

// NOTE: mdns includes
#include "mdns.h"

// NOTE: toggle led via http server post method
#include "toggle_led.h"

// NOTE: push button status via web socket
#include "push_button.h"

// NOTE: cJSON library to parse http post request payload
#include "cJSON.h"

#define RUN_SNTP    (0)
#define RUN_HTTP_CLIENT     (0)
#define RUN_WIFI_EXAMPLE_CONNECT    (0)
#define RUN_WIFI_SCANNER    (0)
#define RUN_WIFI_LIBRARY    (0)
#define RUN_WIFI_LIBRARY_AP_MODE    (0)
#define RUN_AP_TO_STA_LOOP_MODE     (0)
#define RUN_HTTP_GET_MODE   (0)
#define RUN_HTTPS_GET_MODE  (0)
#define RUN_HTTP_SERVER_MODE    (0)
#define RUN_HTTP_SERVER_TOGGLE_LED_MODE     (0)
#define RUN_HTTP_SERVER_TOGGLE_LED_PUSH_BUTTON_WEBSOCKET_MODE     (1)

#define TAG     "NTP TIME"
#define HTTP_CLIENT_TAG     "HTTP_CLIENT"
#define HTTP_SERVER_TAG     "HTTP_SERVER"

#define MAX_AP_COUNT    (20)

extern const uint8_t https_api_cert[] asm("_binary_amazon_crt_start");

SemaphoreHandle_t got_time_semaphore;

static httpd_handle_t httpd_handle = NULL;

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
        case WIFI_AUTH_OPEN:    return "WIFI_AUTH_OPEN";
        case WIFI_AUTH_WEP:     return "WIFI_AUTH_WEP";
        case WIFI_AUTH_WPA_PSK:     return "WIFI_AUTH_WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:    return "WIFI_AUTH_WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WIFI_AUTH_WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:     return "WIFI_AUTH_WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:    return "WIFI_AUTH_WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WIFI_AUTH_WPA2_WPA3_PSK";
        case WIFI_AUTH_WAPI_PSK:    return "WIFI_AUTH_WAPI_PSK";
        case WIFI_AUTH_OWE:     return "WIFI_AUTH_OWE";
        case WIFI_AUTH_MAX:     return "WIFI_AUTH_MAX";
        default:    return "UNKNOWN_AUTH_MODE";
    }
}

typedef struct chunk_payload_t
{
    uint8_t * buffer;
    int buffer_index;
} chunk_payload_t;

esp_err_t on_http_client_data(esp_http_client_event_t *evt)
{
    esp_err_t esp_err = ESP_OK;

    switch (evt->event_id)
    {
        case HTTP_EVENT_ON_DATA:
        {
            /*
            ESP_LOGI(HTTP_CLIENT_TAG, "Length: %d", evt->data_len);
            printf("%.*s\n", evt->data_len, (char *) evt->data);
            */

            chunk_payload_t * chunk_payload = evt->user_data;
            chunk_payload->buffer = realloc(chunk_payload->buffer, chunk_payload->buffer_index + evt->data_len + 1);
            memcpy(&chunk_payload->buffer[chunk_payload->buffer_index], (uint8_t *) evt->data, evt->data_len);
            chunk_payload->buffer_index = chunk_payload->buffer_index + evt->data_len;
            chunk_payload->buffer[chunk_payload->buffer_index] = NULL;

            // NOTE: Commented because wdt problem.
            //printf("http client buffer: %s\n", chunk_payload->buffer);

            break;
        }
        default:
        {
            break;
        }
    }

    return esp_err;
}

void fetch_jsonplaceholder_get_posts(void)
{
    chunk_payload_t chunk_payload = { 0 };

    esp_http_client_config_t esp_http_client_config = {
        .url = "http://jsonplaceholder.typicode.com/posts/1",
        .method = HTTP_METHOD_GET,
        .event_handler = on_http_client_data,
        .user_data = &chunk_payload
    };
    esp_http_client_handle_t esp_http_client_handle = esp_http_client_init(&esp_http_client_config);
    esp_http_client_set_header(esp_http_client_handle, "Content-Type", "application/json");
    esp_err_t esp_err = esp_http_client_perform(esp_http_client_handle);
    if (esp_err == ESP_OK)
    {
        ESP_LOGI(HTTP_CLIENT_TAG, "HTTP GET status: %d, content-length: %lld, result: %s\n", 
            esp_http_client_get_status_code(esp_http_client_handle), esp_http_client_get_content_length(esp_http_client_handle), chunk_payload.buffer);
    }
    else
    {
        ESP_LOGE(HTTP_CLIENT_TAG, "HTTP GET status error: %s", esp_err_to_name(esp_err));
    }

    if (chunk_payload.buffer != NULL)
    {
        free(chunk_payload.buffer);
    }

    esp_http_client_cleanup(esp_http_client_handle);
}

void fetch_rapid_api_weather_api_get_forecast_weather(void)
{
    chunk_payload_t chunk_payload = { 0 };

    esp_http_client_config_t esp_http_client_config = {
        .url = "https://weatherapi-com.p.rapidapi.com/forecast.json?q=Bursa&days=3",
        .method = HTTP_METHOD_GET,
        .event_handler = on_http_client_data,
        .user_data = &chunk_payload,
        .cert_pem = (char *) https_api_cert
    };
    esp_http_client_handle_t esp_http_client_handle = esp_http_client_init(&esp_http_client_config);
    esp_http_client_set_header(esp_http_client_handle, "Content-Type", "application/json");
    esp_http_client_set_header(esp_http_client_handle, "X-RapidAPI-Host", "weatherapi-com.p.rapidapi.com");
    esp_http_client_set_header(esp_http_client_handle, "X-RapidAPI-Key", "your-api-key");
    esp_err_t esp_err = esp_http_client_perform(esp_http_client_handle);
    if (esp_err == ESP_OK)
    {
        ESP_LOGI(HTTP_CLIENT_TAG, "HTTP GET status: %d, content-length: %lld, result: %s\n", 
            esp_http_client_get_status_code(esp_http_client_handle), esp_http_client_get_content_length(esp_http_client_handle), chunk_payload.buffer);
    }
    else
    {
        ESP_LOGE(HTTP_CLIENT_TAG, "HTTP GET status error: %s", esp_err_to_name(esp_err));
    }

    if (chunk_payload.buffer != NULL)
    {
        free(chunk_payload.buffer);
    }

    esp_http_client_cleanup(esp_http_client_handle);
}

esp_err_t on_default_url(httpd_req_t *request)
{
    ESP_LOGI(HTTP_SERVER_TAG, "URL: %s", request->uri);
    httpd_resp_sendstr(request, "hello world");

    return ESP_OK;
}

esp_err_t on_toggle_led_url(httpd_req_t * request)
{
    char buffer[100];
    memset(&buffer, 0, sizeof(buffer));
    // TODO: Check buffer size is fit
    httpd_req_recv(request, buffer, request->content_len);
    cJSON * payload = cJSON_Parse(buffer);
    // TODO: Check payload is not null
    cJSON * is_on_json = cJSON_GetObjectItem(payload, "is_on");
    // TODO: Check is_on_json is not null
    bool is_on = cJSON_IsTrue(is_on_json);
    cJSON_Delete(payload);

    toggle_led(is_on);
    httpd_resp_set_status(request, "204 NO CONTENT");
    httpd_resp_send(request, NULL, 0);

    return ESP_OK;
}

#define WS_MAX_SIZE     (1024)
static int client_session_id; 

esp_err_t send_ws_message(char * message)
{
    if (!client_session_id)
    {
        ESP_LOGE(TAG, "no client_session_id");
        return -1;
    }
    httpd_ws_frame_t httpd_ws_frame_message = {
        .final = true,
        .fragmented = false,
        .len = strlen(message),
        .payload = (uint8_t *)message,
        .type = HTTPD_WS_TYPE_TEXT
    };
  
    return httpd_ws_send_frame_async(httpd_handle, client_session_id, &httpd_ws_frame_message);
}

esp_err_t on_push_button_url(httpd_req_t * request)
{
    client_session_id = httpd_req_to_sockfd(request);
    if (request->method == HTTP_GET)
    {
        return ESP_OK;
    }

    httpd_ws_frame_t httpd_ws_frame_request;
    memset(&httpd_ws_frame_request, 0, sizeof(httpd_ws_frame_t));
    httpd_ws_frame_request.type = HTTPD_WS_TYPE_TEXT;
    httpd_ws_frame_request.payload = malloc(WS_MAX_SIZE);
    httpd_ws_recv_frame(request, &httpd_ws_frame_request, WS_MAX_SIZE);
    printf("ws payload: %.*s\n", httpd_ws_frame_request.len, httpd_ws_frame_request.payload);
    free(httpd_ws_frame_request.payload);

    char * response = "connected ok";
    httpd_ws_frame_t httpd_ws_frame_response = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *) response,
        .len = strlen(response)
    };
    return httpd_ws_send_frame(request, &httpd_ws_frame_response);
}

void init_http_server()
{
    httpd_handle_t httpd_handle = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(httpd_start(&httpd_handle, &httpd_config));

    httpd_uri_t default_url = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = on_default_url
    };
    httpd_register_uri_handler(httpd_handle, &default_url);

    httpd_uri_t toggle_led_url = {
        .uri = "/api/toggle-led",
        .method = HTTP_POST,
        .handler = on_toggle_led_url
    };
    httpd_register_uri_handler(httpd_handle, &toggle_led_url);

    httpd_uri_t push_button_url = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = on_push_button_url,
        .is_websocket = true
    };
    httpd_register_uri_handler(httpd_handle, &push_button_url);
}

void start_mdns_service()
{
    mdns_init();
    mdns_hostname_set("my-esp32");
    mdns_instance_name_set("esp32 thing.");
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

#if RUN_HTTP_GET_MODE
    wifi_connect_init();
    esp_err_t esp_err = wifi_connect_sta("erhan", "1963erhan", 20000);
    fetch_jsonplaceholder_get_posts();
#endif

#if RUN_HTTPS_GET_MODE
    wifi_connect_init();
    esp_err_t esp_err = wifi_connect_sta("erhan", "1963erhan", 20000);
    fetch_rapid_api_weather_api_get_forecast_weather();
#endif

#if RUN_HTTP_SERVER_MODE
    wifi_connect_init();
    esp_err_t esp_err = wifi_connect_sta("erhan", "1963erhan", 20000);

    start_mdns_service();
    init_http_server(); 
#endif

#if RUN_HTTP_SERVER_TOGGLE_LED_MODE
    init_led();
    wifi_connect_init();
    esp_err_t esp_err = wifi_connect_sta("erhan", "1963erhan", 20000);

    start_mdns_service();
    init_http_server();
#endif

#if RUN_HTTP_SERVER_TOGGLE_LED_PUSH_BUTTON_WEBSOCKET_MODE
    init_led();
    init_button();
    wifi_connect_init();
    esp_err_t esp_err = wifi_connect_sta("erhan", "1963erhan", 20000);

    start_mdns_service();
    init_http_server();
#endif
}
