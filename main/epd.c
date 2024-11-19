#include "epd.h"
#include "qrcode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fonts.h"
#include <string.h>
#include "esp_system.h"
#include "string.h"

#define TAG "EPD"

// Display resolution
#define EPD_WIDTH 200
#define EPD_HEIGHT 200

// Display dimensions in bytes (200x200 pixels, 8 pixels per byte)
#define DISPLAY_WIDTH_BYTES (200 / 8)
#define DISPLAY_HEIGHT 200
#define BUFFER_SIZE (DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT)

static uint8_t *shared_black_buffer = NULL;
static uint8_t *shared_red_buffer = NULL;

uint8_t frame_buffer[EPD_WIDTH * EPD_HEIGHT / 8];

static void send_command(spi_device_handle_t spi, uint8_t cmd)
{
    gpio_set_level(EPD_DC_PIN, 0); // Command mode
    gpio_set_level(EPD_CS_PIN, 0); // Select device

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = (void *)0};

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
    }

    gpio_set_level(EPD_CS_PIN, 1); // Deselect device
}

static void send_data(spi_device_handle_t spi, uint8_t data)
{
    gpio_set_level(EPD_DC_PIN, 1); // Data mode
    gpio_set_level(EPD_CS_PIN, 0); // Select device

    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
        .user = (void *)1};

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
    }

    gpio_set_level(EPD_CS_PIN, 1); // Deselect device
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
    while (gpio_get_level(EPD_BUSY_PIN) == 1)
    { // BUSY is high when device is busy
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
        .max_transfer_sz = 5000 // Reduced size
    };

    // Initialize SPI device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000, // Start with 1 MHz to be safe
        .mode = 0,
        .spics_io_num = -1, // CS pin handled manually
        .queue_size = 1,    // Minimum queue size
        .flags = 0,
        .pre_cb = NULL};

    // Initialize SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    // Add device to the SPI bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, spi);
    ESP_ERROR_CHECK(ret);

    if (*spi == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI device");
        return;
    }

    // Hardware reset
    epd_reset();

    // Wait for the electronic paper IC to release the idle signal
    WaitUntilIdle();

    // Software reset
    send_command(*spi, 0x12); // SWRESET
    WaitUntilIdle();

    // Initial settings sequence for Waveshare 1.54" B
    send_command(*spi, 0x01); // Driver output control
    // send_data(*spi, (EPD_HEIGHT-1) & 0xFF);
    send_data(*spi, 0xC7);
    //  send_data(*spi, ((EPD_HEIGHT-1) >> 8) & 0xFF);
    send_data(*spi, 0x00); // GD = 0; SM = 0; TB = 0;
    send_data(*spi, 0x01);

    send_command(*spi, 0x11); // Data entry mode
    send_data(*spi, 0x01);    // X increment; Y increment

    send_command(*spi, 0x44); // Set RAM-X address start/end position
    send_data(*spi, 0x00);
    send_data(*spi, 0x18); // 0x18 -> 24

    send_command(*spi, 0x45); // Set RAM-Y address start/end position
    send_data(*spi, 0xC7);
    send_data(*spi, 0x00);
    send_data(*spi, 0x00); // 0xC3 -> 195
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
    Width = (EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;

    send_command(*spi, 0x24);
    for (uint16_t j = 0; j < Height; j++)
    {
        for (uint16_t i = 0; i < Width; i++)
        {
            send_data(*spi, 0xFF);
        }
    }

    send_command(*spi, 0x26);
    for (uint16_t j = 0; j < Height; j++)
    {
        for (uint16_t i = 0; i < Width; i++)
        {
            send_data(*spi, 0x00);
        }
    }

    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    send_command(*spi, 0x20);
    WaitUntilIdle();
    ESP_LOGI(TAG, "The Display is cleared");
}

