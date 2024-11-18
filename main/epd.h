#ifndef DISPLAY_H
#define DISPLAY_H

#include "driver/spi_master.h"
#include "pindefs.h"
#include "fonts.h"

#define EPD_WIDTH 200
#define EPD_HEIGHT 200

/**
 * @brief Initializes the E-Paper Display.
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 */
void display_init(spi_device_handle_t *spi);

/**
 * @brief Clears the Display. Can be used to display White Screen
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 */
void display_clear(spi_device_handle_t *spi);

/**
 * @brief Sends the Display to Deep Sleep Mode.
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 */
void display_zzz(spi_device_handle_t *spi);

/**
 * @brief Turns the Whole Display to Black
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 */
void display_black(spi_device_handle_t *spi);

/**
 * @brief Turns the Whole Display to Red.
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 */
void display_red(spi_device_handle_t *spi);

/**
 * @brief Internal Function used to draw a pixel on the Display. Used by other Functions.
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 * @param blackimage Black Image Buffer
 * @param redimage   Red Image Buffer
 */
void Image_Display(spi_device_handle_t *spi, const uint8_t *blackimage, const uint8_t *redimage);

/**
 * @brief Internal Function used to set Pixel onto the Display. Not to be used Directly
 *
 * @param x X Co-ordinate to Start
 * @param y Y Co-ordinate to Start
 * @param black_color To Set up the Black Color
 * @param red_color  To set up the Red Color
 * @param black_buffer Buffer for the black Color
 * @param red_buffer Buffer for the red color
 */
void EPD_SetPixel(uint16_t x, uint16_t y, uint8_t black_color, uint8_t red_color, uint8_t *black_buffer, uint8_t *red_buffer);

/**
 * @brief Function used to display a Particular String Onto the E-Paper Display.
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 * @param str *Pointer to String. String can be Written as "Hello World"
 * @param x The X Co-ordinate to Start Drawing the String
 * @param y The Y Co-ordinate to Start Drawing the String
 * @param font *Pointer to Font which we wanna use. Used as &Font8, &Font12, &Font16, &Font20, &Font24,
 * @param is_red Decides the color of the String. 0 for Black, 1 for Red
 *
 * @note This is not a DMA - buffer based function So only and only String can be displayed at once. The Buffer based functions are
 * there at the end of this file. Buffer Based functions are used to Display multiple things like bitmap, String etc. at one shot
 */
void EPD_DisplayString(spi_device_handle_t *spi, const char *str, uint16_t x, uint16_t y, sFONT *font, uint8_t is_red);

/**
 * @brief Demo of String being displayed onto the E paper Display
 *
 * @param spi_device_handle_t *spi Pointer to the SPI device handle
 *
 * @note Contains Black, Red, Multiline texts
 */
void EPD_DisplayTextDemo(spi_device_handle_t *spi);

/**
 * @brief Displays String with multiple Changes like Color, Font, Bold etc.
 * // Format commands in string:
// {R} - Switch to red
// {B} - Switch to black
// {F1} - Switch to Font16
// {F2} - Switch to Font24
// {+B} - Start bold
// {-B} - End bold
// {RST} - Reset all formatting to default
 *
 * @param spi spi_device_handle_t *spi Pointer to the SPI device handle
 * @param str *Pointer to String.
 * @param x The X Co-ordinate to Start Drawing the String
 * @param y The Y Co-ordinate to Start Drawing the String
 *
 * @note For more Explanation go to EPD_TextFormattingDemo.
 */
void EPD_DisplayFormattedString(spi_device_handle_t *spi, const char *str, uint16_t x, uint16_t y);

/**
 * @brief Demo for the EPD_DisplayFormatted String function
 *
 * @param spi *Pointer to the SPI device handle
 */
void EPD_TextFormattingDemo(spi_device_handle_t *spi);

/**
 * @brief Draws the Bitmap onto the E-Paper Display
 *
 * @param spi *Pointer to the SPI device handle
 * @param bitmap_data *Pointer to bitmap data
 * @param x The X Co-ordinate to Start Drawing the Bitmap
 * @param y The Y Co-ordinate to Start Drawing the Bitmap
 * @param width The Width of the Bitmap. Should be Same as Where it was created
 * @param height The Height of the Bitmap. Should be Same as Where it was created
 * @param color Color of the Bitmap. 0 - Black, 1 - Red
 *
 * @note https://diyusthad.com/image2cpp. This website is used to convert the image to Bitmap Code. In image Settings Canvas Size is basically the width and height of the Bitmap.
 * Scaling we need to keep as Scale to fit, Keeping proportions. Invert Image Colors -> yes, Tick. Output --> Code Output Format Arduino Code rest everything is default.
 *
 * @note For more info go to EPD_BitmapDemo
 */
