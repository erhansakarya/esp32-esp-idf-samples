#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// NOTE: FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// NOTE: GPIO includes
#include "driver/gpio.h"

// NOTE: cJSON includes
#include "cJSON.h"

// NOTE: websocket send message includes
#include "main.h"

#define BUTTON  (0)

static SemaphoreHandle_t push_button_semaphore;

static void IRAM_ATTR on_button_pushed(void * args)
{
    xSemaphoreGiveFromISR(push_button_semaphore, NULL);
}

static void push_button_task(void * params)
{
    for(;;)
    {
        xSemaphoreTake(push_button_semaphore, portMAX_DELAY);
        cJSON * payload = cJSON_CreateObject();
        cJSON_AddBoolToObject(payload, "push_button_state", gpio_get_level(BUTTON));
        char * message = cJSON_Print(payload);
        printf("message: %s", message);
        // NOTE: send message over web socket
        send_ws_message(message);

        cJSON_Delete(payload);
        free(message);
    }
}

void init_button(void)
{
    push_button_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(push_button_task, "push_button_task", 2048, NULL, 5, NULL);

    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);
    gpio_set_intr_type(BUTTON, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON, on_button_pushed, NULL);
}