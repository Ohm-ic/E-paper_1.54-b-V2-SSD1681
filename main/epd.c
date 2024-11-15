#include "epd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fonts.h"
#include <string.h>

#define TAG "EPD"

// Display resolution
#define EPD_WIDTH   200
#define EPD_HEIGHT  200

uint8_t frame_buffer[EPD_WIDTH * EPD_HEIGHT / 8];

static void send_command(spi_device_handle_t spi, uint8_t cmd)
{
    gpio_set_level(EPD_DC_PIN, 0);  // Command mode
    gpio_set_level(EPD_CS_PIN, 0);  // Select device
    
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = (void *)0
    };
    
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
    }
    
    gpio_set_level(EPD_CS_PIN, 1);  // Deselect device
}

static void send_data(spi_device_handle_t spi, uint8_t data)
{
    gpio_set_level(EPD_DC_PIN, 1);  // Data mode
    gpio_set_level(EPD_CS_PIN, 0);  // Select device
    
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
        .user = (void *)1
    };
    
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
    }
    
    gpio_set_level(EPD_CS_PIN, 1);  // Deselect device
}

static void epd_reset(void)
{
    gpio_set_level(EPD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(EPD_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(EPD_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void WaitUntilIdle(void)
{
    while(gpio_get_level(EPD_BUSY_PIN) == 1) {  // BUSY is high when device is busy
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void display_init(spi_device_handle_t *spi)
{
    esp_err_t ret;
    
    // Configure GPIO pins
    gpio_reset_pin(EPD_RST_PIN);
    gpio_reset_pin(EPD_DC_PIN);
    gpio_reset_pin(EPD_CS_PIN);
    gpio_reset_pin(EPD_BUSY_PIN);
    
    gpio_set_direction(EPD_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EPD_DC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EPD_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(EPD_BUSY_PIN, GPIO_MODE_INPUT);
    
    gpio_set_level(EPD_CS_PIN, 1); // Deselect epaper
    
    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = EPD_DIN_PIN,
        .miso_io_num = -1,
        .sclk_io_num = EPD_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 5000    // Reduced size
    };
    
    // Initialize SPI device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1*1000*1000,    // Start with 1 MHz to be safe
        .mode = 0,
        .spics_io_num = -1,               // CS pin handled manually
        .queue_size = 1,                  // Minimum queue size
        .flags = 0,
        .pre_cb = NULL
    };
    
    // Initialize SPI bus
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    
    // Add device to the SPI bus
    ret = spi_bus_add_device(SPI3_HOST, &devcfg, spi);
    ESP_ERROR_CHECK(ret);
    
    if (*spi == NULL) {
        ESP_LOGE(TAG, "Failed to initialize SPI device");
        return;
    }
    
    // Hardware reset
    epd_reset();
    
    // Wait for the electronic paper IC to release the idle signal
    WaitUntilIdle();
    
    // Software reset
    send_command(*spi, 0x12);  // SWRESET
    WaitUntilIdle();
    
    // Initial settings sequence for Waveshare 1.54" B
    send_command(*spi, 0x01); // Driver output control
    //send_data(*spi, (EPD_HEIGHT-1) & 0xFF);
    send_data(*spi, 0xC7);
  //  send_data(*spi, ((EPD_HEIGHT-1) >> 8) & 0xFF);
    send_data(*spi, 0x00); // GD = 0; SM = 0; TB = 0;
    send_data(*spi, 0x01); 
    
    send_command(*spi, 0x11); // Data entry mode
    send_data(*spi, 0x01);    // X increment; Y increment
    
    send_command(*spi, 0x44); // Set RAM-X address start/end position
    send_data(*spi, 0x00);
    send_data(*spi, 0x18);    // 0x18 -> 24
    
    send_command(*spi, 0x45); // Set RAM-Y address start/end position
    send_data(*spi, 0xC7);
    send_data(*spi, 0x00);
    send_data(*spi, 0x00);    // 0xC3 -> 195
    send_data(*spi, 0x00);
    
    send_command(*spi, 0x3C); // BorderWavefrom
    send_data(*spi, 0x05);    
    
    // send_command(*spi, 0x21); // Display update control
    // send_data(*spi, 0x00);
    // send_data(*spi, 0x80);
    
    send_command(*spi, 0x18); // Read built-in temperature sensor
    send_data(*spi, 0x80);
    
    send_command(*spi, 0x4E); // Set RAM X address counter
    send_data(*spi, 0x00);
    
    send_command(*spi, 0x4F); // Set RAM Y address counter
    send_data(*spi, 0xC7);
    send_data(*spi, 0x00);
    
    WaitUntilIdle();
    
    ESP_LOGI(TAG, "E-Paper initialized successfully");
}

void display_clear(spi_device_handle_t *spi)
{
    uint16_t Width, Height;
    Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;

    
    send_command(*spi, 0x24);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            send_data(*spi, 0xFF);
        }
    }
    
    send_command(*spi, 0x26);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            send_data(*spi, 0x00);
        }
    }
    
    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    send_command(*spi, 0x20);
    WaitUntilIdle();
    ESP_LOGI(TAG,"The Display is cleared");
}

void Image_Display(spi_device_handle_t *spi, const uint8_t *blackimage, const uint8_t *redimage)
{
    uint16_t Width, Height;
    Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;
    
    send_command(*spi, 0x24);   // Write Black and White image to RAM
    for(uint32_t i = 0; i < Width * Height; i++) {
        send_data(*spi, blackimage[i]);
    }
    
    send_command(*spi, 0x26);    // Write Red image to RAM
    for(uint32_t i = 0; i < Width * Height; i++) {
        send_data(*spi, ~redimage[i]);    // Invert red data
    }
   
    // Display update control
    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    // Activate display update sequence
    send_command(*spi, 0x20);
    WaitUntilIdle();
}

void display_zzz(spi_device_handle_t *spi){
    send_command(*spi, 0x10);
    send_data(*spi, 0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG,"The display is in Deep Sleep");
}


void display_red(spi_device_handle_t *spi)
{
    uint16_t Width, Height;
    Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;

    
    send_command(*spi, 0x24);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            send_data(*spi, 0x00);
        }
    }
    
    send_command(*spi, 0x26);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            send_data(*spi, 0xFF);
        }
    }
    
    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    send_command(*spi, 0x20);
    WaitUntilIdle();
    ESP_LOGI(TAG,"The Display is turned Red");
}