void EPD_DrawBitmap(spi_device_handle_t *spi, const uint8_t *bitmap_data, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);

/**
 * @brief Demo for displaying a bitmap onto the E-Paper Display
 *
 * @param spi *Pointer to the SPI Devide handle
 */
void EPD_BitmapDemo(spi_device_handle_t *spi);

/**
 * @brief Initalizes the Buffers for Displaying Everything at one Shot like String, bitmaps etc...
 *
 * @note Has to be Initialized after display_init(*spi_device_handle_t) and display_clear(*spi_device_handle_t) and if we wanna update the multiple Fuctions at one shot.
 *
 * @note For more info go to EPD_BufferDemo.
 */
void EPD_InitBuffers(void);

/**
 * @brief Frees the display buffer. Has to used when displaying Multiple Functions at one shot.
 *
 * @note The Code has to be ended with this in order to Free the Buffers for next iterations.
 *
 * @note For more info and example go to EPD_CombinedDemo_Buffer
 */
void EPD_FreeBuffers(void);

/**
 * @brief Draws Bitmap onto the Screen with Buffers. Used to Display Multiple things like Strings, Bitmpas etc onto the Display
 *
 * @param bitmap_data *Pointer to the Bitmap Data
 * @param x The X Co-ordinate to Start Drawing the Bitmap
 * @param y The Y Co-ordinate to Start Drawing the Bitmap
 * @param width The Width of the Bitmap. Should be same as Where it was created
 * @param height The Height of the Bitmap. Should be Same as Where it was created
 * @param color Color of the Bitmap. 0 - Black, 1 - Red
 *
 * @note Has to be used with buffers. For more info and Example go to EPD_CombinedDemo_Buffer.
 */
void EPD_DrawBitmap_Buffer(const uint8_t *bitmap_data, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t color);

/**
 * @brief Used to Display String with Buffers. Used to Display multiple things like Strings, Bitmpas etc onto the Display
 *
 * @param str *Pointer to the Sring. Can be written as "Hello World" as well
 * @param x The X- Coordinate to Start Drawing the Bitmap
 * @param y The Y- Coordinate to Start Drawing the Bitmap
 * @param font the Font to be used while Displaying. Can be used as &Font8, &Font12, &Font16, &Font20, &Font24
 * @param is_red Color of the String. 0 - Black, 1 - Red
 *
 * @note Has to be used with buffers. For more info and Example go to EPD_CombinedDemo_Buffer.
 */
void EPD_DisplayString_Buffer(const char *str, uint16_t x, uint16_t y, sFONT *font, uint8_t is_red);

/**
 * @brief Update the buffers after Different Buffer Based Functions to display Things at one Shot.
 *
 * @param spi *SPI Pointer to SPI Device Handle
 */
void EPD_UpdateDisplay(spi_device_handle_t *spi);

/**
 * @brief Demo to Display Buffer based Functions. Used to Display Multiple things like Strings, Bitmpas etc onto the Display at One Shot
 *
 * @param spi *SPI Pointer to SPI Device Handle
 */
void EPD_CombinedDemo_Buffer(spi_device_handle_t *spi);

/**
 * @brief Displays String with multiple Changes like Color, Font, Bold etc with Buffers. 
 * // Format commands in string:
// {R} - Switch to red
// {B} - Switch to black
// {F1} - Switch to Font16
// {F2} - Switch to Font24
// {+B} - Start bold
// {-B} - End bold
// {RST} - Reset all formatting to default
 *
 * @param spi spi_device_handle_t *spi Pointer to the SPI device handle
 * @param str *Pointer to String.
 * @param x The X Co-ordinate to Start Drawing the String
 * @param y The Y Co-ordinate to Start Drawing the String
 *
 * @note For more Explanation go to EPD_TextFormattingDemo_Buffer.
 */
void EPD_DisplayFormattedString_Buffer(const char *str, uint16_t x, uint16_t y);


/**
 * @brief Demo For Combined Formatting Demo . Used to Display Multiple things like Strings With diffrent Fonts, Color, Boldness etc, Bitmpas etc onto the Display at One Shot with buffers 
 * 
 * @param spi *SPI Pointer to SPI Device Handler 
 */
void EPD_CombinedFormattingDemo_Buffer(spi_device_handle_t *spi);

#endif // DISPLAY_H
