/**
 * @file ds3231.h
 * @brief Header file for DS3231 RTC module
 */

#ifndef DS3231_H
#define DS3231_H

#include <time.h>
#include "esp_err.h"

// DS3231 I2C Address and Register Definitions
#define DS3231_ADDR 0x68
#define DS3231_SECONDS 0x00
#define DS3231_MINUTES 0x01
#define DS3231_HOURS   0x02
#define DS3231_DAY     0x03
#define DS3231_DATE    0x04
#define DS3231_MONTH   0x05
#define DS3231_YEAR    0x06

// Function declarations
esp_err_t ds3231_init(int sda_pin, int scl_pin);
esp_err_t ds3231_set_time(const struct tm *time_info);
esp_err_t ds3231_get_time(struct tm *time_info);
esp_err_t ds3231_get_unix_time(time_t *unix_time);
esp_err_t ds3231_set_unix_time(time_t unix_time);

#endif // DS3231_H