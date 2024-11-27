/**
 * @file main.c
 * @author Hariom Agrahari ({hariomagrahri06@gmail.com})
 * @brief 
 * @version 0.1
 * @date 2024-11-27
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "epd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "fonts.h"
#include "ds3231.h"

#define WIFI_SSID "ESPP"
#define WIFI_PASS "Nasa@2023"

#define update_interval 30 // 30 Seconds

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
bool wifi_flag = false;
bool sntp_init_flag = false;

static const char *TAG = "MAIN";
static int s_retry_num = 0;
#define MAXIMUM_RETRY 5

// WiFi event handler
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAXIMUM_RETRY)
        { wifi_flag = false;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
        
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_flag = true;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize WiFi
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap");
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to ap");
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// Time sync notification callback
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

// Initialize SNTP
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "time.google.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
    ESP_LOGI(TAG,"SNTP Initialized...");
    sntp_init_flag = true;
}

// Get formatted time string
void get_formatted_time(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm timeinfo;
    time(&now);

    // Convert to IST (UTC+5:30)
    now += (5 * 60 * 60) + (30 * 60); // Add 5 hours and 30 minutes
    gmtime_r(&now, &timeinfo);

    strftime(buffer, buffer_size, "%d/%m/%y|%H:%M", &timeinfo);
}

// Example WiFi logo bitmap
const uint8_t wifi_logo[] = {
    0x03, 0xfc, 0x00, 0x1f, 0xff, 0x80, 0x3e, 0x07, 0xc0, 0xf0, 0x00, 0xf0,
    0xc1, 0xf8, 0x30, 0x0f, 0xff, 0x00, 0x1e, 0x07, 0x80, 0x18, 0x01, 0x80,
    0x00, 0xf0, 0x00, 0x03, 0xfc, 0x00, 0x03, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x60, 0x00, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00,
    0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0};

    void initialize_rtc(void) {
    esp_err_t ret = ds3231_init(8,9); // Adjust SDA/SCL pins
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DS3231");
    } else {
        ESP_LOGI(TAG, "RTC initialized successfully");
    }
}

void sync_rtc_with_ntp(void) {
    initialize_sntp();

    int retry = 0;
    const int retry_count = 20;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for NTP sync... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    if (sntp_init_flag) {
        time_t now;
        time(&now);
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);

        // Update RTC with synchronized time
        esp_err_t ret = ds3231_set_time(&timeinfo);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "RTC synced with NTP successfully");
        } else {
            ESP_LOGE(TAG, "Failed to sync RTC with NTP");
        }
    } else {
        ESP_LOGW(TAG, "NTP sync failed, proceeding with RTC time");
    }
}

void get_time_from_rtc(char *buffer, size_t buffer_size) {
    struct tm timeinfo;
  

    if (ds3231_get_time(&timeinfo) == ESP_OK) {
        strftime(buffer, buffer_size, "%d/%m/%y|%H:%M", &timeinfo);
        ESP_LOGI(TAG, "RTC Time: %s", buffer);
    } else {
        ESP_LOGE(TAG, "Failed to retrieve time from RTC");
        snprintf(buffer, buffer_size, "RTC Error");
    }
}




void app_main(void) {
    spi_device_handle_t spi_handle;
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
     display_init(&spi_handle);
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize Wi-Fi
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // Initialize RTC
    initialize_rtc();

    // Sync RTC with NTP if Wi-Fi is available
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    if (bits & WIFI_CONNECTED_BIT) {
        sync_rtc_with_ntp();
    } else {
        ESP_LOGW(TAG, "Wi-Fi not available, using RTC time");
        
    }



    // Main loop to display time
    while (1) {
      
        time_t now;
        char strftime_buf[64];
        char qr_string[128];
        time(&now);

              esp_err_t reet;
      time_t unix_time;

        reet = ds3231_get_unix_time(&unix_time);
        if (reet == ESP_OK) {
            ESP_LOGI(TAG,"UNIX_TIME: %lld",unix_time);
        }else{
            ESP_LOGE(TAG,"Failed to get time");
        }

        // Get time
        if (bits & WIFI_CONNECTED_BIT) {
            get_formatted_time(strftime_buf, sizeof(strftime_buf)); // Use NTP synced time
        } else {
            get_time_from_rtc(strftime_buf, sizeof(strftime_buf)); // Use RTC time
        }

        // Display time
        snprintf(qr_string, sizeof(qr_string), "{\"Machine_ID\": \"66b5e59a388d38768bb00a0d\",\"Time\": \"%lld\"}\n", unix_time);
        ESP_LOGI(TAG, "QR String: %s", qr_string);

        EPD_InitBuffers(); // Initialize shared buffers
        EPD_DisplayQRCode_Buffer(qr_string, 30, 5, 3, ECC_LOW, 0);

        if (wifi_flag) {
            EPD_DrawBitmap_Buffer(wifi_logo, 0, 180, 20, 20, 0); // Draw Wi-Fi logo in black
        }
        EPD_DisplayString_Buffer(strftime_buf, 25, 185, &Font16, 1); // Draw time in red
        EPD_DisplayFormattedString_Buffer("{F3}{+B}PL{R}a{B}Y ARENA{-B}{RST}",20,150);
        EPD_UpdateDisplay(&spi_handle); // Update display with buffer content

        EPD_FreeBuffers(); // Free shared buffers
        vTaskDelay(pdMS_TO_TICKS(update_interval*1000));
    }
} 