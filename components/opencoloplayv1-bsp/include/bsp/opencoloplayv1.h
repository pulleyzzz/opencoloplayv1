/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

/**
 * @file
 * @brief Board Support Package: coloplay v1
 *
 * Pin map and board capabilities extracted from the coloplay product
 * (closed source, ESP-IDF 5.4) and re-implemented for ESP-IDF 6.1+.
 */

#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_common.h"
#include "bsp/audio.h"
#include "bsp/button.h"
#include "bsp/display.h"
#include "bsp/knob.h"
#include "bsp/led.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_BOARD_COLOPLAY_V1

/**
 * @brief Create the shared I2C0 master bus (idempotent).
 *
 * Called automatically by the audio codec constructors; call it directly
 * when adding more I2C devices.
 */
esp_err_t bsp_i2c_init(void);

/** @brief Get the shared I2C0 bus handle, NULL before bsp_i2c_init(). */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

#define BSP_CAPS_DISPLAY          1
#define BSP_CAPS_BUTTONS          1
#define BSP_CAPS_AUDIO            1
#define BSP_CAPS_AUDIO_SPEAKER    1
#define BSP_CAPS_AUDIO_MIC        1
#define BSP_CAPS_KNOB             1
#define BSP_CAPS_LED              1

/* Shared I2C0 bus: ES8311 codec (only device) */
#define BSP_I2C_PORT              I2C_NUM_0
#define BSP_I2C_SDA               GPIO_NUM_42
#define BSP_I2C_SCL               GPIO_NUM_41

/* GC9A01 round 240x240 LCD, SPI (SIO) on SPI2_HOST */
#define BSP_LCD_SPI_HOST          SPI2_HOST
#define BSP_LCD_MOSI              GPIO_NUM_13
#define BSP_LCD_CLK               GPIO_NUM_14
#define BSP_LCD_CS                GPIO_NUM_12
#define BSP_LCD_DC                GPIO_NUM_11
#define BSP_LCD_RST               GPIO_NUM_45
#define BSP_LCD_BACKLIGHT         GPIO_NUM_18  /* active high, LEDC PWM */

/* Audio: ES8311 codec + I2S0, speaker and microphone */
#define BSP_AUDIO_I2S_MCLK        GPIO_NUM_40
#define BSP_AUDIO_I2S_SCLK        GPIO_NUM_39
#define BSP_AUDIO_I2S_LRCLK       GPIO_NUM_47
#define BSP_AUDIO_I2S_SDOUT       GPIO_NUM_21
#define BSP_AUDIO_I2S_DSIN        GPIO_NUM_38
#define BSP_AUDIO_PA_CTRL         GPIO_NUM_2   /* high = sound on, low = muted */
/* esp_codec_dev expects the 8-bit form of the schematic's 7-bit address 0x18. */
#define BSP_AUDIO_CODEC_I2C_ADDR  (0x18U << 1)

/* Buttons: active low, internal pull-ups (GPIO46 is input-only on S3) */
#define BSP_BUTTON_ACTIVE_LEVEL   0

/* EC11 rotary encoder (knob), decoded by PCNT; no press pin on the encoder
 * itself — the knob press is wired as BSP_BUTTON_KNOB (GPIO46). */
#define BSP_KNOB_GPIO_A           GPIO_NUM_9
#define BSP_KNOB_GPIO_B           GPIO_NUM_10

/* SK6812 LED ring + LED power rail enable */
#define BSP_LED_STRIP_GPIO        GPIO_NUM_48
#define BSP_LED_POWER_ENABLE      GPIO_NUM_8   /* high = LED rail on */

#ifdef __cplusplus
}
#endif
