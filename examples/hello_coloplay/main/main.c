/*
 * SPDX-FileCopyrightText: 2026 opencoloplayv1 contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * opencoloplayv1 — hello_coloplay example
 */

#include <math.h>
#include <string.h>
#include "bsp/opencoloplayv1.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "lvgl.h"

static const char *TAG = "hello_coloplay";

/* ---- UI state (touched from button callbacks via pending flags) ---- */
typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_SOLID,
    LED_MODE_RAINBOW,
    LED_MODE_MAX,
} led_mode_t;

static lv_obj_t *s_title_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_volume_arc;
static lv_obj_t *s_volume_value;
static volatile int32_t s_volume = 70;         /* 0..100 */
static volatile int32_t s_brightness = 70;     /* 0..100 */
static volatile led_mode_t s_led_mode = LED_MODE_RAINBOW;
static volatile bool s_beep_requested;
static volatile bool s_record_requested;
static volatile bool s_backlight_toggle_requested;
static volatile bool s_led_mode_advance_requested;
static int32_t s_last_knob_count;

#define SAMPLE_RATE       BSP_AUDIO_DEFAULT_SAMPLE_RATE
#define BEEP_FREQ_HZ      880
#define BEEP_DURATION_MS  180
#define RECORD_SECONDS    2

/* ---- helpers ---- */

static void hsv_to_rgb(uint16_t hue, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = (hue / 60) % 6;
    uint8_t remainder = (hue % 60) * 255 / 60;
    uint8_t q = 255 - remainder;
    uint8_t t = remainder;
    switch (region) {
    case 0: *r = 255; *g = t;    *b = 0;    break;
    case 1: *r = q;    *g = 255; *b = 0;    break;
    case 2: *r = 0;    *g = 255; *b = t;    break;
    case 3: *r = 0;    *g = q;    *b = 255; break;
    case 4: *r = t;    *g = 0;    *b = 255; break;
    default: *r = 255; *g = 0;    *b = q;   break;
    }
}

static esp_err_t play_beep(esp_codec_dev_handle_t speaker)
{
    const size_t samples = SAMPLE_RATE * BEEP_DURATION_MS / 1000;
    int16_t *buffer = heap_caps_malloc(samples * sizeof(int16_t), MALLOC_CAP_INTERNAL);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < samples; ++i) {
        buffer[i] = (int16_t)(sinf(2.0f * (float)M_PI * BEEP_FREQ_HZ * i / SAMPLE_RATE) * 12000);
    }
    esp_err_t ret = esp_codec_dev_write(speaker, buffer, samples * sizeof(int16_t));
    free(buffer);
    return ret;
}

