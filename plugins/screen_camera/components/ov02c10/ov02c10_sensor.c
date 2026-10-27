#include "ov02c10_compat.h"

/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * OV02C10 driver for esp_cam_sensor. The register tables and this file's structure are Guition's, as their
 * esp_cam_sensor copy publishes them under Apache-2.0, through github.com/sullb/esphome-p4-csi-camera (MIT).
 *
 * Changed for Tessera's camera plugins (2026-10-19, Jeroen Peters, @Heronimonimo): exposure and analog gain on the
 * OV02C10's own registers (0x3501, 0x3508), so esp_video's ISP pipeline runs auto exposure; mirror and flip that keep
 * the Bayer order, with the true view by default; the OV5640 auto-exposure and banding registers the earlier copy
 * wrote are gone; a sensor with another chip id is no longer taken for an OV02C10. Which register does what was looked
 * up in Linux's drivers/media/i2c/ov02c10.c; no code or table was taken from it.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "ov02c10_settings.h"
#include "ov02c10.h"

#define OV02C10_PID         0x5602
#define OV02C10_SENSOR_NAME "OV02C10"

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms / portTICK_PERIOD_MS : 1))

// Exposure in lines (0x3501..0x3502): at least 4, at most VTS - 8 (Linux ov02c10.c).
#define OV02C10_REG_EXPOSURE_H      0x3501
#define OV02C10_REG_EXPOSURE_L      0x3502
#define OV02C10_EXPOSURE_MIN        4
#define OV02C10_EXPOSURE_MAX_MARGIN 8
#define OV02C10_EXPOSURE_DEFAULT    0x046c

// Analog gain (0x3508..0x3509) holds gain x 256: Linux's control runs 0x10..0xf8 (1x to 15.5x in steps of 1/16) and
// writes it shifted left by four.
#define OV02C10_REG_ANALOG_GAIN_H   0x3508
#define OV02C10_REG_ANALOG_GAIN_L   0x3509
#define OV02C10_GAIN_STEP_MIN       0x10
#define OV02C10_GAIN_STEP_MAX       0xf8
#define OV02C10_GAIN_COUNT          (OV02C10_GAIN_STEP_MAX - OV02C10_GAIN_STEP_MIN + 1)
#define OV02C10_GAIN_DEFAULT_INDEX  (0x20 - OV02C10_GAIN_STEP_MIN)   // 2x, where auto exposure starts

// Orientation (Linux ov02c10.c): 0x3820 bit 3 mirrors, bit 4 flips. Each moves the readout window by one pixel
// (0x3811, 0x3813) so the Bayer order stays the same.
#define OV02C10_REG_ROTATE          0x3820
#define OV02C10_REG_X_WIN_L         0x3811
#define OV02C10_REG_Y_WIN_L         0x3813
#define OV02C10_ROTATE_MIRROR       BIT(3)
#define OV02C10_ROTATE_FLIP         BIT(4)

#define OV02C10_REG_TEST_PATTERN    0x4503

// HTS 2280 and VTS 1164 in every table: one line lasts HTS / pixel clock.
#define OV02C10_HTS                 2280
#define OV02C10_VTS                 1164
#define OV02C10_TLINE_NS(pclk)      ((uint32_t)((OV02C10_HTS * 1000000000ULL) / (pclk)))

static const char *TAG = "ov02c10";

typedef struct {
    uint32_t exposure_val;
    uint32_t gain_index;
    int hmirror;
    int vflip;
    // The orientation registers as the current format's table writes them.
    uint8_t base_rotate;
    uint8_t base_x_win;
    uint8_t base_y_win;
    // How far the picture may move inside the sensor's window: window size less picture size.
    uint16_t room_x;
    uint16_t room_y;
    // A field of view's mode (Screen camera's setting): its window offsets stay as its table has them (below).
    bool field_mode;
} ov02c10_state_t;

// Total gain x 1000 for each analog gain step, the menu esp_video's ISP pipeline picks from.
static uint32_t s_gain_values[OV02C10_GAIN_COUNT];

