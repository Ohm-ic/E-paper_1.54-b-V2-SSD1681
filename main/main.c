#include "epd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "fonts.h"

void app_main(void)
{
    spi_device_handle_t spi;

    // Initialize the display
    display_init(&spi);

    while (1)
    {
        display_clear(&spi);
        display_black(&spi);
        vTaskDelay(pdMS_TO_TICKS(5000));
        display_red(&spi);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    // Above this everything works Smoothly------------------------------------------------------------------------------------------------------>

    // EPD_WriteText(&spi,"Hello Display", 10, 20, &Font8);
}