static esp_err_t record_and_playback(esp_codec_dev_handle_t speaker, esp_codec_dev_handle_t mic)
{
    const size_t samples = SAMPLE_RATE * RECORD_SECONDS;
    int16_t *buffer = heap_caps_malloc(samples * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!buffer) {
        ESP_LOGE(TAG, "record buffer allocation failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "recording %d s from microphone...", RECORD_SECONDS);
    esp_err_t ret = esp_codec_dev_read(mic, buffer, samples * sizeof(int16_t));
    if (ret == ESP_OK) {
        int peak = 0;
        for (size_t i = 0; i < samples; ++i) {
            int v = buffer[i] < 0 ? -buffer[i] : buffer[i];
            if (v > peak) {
                peak = v;
            }
        }
        ESP_LOGI(TAG, "recorded: peak=%d%s", peak, peak < 100 ? " (SILENCE?)" : "");
        ESP_LOGI(TAG, "playing back...");
        ret = esp_codec_dev_write(speaker, buffer, samples * sizeof(int16_t));
    } else {
        ESP_LOGE(TAG, "microphone read failed: %s", esp_err_to_name(ret));
    }
    free(buffer);
    return ret;
}

static const char *led_mode_name(led_mode_t mode)
{
    switch (mode) {
    case LED_MODE_OFF:     return "LED off";
    case LED_MODE_SOLID:   return "LED solid";
    case LED_MODE_RAINBOW: return "LED rainbow";
    default:               return "?";
    }
}

/* ---- LVGL timers (run under the LVGL lock) ---- */

static void knob_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    int32_t count = 0;
    if (bsp_knob_get_count(&count) != ESP_OK) {
        return;
    }
    /* The knob wraps at the PCNT limits; accumulate the delta so the volume
     * keeps tracking across wraps. One EC11 detent = 4 counts; keep the
     * remainder so slow half-detent turns are not lost. */
    int32_t delta = count - s_last_knob_count;
    s_last_knob_count = count;
    static int32_t residual;
    residual += delta;
    int32_t steps = residual / 4;
    residual -= steps * 4;
    s_volume += steps;
    if (s_volume < 0) {
        s_volume = 0;
    }
    if (s_volume > 100) {
        s_volume = 100;
    }
    if (delta != 0) {
        ESP_LOGI(TAG, "knob: count=%d delta=%d vol=%d", (int)count, (int)delta, (int)s_volume);
    }
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    lv_arc_set_value(s_volume_arc, s_volume);
    lv_label_set_text_fmt(s_volume_value, "%d", (int)s_volume);
    lv_label_set_text_fmt(s_status_label, "%s  BL %d",
                          led_mode_name(s_led_mode), (int)s_brightness);
}

static void action_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    static bool backlight_on = true;
    if (s_backlight_toggle_requested) {
        s_backlight_toggle_requested = false;
        backlight_on = !backlight_on;
        bsp_display_brightness_set(backlight_on ? s_brightness : 0);
    }
    if (s_led_mode_advance_requested) {
        s_led_mode_advance_requested = false;
        s_led_mode = (s_led_mode + 1) % LED_MODE_MAX;
    }

    static esp_codec_dev_handle_t speaker;
    if (s_beep_requested || s_record_requested) {
        if (!speaker) {
            speaker = bsp_audio_codec_speaker_init();
            if (!speaker) {
                ESP_LOGE(TAG, "speaker init failed");
                s_beep_requested = s_record_requested = false;
                return;
            }
            /* esp_codec_dev requires open() before any write() */
            esp_codec_dev_sample_info_t fs = {
                .sample_rate = SAMPLE_RATE,
                .channel = 1,
                .bits_per_sample = 16,
            };
            if (esp_codec_dev_open(speaker, &fs) != ESP_CODEC_DEV_OK) {
                ESP_LOGE(TAG, "speaker open failed");
                speaker = NULL;
                s_beep_requested = s_record_requested = false;
                return;
            }
        }
        esp_codec_dev_set_out_vol(speaker, s_volume);
        esp_codec_dev_set_out_mute(speaker, false);
    }
    if (s_beep_requested) {
        s_beep_requested = false;
        ESP_ERROR_CHECK(play_beep(speaker));
        /* Keep the device open: closing it would drop the codec refcount to
         * zero and power the ES8311 down, and the ADC does not reliably come
         * back after repeated power cycles. Mute the DAC instead. */
        esp_codec_dev_set_out_mute(speaker, true);
    }
    if (s_record_requested) {
        s_record_requested = false;
        esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
        if (!mic) {
            ESP_LOGE(TAG, "microphone init failed");
            return;
        }
        esp_codec_dev_sample_info_t fs = {
            .sample_rate = SAMPLE_RATE,
            .channel = 1,
            .bits_per_sample = 16,
        };
        if (esp_codec_dev_open(mic, &fs) != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "microphone open failed");
            return;
        }
        esp_codec_dev_set_in_gain(mic, 30.0);
        /* The beep path may have muted the DAC in this same tick; make sure
         * the playback below is audible. */
        esp_codec_dev_set_out_mute(speaker, false);
        record_and_playback(speaker, mic);
        /* Closing the mic only stops the RX stream: the speaker keeps the
         * codec refcount above zero, so the chip stays powered. */
        esp_codec_dev_close(mic);
        esp_codec_dev_set_out_mute(speaker, true);
    }
}

static void led_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    switch (s_led_mode) {
    case LED_MODE_OFF:
        bsp_led_clear();
        break;
    case LED_MODE_SOLID:
        /* Idempotent: rewriting every cycle also recovers after a mode change */
        bsp_led_set_all(0, 40, 120);
        break;
    case LED_MODE_RAINBOW: {
        static uint16_t hue_offset;
        for (uint32_t i = 0; i < BSP_LED_NUM; ++i) {
            uint8_t r, g, b;
            hsv_to_rgb(hue_offset + i * 360 / BSP_LED_NUM, &r, &g, &b);
            bsp_led_set_pixel(i, r / 8, g / 8, b / 8); /* keep it dim */
        }
        bsp_led_refresh();
        hue_offset = (hue_offset + 3) % 360;
        break;
    }
    default:
        break;
    }
}

/* ---- button callbacks (button task context: only set flags) ---- */

