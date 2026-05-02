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
    display_init(); // Inicjalizacja wyświetlacza OLED
    can_mock_init(); //Odpalamy CAN
    while(1) {
        //int rpm = can_mock_get_rpm(); nie potrzebujemy już rpm tutaj 
        //ESP_LOGI(TAG, "RX zwraca aktualnie:%d RPM", rpm);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}