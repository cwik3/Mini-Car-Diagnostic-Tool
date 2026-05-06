#include "Mock_CAN.h"
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "display.h"
#include <math.h>

static const char *TAG = "TWAI/CAN_MOCK_test";

//gloabal handle for the TWAI node
static twai_node_handle_t node_hdl = NULL;
static QueueHandle_t rx_queue = NULL;

static int current_rpm = 0;

typedef struct{
    uint32_t id;
    uint8_t data[8];
    uint8_t dlc;
} can_message_t;

int can_mock_get_rpm(void) {
    return current_rpm;
}

static void tx_task(void *arg){
    ESP_LOGI(TAG, "Starting TX task");
    int current_rpm = 800;
    int current_speed = 10;
    int coolant_temp = 20;
    int add = 10;
    int inc = 50;
    int state = 0;
    int current_fuel_value = 100;
    int fuel_dec = -1;
    static int step = 0;
    static const int MAX_STEPS = 100;
    int throtle_pos = 0;
    int engine_load = 0;

    const TickType_t fuel_decrease_interval = pdMS_TO_TICKS(1500); // fuel drop interval for simluation
    TickType_t last_fuel_decrease_time = xTaskGetTickCount();
    uint8_t data_payload1[8] = {0x04, 0x41, 0x00, 0x00, 0x00, 0xAA, 0xAA, 0xAA}; 
    // can FRAMEWORK WE want to recieve
    twai_frame_t message = { 
        .header.id = 0x7E8,       
        .header.ide = false,     
        .header.rtr = false,    
        .header.dlc = 8,                
        .buffer = data_payload1,   
        .buffer_len = 8
    };

    while (1) {
        if(state == 0){
            data_payload1[2] = 0x0C;
            int rpm_value = current_rpm * 4;
            data_payload1[3] = (rpm_value >> 8) & 0xFF; // High byte A[3]
            data_payload1[4] = rpm_value & 0xFF;        // Low byte A[4]

            esp_err_t ret = twai_node_transmit(node_hdl, &message, pdMS_TO_TICKS(100));
            if (ret != ESP_OK) ESP_LOGE(TAG, "TX Error RPM: %s", esp_err_to_name(ret));
            current_rpm += inc;
            if (current_rpm > 3000 || current_rpm < 800) {
                inc = -inc;
            }
            state = 1;
        } else if (state == 1){
            data_payload1[2] = 0x2F;
            TickType_t current_time = xTaskGetTickCount();
            if(current_time - last_fuel_decrease_time >= fuel_decrease_interval){
                if(current_fuel_value > 0){
                    current_fuel_value += fuel_dec; // decrease fuel level
                }
                last_fuel_decrease_time = current_time; 
            }
            if (current_fuel_value <= 0) {
                current_fuel_value = 0; 
                ESP_LOGE(TAG, "Fuel level is empty Carr is stopped");
            }
            data_payload1[3] = (current_fuel_value * 255) / 100;
            esp_err_t ret = twai_node_transmit(node_hdl, &message, pdMS_TO_TICKS(100));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Error w Ramce CANu w fuel status: %s", esp_err_to_name(ret));
            } 
            state = 2;
        } else if (state == 2){
            data_payload1[2] = 0x0D; //Vehicle speed PID
            data_payload1[3] = current_speed;
            esp_err_t ret = twai_node_transmit(node_hdl, &message, pdMS_TO_TICKS(100));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "TX Error w ramce vehicle speed: %s", esp_err_to_name(ret));
            }
            current_speed += add;
            if (current_speed >= 150) {
                add = -15; 
            } else if (current_speed <= 30) {
                add = 35;  
            }
            state = 3;
        }else if(state == 3){
            data_payload1[2] = 0x05; // Coolant temperature PID
            step++;
            if (step > MAX_STEPS) { 
                step = MAX_STEPS; // Zatrzymuje się na maxie
            }
            int current_coolant = 20 + (int)(70.0f * (log(1.0f + step) / log(1.0f + MAX_STEPS)));
            data_payload1[3] = current_coolant + 40;
            twai_node_transmit(node_hdl, &message, pdMS_TO_TICKS(100));
            state = 4;
        }else if(state == 4){
            data_payload1[2] = 0x11; // Throttle position PID
            static float angle = 0.0f;
            angle += 0.2f;
            if (angle > 2.0f * 3.14159f) {
                angle -= 2.0f * 3.14159f; 
            }
            int throttle_pos = (int)(50.0f + 50.0f * sin(angle));
            data_payload1[3] = (throttle_pos * 255) / 100; 
            
            twai_node_transmit(node_hdl, &message, pdMS_TO_TICKS(100));
            state = 5;
        } else if(state == 5){
            data_payload1[2] = 0x04; // Engine Load PID
            data_payload1[3] = (engine_load * 255) / 100;
            esp_err_t ret = twai_node_transmit(node_hdl, &message, pdMS_TO_TICKS(100));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "TX Error w ramce engine_load: %s", esp_err_to_name(ret));
            }
            engine_load += 2;
            if (engine_load > 100) engine_load = 0; 
            
            state = 0;
        } 
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


static bool rx_callback(twai_node_handle_t node,const twai_rx_done_event_data_t *event_data, void *user_data){
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


static void rx_task(void *arg)
{
    ESP_LOGI(TAG, "starting RX task");
    can_message_t received_msg;
    int rx_fuel_level = 0;
    int rx_speed = 0;
    int rx_coolant_temp = 0;
    int rx_throttle_pos = 0;
    int rx_engine_load = 0;
    while (1) {
        if (xQueueReceive(rx_queue, &received_msg, pdMS_TO_TICKS(1100)) == pdTRUE) {
            ESP_LOGI("RX", "Ramka otzrymana ID: 0x%lX", received_msg.id);
            if (received_msg.id == 0x7E8) {
                if(received_msg.data[2] == 0x0C){             
                    current_rpm = ((received_msg.data[3] * 256) + received_msg.data[4]) / 4;
            }else if(received_msg.data[2] == 0x2F){
                    rx_fuel_level = (received_msg.data[3] * 100)/ 255; 
            } else if(received_msg.data[2] == 0x0D){
                    rx_speed = received_msg.data[3];
            } else if(received_msg.data[2] == 0x05){
                    rx_coolant_temp = received_msg.data[3] - 40;
            }else if(received_msg.data[2] == 0x11){
                    rx_throttle_pos = (received_msg.data[3] * 100) / 255;
            }else if(received_msg.data[2] == 0x04){
                    rx_engine_load = (received_msg.data[3] * 100) / 255;
            }
                ESP_LOGI(TAG, "Ramka CAN z Paramterami Odebarana %d, Fuel Level: %d%%, Speed: %d KM/H, Coolant Temp: %d, Throttle Pos: %d, Engine Load: %d", current_rpm, rx_fuel_level, rx_speed, rx_coolant_temp, rx_throttle_pos, rx_engine_load);
                display_update(current_rpm, rx_fuel_level, rx_speed, rx_coolant_temp, rx_throttle_pos, rx_engine_load);
            }
        }
    }
}



void can_mock_init(void)
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

    twai_event_callbacks_t cbs = {
        .on_rx_done = rx_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &cbs, NULL));

    ESP_ERROR_CHECK(twai_node_enable(node_hdl));
    ESP_LOGI(TAG, "TWAI Node enabled and running");

    xTaskCreate(tx_task, "twai_tx_task", 4096, NULL, 5, NULL);
    xTaskCreate(rx_task, "twai_rx_task", 4096, NULL, 5, NULL);
}