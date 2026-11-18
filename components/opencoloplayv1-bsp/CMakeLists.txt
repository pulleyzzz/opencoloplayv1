set(BSP_SRCS
    "src/opencoloplayv1.c"
    "src/display.c"
    "src/audio.c"
    "src/button.c"
    "src/knob.c"
    "src/led.c"
)

set(BSP_REQUIRES
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_ledc
    esp_driver_pcnt
    esp_driver_rmt
    esp_driver_spi
    esp_lcd
    esp_timer
    espressif__esp_lcd_gc9a01
    espressif__esp_codec_dev
    espressif__button
    led_strip
    espressif__esp_lvgl_adapter
)

idf_component_register(SRCS ${BSP_SRCS}
                       INCLUDE_DIRS "include"
                       REQUIRES ${BSP_REQUIRES})
