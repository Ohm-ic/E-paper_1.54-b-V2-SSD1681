/**
 * @file ds3231.c
 * @brief Implementation file for DS3231 RTC module
 */

#include "ds3231.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "DS3231";
static i2c_port_t i2c_port = I2C_NUM_0;

// Convert BCD to decimal
static uint8_t bcd_to_dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
}

// Convert decimal to BCD
static uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// Initialize I2C
static esp_err_t i2c_master_init(int sda_pin, int scl_pin) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    
    esp_err_t err = i2c_param_config(i2c_port, &conf);
    if (err != ESP_OK) return err;
    
    return i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
}

esp_err_t ds3231_init(int sda_pin, int scl_pin) {
    esp_err_t err = i2c_master_init(sda_pin, scl_pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C master");
        return err;
    }
    
    ESP_LOGI(TAG, "DS3231 initialized successfully");
    return ESP_OK;
}

esp_err_t ds3231_set_time(const struct tm *time_info) {
    uint8_t data[7];
    data[0] = dec_to_bcd(time_info->tm_sec);
    data[1] = dec_to_bcd(time_info->tm_min);
    data[2] = dec_to_bcd(time_info->tm_hour);
    data[3] = dec_to_bcd(time_info->tm_wday + 1);
    data[4] = dec_to_bcd(time_info->tm_mday);
    data[5] = dec_to_bcd(time_info->tm_mon + 1);
    data[6] = dec_to_bcd(time_info->tm_year - 100);  // Convert from 2024 to 24

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);  // Start from register 0
    i2c_master_write(cmd, data, 7, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set time on DS3231");
    }
    return ret;
}

esp_err_t ds3231_get_time(struct tm *time_info) {
    uint8_t data[7];
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[6], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        time_info->tm_sec = bcd_to_dec(data[0]);
        time_info->tm_min = bcd_to_dec(data[1]);
        time_info->tm_hour = bcd_to_dec(data[2]);
        time_info->tm_wday = bcd_to_dec(data[3]) - 1;
        time_info->tm_mday = bcd_to_dec(data[4]);
        time_info->tm_mon = bcd_to_dec(data[5] & 0x1F) - 1;
        time_info->tm_year = bcd_to_dec(data[6]) ;  // Currrently it is 24,25,26 like that. If wanna Convert the year to 2024,2025,2026 then you have to do + 100 in the end...
    } else {
        ESP_LOGE(TAG, "Failed to read time from DS3231");
    }
    return ret;
}

esp_err_t ds3231_get_unix_time(time_t *unix_time) {
    struct tm timeinfo;
    esp_err_t ret = ds3231_get_time(&timeinfo);
    if (ret == ESP_OK) {
        *unix_time = mktime(&timeinfo);
    }
    return ret;
}

esp_err_t ds3231_set_unix_time(time_t unix_time) {
    struct tm timeinfo;
    localtime_r(&unix_time, &timeinfo);
    return ds3231_set_time(&timeinfo);
}