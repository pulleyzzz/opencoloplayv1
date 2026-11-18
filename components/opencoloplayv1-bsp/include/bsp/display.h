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
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lv_adapter.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_H_RES                 240
#define BSP_LCD_V_RES                 240
#define BSP_LCD_BITS_PER_PIXEL         16

/**
 * @brief Display rotation angle, applied through the GC9A01 MADCTL register.
 */
typedef enum {
    BSP_DISPLAY_ROTATE_0   = 0,
    BSP_DISPLAY_ROTATE_90  = 90,
    BSP_DISPLAY_ROTATE_180 = 180,
    BSP_DISPLAY_ROTATE_270 = 270,
} bsp_display_rotation_t;

#define BSP_LCD_ROTATION_DEFAULT      BSP_DISPLAY_ROTATE_0

/*
 * Mounting orientation of the GC9A01 glass at BSP_DISPLAY_ROTATE_0, expressed
 * as MADCTL swap/mirror flags. Adjust these three values if a board revision
 * mounts the panel differently.
 */
#define BSP_LCD_BASE_SWAP_XY          false
#define BSP_LCD_BASE_MIRROR_X         false
#define BSP_LCD_BASE_MIRROR_Y         false

typedef struct {
    bsp_display_rotation_t rotation;
    uint16_t buffer_height;      /**< 0 = adapter default */
    uint32_t task_stack_size;    /**< 0 = adapter default */
} bsp_display_config_t;

#define BSP_DISPLAY_DEFAULT_CONFIG() {                 \
    .rotation = BSP_LCD_ROTATION_DEFAULT,              \
    .buffer_height = 0,                                \
    .task_stack_size = 0,                              \
}

/**
 * @brief Create the GC9A01 panel without starting LVGL.
 *
 * Initializes the SPI bus, panel IO (with the coloplay vendor init sequence),
 * resets and enables the panel, and turns the backlight on at the
 * CONFIG_BSP_GC9A01_BACKLIGHT_DEFAULT_DUTY level.
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel);

/** @brief Create the panel (if needed) and start LVGL through esp_lvgl_adapter. */
lv_display_t *bsp_display_start(void);
lv_display_t *bsp_display_start_with_config(const bsp_display_config_t *config);
lv_display_t *bsp_display_get(void);
esp_lcd_panel_handle_t bsp_display_get_panel(void);
esp_lcd_panel_io_handle_t bsp_display_get_panel_io(void);

/** @brief Rotate the display at runtime (MADCTL swap/mirror). */
esp_err_t bsp_display_set_rotation(bsp_display_rotation_t rotation);
bsp_display_rotation_t bsp_display_get_rotation(void);

/** @brief Set backlight brightness in percent (LEDC PWM on GPIO18). */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/** @brief Display On (0x29) / Sleep Out (0x11) and resume LVGL. */
esp_err_t bsp_display_on(void);

/** @brief Pause LVGL, Display Off (0x28) and Sleep In (0x10). */
esp_err_t bsp_display_off(void);

/** @brief LVGL lock helpers, safe to call before bsp_display_start(). */
bool bsp_display_lock(int32_t timeout_ms);
void bsp_display_unlock(void);

#ifdef __cplusplus
}
#endif
