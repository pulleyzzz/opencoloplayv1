/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#include "bsp/opencoloplayv1.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "coloplay-Knob";
static pcnt_unit_handle_t s_pcnt_unit;
static bool s_knob_initialized;

/* Same glitch filter width as the product firmware (5 us). */
#define BSP_KNOB_MAX_GLITCH_NS  5000

esp_err_t bsp_knob_init(void)
{
    if (s_knob_initialized) {
        return ESP_OK;
    }
    const pcnt_unit_config_t unit_config = {
        .high_limit = BSP_KNOB_COUNT_HIGH_LIMIT,
        .low_limit = BSP_KNOB_COUNT_LOW_LIMIT,
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_config, &s_pcnt_unit), TAG, "create PCNT unit failed");
    const pcnt_glitch_filter_config_t filter = {
        .max_glitch_ns = BSP_KNOB_MAX_GLITCH_NS,
    };
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter), TAG,
                        "set PCNT glitch filter failed");

    /* EC11 phases are open-collector; the product relies on internal pull-ups. */
    ESP_RETURN_ON_ERROR(gpio_pullup_en(BSP_KNOB_GPIO_A), TAG, "enable knob A pull-up failed");
    ESP_RETURN_ON_ERROR(gpio_pullup_en(BSP_KNOB_GPIO_B), TAG, "enable knob B pull-up failed");

    /* 4x quadrature decode: two channels cross-wired, edges counted on both
     * phases with the other phase as level control (same scheme as the
     * ESP-IDF pcnt rotary_encoder example). Edge polarity verified on real
     * hardware so that clockwise = increasing count. */
    const pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = BSP_KNOB_GPIO_A,
        .level_gpio_num = BSP_KNOB_GPIO_B,
    };
    pcnt_channel_handle_t chan_a = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_a_config, &chan_a), TAG,
                        "create PCNT channel A failed");
    const pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = BSP_KNOB_GPIO_B,
        .level_gpio_num = BSP_KNOB_GPIO_A,
    };
    pcnt_channel_handle_t chan_b = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_b_config, &chan_b), TAG,
                        "create PCNT channel B failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                     PCNT_CHANNEL_EDGE_ACTION_DECREASE), TAG,
                        "set channel A edge action failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG,
                        "set channel A level action failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                     PCNT_CHANNEL_EDGE_ACTION_INCREASE), TAG,
                        "set channel B edge action failed");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE), TAG,
                        "set channel B level action failed");

    /* Watch points at the limits fire the on_reach event when the count
     * wraps, so applications tracking continuous rotation can notice it. */
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit, BSP_KNOB_COUNT_HIGH_LIMIT), TAG,
                        "add high watch point failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit, BSP_KNOB_COUNT_LOW_LIMIT), TAG,
                        "add low watch point failed");

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_pcnt_unit), TAG, "enable PCNT unit failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt_unit), TAG, "clear PCNT count failed");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(s_pcnt_unit), TAG, "start PCNT unit failed");
    s_knob_initialized = true;
    ESP_LOGI(TAG, "EC11 knob initialized: A=%d B=%d (4x decode, glitch filter %d ns)",
             BSP_KNOB_GPIO_A, BSP_KNOB_GPIO_B, BSP_KNOB_MAX_GLITCH_NS);
    return ESP_OK;
}

esp_err_t bsp_knob_get_count(int32_t *count)
{
    ESP_RETURN_ON_FALSE(count, ESP_ERR_INVALID_ARG, TAG, "count output is null");
    ESP_RETURN_ON_FALSE(s_knob_initialized, ESP_ERR_INVALID_STATE, TAG, "knob is not initialized");
    int value = 0;
    ESP_RETURN_ON_ERROR(pcnt_unit_get_count(s_pcnt_unit, &value), TAG, "read PCNT count failed");
    *count = value;
    return ESP_OK;
}

esp_err_t bsp_knob_clear_count(void)
{
    ESP_RETURN_ON_FALSE(s_knob_initialized, ESP_ERR_INVALID_STATE, TAG, "knob is not initialized");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt_unit), TAG, "clear PCNT count failed");
    return ESP_OK;
}

bool bsp_knob_is_initialized(void)
{
    return s_knob_initialized;
}