void Image_Display(spi_device_handle_t *spi, const uint8_t *blackimage, const uint8_t *redimage)
{
    uint16_t Width, Height;
    Width = (EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;

    send_command(*spi, 0x24); // Write Black and White image to RAM
    for (uint32_t i = 0; i < Width * Height; i++)
    {
        send_data(*spi, blackimage[i]);
    }

    send_command(*spi, 0x26); // Write Red image to RAM
    for (uint32_t i = 0; i < Width * Height; i++)
    {
        send_data(*spi, ~redimage[i]); // Invert red data
    }

    // Display update control
    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    // Activate display update sequence
    send_command(*spi, 0x20);
    WaitUntilIdle();

    ESP_LOGI(TAG, "The Image had been Displayed");
}

void display_zzz(spi_device_handle_t *spi)
{
    send_command(*spi, 0x10);
    send_data(*spi, 0x01);
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "The display is in Deep Sleep");
}

void display_red(spi_device_handle_t *spi)
{
    uint16_t Width, Height;
    Width = (EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;

    send_command(*spi, 0x24);
    for (uint16_t j = 0; j < Height; j++)
    {
        for (uint16_t i = 0; i < Width; i++)
        {
            send_data(*spi, 0x00);
        }
    }

    send_command(*spi, 0x26);
    for (uint16_t j = 0; j < Height; j++)
    {
        for (uint16_t i = 0; i < Width; i++)
        {
            send_data(*spi, 0xFF);
        }
    }

    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    send_command(*spi, 0x20);
    WaitUntilIdle();
    ESP_LOGI(TAG, "The Display is turned Red");
}

void display_black(spi_device_handle_t *spi)
{
    uint16_t Width, Height;
    Width = (EPD_WIDTH % 8 == 0) ? (EPD_WIDTH / 8) : (EPD_WIDTH / 8 + 1);
    Height = EPD_HEIGHT;

    send_command(*spi, 0x24);
    for (uint16_t j = 0; j < Height; j++)
    {
        for (uint16_t i = 0; i < Width; i++)
        {
            send_data(*spi, 0x00);
        }
    }

    send_command(*spi, 0x26);
    for (uint16_t j = 0; j < Height; j++)
    {
        for (uint16_t i = 0; i < Width; i++)
        {
            send_data(*spi, 0x00);
        }
    }

    send_command(*spi, 0x22);
    send_data(*spi, 0xF7);
    send_command(*spi, 0x20);
    WaitUntilIdle();
    ESP_LOGI(TAG, "The Display is turned Black");
}

void EPD_SetPixel(uint16_t x, uint16_t y, uint8_t black_color, uint8_t red_color, uint8_t *black_buffer, uint8_t *red_buffer)
{
    if (x >= EPD_WIDTH || y >= EPD_HEIGHT)
    {
        return; // Out of bounds check
    }

    uint16_t byte_idx = (y * EPD_WIDTH + x) / 8;
    uint8_t bit_idx = 7 - (x % 8); // MSB first

    // Set black pixel
    if (black_color)
    {
        black_buffer[byte_idx] &= ~(1 << bit_idx); // Set to black (0)
    }
    else
    {
        black_buffer[byte_idx] |= (1 << bit_idx); // Set to white (1)
    }

    // Set red pixel
    if (red_color)
    {
        red_buffer[byte_idx] &= ~(1 << bit_idx); // Set to red (0)
    }
    else
    {
        red_buffer[byte_idx] |= (1 << bit_idx); // Set to no-red (1)
    }
}

void EPD_DisplayString(spi_device_handle_t *spi, const char *str, uint16_t x, uint16_t y,
                       sFONT *font, uint8_t is_red)
{
    uint8_t *black_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);
    uint8_t *red_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);

    if (!black_buffer || !red_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for text buffers");
        return;
    }

    // Initialize buffers to white
    memset(black_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);
    memset(red_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);

    uint16_t char_offset = 0;
    const char *p = str;

    while (*p != '\0')
    {
        // Check for newline character
        if (*p == '\n')
        {
            y += font->Height;
            char_offset = 0;
            p++;
            continue;
        }

        // Skip if character is not printable
        if (*p < 32 || *p > 126)
        {
            p++;
            continue;
        }

        // Get character bitmap
        const uint8_t *char_bitmap = &font->table[(*p - 32) * font->Height * ((font->Width + 7) / 8)];

        // Draw character pixel by pixel
        for (uint16_t row = 0; row < font->Height; row++)
        {
            for (uint16_t col = 0; col < font->Width; col++)
            {
                uint8_t byte = char_bitmap[row * ((font->Width + 7) / 8) + (col / 8)];
                if (byte & (0x80 >> (col % 8)))
                {
                    uint16_t pixel_x = x + char_offset + col;
                    uint16_t pixel_y = y + row;

                    if (is_red)
                    {
                        EPD_SetPixel(pixel_x, pixel_y, 0, 1, black_buffer, red_buffer);
                    }
                    else
                    {
                        EPD_SetPixel(pixel_x, pixel_y, 1, 0, black_buffer, red_buffer);
                    }
                }
            }
        }

        char_offset += font->Width;
        // Check if we need to move to next line
        if (x + char_offset >= EPD_WIDTH - font->Width)
        {
            y += font->Height;
            char_offset = 0;
        }
        p++;
    }

    // Display the text
    Image_Display(spi, black_buffer, red_buffer);

    // Free the buffers
    free(black_buffer);
    free(red_buffer);

    ESP_LOGI(TAG, "String displayed successfully");
}

