/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "led_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Number of SK6812 LEDs in the ring. */
#define BSP_LED_NUM             14

/**
 * @brief Initialize the SK6812 LED ring (RMT backend) and enable the LED
 * power rail (GPIO8).
 *
 * The ring starts cleared (all off).
 */
esp_err_t bsp_led_init(void);

/** @brief Enable or disable the LED power rail (GPIO8, high = on). */
esp_err_t bsp_led_set_power(bool on);

/**
 * @brief Set one pixel's RGB value (call bsp_led_refresh() to latch).
 *
 * @param index 0 .. BSP_LED_NUM-1
 */
esp_err_t bsp_led_set_pixel(uint32_t index, uint32_t red, uint32_t green, uint32_t blue);

/** @brief Set every pixel to the same RGB value (call bsp_led_refresh() to latch). */
esp_err_t bsp_led_set_all(uint32_t red, uint32_t green, uint32_t blue);

/** @brief Clear the buffer and turn all pixels off immediately. */
esp_err_t bsp_led_clear(void);

/** @brief Latch the buffer to the ring. */
esp_err_t bsp_led_refresh(void);

/** @brief Get the led_strip handle for advanced use (transparency, waits). */
led_strip_handle_t bsp_led_get_strip(void);

#ifdef __cplusplus
}
#endif
