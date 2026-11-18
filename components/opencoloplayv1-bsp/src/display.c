/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#include <stdint.h>
#include "bsp/opencoloplayv1.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_gc9a01.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "coloplay-LCD";
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static lv_display_t *s_display;
static bsp_display_config_t s_config;
static bool s_config_valid;
static bool s_spi_bus_initialized;
static bool s_backlight_initialized;
static bool s_display_sleeping;

/*
 * Vendor init sequence for the coloplay GC9A01_V1 panel, verbatim from the
 * product firmware (components/lvgl/lvgl_tft/GC9A01.c, GC9A01_V1 table).
 * MADCTL (0x36), COLMOD (0x3A), SLPOUT (0x11) and DISPON (0x29) are left out:
 * the esp_lcd_gc9a01 driver sends those itself from the panel config.
 * 0x21 (display inversion on) is required for correct colors on this glass.
 */
static const gc9a01_lcd_init_cmd_t s_vendor_init[] = {
    {0xEF, (uint8_t[]){0}, 0, 0},
    {0xEB, (uint8_t[]){0x14}, 1, 0},
    {0xFE, (uint8_t[]){0}, 0, 0},
    {0xEF, (uint8_t[]){0}, 0, 0},
    {0xEB, (uint8_t[]){0x14}, 1, 0},
    {0x84, (uint8_t[]){0x40}, 1, 0},
    {0x85, (uint8_t[]){0xFF}, 1, 0},
    {0x86, (uint8_t[]){0xFF}, 1, 0},
    {0x87, (uint8_t[]){0xFF}, 1, 0},
    {0x88, (uint8_t[]){0x0A}, 1, 0},
    {0x89, (uint8_t[]){0x21}, 1, 0},
    {0x8A, (uint8_t[]){0x00}, 1, 0},
    {0x8B, (uint8_t[]){0x80}, 1, 0},
    {0x8C, (uint8_t[]){0x01}, 1, 0},
    {0x8D, (uint8_t[]){0x01}, 1, 0},
    {0x8E, (uint8_t[]){0xFF}, 1, 0},
    {0x8F, (uint8_t[]){0xFF}, 1, 0},
    {0xB6, (uint8_t[]){0x00, 0x20}, 2, 0},
    {0x90, (uint8_t[]){0x08, 0x08, 0x08, 0x08}, 4, 0},
    {0xBD, (uint8_t[]){0x06}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x60, 0x01, 0x04}, 3, 0},
    {0xC3, (uint8_t[]){0x13}, 1, 0},
    {0xC4, (uint8_t[]){0x13}, 1, 0},
    {0xC9, (uint8_t[]){0x22}, 1, 0},
    {0xBE, (uint8_t[]){0x11}, 1, 0},
    {0xE1, (uint8_t[]){0x10, 0x0E}, 2, 0},
    {0xDF, (uint8_t[]){0x21, 0x0C, 0x02}, 3, 0},
    {0xF0, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6, 0},
    {0xF1, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6, 0},
    {0xF2, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6, 0},
    {0xF3, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6, 0},
    {0xED, (uint8_t[]){0x1B, 0x0B}, 2, 0},
    {0xAE, (uint8_t[]){0x77}, 1, 0},
    {0xCD, (uint8_t[]){0x63}, 1, 0},
    {0x70, (uint8_t[]){0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9, 0},
    {0xE8, (uint8_t[]){0x34}, 1, 0},
    {0x62, (uint8_t[]){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12, 0},
    {0x63, (uint8_t[]){0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12, 0},
    {0x64, (uint8_t[]){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7, 0},
    {0x66, (uint8_t[]){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10, 0},
    {0x67, (uint8_t[]){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10, 0},
    {0x74, (uint8_t[]){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7, 0},
    {0x98, (uint8_t[]){0x3E, 0x07}, 2, 0},
    {0x35, (uint8_t[]){0}, 0, 0},
    {0x21, (uint8_t[]){0}, 0, 0},
};

/* Backlight PWM: same setup as the product (LEDC low-speed, timer 0,
 * channel 0, 10-bit resolution, 5 kHz, active high). */
#define BSP_BACKLIGHT_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define BSP_BACKLIGHT_LEDC_TIMER     LEDC_TIMER_0
#define BSP_BACKLIGHT_LEDC_CHANNEL   LEDC_CHANNEL_0
#define BSP_BACKLIGHT_LEDC_RES       LEDC_TIMER_10_BIT
#define BSP_BACKLIGHT_LEDC_FREQ_HZ   5000

static esp_err_t backlight_init(void)
{
    if (s_backlight_initialized) {
        return ESP_OK;
    }
    const ledc_timer_config_t timer = {
        .speed_mode = BSP_BACKLIGHT_LEDC_MODE,
        .duty_resolution = BSP_BACKLIGHT_LEDC_RES,
        .timer_num = BSP_BACKLIGHT_LEDC_TIMER,
        .freq_hz = BSP_BACKLIGHT_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "configure backlight timer failed");
    const uint32_t max_duty = (1U << 10) - 1U;
    const ledc_channel_config_t channel = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = BSP_BACKLIGHT_LEDC_MODE,
        .channel = BSP_BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BSP_BACKLIGHT_LEDC_TIMER,
        .duty = max_duty * CONFIG_BSP_GC9A01_BACKLIGHT_DEFAULT_DUTY / 100,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "configure backlight channel failed");
    s_backlight_initialized = true;
    return ESP_OK;
}

/*
 * Orientation is tracked as the 2x2 matrix mapping logical axes onto panel
 * axes, so a mounting base orientation and a requested rotation compose
 * before being reduced back to MADCTL swap/mirror flags.
 */
typedef struct {
    int8_t m00;
    int8_t m01;
    int8_t m10;
    int8_t m11;
} orientation_matrix_t;

static orientation_matrix_t matrix_from_flags(bool swap_xy, bool mirror_x, bool mirror_y)
{
    int8_t axis_x_x = 1;
    int8_t axis_x_y = 0;
    int8_t axis_y_x = 0;
    int8_t axis_y_y = 1;

    if (swap_xy) {
        int8_t tmp_x = axis_x_x;
        int8_t tmp_y = axis_x_y;
        axis_x_x = axis_y_x;
        axis_x_y = axis_y_y;
        axis_y_x = tmp_x;
        axis_y_y = tmp_y;
    }
    if (mirror_x) {
        axis_x_x = -axis_x_x;
        axis_x_y = -axis_x_y;
    }
    if (mirror_y) {
        axis_y_x = -axis_y_x;
        axis_y_y = -axis_y_y;
    }
    return (orientation_matrix_t) {
        .m00 = axis_x_x, .m01 = axis_y_x, .m10 = axis_x_y, .m11 = axis_y_y,
    };
}

static orientation_matrix_t matrix_from_rotation(bsp_display_rotation_t rotation)
{
    switch (rotation) {
    case BSP_DISPLAY_ROTATE_90:
        return matrix_from_flags(true, true, false);
    case BSP_DISPLAY_ROTATE_180:
        return matrix_from_flags(false, true, true);
    case BSP_DISPLAY_ROTATE_270:
        return matrix_from_flags(true, false, true);
    case BSP_DISPLAY_ROTATE_0:
    default:
        return matrix_from_flags(false, false, false);
    }
}

static void compose_orientation(bool base_swap_xy, bool base_mirror_x, bool base_mirror_y,
                                bsp_display_rotation_t rotation,
                                bool *swap_xy, bool *mirror_x, bool *mirror_y)
{
    const orientation_matrix_t base = matrix_from_flags(base_swap_xy, base_mirror_x, base_mirror_y);
    const orientation_matrix_t rot = matrix_from_rotation(rotation);
    const orientation_matrix_t desired = {
        .m00 = rot.m00 * base.m00 + rot.m01 * base.m10,
        .m01 = rot.m00 * base.m01 + rot.m01 * base.m11,
        .m10 = rot.m10 * base.m00 + rot.m11 * base.m10,
        .m11 = rot.m10 * base.m01 + rot.m11 * base.m11,
    };

    for (int test_swap = 0; test_swap <= 1; ++test_swap) {
        for (int test_mirror_x = 0; test_mirror_x <= 1; ++test_mirror_x) {
            for (int test_mirror_y = 0; test_mirror_y <= 1; ++test_mirror_y) {
                const orientation_matrix_t candidate = matrix_from_flags(test_swap, test_mirror_x, test_mirror_y);
                if (candidate.m00 == desired.m00 && candidate.m01 == desired.m01 &&
                        candidate.m10 == desired.m10 && candidate.m11 == desired.m11) {
                    *swap_xy = (bool)test_swap;
                    *mirror_x = (bool)test_mirror_x;
                    *mirror_y = (bool)test_mirror_y;
                    return;
                }
            }
        }
    }

    *swap_xy = base_swap_xy;
    *mirror_x = base_mirror_x;
    *mirror_y = base_mirror_y;
}

static bool rotation_is_valid(bsp_display_rotation_t rotation)
{
    return rotation == BSP_DISPLAY_ROTATE_0 || rotation == BSP_DISPLAY_ROTATE_90 ||
           rotation == BSP_DISPLAY_ROTATE_180 || rotation == BSP_DISPLAY_ROTATE_270;
}

static esp_err_t apply_panel_orientation(bsp_display_rotation_t rotation)
{
    bool swap_xy, mirror_x, mirror_y;
    compose_orientation(BSP_LCD_BASE_SWAP_XY, BSP_LCD_BASE_MIRROR_X, BSP_LCD_BASE_MIRROR_Y,
                        rotation, &swap_xy, &mirror_x, &mirror_y);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, swap_xy), TAG, "set GC9A01 axis swap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, mirror_x, mirror_y), TAG, "set GC9A01 mirror failed");
    return ESP_OK;
}

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(ret_panel, ESP_ERR_INVALID_ARG, TAG, "panel output is null");
    if (s_panel) {
        *ret_panel = s_panel;
        return ESP_OK;
    }
    const bsp_display_config_t default_config = BSP_DISPLAY_DEFAULT_CONFIG();
    const bsp_display_config_t active = config ? *config : default_config;
    ESP_RETURN_ON_FALSE(rotation_is_valid(active.rotation), ESP_ERR_INVALID_ARG, TAG,
                        "unsupported rotation %d", (int)active.rotation);

    const spi_bus_config_t bus_config = GC9A01_PANEL_BUS_SPI_CONFIG(
        BSP_LCD_CLK, BSP_LCD_MOSI, BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG,
                        "initialize GC9A01 SPI bus failed");
    s_spi_bus_initialized = true;

    esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(BSP_LCD_CS, BSP_LCD_DC, NULL, NULL);
    io_config.pclk_hz = CONFIG_BSP_GC9A01_SPI_CLK_HZ;
    esp_err_t ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_HOST, &io_config, &s_panel_io);
    if (ret != ESP_OK) {
        spi_bus_free(BSP_LCD_SPI_HOST);
        s_spi_bus_initialized = false;
        return ret;
    }
    const gc9a01_vendor_config_t vendor_config = {
        .init_cmds = s_vendor_init,
        .init_cmds_size = sizeof(s_vendor_init) / sizeof(s_vendor_init[0]),
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .vendor_config = (void *)&vendor_config,
    };
    ret = esp_lcd_new_panel_gc9a01(s_panel_io, &panel_config, &s_panel);
    if (ret != ESP_OK) {
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
        spi_bus_free(BSP_LCD_SPI_HOST);
        s_spi_bus_initialized = false;
        return ret;
    }
    ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(s_panel), fail, TAG, "reset GC9A01 failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(s_panel), fail, TAG, "initialize GC9A01 failed");
    ESP_GOTO_ON_ERROR(apply_panel_orientation(active.rotation), fail, TAG, "set GC9A01 orientation failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), fail, TAG, "turn on GC9A01 failed");
    ESP_GOTO_ON_ERROR(backlight_init(), fail, TAG, "initialize backlight failed");
    s_config = active;
    s_config_valid = true;
    *ret_panel = s_panel;
    ESP_LOGI(TAG, "GC9A01 initialized: %dx%d MOSI=%d CLK=%d CS=%d DC=%d RST=%d BL=%d pclk=%d rotation=%d",
             BSP_LCD_H_RES, BSP_LCD_V_RES, BSP_LCD_MOSI, BSP_LCD_CLK, BSP_LCD_CS, BSP_LCD_DC,
             BSP_LCD_RST, BSP_LCD_BACKLIGHT, CONFIG_BSP_GC9A01_SPI_CLK_HZ, (int)active.rotation);
    return ESP_OK;

fail:
    esp_lcd_panel_del(s_panel);
    s_panel = NULL;
    esp_lcd_panel_io_del(s_panel_io);
    s_panel_io = NULL;
    if (s_spi_bus_initialized) {
        spi_bus_free(BSP_LCD_SPI_HOST);
        s_spi_bus_initialized = false;
    }
    return ret;
}

lv_display_t *bsp_display_start_with_config(const bsp_display_config_t *config)
{
    if (s_display) {
        return s_display;
    }
    bsp_display_config_t active = config ? *config : (bsp_display_config_t)BSP_DISPLAY_DEFAULT_CONFIG();
    esp_err_t ret = bsp_display_new(&active, &s_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "display init failed (%s)", esp_err_to_name(ret));
        return NULL;
    }
    if (s_config_valid) {
        active = s_config;
    }
    if (!esp_lv_adapter_is_initialized()) {
        esp_lv_adapter_config_t adapter = ESP_LV_ADAPTER_DEFAULT_CONFIG();
        if (active.task_stack_size) {
            adapter.task_stack_size = active.task_stack_size;
        }
        ret = esp_lv_adapter_init(&adapter);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LVGL adapter init failed (%s)", esp_err_to_name(ret));
            return NULL;
        }
    }
    /* Rotation lives entirely in MADCTL, so the adapter always sees an
     * upright display. */
    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
        s_panel, s_panel_io, BSP_LCD_H_RES, BSP_LCD_V_RES, ESP_LV_ADAPTER_ROTATE_0);
    if (active.buffer_height) {
        display_config.profile.buffer_height = active.buffer_height;
    }
    s_display = esp_lv_adapter_register_display(&display_config);
    if (!s_display) {
        ESP_LOGE(TAG, "register GC9A01 with LVGL adapter failed");
        return NULL;
    }
    ret = esp_lv_adapter_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL adapter start failed (%s)", esp_err_to_name(ret));
        esp_lv_adapter_unregister_display(s_display);
        s_display = NULL;
        return NULL;
    }
    return s_display;
}