static void button_cb(void *arg, void *data)
{
    (void)arg;
    switch ((intptr_t)data) {
    case BSP_BUTTON_1:    s_led_mode_advance_requested = true; break;
    case BSP_BUTTON_2:    s_volume -= 10; if (s_volume < 0) s_volume = 0; break;
    case BSP_BUTTON_3:    s_record_requested = true; break;
    case BSP_BUTTON_4:    s_volume += 10; if (s_volume > 100) s_volume = 100; break;
    case BSP_BUTTON_KNOB: s_beep_requested = true; break;
    case BSP_BUTTON_PWR:  s_backlight_toggle_requested = true; break;
    default: break;
    }
}

/* ---- UI ---- */

/*
 * Round-screen layout: everything lives inside the inscribed circle
 * (center 120,120, radius 120). At a vertical offset y from center the
 * usable width is 2*sqrt(120^2 - y^2):
 *   y=±90 -> 157px, y=±62 -> 195px, y=±80 -> 178px, y=±97 -> 129px
 */
static void create_ui(lv_display_t *display)
{
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101018), LV_PART_MAIN);

    s_title_label = lv_label_create(screen);
    lv_label_set_text(s_title_label, "opencoloplayv1");
    lv_obj_set_style_text_font(s_title_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0x66ccff), LV_PART_MAIN);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 22);

    s_volume_arc = lv_arc_create(screen);
    lv_obj_set_size(s_volume_arc, 130, 130);
    lv_obj_align(s_volume_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_range(s_volume_arc, 0, 100);
    lv_arc_set_value(s_volume_arc, s_volume);
    lv_obj_remove_style(s_volume_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_volume_arc, LV_OBJ_FLAG_CLICKABLE);

    s_volume_value = lv_label_create(screen);
    lv_obj_set_style_text_font(s_volume_value, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_volume_value, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_label_set_text(s_volume_value, "0");
    lv_obj_align(s_volume_value, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *vol_caption = lv_label_create(screen);
    lv_obj_set_style_text_color(vol_caption, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_label_set_text(vol_caption, "vol");
    lv_obj_align(vol_caption, LV_ALIGN_CENTER, 0, 16);

    s_status_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_status_label, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_text(s_status_label, "");
    lv_obj_align(s_status_label, LV_ALIGN_CENTER, 0, 62);

    lv_obj_t *hint1 = lv_label_create(screen);
    lv_obj_set_style_text_font(hint1, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint1, lv_color_hex(0x707070), LV_PART_MAIN);
    lv_label_set_text(hint1, "K1:LED  K2/K4:vol  K3:rec");
    lv_obj_align(hint1, LV_ALIGN_CENTER, 0, 80);

    lv_obj_t *hint2 = lv_label_create(screen);
    lv_obj_set_style_text_font(hint2, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint2, lv_color_hex(0x707070), LV_PART_MAIN);
    lv_label_set_text(hint2, "knob:vol  EKEY:beep");
    lv_obj_align(hint2, LV_ALIGN_CENTER, 0, 97);
}

/* ---- app_main ---- */

void app_main(void)
{
    /* Display + LVGL */
    lv_display_t *display = bsp_display_start();
    ESP_ERROR_CHECK(display ? ESP_OK : ESP_FAIL);
    create_ui(display);

    /* Knob */
    ESP_ERROR_CHECK(bsp_knob_init());
    bsp_knob_get_count(&s_last_knob_count);

    /* LED ring */
    ESP_ERROR_CHECK(bsp_led_init());

    /* Buttons */
    button_handle_t buttons[BSP_BUTTON_NUM] = {0};
    int button_count = 0;
    ESP_ERROR_CHECK(bsp_iot_button_create(buttons, &button_count, BSP_BUTTON_NUM));
    for (int i = 0; i < button_count; ++i) {
        ESP_ERROR_CHECK(iot_button_register_cb(buttons[i], BUTTON_PRESS_DOWN, NULL, button_cb, (void *)(intptr_t)i));
    }

    /* Timers: all LVGL work happens under the adapter's lock */
    lv_timer_create(knob_timer_cb, 50, NULL);
    lv_timer_create(ui_timer_cb, 200, NULL);
    lv_timer_create(action_timer_cb, 20, NULL);
    lv_timer_create(led_timer_cb, 30, NULL);

    /* Boot beep: also proves the audio path end to end */
    s_beep_requested = true;

    ESP_LOGI(TAG, "hello_coloplay ready: all BSP drivers initialized");
}
