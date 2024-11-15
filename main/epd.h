// #ifndef DISPLAY_H
// #define DISPLAY_H

// #include "driver/spi_master.h"
// #include "pindefs.h"



// // Initialize the display
// void display_init(spi_device_handle_t *spi);

// // Display "Hello World" on the screen
// void display_hello_world(spi_device_handle_t spi);

// // Clear the display
// void display_clear(spi_device_handle_t spi);

// #endif // DISPLAY_H

// epd.h
#ifndef DISPLAY_H
#define DISPLAY_H

#include "driver/spi_master.h"
#include "pindefs.h"
#include "fonts.h"

#define EPD_WIDTH   200
#define EPD_HEIGHT  200
#define BUFFER_SIZE 40000/8;



void display_init(spi_device_handle_t *spi);   
void display_clear(spi_device_handle_t *spi);
void EPD_WriteText(spi_device_handle_t *spi, const char *text, uint16_t x, uint16_t y, sFONT *font);
void display_zzz(spi_device_handle_t *spi);
void Image_Display(spi_device_handle_t *spi, const uint8_t *blackimage, const uint8_t *redimage);
void display_black(spi_device_handle_t *spi);
void display_red(spi_device_handle_t *spi);


#endif // DISPLAY_H