// Example usage function to demonstrate different text styles
void EPD_DisplayTextDemo(spi_device_handle_t *spi)
{
    // Display black text using different fonts
    EPD_DisplayString(spi, "Hello World!", 10, 10, &Font16, 0); // Small black text
    vTaskDelay(pdMS_TO_TICKS(500));

    EPD_DisplayString(spi, "Red Text\nRed Text Second Line\nRed Tect  ", 10, 40, &Font24, 1); // Larger red text
    vTaskDelay(pdMS_TO_TICKS(10000));

    // Display multi-line text
    const char *multiline = "First Line\nSecond Line\nThird Line";
    EPD_DisplayString(spi, multiline, 10, 80, &Font16, 0);

    WaitUntilIdle();
}

typedef struct
{
    uint8_t is_bold : 1;
    uint8_t is_red : 1;
    sFONT *font;
} TextStyle;

// Format commands in string:
// {R} - Switch to red
// {B} - Switch to black
// {F1} - Switch to Font16
// {F2} - Switch to Font24
// {+B} - Start bold
// {-B} - End bold
// {RST} - Reset all formatting to default

void EPD_DisplayFormattedString(spi_device_handle_t *spi, const char *str, uint16_t x, uint16_t y)
{
    if (!str)
        return;

    uint8_t *black_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);
    uint8_t *red_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);

    if (!black_buffer || !red_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for text buffers");
        return;
    }

    // Initialize buffers to white
    memset(black_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);
    memset(red_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);

    // Initialize default style
    TextStyle current_style = {
        .is_bold = 0,
        .is_red = 0,
        .font = &Font16 // Default font
    };

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    const char *p = str;

    while (*p)
    {
        // Check for formatting commands
        if (*p == '{')
        {
            if (strncmp(p, "{R}", 3) == 0)
            {
                current_style.is_red = 1;
                p += 3;
                continue;
            }
            else if (strncmp(p, "{B}", 3) == 0)
            {
                current_style.is_red = 0;
                p += 3;
                continue;
            }
            else if (strncmp(p, "{F1}", 4) == 0)
            {
                current_style.font = &Font16;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{F2}", 4) == 0)
            {
                current_style.font = &Font24;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{+B}", 4) == 0)
            {
                current_style.is_bold = 1;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{-B}", 4) == 0)
            {
                current_style.is_bold = 0;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{RST}", 5) == 0)
            {
                current_style.is_bold = 0;
                current_style.is_red = 0;
                current_style.font = &Font16;
                p += 5;
                continue;
            }
        }

        // Handle newline
        if (*p == '\n')
        {
            cursor_x = x;
            cursor_y += current_style.font->Height + 2; // Add small line spacing
            p++;
            continue;
        }

        // Skip if character is not printable
        if (*p < 32 || *p > 126)
        {
            p++;
            continue;
        }

        // Get character bitmap
        const uint8_t *char_bitmap = &current_style.font->table[(*p - 32) *
                                                                current_style.font->Height *
                                                                ((current_style.font->Width + 7) / 8)];

        // Draw character
        for (uint16_t row = 0; row < current_style.font->Height; row++)
        {
            for (uint16_t col = 0; col < current_style.font->Width; col++)
            {
                uint8_t byte = char_bitmap[row * ((current_style.font->Width + 7) / 8) + (col / 8)];
                if (byte & (0x80 >> (col % 8)))
                {
                    uint16_t pixel_x = cursor_x + col;
                    uint16_t pixel_y = cursor_y + row;

                    // Draw normal pixel
                    EPD_SetPixel(pixel_x, pixel_y,
                                 !current_style.is_red,
                                 current_style.is_red,
                                 black_buffer, red_buffer);

                    // If bold, draw additional pixel
                    if (current_style.is_bold)
                    {
                        EPD_SetPixel(pixel_x + 1, pixel_y,
                                     !current_style.is_red,
                                     current_style.is_red,
                                     black_buffer, red_buffer);
                    }
                }
            }
        }

        // Move cursor
        cursor_x += current_style.font->Width + (current_style.is_bold ? 1 : 0);

        // Check if we need to wrap to next line
        if (cursor_x >= EPD_WIDTH - current_style.font->Width)
        {
            cursor_x = x;
            cursor_y += current_style.font->Height + 2;
        }

        p++;
    }

    // Display the text
    Image_Display(spi, black_buffer, red_buffer);

    // Free the buffers
    free(black_buffer);
    free(red_buffer);

    ESP_LOGI(TAG, "Formatted string displayed successfully");
}

// Example usage function to demonstrate formatting
void EPD_TextFormattingDemo(spi_device_handle_t *spi)
{
    // Example string with multiple formats
    const char *formatted_text =
        "{F2}Big Text\n"
        "{F1}Normal size {R}in red{B} and black\n"
        "{+B}This is bold{-B} and this is not\n"
        "{R}{+B}Bold red text{RST}\n"
        "Back to normal";

    EPD_DisplayFormattedString(spi, formatted_text, 10, 10);
    WaitUntilIdle();
}

void EPD_DrawBitmap(spi_device_handle_t *spi, const uint8_t *bitmap_data,
                    uint16_t x, uint16_t y,
                    uint16_t width, uint16_t height,
                    uint8_t color)
{
    // Allocate buffers for black and red data
    uint8_t *black_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);
    uint8_t *red_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);

    if (!black_buffer || !red_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for bitmap buffers");
        return;
    }

    // Initialize buffers to white
    memset(black_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);
    memset(red_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);

    // Calculate bytes per row in the bitmap
    uint16_t bytes_per_row = (width + 7) / 8;

    // Process each pixel in the bitmap
    for (uint16_t row = 0; row < height; row++)
    {
        for (uint16_t col = 0; col < width; col++)
        {
            // Calculate the byte index and bit position in the bitmap data
            uint16_t byte_idx = row * bytes_per_row + (col / 8);
            uint8_t bit_pos = 7 - (col % 8); // MSB first

            // Check if the current pixel is set in the bitmap
            if (bitmap_data[byte_idx] & (1 << bit_pos))
            {
                // Calculate the absolute position on the display
                uint16_t disp_x = x + col;
                uint16_t disp_y = y + row;

                // Check if the pixel is within display bounds
                if (disp_x < EPD_WIDTH && disp_y < EPD_HEIGHT)
                {
                    if (color == 0)
                    { // Black
                        EPD_SetPixel(disp_x, disp_y, 1, 0, black_buffer, red_buffer);
                    }
                    else
                    { // Red
                        EPD_SetPixel(disp_x, disp_y, 0, 1, black_buffer, red_buffer);
                    }
                }
            }
        }
    }

    // Display the bitmap
    Image_Display(spi, black_buffer, red_buffer);

    // Free the buffers
    free(black_buffer);
    free(red_buffer);

    ESP_LOGI(TAG, "Bitmap displayed successfully");
}

// Example usage function
void EPD_BitmapDemo(spi_device_handle_t *spi)
{
    // Example WiFi logo bitmap data
    const uint8_t wifi_logo[] = {
        0x03, 0xfc, 0x00, 0x1f, 0xff, 0x80, 0x3e, 0x07, 0xc0, 0xf0, 0x00, 0xf0,
        0xc1, 0xf8, 0x30, 0x0f, 0xff, 0x00, 0x1e, 0x07, 0x80, 0x18, 0x01, 0x80,
        0x00, 0xf0, 0x00, 0x03, 0xfc, 0x00, 0x03, 0x0c, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x60, 0x00, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00,
        0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0};

    // Draw the WiFi logo in black
    EPD_DrawBitmap(spi, wifi_logo, 0, 180, 20, 20, 0); // Black color =0, Red Color = 1

    // Wait for display to update
    WaitUntilIdle();
}

// Initialize shared buffers
void EPD_InitBuffers(void)
{
    if (shared_black_buffer == NULL)
    {
        shared_black_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);
    }
    if (shared_red_buffer == NULL)
    {
        shared_red_buffer = heap_caps_calloc(EPD_WIDTH * EPD_HEIGHT / 8, 1, MALLOC_CAP_DMA);
    }

    if (!shared_black_buffer || !shared_red_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate shared buffers");
        return;
    }

    // Initialize buffers to white
    memset(shared_black_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);
    memset(shared_red_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 8);
}

