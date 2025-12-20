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

#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>
#include <linux/hid.h>
#include <libusb.h>

#include <cstdint>
#include <thread>
#include <functional>

#include "structs.h"

class DualShockEmulator;

const int charging_status_bitmask = 0b11110000;

static dualshock4_input_report ds_to_ds4_input(dualsense_input_report *in_report)
{
    dualshock4_input_report out_report = {0};

    out_report.report_id = 0x01;
    out_report.x = in_report->x;
    out_report.y = in_report->y;
    out_report.rx = in_report->rx;
    out_report.ry = in_report->ry;
    out_report.z = in_report->z;
    out_report.rz = in_report->rz;

    out_report.buttons[0] = in_report->buttons[0];
    out_report.buttons[1] = in_report->buttons[1];
    out_report.buttons[2] = in_report->buttons[2];

    // DualSense reports timestamps in units of 0.33us, but DualShock 4 uses 5.33us
    // TODO: Should we round this instead of flooring it?
    out_report.sensor_timestamp = in_report->sensor_timestamp / 16;
    out_report.sensor_temperature = 0; // TODO
    out_report.gyro[0] = in_report->gyro[0];
    out_report.gyro[1] = in_report->gyro[1];
    out_report.gyro[2] = in_report->gyro[2];
    out_report.accel[0] = in_report->accel[0];
    out_report.accel[1] = in_report->accel[1];
    out_report.accel[2] = in_report->accel[2];
    out_report.num_touch_reports = 1;
    out_report.points[0].timestamp = 0; // TODO
    out_report.points[0].points[0] = in_report->points[0];
    out_report.points[0].points[1] = in_report->points[1];

    // Copy status bits as needed
    out_report.status[0] |= in_report->status[0] & 0b00001111; // Bits 0-3 store the battery level

    // The DualSense reports extensive charging states, however the DualShock 4
    // only has one state, charging or not charging, represented by the fourth bit in status[0].
    // 0x01 in bits 4-7 of the DualSense report means charging, 0x02 means fully charged.
    out_report.status[0] |= (((in_report->status[0] & charging_status_bitmask) >> 4) == 0x01) ? (1 << 4) : 0;
    out_report.status[0] |= (((in_report->status[0] & charging_status_bitmask) >> 4) == 0x02) ? 0b00001111 : 0;

    out_report.status[1] = in_report->status[1];

    return out_report;
}

/* Responsible for emulating the HID protocol */
class HidEmulator
{
public:
    enum HidType {
        HID_USB,
        HID_BLUETOOTH
    };

    HidEmulator(DualShockEmulator *emulator):
      m_dualshock_emulator(emulator)
    {}

    virtual ~HidEmulator() = default;

    virtual HidType type() const = 0;

    virtual void start(int output_fd) = 0;

    virtual void hid_get_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *out) = 0;
    virtual void hid_set_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *in) = 0;

    virtual void hid_send_report(dualsense_output_report_common report) = 0;

    virtual void search_for_device() = 0;
    virtual void stop_device_search() = 0;
    virtual void process_device_events() = 0;

    DualShockEmulator *get_emulator() { return m_dualshock_emulator; }

protected:
    DualShockEmulator *m_dualshock_emulator;
    int m_out_fd = -1;
};
