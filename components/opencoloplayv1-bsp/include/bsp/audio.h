/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#pragma once

#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The codec wiring is mono: the product firmware runs I2S in mono-left mode,
 * so the defaults match it. Pass a custom i2s_std_config_t to bsp_audio_init()
 * to override.
 */
#define BSP_AUDIO_DEFAULT_SAMPLE_RATE     16000
#define BSP_AUDIO_DEFAULT_BITS_PER_SAMPLE 16
#define BSP_AUDIO_DEFAULT_CHANNELS        1

/**
 * @brief Initialize I2S0 (TX+RX, master) and the codec data interface.
 *
 * Safe to call multiple times; the first call wins unless the channels are
 * deinitialized. Called automatically by the codec device constructors.
 *
 * @param[in] i2s_config NULL for board defaults (16 kHz / 16 bit / mono-left)
 */
esp_err_t bsp_audio_init(const i2s_std_config_t *i2s_config);

/** @brief Get the ES8311 speaker device (esp_codec_dev), creating it on first use. */
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);

/** @brief Get the ES8311 microphone device (esp_codec_dev), creating it on first use. */
esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);

#ifdef __cplusplus
}
#endif