// Free shared buffers
void EPD_FreeBuffers(void)
{
    if (shared_black_buffer)
    {
        free(shared_black_buffer);
        shared_black_buffer = NULL;
    }
    if (shared_red_buffer)
    {
        free(shared_red_buffer);
        shared_red_buffer = NULL;
    }
}

// Modified EPD_DrawBitmap to use shared buffers
void EPD_DrawBitmap_Buffer(const uint8_t *bitmap_data,
                           uint16_t x, uint16_t y,
                           uint16_t width, uint16_t height,
                           uint8_t color)
{
    if (!shared_black_buffer || !shared_red_buffer)
    {
        ESP_LOGE(TAG, "Shared buffers not initialized");
        return;
    }

    uint16_t bytes_per_row = (width + 7) / 8;

    for (uint16_t row = 0; row < height; row++)
    {
        for (uint16_t col = 0; col < width; col++)
        {
            uint16_t byte_idx = row * bytes_per_row + (col / 8);
            uint8_t bit_pos = 7 - (col % 8);

            if (bitmap_data[byte_idx] & (1 << bit_pos))
            {
                uint16_t disp_x = x + col;
                uint16_t disp_y = y + row;

                if (disp_x < EPD_WIDTH && disp_y < EPD_HEIGHT)
                {
                    if (color == 0)
                    { // Black
                        EPD_SetPixel(disp_x, disp_y, 1, 0, shared_black_buffer, shared_red_buffer);
                    }
                    else
                    { // Red
                        EPD_SetPixel(disp_x, disp_y, 0, 1, shared_black_buffer, shared_red_buffer);
                    }
                }
            }
        }
    }
}

