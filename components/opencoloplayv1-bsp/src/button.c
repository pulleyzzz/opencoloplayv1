/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#include "bsp/opencoloplayv1.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "coloplay-Button";

/* KEY1..KEY4, EKEY (encoder press) and P, all active low with pull-ups.
 * Order matches the bsp_button_t enum. */
static const gpio_num_t s_button_gpios[BSP_BUTTON_NUM] = {
    [BSP_BUTTON_1]    = GPIO_NUM_6,
    [BSP_BUTTON_2]    = GPIO_NUM_1,
    [BSP_BUTTON_3]    = GPIO_NUM_4,
    [BSP_BUTTON_4]    = GPIO_NUM_5,
    [BSP_BUTTON_KNOB] = GPIO_NUM_46,
    [BSP_BUTTON_PWR]  = GPIO_NUM_15,
};

esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size)
{
    ESP_RETURN_ON_FALSE(btn_array && btn_array_size >= BSP_BUTTON_NUM, ESP_ERR_INVALID_ARG, TAG,
                        "button array is too small");
    const button_config_t common = {0};
    for (int i = 0; i < BSP_BUTTON_NUM; ++i) {
        const button_gpio_config_t gpio_config = {
            .gpio_num = s_button_gpios[i],
            .active_level = BSP_BUTTON_ACTIVE_LEVEL,
            .disable_pull = false,
        };
        ESP_RETURN_ON_ERROR(iot_button_new_gpio_device(&common, &gpio_config, &btn_array[i]), TAG,
                            "create button %d (GPIO%d) failed", i, s_button_gpios[i]);
    }
    if (btn_cnt) {
        *btn_cnt = BSP_BUTTON_NUM;
    }
    return ESP_OK;
}
