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

#ifdef __cplusplus
extern "C" {
#endif

/** PCNT counter limits; the knob count wraps between them. */
#define BSP_KNOB_COUNT_HIGH_LIMIT   300
#define BSP_KNOB_COUNT_LOW_LIMIT   -300

/**
 * @brief Initialize the EC11 rotary encoder (PCNT, 4x quadrature decode).
 *
 * Enables internal pull-ups on both phases and applies a glitch filter
 * (5 us, same as the product firmware).
 */
esp_err_t bsp_knob_init(void);

/**
 * @brief Read the encoder count since the last clear/wrap.
 *
 * One physical detent of the EC11 is 4 counts with 4x decoding.
 *
 * @param[out] count signed position, positive = clockwise
 */
esp_err_t bsp_knob_get_count(int32_t *count);

/** @brief Clear the encoder count to zero. */
esp_err_t bsp_knob_clear_count(void);

/** @brief Whether bsp_knob_init() has completed successfully. */
bool bsp_knob_is_initialized(void);

#ifdef __cplusplus
}
#endif