lv_display_t *bsp_display_start(void) { return bsp_display_start_with_config(NULL); }
lv_display_t *bsp_display_get(void) { return s_display; }
esp_lcd_panel_handle_t bsp_display_get_panel(void) { return s_panel; }
esp_lcd_panel_io_handle_t bsp_display_get_panel_io(void) { return s_panel_io; }

bsp_display_rotation_t bsp_display_get_rotation(void)
{
    return s_config_valid ? s_config.rotation : BSP_LCD_ROTATION_DEFAULT;
}

esp_err_t bsp_display_set_rotation(bsp_display_rotation_t rotation)
{
    ESP_RETURN_ON_FALSE(s_panel, ESP_ERR_INVALID_STATE, TAG, "GC9A01 is not initialized");
    ESP_RETURN_ON_FALSE(rotation_is_valid(rotation), ESP_ERR_INVALID_ARG, TAG,
                        "unsupported rotation %d", (int)rotation);
    const bsp_display_rotation_t previous = bsp_display_get_rotation();
    if (rotation == previous) {
        return ESP_OK;
    }

    /* Pausing drains the worker, so no flush can be mid-CASET/RASET/RAMWR
     * while MADCTL changes. */
    bool paused_here = false;
    if (esp_lv_adapter_is_initialized() && s_display) {
        ESP_RETURN_ON_ERROR(esp_lv_adapter_pause(-1), TAG, "pause LVGL adapter failed");
        paused_here = true;
    }

    esp_err_t ret = apply_panel_orientation(rotation);
    if (ret == ESP_OK) {
        s_config.rotation = rotation;
        s_config_valid = true;
        /* Frame buffers still hold pixels laid out for the previous orientation. */
        if (s_display && esp_lv_adapter_lock(-1) == ESP_OK) {
            lv_obj_t *screen = lv_display_get_screen_active(s_display);
            if (screen) {
                lv_obj_invalidate(screen);
            }
            esp_lv_adapter_unlock();
        }
    } else {
        (void)apply_panel_orientation(previous);
    }

    if (paused_here) {
        esp_err_t resume_ret = esp_lv_adapter_resume();
        if (resume_ret != ESP_OK) {
            ESP_LOGE(TAG, "resume LVGL adapter failed (%s)", esp_err_to_name(resume_ret));
            if (ret == ESP_OK) {
                ret = resume_ret;
            }
        }
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "set GC9A01 rotation to %d failed", (int)rotation);
    ESP_LOGI(TAG, "GC9A01 rotation set to %d", (int)rotation);
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    ESP_RETURN_ON_FALSE(s_backlight_initialized, ESP_ERR_INVALID_STATE, TAG, "backlight is not initialized");
    ESP_RETURN_ON_FALSE(brightness_percent >= 0 && brightness_percent <= 100,
                        ESP_ERR_INVALID_ARG, TAG, "brightness must be within 0-100%%");
    const uint32_t max_duty = (1U << 10) - 1U;
    const uint32_t duty = max_duty * (uint32_t)brightness_percent / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(BSP_BACKLIGHT_LEDC_MODE, BSP_BACKLIGHT_LEDC_CHANNEL, duty),
                        TAG, "set backlight duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(BSP_BACKLIGHT_LEDC_MODE, BSP_BACKLIGHT_LEDC_CHANNEL),
                        TAG, "update backlight duty failed");
    return ESP_OK;
}

