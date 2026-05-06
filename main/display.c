#include "display.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OLED_RAW";

#define I2C_SCL 22     
#define I2C_SDA 21   
#define I2C_MASTER_NUM I2C_NUM_0 
#define I2C_HZ 400000
#define OLED_I2C_ADDRESS 0x3C    // SSD1306 standard adress

static void oled_send_command(uint8_t command) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_write_byte(cmd, command, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
}

static void oled_send_data(uint8_t *data, size_t length) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x40, true);
    i2c_master_write(cmd, data, length, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
}

static const uint8_t font_5x7[][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, 
    {0x00, 0x42, 0x7F, 0x40, 0x00}, 
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31}, 
    {0x18, 0x14, 0x12, 0x7F, 0x10}, 
    {0x27, 0x45, 0x45, 0x45, 0x39}, 
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, 
    {0x01, 0x71, 0x09, 0x05, 0x03}, 
    {0x36, 0x49, 0x49, 0x49, 0x36}, 
    {0x06, 0x49, 0x49, 0x29, 0x1E}, 
    {0x00, 0x00, 0x00, 0x00, 0x00}, 
    {0x7F, 0x09, 0x19, 0x29, 0x46}, 
    {0x7F, 0x09, 0x09, 0x09, 0x06}, 
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x09, 0x09, 0x09, 0x01}, 
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, 
    {0x7F, 0x49, 0x49, 0x49, 0x41}, 
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x20, 0x10, 0x08, 0x04, 0x02},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x22}, 
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, 
    {0x01, 0x01, 0x7F, 0x01, 0x01}, 
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, 
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, 
    {0x06, 0x09, 0x09, 0x06, 0x00} 

};

static void oled_print_char(uint8_t character) {
    uint8_t index = 10;
    if (character >= '0' && character <= '9') index = character - '0';
    if (character == 'R') index = 11;
    if (character == 'P') index = 12;
    if (character == 'M') index = 13;
    if (character == 'F') index = 14;
    if (character == 'U') index = 15;
    if (character == 'E') index = 16;
    if (character == 'L') index = 17;
    if (character == '%') index = 18;
    if (character == 'K') index = 19;
    if(character == 'H') index = 21;
    if(character == '/') index = 20;
    if (character == 'C') index = 22;
    if (character == 'O') index = 23;   
    if (character == 'T') index = 24;
    if (character == 'A') index = 25;
    if (character == 'D') index = 26;
    if (character == '*') index = 27;
    if (character == ' ') index = 10;
    uint8_t buf[6];
    memcpy(buf, font_5x7[index], 5);
    buf[5] = 0x00; 
    oled_send_data(buf, 6);
}

static void oled_xy(uint8_t x, uint8_t y) {
    oled_send_command(0xB0 + y);            // Now we can adjust x y postion of the data where to display
    oled_send_command(0x00 + (x & 0x0F));
    oled_send_command(0x10 + ((x >> 4) & 0x0F));
}

void display_update(int rpm, int fuel_level, int speed, int coolant_temp, int throttle_pos, int engine_load) {
    char text[32];
    oled_xy(10, 1); 
    snprintf(text, sizeof(text), "%3d KM/H    ", speed);
    for (int i = 0; text[i] != '\0'; i++) {
        oled_print_char(text[i]);
    }
    oled_xy(10, 4); 
    snprintf(text, sizeof(text), "%4d RPM    ", rpm);
    for (int i = 0; text[i] != '\0'; i++) {
        oled_print_char(text[i]);
    }
    oled_xy(10, 7); 
    snprintf(text, sizeof(text), "FUEL%3d%%  ", fuel_level);
    for (int i = 0; text[i] != '\0'; i++) {
        oled_print_char(text[i]);
    }
    oled_xy(65, 1); 
    snprintf(text, sizeof(text), "COOL %3dC ", coolant_temp);
    for (int i = 0; text[i] != '\0'; i++) {
        oled_print_char(text[i]);
    }
    oled_xy(65, 4); 
    snprintf(text, sizeof(text), "THRO %3d%% ", throttle_pos);
    for (int i = 0; text[i] != '\0'; i++) {
        oled_print_char(text[i]);
    }
    oled_xy(65, 7); 
    snprintf(text, sizeof(text), "ELOAD%3d%% ", engine_load);
    for (int i = 0; text[i] != '\0'; i++) {
        oled_print_char(text[i]);
    }
}

void display_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    ESP_LOGI(TAG, "Cleaning Screen");
    uint8_t init_cmds[] = {
        0xAE,       // Display OFF
        0x20, 0x02, 
        0xA8, 0x3F, // Multiplex Ratio (dla ekranu 128x64)
        0xD3, 0x00,
        0x40,       
        0xA1,       
        0xC8,       
        0xDA, 0x12, 
        0x81, 0x7F, 
        0xA4,       
        0xA6,       
        0xD5, 0x80, 
        0x8D, 0x14, 
    };
    for (int i = 0; i < sizeof(init_cmds); i++) {
        oled_send_command(init_cmds[i]);
    }
    for (int page = 0; page < 8; page++) {
        oled_send_command(0xB0 + page);
        oled_send_command(0x00);
        oled_send_command(0x10);
        uint8_t blank[128] = {0};
        oled_send_data(blank, 128);
    }
    oled_send_command(0xAF);
    ESP_LOGI(TAG, "OLED ON");
}