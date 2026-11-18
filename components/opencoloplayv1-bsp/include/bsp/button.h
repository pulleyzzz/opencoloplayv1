/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#pragma once

#include "esp_err.h"
#include "button_gpio.h"
#include "iot_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Board buttons, all active low with internal pull-ups.
 *
 * BSP_BUTTON_KNOB is the EC11 encoder push button (EKEY in the product).
 */
typedef enum {
    BSP_BUTTON_1 = 0,  /* KEY1, GPIO6  */
    BSP_BUTTON_2,      /* KEY2, GPIO1  */
    BSP_BUTTON_3,      /* KEY3, GPIO4  */
    BSP_BUTTON_4,      /* KEY4, GPIO5  */
    BSP_BUTTON_KNOB,   /* EKEY, GPIO46 (encoder press, input-only pin) */
    BSP_BUTTON_PWR,    /* P,    GPIO15 */
    BSP_BUTTON_NUM,
} bsp_button_t;

/**
 * @brief Create all board buttons as iot_button GPIO devices.
 *
 * @param[out] btn_array    output array, size >= BSP_BUTTON_NUM, indexed by bsp_button_t
 * @param[out] btn_cnt      optional, receives BSP_BUTTON_NUM
 * @param[in]  btn_array_size size of btn_array
 */
esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size);

#ifdef __cplusplus
}
#endif