esp_err_t bsp_display_on(void)
{
    ESP_RETURN_ON_FALSE(s_panel, ESP_ERR_INVALID_STATE, TAG, "GC9A01 is not initialized");
    if (s_display_sleeping) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(s_panel_io, 0x11, NULL, 0), TAG,
                            "GC9A01 Sleep Out failed");
        vTaskDelay(pdMS_TO_TICKS(120));
        s_display_sleeping = false;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "turn on GC9A01 failed");
    if (s_display) {
        ESP_RETURN_ON_ERROR(esp_lv_adapter_resume(), TAG, "resume LVGL adapter failed");
    }
    ESP_LOGI(TAG, "GC9A01 display on");
    return ESP_OK;
}

esp_err_t bsp_display_off(void)
{
    ESP_RETURN_ON_FALSE(s_panel, ESP_ERR_INVALID_STATE, TAG, "GC9A01 is not initialized");
    if (s_display) {
        ESP_RETURN_ON_ERROR(esp_lv_adapter_pause(-1), TAG, "pause LVGL adapter failed");
    }
    esp_err_t ret = esp_lcd_panel_disp_on_off(s_panel, false);
    if (ret != ESP_OK) {
        if (s_display) {
            (void)esp_lv_adapter_resume();
        }
        ESP_RETURN_ON_ERROR(ret, TAG, "turn off GC9A01 failed");
    }
    if (!s_display_sleeping) {
        ret = esp_lcd_panel_io_tx_param(s_panel_io, 0x10, NULL, 0);
        if (ret != ESP_OK) {
            (void)esp_lcd_panel_disp_on_off(s_panel, true);
            if (s_display) {
                (void)esp_lv_adapter_resume();
            }
        }
        ESP_RETURN_ON_ERROR(ret, TAG, "GC9A01 Sleep In failed");
        s_display_sleeping = true;
    }
    ESP_LOGI(TAG, "GC9A01 display off (Display Off + Sleep In); LVGL paused");
    return ESP_OK;
}

bool bsp_display_lock(int32_t timeout_ms)
{
    if (!esp_lv_adapter_is_initialized()) {
        return true;
    }
    return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
}

void bsp_display_unlock(void)
{
    if (esp_lv_adapter_is_initialized()) {
        esp_lv_adapter_unlock();
    }
}