// Modified EPD_DisplayString to use shared buffers
void EPD_DisplayString_Buffer(const char *str, uint16_t x, uint16_t y,
                              sFONT *font, uint8_t is_red)
{
    if (!shared_black_buffer || !shared_red_buffer)
    {
        ESP_LOGE(TAG, "Shared buffers not initialized");
        return;
    }

    uint16_t char_offset = 0;
    const char *p = str;

    while (*p != '\0')
    {
        if (*p == '\n')
        {
            y += font->Height;
            char_offset = 0;
            p++;
            continue;
        }

        if (*p < 32 || *p > 126)
        {
            p++;
            continue;
        }

        const uint8_t *char_bitmap = &font->table[(*p - 32) * font->Height * ((font->Width + 7) / 8)];

        for (uint16_t row = 0; row < font->Height; row++)
        {
            for (uint16_t col = 0; col < font->Width; col++)
            {
                uint8_t byte = char_bitmap[row * ((font->Width + 7) / 8) + (col / 8)];
                if (byte & (0x80 >> (col % 8)))
                {
                    uint16_t pixel_x = x + char_offset + col;
                    uint16_t pixel_y = y + row;

                    if (is_red)
                    {
                        EPD_SetPixel(pixel_x, pixel_y, 0, 1, shared_black_buffer, shared_red_buffer);
                    }
                    else
                    {
                        EPD_SetPixel(pixel_x, pixel_y, 1, 0, shared_black_buffer, shared_red_buffer);
                    }
                }
            }
        }

        char_offset += font->Width;
        if (x + char_offset >= EPD_WIDTH - font->Width)
        {
            y += font->Height;
            char_offset = 0;
        }
        p++;
    }
}