static const esp_cam_sensor_isp_info_t ov02c10_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81666700,
            .vts = OV02C10_VTS,
            .hts = OV02C10_HTS,
            .exp_def = OV02C10_EXPOSURE_DEFAULT,
            .gain_def = OV02C10_GAIN_DEFAULT_INDEX,
            .tline_ns = OV02C10_TLINE_NS(81666700),
            .bayer_type = ESP_CAM_SENSOR_BAYER_GBRG,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81666700,
            .vts = OV02C10_VTS,
            .hts = OV02C10_HTS,
            .exp_def = OV02C10_EXPOSURE_DEFAULT,
            .gain_def = OV02C10_GAIN_DEFAULT_INDEX,
            .tline_ns = OV02C10_TLINE_NS(81666700),
            .bayer_type = ESP_CAM_SENSOR_BAYER_GBRG,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 80000000,
            .vts = OV02C10_VTS,
            .hts = OV02C10_HTS,
            .exp_def = OV02C10_EXPOSURE_DEFAULT,
            .gain_def = OV02C10_GAIN_DEFAULT_INDEX,
            .tline_ns = OV02C10_TLINE_NS(80000000),
            .bayer_type = ESP_CAM_SENSOR_BAYER_GBRG,
        }
    },
};

static const esp_cam_sensor_format_t ov02c10_format_info[] = {
    {
        // First, so the camera's own choice (`resolution: auto`) is a size in whole macroblocks: the part Video encodes
        // it as it is, no cutting on the way.
        .name = "MIPI_1lane_24Minput_RAW10_1280x720_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 720,
        .regs = ov02c10_input_24M_MIPI_1lane_raw10_1280x720_30fps,
        .regs_size = ARRAY_SIZE(ov02c10_input_24M_MIPI_1lane_raw10_1280x720_30fps),
        .fps = 30,
        .isp_info = &ov02c10_isp_info[0],
        .mipi_info = {
            .mipi_clk = OV02C10_MIPI_CSI_LINE_RATE_800x640_50FPS,
            .lane_num = 1,
            .line_sync_en = CONFIG_CAMERA_OV02C10_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW10_1288x728_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1288,
        .height = 728,
        .regs = ov02c10_input_24M_MIPI_1lane_raw10_1288x728_30fps,
        .regs_size = ARRAY_SIZE(ov02c10_input_24M_MIPI_1lane_raw10_1288x728_30fps),
        .fps = 30,
        .isp_info = &ov02c10_isp_info[0],
        .mipi_info = {
            .mipi_clk = OV02C10_MIPI_CSI_LINE_RATE_800x640_50FPS,
            .lane_num = 1,
            .line_sync_en = CONFIG_CAMERA_OV02C10_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = ov02c10_input_24M_MIPI_1lane_raw10_1920x1080_30fps,
        .regs_size = ARRAY_SIZE(ov02c10_input_24M_MIPI_1lane_raw10_1920x1080_30fps),
        .fps = 30,
        .isp_info = &ov02c10_isp_info[1],
        .mipi_info = {
            .mipi_clk = OV02C10_IDI_CLOCK_RATE_1280x960_45FPS,
            .lane_num = 1,
            .line_sync_en = CONFIG_CAMERA_OV02C10_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = ov02c10_input_24M_MIPI_2lane_raw10_1920x1080_30fps,
        .regs_size = ARRAY_SIZE(ov02c10_input_24M_MIPI_2lane_raw10_1920x1080_30fps),
        .fps = 30,
        .isp_info = &ov02c10_isp_info[2],
        .mipi_info = {
            .mipi_clk = OV02C10_IDI_CLOCK_RATE_1280x960_45FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV02C10_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
};

// The exposure and gain the ISP pipeline set last, and one line's time, for a plugin that reads the room's light level.
// Written by this driver (the camera's own task), read by the plugin: each a single 32-bit word.
static volatile uint32_t s_exposure_lines;
static volatile uint32_t s_gain_x16;
static volatile uint32_t s_tline_ns;

void ov02c10_sensor_exposure(uint32_t *lines, uint32_t *gain_x16, uint32_t *tline_ns)
{
    *lines = s_exposure_lines;
    *gain_x16 = s_gain_x16;
    *tline_ns = s_tline_ns;
}

// Referenced by esp_video_camera.cpp so the static component archive keeps this
// object file and, with it, ESP_CAM_SENSOR_DETECT_FN's linker-section entry.
void ov02c10_sensor_link_anchor(void)
{
}

static esp_err_t ov02c10_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t ov02c10_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers */
static esp_err_t ov02c10_write_array(esp_sccb_io_handle_t sccb_handle, const ov02c10_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != OV02C10_REG_END) {
        if (regarray[i].reg != OV02C10_REG_DELAY) {
            ret = ov02c10_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

// The value a table writes to one register, or `fallback` when it does not.
static uint8_t ov02c10_table_value(const ov02c10_reginfo_t *regarray, uint16_t reg, uint8_t fallback)
{
    uint8_t value = fallback;
    for (int i = 0; regarray[i].reg != OV02C10_REG_END; i++) {
        if (regarray[i].reg == reg) {
            value = regarray[i].val;
        }
    }
    return value;
}

static esp_err_t ov02c10_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = ov02c10_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = ov02c10_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t ov02c10_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ov02c10_set_reg_bits(dev->sccb_handle, OV02C10_REG_TEST_PATTERN, 7, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov02c10_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t ov02c10_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ov02c10_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t ov02c10_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    uint8_t pid_h, pid_l;
    esp_err_t ret = ov02c10_read(dev->sccb_handle, OV02C10_REG_SENSOR_ID_H, &pid_h);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_h failed");

    ret = ov02c10_read(dev->sccb_handle, OV02C10_REG_SENSOR_ID_L, &pid_l);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_l failed");

    uint16_t pid = (pid_h << 8) | pid_l;
    if (pid) {
        id->pid = pid;
    }
    return ret;
}

static esp_err_t ov02c10_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ov02c10_write(dev->sccb_handle, OV02C10_REG_MIPI_CTRL00,
                                  CONFIG_CAMERA_OV02C10_CSI_LINESYNC_ENABLE ? 0x64 : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write mipi ctrl failed");

    ret = ov02c10_write(dev->sccb_handle, OV02C10_REG_SLEEP_MODE, enable ? 0x01 : 0x00);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write stream failed");

    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t ov02c10_set_exposure(esp_cam_sensor_device_t *dev, uint32_t lines)
{
    ov02c10_state_t *state = (ov02c10_state_t *)dev->priv;
    uint32_t max = dev->cur_format->isp_info->isp_v1_info.vts - OV02C10_EXPOSURE_MAX_MARGIN;
    lines = MAX(lines, OV02C10_EXPOSURE_MIN);
    lines = MIN(lines, max);

    esp_err_t ret = ov02c10_write(dev->sccb_handle, OV02C10_REG_EXPOSURE_H, (lines >> 8) & 0xff);
    ret |= ov02c10_write(dev->sccb_handle, OV02C10_REG_EXPOSURE_L, lines & 0xff);
    if (ret == ESP_OK) {
        state->exposure_val = lines;
        s_exposure_lines = lines;
        s_tline_ns = dev->cur_format->isp_info->isp_v1_info.tline_ns;
    }
    return ret;
}

static esp_err_t ov02c10_set_gain_index(esp_cam_sensor_device_t *dev, uint32_t index)
{
    ov02c10_state_t *state = (ov02c10_state_t *)dev->priv;
    index = MIN(index, OV02C10_GAIN_COUNT - 1);
    uint32_t reg = (index + OV02C10_GAIN_STEP_MIN) << 4;

    esp_err_t ret = ov02c10_write(dev->sccb_handle, OV02C10_REG_ANALOG_GAIN_H, (reg >> 8) & 0xff);
    ret |= ov02c10_write(dev->sccb_handle, OV02C10_REG_ANALOG_GAIN_L, reg & 0xff);
    if (ret == ESP_OK) {
        state->gain_index = index;
        s_gain_x16 = index + OV02C10_GAIN_STEP_MIN;
    }
    return ret;
}

// A 16-bit value a table writes to two registers (high byte first).
static uint16_t ov02c10_table_value16(const ov02c10_reginfo_t *regarray, uint16_t reg)
{
    return (ov02c10_table_value(regarray, reg, 0) << 8) | ov02c10_table_value(regarray, reg + 1, 0);
}

// The picture's offset one pixel further, to keep the Bayer order after a mirror or flip: what counts is that the offset
// changes between even and odd. Linux moves it the way `preferred` says; where the window has no room on that side (the
// 1288x728 table leaves no spare row below), it moves the other way. A picture past the window's edge is a line short,
// and the camera interface then waits for a frame that never ends.
static uint8_t ov02c10_shift_window(uint8_t offset, int preferred, uint16_t room)
{
    int moved = offset + preferred;
    if (moved < 0 || moved > room) {
        moved = offset - preferred;
    }
    return (uint8_t)moved;
}

// Mirror and flip. Each toggles its bit in 0x3820 and moves the window one pixel, the way Linux pairs them: bit 3 set
// with the X offset one lower, bit 4 set with the Y offset one higher (or the other way where the window has no room,
// ov02c10_shift_window). Bit 3 set is the true view, as Linux has it; the tables leave it clear, which is a mirror image
// (GitHub issue #1), so mirror off sets it. Flip is relative to the table, whose bit 4 clear is upright.
static esp_err_t ov02c10_apply_orientation(esp_cam_sensor_device_t *dev)
{
    ov02c10_state_t *state = (ov02c10_state_t *)dev->priv;
    uint8_t rotate = state->base_rotate;
    uint8_t x_win = state->base_x_win;
    uint8_t y_win = state->base_y_win;

    const bool true_view_bit = !state->hmirror;
    // In a field of view's mode the offsets count binned or skipped pixels and the window has 8 columns to spare (968 x
    // 544 for 960 x 544): its table's even offsets stay, the bits alone turn the picture. A Guition tester saw
    // the first build's picture moved and striped (2026-10-25); the log reads back what the ISP makes of it.
    const bool shift = !state->field_mode;
    if (((rotate & OV02C10_ROTATE_MIRROR) != 0) != true_view_bit) {
        rotate ^= OV02C10_ROTATE_MIRROR;
        if (shift) {
            x_win = ov02c10_shift_window(x_win, (rotate & OV02C10_ROTATE_MIRROR) ? -1 : 1, state->room_x);
        }
    }
    if (state->vflip) {
        rotate ^= OV02C10_ROTATE_FLIP;
        if (shift) {
            y_win = ov02c10_shift_window(y_win, (rotate & OV02C10_ROTATE_FLIP) ? 1 : -1, state->room_y);
        }
    }

    esp_err_t ret = ov02c10_write(dev->sccb_handle, OV02C10_REG_ROTATE, rotate);
    ret |= ov02c10_write(dev->sccb_handle, OV02C10_REG_X_WIN_L, x_win);
    ret |= ov02c10_write(dev->sccb_handle, OV02C10_REG_Y_WIN_L, y_win);
    return ret;
}

static esp_err_t ov02c10_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = OV02C10_EXPOSURE_MIN;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - OV02C10_EXPOSURE_MAX_MARGIN;
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = OV02C10_GAIN_COUNT;
        qdesc->enumeration.elements = s_gain_values;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def;
        break;
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    default: {
        ESP_LOGD(TAG, "id=%" PRIx32 " is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t ov02c10_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    ov02c10_state_t *state = (ov02c10_state_t *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        *(uint32_t *)arg = state->exposure_val;
        return ESP_OK;
    case ESP_CAM_SENSOR_GAIN:
        *(uint32_t *)arg = state->gain_index;
        return ESP_OK;
    case ESP_CAM_SENSOR_VFLIP:
        *(int *)arg = state->vflip;
        return ESP_OK;
    case ESP_CAM_SENSOR_HMIRROR:
        *(int *)arg = state->hmirror;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t ov02c10_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    ov02c10_state_t *state = (ov02c10_state_t *)dev->priv;
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        ret = ov02c10_set_exposure(dev, *(const uint32_t *)arg);
        break;
    case ESP_CAM_SENSOR_GAIN:
        ret = ov02c10_set_gain_index(dev, *(const uint32_t *)arg);
        break;
    case ESP_CAM_SENSOR_VFLIP:
        state->vflip = *(const int *)arg ? 1 : 0;
        ret = ov02c10_apply_orientation(dev);
        break;
    case ESP_CAM_SENSOR_HMIRROR:
        state->hmirror = *(const int *)arg ? 1 : 0;
        ret = ov02c10_apply_orientation(dev);
        break;
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t ov02c10_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(ov02c10_format_info);
    formats->format_array = &ov02c10_format_info[0];
    return ESP_OK;
}

static esp_err_t ov02c10_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return ESP_OK;
}

static esp_err_t ov02c10_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ov02c10_state_t *state = (ov02c10_state_t *)dev->priv;

    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        ESP_RETURN_ON_FALSE(dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI, ESP_ERR_NOT_SUPPORTED, TAG,
                            "Not support DVP port");
        format = &ov02c10_format_info[CONFIG_CAMERA_OV02C10_MIPI_IF_FORMAT_INDEX_DAFAULT];
    }

    esp_err_t ret = ov02c10_write_array(dev->sccb_handle, ov02c10_mipi_reset_regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write reset regs failed");
    const ov02c10_reginfo_t *regs = (const ov02c10_reginfo_t *)format->regs;
    ret = ov02c10_write_array(dev->sccb_handle, regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write fmt regs failed");

    dev->cur_format = format;

    // Exposure and gain start where the ISP pipeline expects them; the orientation chosen before stays.
    state->field_mode = format->name != NULL && strncmp(format->name, "OV02C10_full_", 13) == 0;
    state->base_rotate = ov02c10_table_value(regs, OV02C10_REG_ROTATE, 0xa0);
    state->base_x_win = ov02c10_table_value(regs, OV02C10_REG_X_WIN_L, 0x07);
    state->base_y_win = ov02c10_table_value(regs, OV02C10_REG_Y_WIN_L, 0x04);
    // Window 0x3800..0x3807 (start and end, inclusive), picture 0x3808..0x380b.
    int window_w = ov02c10_table_value16(regs, 0x3804) - ov02c10_table_value16(regs, 0x3800) + 1;
    int window_h = ov02c10_table_value16(regs, 0x3806) - ov02c10_table_value16(regs, 0x3802) + 1;
    state->room_x = (uint16_t)MAX(window_w - (int)format->width, 0);
    state->room_y = (uint16_t)MAX(window_h - (int)format->height, 0);
    ret = ov02c10_set_exposure(dev, format->isp_info->isp_v1_info.exp_def);
    ret |= ov02c10_set_gain_index(dev, format->isp_info->isp_v1_info.gain_def);
    ret |= ov02c10_apply_orientation(dev);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write exposure/orientation failed");

    // stop stream default
    ret = ov02c10_set_stream(dev, 0);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write stream regs failed");

    return ret;
}

static esp_err_t ov02c10_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);

    esp_err_t ret = ESP_FAIL;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(esp_cam_sensor_format_t));
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t ov02c10_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_FAIL;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = ov02c10_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = ov02c10_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov02c10_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = ov02c10_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = ov02c10_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov02c10_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = ov02c10_get_sensor_id(dev, arg);
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ret;
}

static esp_err_t ov02c10_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "pwdn pin config failed");

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);
        ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "reset pin config failed");

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t ov02c10_power_off(esp_cam_sensor_device_t *dev)
{
    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ESP_OK;
}

// The sensor the field of view's status reads (below), forgotten when it goes.
static bool s_detected;
static esp_cam_sensor_device_t *s_device;

static esp_err_t ov02c10_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del ov02c10 (%p)", dev);
    if (dev != NULL && dev == s_device) {
        s_device = NULL;
        s_detected = false;
    }
    if (dev) {
        free(dev->priv);
        free(dev);
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t ov02c10_ops = {
    .query_para_desc = ov02c10_query_para_desc,
    .get_para_value = ov02c10_get_para_value,
    .set_para_value = ov02c10_set_para_value,
    .query_support_formats = ov02c10_query_support_formats,
    .query_support_capability = ov02c10_query_support_capability,
    .set_format = ov02c10_set_format,
    .get_format = ov02c10_get_format,
    .priv_ioctl = ov02c10_priv_ioctl,
    .del = ov02c10_delete
};

// ---- The field of view (Screen camera's setting "Field of view"; Tessera's docs/PLATFORM_PLAN.md 4.6) ----
// Two modes over the whole array for a tester to try, each the 1288x728 table with a few registers changed, built once
// when first asked for, after OmniVision's OV02C10 datasheet (v2.01): the sensor has no scaler, only a window, binning
// and skipping (table 2-1: "sub2 ... bin2 or skip2"); 0x3814-0x3817 step the columns and lines, 0x3820 (FORMAT1) bit 1
// bins two columns and bit 0 two lines, and 0x4501 (HBIN_CTRL) bits 3:2 at 00 average them (table 3-1). Neither is
// proven on a Guition yet: both stay off until a tester says the picture is right.
static bool s_detected = false;                   // an OV02C10 answered (ov02c10_detect)
static esp_cam_sensor_device_t *s_device = NULL;  // and which, for the status the log reads back

typedef struct {
    uint16_t reg;
    uint8_t val;
} ov02c10_change_t;

// Every other pair of columns and of lines (odd increment 3, even 1: the Bayer pairs stay whole), 960 x 544 out of the
// 968 x 544 that leaves: every line, the columns' middle, in whole macroblocks for the video's encoder (a horizontal
// size must be a multiple of 8, section 2.3), at even offsets (4 and 0) that a mirror or flip leaves where they are
// (field_mode). With 528 of the 544 lines asked for, a Guition 10.1-inch gave one frame and stalled whenever the lines
// were halved, while the columns halved alone ran at 29.8 frames a second (Jeroen Peters' test builds, 2026-10-25).
// On the 1288x728 table, Sharp's: the same PLL, timing and lane rate as the 1920x1080 table, but that one turns the
// stream on at its end (0x0100 0x01), while the driver still writes exposure and orientation and stops it again, and on
// a Guition 4.3-inch no frame came after that (GitHub issue #326); the 1288x728 table leaves the stream to esp_video.
// Its window opened to the whole array (0..1935 x 4..1091).
#define OV02C10_HALF_CHANGES                                                                    \
    {0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0x04},                             \
    {0x3804, 0x07}, {0x3805, 0x8f}, {0x3806, 0x04}, {0x3807, 0x43},                             \
    {0x3808, 0x03}, {0x3809, 0xc0}, {0x380a, 0x02}, {0x380b, 0x20},                             \
    {0x3810, 0x00}, {0x3811, 0x04}, {0x3812, 0x00}, {0x3813, 0x00},                             \
    {0x3814, 0x03}, {0x3815, 0x01}, {0x3816, 0x03}, {0x3817, 0x01}
// Full (binning): the pairs averaged, columns and lines (0x3820 0xa3: the table's 0xa0 with hbin2_o and vbinf_o; the
// driver's mirror and flip keep bits 0 and 1; the table's 0x4501 0x00 averages).
static const ov02c10_change_t s_binned_changes[] = {OV02C10_HALF_CHANGES, {0x3820, 0xa3}, {0x4501, 0x00}};
// Full (skip): the pairs read, the others skipped: fine patterns may shimmer.
static const ov02c10_change_t s_skipped_changes[] = {OV02C10_HALF_CHANGES};
#define OV02C10_FIELD_REGS 320
static ov02c10_reginfo_t s_field_regs[2][OV02C10_FIELD_REGS];
static esp_cam_sensor_format_t s_field_format[2];

static const esp_cam_sensor_format_t *ov02c10_build_field(int index, const esp_cam_sensor_format_t *base,
                                                          const ov02c10_change_t *changes, size_t count,
                                                          const char *name, uint16_t width, uint16_t height)
{
    const ov02c10_reginfo_t *regs = (const ov02c10_reginfo_t *)base->regs;
    ov02c10_reginfo_t *out = s_field_regs[index];
    bool used[32] = {false};
    size_t n = 0;
    for (size_t i = 0; regs[i].reg != OV02C10_REG_END; i++) {
        if (n + count + 1 >= OV02C10_FIELD_REGS) {
            ESP_LOGE(TAG, "The table for %s is too long", name);
            return NULL;
        }
        out[n] = regs[i];
        for (size_t c = 0; c < count; c++) {
            if (changes[c].reg == regs[i].reg) {
                out[n].val = changes[c].val;
                used[c] = true;
            }
        }
        n++;
    }
    for (size_t c = 0; c < count; c++) {
        if (!used[c]) {
            out[n].reg = changes[c].reg;
            out[n].val = changes[c].val;
            n++;
        }
    }
    out[n].reg = OV02C10_REG_END;
    out[n].val = 0;
    n++;
    s_field_format[index] = *base;
    s_field_format[index].name = name;
    s_field_format[index].width = width;
    s_field_format[index].height = height;
    s_field_format[index].regs = out;
    s_field_format[index].regs_size = (int)n;
    return &s_field_format[index];
}

// TEST BUILD 6 (not for release), on 0.6.4: what a Full mode tries in turn until one runs (esp_video_camera keeps the
// first that does). 0.6.4's twelve windows (544 to 528 lines, offsets 0 to 8) each gave one whole frame and then the
// same MIPI frame boundary error (Guition 10.1-inch, 2026-10-27), so the window is not it; what is left is the frame's
// timing and the rows it starts from. All tries are 960 x 544 at 4,0 (2 with the rows from 0); in order, the fastest
// first: the rows read from 0 as the sensor's relatives bin them (OV5675, OV8856, OV13858: 0x3803 0 and the window's
// end a few lines further), with and without their 0x3714 0x28 and 0x4009 0x11; then the frame longer (VTS
// 0x380e/0x380f, 1164 in the table, 30 frames a second) up to 3492 (10 a second); then the long frames with those two.
#define OV02C10_TRY_SIBLING 0x01   // 0x3714 0x28, 0x4009 0x11
#define OV02C10_TRY_ROW0    0x02   // rows 0..1095 (548 halved), isp_y_win 2
typedef struct {
    uint16_t vts;
    uint8_t extra;
} ov02c10_try_t;
static const ov02c10_try_t s_tries[] = {
    {1164, OV02C10_TRY_ROW0},
    {1164, OV02C10_TRY_ROW0 | OV02C10_TRY_SIBLING},
    {1300, 0},
    {1456, 0},
    {1746, 0},
    {2328, 0},
    {3492, 0},
    {2328, OV02C10_TRY_SIBLING},
    {2328, OV02C10_TRY_ROW0},
    {3492, OV02C10_TRY_ROW0 | OV02C10_TRY_SIBLING},
};
#define OV02C10_TRIES ((int)(sizeof(s_tries) / sizeof(s_tries[0])))
static char s_field_name[2][56];

// How many tries a Full mode has; 0 without an OV02C10.
int tessera_camera_field_tries(void)
{
    return s_detected ? OV02C10_TRIES : 0;
}

// The sensor format for Screen camera's field of view, 1 Full (binning) or 2 Full (skip), in its try `attempt`; NULL
// for any other, or when no OV02C10 answered. Built anew each time (the capture task asks right before it hands the
// format to the sensor).
const esp_cam_sensor_format_t *tessera_camera_field_format_try(int field, int attempt)
{
    if (!s_detected || field < 1 || field > 2 || attempt < 0 || attempt >= OV02C10_TRIES) {
        return NULL;
    }
    const int index = field - 1;
    const ov02c10_change_t *base = field == 1 ? s_binned_changes : s_skipped_changes;
    const size_t count = field == 1 ? sizeof(s_binned_changes) / sizeof(s_binned_changes[0])
                                    : sizeof(s_skipped_changes) / sizeof(s_skipped_changes[0]);
    _Static_assert(sizeof(s_binned_changes) / sizeof(s_binned_changes[0]) + 8 <= 32, "changes fit the used marks");
    ov02c10_change_t changes[32];
    const ov02c10_try_t *t = &s_tries[attempt];
    const bool row0 = (t->extra & OV02C10_TRY_ROW0) != 0;
    size_t n = 0;
    for (size_t c = 0; c < count; c++) {
        changes[n] = base[c];
        switch (changes[n].reg) {
        case 0x380a: changes[n].val = 0x02; break;   // 544
        case 0x380b: changes[n].val = 0x20; break;
        case 0x3811: changes[n].val = 4; break;
        case 0x3813: changes[n].val = row0 ? 2 : 0; break;
        case 0x3803: changes[n].val = row0 ? 0x00 : 0x04; break;
        case 0x3807: changes[n].val = row0 ? 0x47 : 0x43; break;
        default: break;
        }
        n++;
    }
    changes[n++] = (ov02c10_change_t){0x380e, (uint8_t)(t->vts >> 8)};
    changes[n++] = (ov02c10_change_t){0x380f, (uint8_t)(t->vts & 0xff)};
    if (t->extra & OV02C10_TRY_SIBLING) {
        changes[n++] = (ov02c10_change_t){0x3714, 0x28};
        changes[n++] = (ov02c10_change_t){0x4009, 0x11};
    }
    snprintf(s_field_name[index], sizeof(s_field_name[index]), "OV02C10_full_%s_960x544_vts%u%s%s",
             field == 1 ? "binned" : "skipped", (unsigned)t->vts, row0 ? "_row0" : "",
             (t->extra & OV02C10_TRY_SIBLING) ? "_sib" : "");
    return ov02c10_build_field(index, &ov02c10_format_info[1], changes, n, s_field_name[index], 960, 544);
}

// What the sensor says it does, for the log ten seconds after a change: 0x3820 (FORMAT1) and the window offsets as
// written, and the ISP's own account (table 6-19, ISP_TOP_36 to 53, 0x5024 to 0x5035): binning, flip and mirror, the
// array it reads, and the output's offset and size. The length written, 0 without an OV02C10.
int tessera_camera_field_report(char *out, size_t size)
{
    if (s_device == NULL || out == NULL || size == 0) {
        return 0;
    }
    uint8_t format1 = 0, x_win = 0, y_win = 0, r[18] = {0};
    esp_err_t ret = ov02c10_read(s_device->sccb_handle, OV02C10_REG_ROTATE, &format1);
    ret |= ov02c10_read(s_device->sccb_handle, OV02C10_REG_X_WIN_L, &x_win);
    ret |= ov02c10_read(s_device->sccb_handle, OV02C10_REG_Y_WIN_L, &y_win);
    for (int i = 0; i < 18; i++) {
        ret |= ov02c10_read(s_device->sccb_handle, 0x5024 + i, &r[i]);
    }
    if (ret != ESP_OK) {
        return snprintf(out, size, "the sensor did not answer");
    }
#define OV02C10_R11(hi, lo) ((unsigned)(((r[hi] & 0x07) << 8) | r[lo]))
    return snprintf(out, size,
                    "0x3820 0x%02X, isp_x_win %u, isp_y_win %u; the ISP: x_bin2 %u y_bin2 %u flip %u mirror %u, "
                    "array x %u..%u y %u..%u, output %ux%u at %u,%u",
                    format1, x_win, y_win, (r[0] >> 5) & 1u, (r[0] >> 4) & 1u, (r[0] >> 7) & 1u, (r[0] >> 6) & 1u,
                    OV02C10_R11(2, 3), OV02C10_R11(4, 5), OV02C10_R11(6, 7), OV02C10_R11(8, 9),
                    OV02C10_R11(12, 13), OV02C10_R11(16, 17), OV02C10_R11(10, 11), OV02C10_R11(14, 15));
#undef OV02C10_R11
}

esp_cam_sensor_device_t *ov02c10_detect(esp_cam_sensor_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }
    if (config->sensor_port != ESP_CAM_SENSOR_MIPI_CSI) {
        ESP_LOGE(TAG, "Not support DVP port");
        return NULL;
    }

    for (uint32_t i = 0; i < OV02C10_GAIN_COUNT; i++) {
        s_gain_values[i] = (i + OV02C10_GAIN_STEP_MIN) * 1000 / 16;
    }

    esp_cam_sensor_device_t *dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    ov02c10_state_t *state = calloc(1, sizeof(ov02c10_state_t));
    if (dev == NULL || state == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        free(dev);
        free(state);
        return NULL;
    }

    const esp_cam_sensor_format_t *format = &ov02c10_format_info[CONFIG_CAMERA_OV02C10_MIPI_IF_FORMAT_INDEX_DAFAULT];
    state->exposure_val = format->isp_info->isp_v1_info.exp_def;
    state->gain_index = format->isp_info->isp_v1_info.gain_def;

    dev->name = (char *)OV02C10_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &ov02c10_ops;
    dev->priv = state;
    dev->cur_format = format;

    // Configure sensor power, clock, and SCCB port
    if (ov02c10_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (ov02c10_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != OV02C10_PID) {
        ESP_LOGE(TAG, "Camera sensor is not OV02C10, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);
    s_detected = true;
    s_device = dev;

    return dev;

err_free_handler:
    ov02c10_power_off(dev);
    free(state);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_OV02C10_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(ov02c10_detect, ESP_CAM_SENSOR_MIPI_CSI, OV02C10_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return ov02c10_detect(config);
}
#endif
