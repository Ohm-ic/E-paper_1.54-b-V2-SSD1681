/**
 * @file main.c
 * @author Hariom Agrahari ({hariomagrahri06@gmail.com})
 * @brief Main File to Driver E paper Display. Waveshare E-Paper Display 1.54"V2, 200*200, SSD1681 Driver IC, (Red,Black,White)
 * @version 0.1
 * @date 2024-11-19
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "epd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "fonts.h"

// Example WiFi logo bitmap
const uint8_t wifi_logo[] = {
    0x03, 0xfc, 0x00, 0x1f, 0xff, 0x80, 0x3e, 0x07, 0xc0, 0xf0, 0x00, 0xf0,
    0xc1, 0xf8, 0x30, 0x0f, 0xff, 0x00, 0x1e, 0x07, 0x80, 0x18, 0x01, 0x80,
    0x00, 0xf0, 0x00, 0x03, 0xfc, 0x00, 0x03, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x60, 0x00, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00,
    0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0};

void app_main(void)
{
    spi_device_handle_t spi_handle;
    display_init(&spi_handle);
    display_clear(&spi_handle);
    // EPD_CombinedDemo_Buffer(&spi_handle);
    //  EPD_CombinedFormattingDemo_Buffer(&spi_handle);

    EPD_InitBuffers(); // Initialize shared buffers

    // Example QR code content
    const char *data = "First Line\nSecond Line\nThired Line\nHello World this is me the QR code.";

    // Display QR code
    EPD_DisplayQRCode_Buffer(data, 30, 5, 3, ECC_LOW, 0);
    EPD_DrawBitmap_Buffer(wifi_logo, 0, 180, 20, 20, 0);             // Draw WiFi logo in black
    EPD_DisplayString_Buffer("15:04|19-11-24", 35, 185, &Font16, 1); // Draw text in red
    EPD_DisplayFormattedString_Buffer("{F3}{+B}PL{R}a{B}Y ARENA{-B}{RST}",20,150);

    // Update the display with buffer content
    EPD_UpdateDisplay(&spi_handle);

    EPD_FreeBuffers(); // Free shared buffers
    display_zzz(&spi_handle);

    while (1)
    {
        vTaskDelay(portMAX_DELAY);
    }

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