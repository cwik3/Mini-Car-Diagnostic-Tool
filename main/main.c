#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "TWAI_MOCK_test";

//gloabal handle for the TWAI node
twai_node_handle_t node_hdl = NULL;
QueueHandle_t rx_queue = NULL;

typedef struct{
    uint32_t id;
    uint8_t data[8];
    uint8_t dlc;
} can_message_t;

void tx_task(void *arg)
{
    ESP_LOGI(TAG, "Starting TX task");
    uint8_t data_payload[8] = {0x04, 0x41, 0x0C, 0x0C, 0x80, 0xAA, 0xAA, 0xAA};
    
    // can FRAMEWORK WE want to recieve
    twai_frame_t message = { // ZMIANA: twai_message_t na twai_frame_t
        .header.id = 0x7E8,       
        .header.ide = false,     
        .header.rtr = false,    
        .header.dlc = 8,                
        .buffer = data_payload,   
        .buffer_len = 8
    };
    
    while (1) {
        esp_err_t ret = twai_node_transmit(node_hdl, &message, pdMS_TO_TICKS(1000));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Ramka RPM wyslana");
        } else {
            ESP_LOGE(TAG, "Error w Ramce CANu: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

bool rx_callback(twai_node_handle_t node,const twai_rx_done_event_data_t *event_data, void *user_data){
    uint8_t isr_buffer[8];
    twai_frame_t rx_message = {
        .buffer = isr_buffer,
        .buffer_len = sizeof(isr_buffer)
    };
    if(twai_node_receive_from_isr(node, &rx_message) == ESP_OK){
        
        can_message_t safe_msg;
        safe_msg.id = rx_message.header.id;
        safe_msg.dlc = rx_message.header.dlc;

        for (int i = 0; i < rx_message.header.dlc; i++) {
            safe_msg.data[i] = isr_buffer[i];
        }
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(rx_queue, &safe_msg, &xHigherPriorityTaskWoken);
        return xHigherPriorityTaskWoken == pdTRUE;
    }
    return false;
}


void rx_task(void *arg)
{
    ESP_LOGI(TAG, "starting RX task");
    can_message_t received_msg;
    while (1) {
        if (xQueueReceive(rx_queue, &received_msg, pdMS_TO_TICKS(1100)) == pdTRUE) {
            ESP_LOGI("RX", "Ramka otzrymana ID: 0x%lX", received_msg.id);
            if (received_msg.id == 0x7E8) {
                int rpm = ((received_msg.data[3] * 256) + received_msg.data[4]) / 4;
                ESP_LOGI("RX", "Ramka CAN odebrana, aktualne RPM to %d", rpm);
            }
        }
    }
}


void app_main(void)
{
    ESP_LOGI(TAG, "Initializing TWAI");
    rx_queue = xQueueCreate(10, sizeof(can_message_t));
    //Configuration inside the function
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = 5,             // assigin tx
        .io_cfg.rx = 4,             // assign rx
        .bit_timing.bitrate = 500000, // baud rate 500k
        .tx_queue_depth = 5,          
        .flags.enable_self_test = true,
        .flags.enable_loopback = true
    };

    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));
    ESP_LOGI(TAG, "TWAI node created");
    twai_mask_filter_config_t filter_cfg = {
        .id = 0,
        .mask = 0,
        .is_ext = false,
    };
    ESP_ERROR_CHECK(twai_node_config_mask_filter(node_hdl, 0, &filter_cfg));
    ESP_LOGI(TAG, "TWAI filter configured to accept all");

    twai_event_callbacks_t cbs = {
        .on_rx_done = rx_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &cbs, NULL));

    ESP_ERROR_CHECK(twai_node_enable(node_hdl));
    ESP_LOGI(TAG, "TWAI Node enabled and running");

    xTaskCreate(tx_task, "twai_tx_task", 4096, NULL, 5, NULL);
    xTaskCreate(rx_task, "twai_rx_task", 4096, NULL, 5, NULL);
}