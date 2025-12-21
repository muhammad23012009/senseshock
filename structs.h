/*
 * This file is part of senseshock (https://github.com/muhammad23012009/senseshock)
 * Copyright (c) 2025 Muhammad  <thevancedgamer@mentallysanemainliners.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

// All of the structures exchanged between the DualSense and the emulator
#include <cstdint>
#include <linux/types.h>

struct hid_class_descriptor {
    uint8_t bDescriptorType;
    __le16 wDescriptorLength;
} __attribute__((packed));

struct hid_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;

    __le16 bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;

    hid_class_descriptor rpt_desc;
   // hid_class_descriptor opt_desc[];
} __attribute__((packed));

// This structure represents a single touch point on the touchpad
struct touch_point {
    uint8_t contact;
    uint8_t x_low;
    uint8_t x_high:4, y_low:4;
    uint8_t y_high;
} __attribute__((packed));

// Input report sent by the DualShock 4 controller, we emulate this report
struct dualshock4_input_report {
    uint8_t report_id; // Will always be 0x01

    uint8_t x, y;
    uint8_t rx, ry;
    uint8_t buttons[3];
    uint8_t z, rz;

    __le16 sensor_timestamp;
    uint8_t sensor_temperature;
    __le16 gyro[3]; // xyz
    __le16 accel[3]; // xyz
    uint8_t reserved2[5];

    uint8_t status[2];
    uint8_t reserved3;

    uint8_t num_touch_reports;
    struct {
        uint8_t timestamp;
        touch_point points[2];
    } __attribute__((packed)) points[3];

    uint8_t reserved[3];
} __attribute__((packed));
const int dualshock4_input_report_size = sizeof(dualshock4_input_report);

// Input report sent by the DualSense controller, we convert this to the DS4 format
struct dualsense_input_report {
    uint8_t report_id; // 0x01 for USB, 0x31 for Bluetooth

    uint8_t x, y;
    uint8_t rx, ry;
    uint8_t z, rz;
    uint8_t seq_num;
    uint8_t buttons[4];
    uint8_t reserved[4];

    __le16 gyro[3]; // xyz
    __le16 accel[3]; // xyz
    __le32 sensor_timestamp;
    uint8_t reserved2;

    touch_point points[2];

    uint8_t reserved3[12];
    uint8_t status[3];
    uint8_t reserved4[8];
} __attribute__((packed));

/* DualShock 4 output report sent to us by the hid-playstation driver. */
struct dualshock4_output_report {
    uint8_t report_id; // Will always be 0x05

    uint8_t valid_flag0;
    uint8_t valid_flag1;

    uint8_t reserved;

    uint8_t motor_right;
    uint8_t motor_left;

    uint8_t lightbar_red;
    uint8_t lightbar_green;
    uint8_t lightbar_blue;
    uint8_t lightbar_blink_on;
    uint8_t lightbar_blink_off;
    uint8_t reserved2[21];
} __attribute__((packed));

// DualSense output report sent to the DualSense controller, we construct this from the DS4 output report
struct dualsense_output_report_common {
    uint8_t valid_flag0;
    uint8_t valid_flag1;

    /* For DualShock 4 compatibility mode. */
    uint8_t motor_right;
    uint8_t motor_left;

    /* Audio controls */
    uint8_t headphone_volume;  /* 0x0 - 0x7f */
    uint8_t speaker_volume;  /* 0x0 - 0xff */
    uint8_t mic_volume;    /* 0x0 - 0x40 */
    uint8_t audio_control;
    uint8_t mute_button_led;
    uint8_t power_save_control;
    uint8_t reserved2[27];
    uint8_t audio_control2;

    /* LEDs and lightbar */
    uint8_t valid_flag2;
    uint8_t reserved3[2];
    uint8_t lightbar_setup;
    uint8_t led_brightness;
    uint8_t player_leds;
    uint8_t lightbar_red;
    uint8_t lightbar_green;
    uint8_t lightbar_blue;
} __attribute__((packed));
static_assert(sizeof(dualsense_output_report_common) == 47, "dualsense_output_report_common size incorrect");

struct dualsense_output_report_usb {
    uint8_t report_id; // Will always be 0x02
    dualsense_output_report_common common;
    uint8_t reserved4[15];
} __attribute__((packed));
static_assert(sizeof(dualsense_output_report_usb) == 63, "dualsense_output_report_usb size incorrect");

struct dualsense_output_report_bt {
    uint8_t report_id; // Will always be 0x31
    uint8_t seq;
    uint8_t tag;

    dualsense_output_report_common common;
    uint8_t reserved4[24];
    __le32 crc32;
} __attribute__((packed));

static_assert(sizeof(dualsense_output_report_bt) == 78, "dualsense_output_report_bt size incorrect");

// Reversed-engineered from Wireshark dumps
struct dualshock_feature_report_firmware {
    uint8_t report_id; // Always 0xA3

    char build_date[16]; // ASCII build date, e.g. "Mar 25 2016"
    char build_time[16]; // ASCII build time, e.g. "12:34:56"

    uint16_t hw_version_major;
    uint16_t hw_version_minor;
    uint32_t fw_version_major;
    uint16_t fw_version_minor;

    uint16_t fw_series;
    uint32_t code_size;
} __attribute__((packed));

static_assert(sizeof(dualshock_feature_report_firmware) == 49, "dualshock_feature_report_firmware size incorrect");