// Function to update display with current buffer contents
void EPD_UpdateDisplay(spi_device_handle_t *spi)
{
    if (!shared_black_buffer || !shared_red_buffer)
    {
        ESP_LOGE(TAG, "Shared buffers not initialized");
        return;
    }

    Image_Display(spi, shared_black_buffer, shared_red_buffer);
    WaitUntilIdle();
}

// Example usage function showing combined bitmap and text
void EPD_CombinedDemo_Buffer(spi_device_handle_t *spi)
{

    // Initialize shared buffers
    EPD_InitBuffers();

    // Example WiFi logo bitmap
    const uint8_t wifi_logo[] = {
        0x03, 0xfc, 0x00, 0x1f, 0xff, 0x80, 0x3e, 0x07, 0xc0, 0xf0, 0x00, 0xf0,
        0xc1, 0xf8, 0x30, 0x0f, 0xff, 0x00, 0x1e, 0x07, 0x80, 0x18, 0x01, 0x80,
        0x00, 0xf0, 0x00, 0x03, 0xfc, 0x00, 0x03, 0x0c, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x60, 0x00, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00,
        0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0};

    // Draw bitmap and text to shared buffer
    EPD_DrawBitmap_Buffer(wifi_logo, 0, 180, 20, 20, 0);          // Draw WiFi logo in black
    EPD_DisplayString_Buffer("WiFi Status", 35, 185, &Font16, 1); // Draw text in red

    // Update display with combined content
    EPD_UpdateDisplay(spi);

    // Free shared buffers
    EPD_FreeBuffers();
}

