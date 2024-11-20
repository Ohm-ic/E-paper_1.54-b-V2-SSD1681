/**
 * @file main.c
 * @author Hariom Agrahari ({hariomagrahri06@gmail.com})
 * @brief Main File for the QR Code Authentication System
 * @version 0.1
 * @date 2024-11-19
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


#define WIFI_SSID "ESPP"
#define WIFI_PASS "Nasa@2023"

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
bool wifi_flag = false;

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
        {
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
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
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

    strftime(buffer, buffer_size, "%d/%m/24|%H:%M", &timeinfo);
}

// Example WiFi logo bitmap
const uint8_t wifi_logo[] = {
    0x03, 0xfc, 0x00, 0x1f, 0xff, 0x80, 0x3e, 0x07, 0xc0, 0xf0, 0x00, 0xf0,
    0xc1, 0xf8, 0x30, 0x0f, 0xff, 0x00, 0x1e, 0x07, 0x80, 0x18, 0x01, 0x80,
    0x00, 0xf0, 0x00, 0x03, 0xfc, 0x00, 0x03, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x60, 0x00, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00,
    0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0};

void app_main(void)
{

    // Initialize NVS
    spi_device_handle_t spi_handle;
    esp_err_t ret = nvs_flash_init();
    display_init(&spi_handle);
    display_clear(&spi_handle);
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // Initialize SNTP
    initialize_sntp();

    // Wait for time to be synchronized
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    // Main loop to display time
    while (1)
    {
        time_t now;
        char strftime_buf[64];
        char qr_string[128];

        
        time(&now);

        // Print Unix Epoch time
        ESP_LOGI(TAG, "Unix Epoch time: %lld", (long long)now);
        snprintf(qr_string, sizeof(qr_string), "{\"Machine_ID\": \"66b5e59a388d38768bb00a0d\",\"Unix_time\": %lld}\n", (long long)now);
        ESP_LOGI(TAG, "Unix time as String: %s", qr_string);

        // Print formatted IST time
        get_formatted_time(strftime_buf, sizeof(strftime_buf));
        ESP_LOGI(TAG, "Indian Standard Time: %s", strftime_buf);

        EPD_InitBuffers(); // Initialize shared buffers
        EPD_DisplayQRCode_Buffer(qr_string, 30, 5, 3, ECC_LOW, 0);

        if(wifi_flag)
        {
        EPD_DrawBitmap_Buffer(wifi_logo, 0, 180, 20, 20, 0);         // Draw WiFi logo in black. 0 = black, 1 = white
        }
        EPD_DisplayString_Buffer(strftime_buf, 25, 185, &Font16, 1); // Draw text in red, Displays Time and Date
        // EPD_DisplayFormattedString_Buffer("{F3}{+B}PL{R}a{B}Y ARENA{-B}{RST}",20,150);

        // Update the display with buffer content
        EPD_UpdateDisplay(&spi_handle);

        EPD_FreeBuffers(); // Free shared buffers

        vTaskDelay(pdMS_TO_TICKS(30000));
    }

    EPD_InitBuffers(); // Initialize shared buffers

    // Above this everything works Smoothly------------------------------------------------------------------------------------------------------>
}

// #include "epd.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "esp_system.h"
// #include "string.h"
// #include "driver/spi_master.h"
// #include "fonts.h"

// #define TAG "MAIN"

// // Display dimensions in bytes (200x200 pixels, 8 pixels per byte)
// #define DISPLAY_WIDTH_BYTES (200/8)
// #define DISPLAY_HEIGHT 200
// #define BUFFER_SIZE (DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT)

// // Create a simple pattern: a frame with diagonal line
// void create_test_pattern(uint8_t *black_buffer, uint8_t *red_buffer) {
//     // Clear buffers
//     memset(black_buffer, 0xFF, BUFFER_SIZE);  // Set all to white
//     memset(red_buffer, 0xFF, BUFFER_SIZE);    // Set all to white

//     // Create black frame
//     for(int i = 0; i < DISPLAY_WIDTH_BYTES; i++) {
//         // Top and bottom borders
//         black_buffer[i] = 0x00;  // First row
//         black_buffer[BUFFER_SIZE - DISPLAY_WIDTH_BYTES + i] = 0x00;  // Last row
//     }

//     // Left and right borders
//     for(int i = 0; i < DISPLAY_HEIGHT; i++) {
//         // Set first and last pixel in each row
//         black_buffer[i * DISPLAY_WIDTH_BYTES] &= ~0x80;  // Left border
//         black_buffer[i * DISPLAY_WIDTH_BYTES + (DISPLAY_WIDTH_BYTES-1)] &= ~0x01;  // Right border
//     }

//     // Create diagonal red line
//     for(int i = 0; i < DISPLAY_HEIGHT; i++) {
//         int byte_pos = (i * DISPLAY_WIDTH_BYTES) + (i / 8);
//         int bit_pos = 7 - (i % 8);
//         red_buffer[byte_pos] &= ~(1 << bit_pos);
//     }
// }

// void app_main(void)
// {
//     spi_device_handle_t spi_handle;

//     // Initialize buffers
//     uint8_t *black_buffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);
//     uint8_t *red_buffer = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);

//     if (!black_buffer || !red_buffer) {
//         ESP_LOGE(TAG, "Failed to allocate memory for display buffers");
//         return;
//     }

//     // Initialize display
//     ESP_LOGI(TAG, "Initializing display...");
//     display_init(&spi_handle);
//     vTaskDelay(pdMS_TO_TICKS(100));  // Short delay after init

//     // Clear the display first
//     ESP_LOGI(TAG, "Clearing display...");
//     display_clear(&spi_handle);
//     vTaskDelay(pdMS_TO_TICKS(1000));  // Wait for clear to complete

//     // Create test pattern
//     ESP_LOGI(TAG, "Creating test pattern...");
//     create_test_pattern(black_buffer, red_buffer);

//     // Display the pattern
//     ESP_LOGI(TAG, "Displaying test pattern...");
//     Image_Display(&spi_handle, black_buffer, red_buffer);
//     vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for display to update

//     // Put display to sleep
//     ESP_LOGI(TAG, "Putting display to sleep...");
//     display_zzz(&spi_handle);

//     // Free memory
//     free(black_buffer);
//     free(red_buffer);

//     ESP_LOGI(TAG, "Demo completed!");

//     while(1) {
//         vTaskDelay(portMAX_DELAY);  // Keep the task alive
//     }
// }