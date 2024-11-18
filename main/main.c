#include "epd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "fonts.h"

void app_main(void)
{
    spi_device_handle_t spi_handle;
    display_init(&spi_handle);
    display_clear(&spi_handle);
    //EPD_CombinedDemo_Buffer(&spi_handle);
    EPD_CombinedFormattingDemo_Buffer(&spi_handle);
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