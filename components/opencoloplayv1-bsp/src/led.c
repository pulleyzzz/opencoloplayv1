/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#include "bsp/opencoloplayv1.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "coloplay-LED";
static led_strip_handle_t s_strip;
static bool s_led_power_on;

static esp_err_t configure_output(gpio_num_t pin, int level)
{
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(pin), ESP_ERR_INVALID_ARG, TAG,
                        "invalid output GPIO: %d", pin);
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(pin, level), TAG, "preset GPIO%d level failed", pin);
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "configure GPIO%d failed", pin);
    return ESP_OK;
}

esp_err_t bsp_led_init(void)
{
    if (s_strip) {
        return ESP_OK;
    }
    /* Enable the LED power rail before talking to the strip. */
    ESP_RETURN_ON_ERROR(bsp_led_set_power(true), TAG, "enable LED power rail failed");

    const led_strip_config_t strip_config = {
        .strip_gpio_num = BSP_LED_STRIP_GPIO,
        .max_leds = BSP_LED_NUM,
        .led_model = LED_MODEL_SK6812,
        .flags = {
            .invert_out = false,
        },
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, /* 10 MHz tick */
    };
    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip), TAG,
                        "create SK6812 RMT strip failed");
    ESP_RETURN_ON_ERROR(led_strip_clear(s_strip), TAG, "clear SK6812 strip failed");
    ESP_LOGI(TAG, "SK6812 ring initialized: %d LEDs on GPIO%d, power rail GPIO%d",
             BSP_LED_NUM, BSP_LED_STRIP_GPIO, BSP_LED_POWER_ENABLE);
    return ESP_OK;
}

esp_err_t bsp_led_set_power(bool on)
{
    if (s_led_power_on == on) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(configure_output(BSP_LED_POWER_ENABLE, on ? 1 : 0), TAG,
                        "configure LED power rail failed");
    s_led_power_on = on;
    ESP_LOGI(TAG, "LED power rail GPIO%d %s", BSP_LED_POWER_ENABLE, on ? "on" : "off");
    return ESP_OK;
}

esp_err_t bsp_led_set_pixel(uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
    ESP_RETURN_ON_FALSE(s_strip, ESP_ERR_INVALID_STATE, TAG, "LED ring is not initialized");
    ESP_RETURN_ON_FALSE(index < BSP_LED_NUM, ESP_ERR_INVALID_ARG, TAG, "pixel index out of range");
    return led_strip_set_pixel(s_strip, index, red, green, blue);
}

esp_err_t bsp_led_set_all(uint32_t red, uint32_t green, uint32_t blue)
{
    ESP_RETURN_ON_FALSE(s_strip, ESP_ERR_INVALID_STATE, TAG, "LED ring is not initialized");
    for (uint32_t i = 0; i < BSP_LED_NUM; ++i) {
        ESP_RETURN_ON_ERROR(led_strip_set_pixel(s_strip, i, red, green, blue), TAG,
                            "set pixel %d failed", i);
    }
    return led_strip_refresh(s_strip);
}

esp_err_t bsp_led_clear(void)
{
    ESP_RETURN_ON_FALSE(s_strip, ESP_ERR_INVALID_STATE, TAG, "LED ring is not initialized");
    return led_strip_clear(s_strip);
}

esp_err_t bsp_led_refresh(void)
{
    ESP_RETURN_ON_FALSE(s_strip, ESP_ERR_INVALID_STATE, TAG, "LED ring is not initialized");
    return led_strip_refresh(s_strip);
}

led_strip_handle_t bsp_led_get_strip(void)
{
    return s_strip;
}