void display_black(spi_device_handle_t *spi)
{
    uint16_t Width, Height;
    Width = (EPD_WIDTH % 8 == 0)? (EPD_WIDTH / 8 ): (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;

    
    send_command(*spi, 0x24);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            send_data(*spi, 0x00);
        }
    }
    
    send_command(*spi, 0x26);
    for (uint16_t j = 0; j < Height; j++) {
        for (uint16_t i = 0; i < Width; i++) {
            send_data(*spi, 0x00);
        }
    }
    
    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    send_command(*spi, 0x20);
    WaitUntilIdle();
    ESP_LOGI(TAG,"The Display is turned Black");
}



// Above this everything works Smoothly------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------->

void EPD_SetPixel(spi_device_handle_t *spi,  uint16_t x, uint16_t y, uint8_t color) {



    if (x >= EPD_WIDTH || y >= EPD_HEIGHT) {
        return; // Out of bounds
    }
    // Assuming frame buffer is a 2D array or a byte array
    // Replace this with the actual implementation based on your library
    uint16_t byte_index = (y * EPD_WIDTH + x) / 8;
    uint8_t bit_index = x % 8;

    if (color) {
        frame_buffer[byte_index] |= (0x80 >> bit_index); // Set pixel
    } else {
        frame_buffer[byte_index] &= ~(0x80 >> bit_index); // Clear pixel
    }
}



void EPD_WriteText(spi_device_handle_t *spi, const char *text, uint16_t x, uint16_t y, sFONT *font) {
    uint16_t char_width = font->Width;
    uint16_t char_height = font->Height;
    const uint8_t *char_bitmap;
    uint16_t start_x = x;
    uint8_t first_char = 32;  // ASCII for space ' '
    uint8_t last_char = 126; // ASCII for '~'

    while (*text) {
        if (*text < first_char || *text > last_char) {
            // Skip unsupported characters
            text++;
            continue;
        }

        // Locate the character's bitmap in the font table
        char_bitmap = &font->table[(*text - first_char) * char_width * ((char_height + 7) / 8)];

        // Render each pixel of the character
        for (uint16_t row = 0; row < char_height; row++) {
            for (uint16_t col = 0; col < char_width; col++) {
                uint8_t byte = char_bitmap[row * ((char_width + 7) / 8) + (col / 8)];
                uint8_t bit = byte & (0x80 >> (col % 8));
                if (bit) {
                    EPD_SetPixel(*spi, x + col, y + row, 1); // Black pixel
                } else {
                    EPD_SetPixel(*spi, x + col, y + row, 0); // White pixel
                }
            }
        }

        // Move to the next character position
        x += char_width;
        if (x + char_width > EPD_WIDTH) {
            // Wrap to the next line
            x = start_x;
            y += char_height;
            if (y + char_height > EPD_HEIGHT) {
                // Stop rendering if no space left
                break;
            }
        }
        text++;
    }

    ESP_LOGI(TAG, "Text has been Written");
}

