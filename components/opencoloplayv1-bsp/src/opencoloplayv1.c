/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — open-source BSP for the coloplay hardware (ESP32-S3)
 */

#include "bsp/opencoloplayv1.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "coloplay";
static i2c_master_bus_handle_t s_i2c_bus;

esp_err_t bsp_i2c_init(void)
{
    if (s_i2c_bus) {
        return ESP_OK;
    }
    const i2c_master_bus_config_t config = {
        .i2c_port = BSP_I2C_PORT,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&config, &s_i2c_bus), TAG, "create shared I2C bus failed");
    ESP_LOGI(TAG, "Shared I2C initialized: port=%d SDA=%d SCL=%d", BSP_I2C_PORT, BSP_I2C_SDA, BSP_I2C_SCL);
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return s_i2c_bus;
}
