#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Mock_CAN.h"
#include "display.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "System start");
    display_init(); 
    can_mock_init(); 
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}