void EPD_DisplayFormattedString_Buffer(const char *str, uint16_t x, uint16_t y)
{
    if (!str || !shared_black_buffer || !shared_red_buffer)
    {
        ESP_LOGE(TAG, "Invalid input or shared buffers not initialized");
        return;
    }

    // Initialize default style
    TextStyle current_style = {
        .is_bold = 0,
        .is_red = 0,
        .font = &Font16 // Default font
    };

    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    const char *p = str;

    while (*p)
    {
        // Check for formatting commands
        if (*p == '{')
        {
            if (strncmp(p, "{R}", 3) == 0)
            {
                current_style.is_red = 1;
                p += 3;
                continue;
            }
            else if (strncmp(p, "{B}", 3) == 0)
            {
                current_style.is_red = 0;
                p += 3;
                continue;
            }
            else if (strncmp(p, "{F1}", 4) == 0)
            {
                current_style.font = &Font12;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{F2}", 4) == 0)
            {
                current_style.font = &Font16;
                p += 4;
                continue;
            }
            else if(strncmp(p, "{F3}", 4) == 0)
            {
                current_style.font = &Font20;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{+B}", 4) == 0)
            {
                current_style.is_bold = 1;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{-B}", 4) == 0)
            {
                current_style.is_bold = 0;
                p += 4;
                continue;
            }
            else if (strncmp(p, "{RST}", 5) == 0)
            {
                current_style.is_bold = 0;
                current_style.is_red = 0;
                current_style.font = &Font16;
                p += 5;
                continue;
            }
        }

        // Handle newline
        if (*p == '\n')
        {
            cursor_x = x;
            cursor_y += current_style.font->Height + 2; // Add small line spacing
            p++;
            continue;
        }

        // Skip if character is not printable
        if (*p < 32 || *p > 126)
        {
            p++;
            continue;
        }

        // Get character bitmap
        const uint8_t *char_bitmap = &current_style.font->table[(*p - 32) *
                                                                current_style.font->Height *
                                                                ((current_style.font->Width + 7) / 8)];

        // Draw character
        for (uint16_t row = 0; row < current_style.font->Height; row++)
        {
            for (uint16_t col = 0; col < current_style.font->Width; col++)
            {
                uint8_t byte = char_bitmap[row * ((current_style.font->Width + 7) / 8) + (col / 8)];
                if (byte & (0x80 >> (col % 8)))
                {
                    uint16_t pixel_x = cursor_x + col;
                    uint16_t pixel_y = cursor_y + row;

                    // Draw normal pixel
                    EPD_SetPixel(pixel_x, pixel_y,
                                 !current_style.is_red,
                                 current_style.is_red,
                                 shared_black_buffer, shared_red_buffer);

                    // If bold, draw additional pixel
                    if (current_style.is_bold)
                    {
                        EPD_SetPixel(pixel_x + 1, pixel_y,
                                     !current_style.is_red,
                                     current_style.is_red,
                                     shared_black_buffer, shared_red_buffer);
                    }
                }
            }
        }

        // Move cursor
        cursor_x += current_style.font->Width + (current_style.is_bold ? 1 : 0);

        // Check if we need to wrap to next line
        if (cursor_x >= EPD_WIDTH - current_style.font->Width)
        {
            cursor_x = x;
            cursor_y += current_style.font->Height + 2;
        }

        p++;
    }
}

// Example usage showing how to combine formatted text with other buffer-based operations
void EPD_CombinedFormattingDemo_Buffer(spi_device_handle_t *spi)
{
    // Initialize shared buffers
    EPD_InitBuffers();

    // Draw a bitmap
    const uint8_t wifi_logo[] = {
        0x03, 0xfc, 0x00, 0x1f, 0xff, 0x80, 0x3e, 0x07, 0xc0, 0xf0, 0x00, 0xf0,
        0xc1, 0xf8, 0x30, 0x0f, 0xff, 0x00, 0x1e, 0x07, 0x80, 0x18, 0x01, 0x80,
        0x00, 0xf0, 0x00, 0x03, 0xfc, 0x00, 0x03, 0x0c, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x60, 0x00, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x60, 0x00,
        0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xf0};
    EPD_DrawBitmap_Buffer(wifi_logo, 0, 180, 20, 20, 1); // Draw logo in Red

    // Add formatted text
    const char *formatted_text =
        "{F2}Status: {R}Online{B}\n"
        "{F1}Signal: {+B}Strong{-B}\n"
        "{R}Connected{RST}";
    EPD_DisplayFormattedString_Buffer(formatted_text, 0, 0);

    // Add regular text
    EPD_DisplayString_Buffer("Last updated: 12:00", 0, 100, &Font16, 0);

    // Update the display with all content
    EPD_UpdateDisplay(spi);

    // Free the shared buffers
    EPD_FreeBuffers();
}

// Displays QR Code onto the Display
void EPD_DisplayQRCode_Buffer(const char *data, 
                               float x, float y, 
                               float size, uint8_t ecc, 
                               uint8_t color)
{
    if (!data || !shared_black_buffer || !shared_red_buffer) {
        ESP_LOGE(TAG, "Invalid input or shared buffers not initialized");
        return;
    }

    QRCode qrcode;
    uint8_t qrcodeBytes[qrcode_getBufferSize(7)]; // Currently Version 7. Supports Upto not known...

    // Initialize the QR code
    if (qrcode_initText(&qrcode, qrcodeBytes, 7, ecc, data) < 0) {
        ESP_LOGE(TAG, "Failed to initialize QR code");
        return;
    }

    // Draw QR code onto the buffer
    for (uint16_t row = 0; row < qrcode.size; row++) {
        for (uint16_t col = 0; col < qrcode.size; col++) {
            // Check if the QR code pixel is black
            if (qrcode_getModule(&qrcode, col, row)) {
                // Calculate scaled positions and iterate over them
                uint16_t start_x = (uint16_t)(x + col * size);
                uint16_t start_y = (uint16_t)(y + row * size);
                uint16_t end_x = (uint16_t)(x + (col + 1) * size);
                uint16_t end_y = (uint16_t)(y + (row + 1) * size);

                for (uint16_t pixel_x = start_x; pixel_x < end_x; pixel_x++) {
                    for (uint16_t pixel_y = start_y; pixel_y < end_y; pixel_y++) {
                        // Ensure the pixel is within bounds
                        if (pixel_x < EPD_WIDTH && pixel_y < EPD_HEIGHT) {
                            if (color == 0) { // Black
                                EPD_SetPixel(pixel_x, pixel_y, 1, 0, shared_black_buffer, shared_red_buffer);
                            } else { // Red
                                EPD_SetPixel(pixel_x, pixel_y, 0, 1, shared_black_buffer, shared_red_buffer);
                            }
                        }
                    }
                }
            }
        }
    }

    ESP_LOGI(TAG, "QR code displayed successfully");
}

// Above this everything works Smoothly------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------->