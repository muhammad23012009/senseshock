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

#include "hid.h"
#include "structs.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <iostream>

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

// Opens a raw USB interface and exchanges HID packets
HidEmulator::HidEmulator(libusb_device *device, int output_fd):
  m_out_fd(output_fd)
{
    // Open the device
    if (!libusb_open(device, &m_device_handle)) {
        // Successfully opened
        std::cout << "Opened libusb device for HID emulation\n";
        libusb_set_auto_detach_kernel_driver(m_device_handle, 1);
        libusb_claim_interface(m_device_handle, 3); // HID interface is at interface 3

        // Start the HID parsing thread
        m_hid_thread_running = true;
        m_hid_thread = std::thread(&HidEmulator::parse_hid_packet, this);
        m_hid_thread.detach();
    } else {
        m_device_handle = nullptr;
    }
}

HidEmulator::~HidEmulator()
{
    m_hid_thread_running = false;
    if (m_hid_thread.joinable())
        m_hid_thread.join();

    libusb_release_interface(m_device_handle, 3);
    libusb_close(m_device_handle);
}

// TODO: Eventually move this into the while loop in main.cpp
void HidEmulator::parse_hid_packet()
{
    uint8_t buffer[64];
    int transferred = 0;

    while (m_hid_thread_running) {
        // Read from the HID IN endpoint
        int ret = libusb_interrupt_transfer(
            m_device_handle,
            0x84, // IN endpoint 3
            buffer,
            sizeof(buffer),
            &transferred,
            1000
        );

        if (ret == 0 && transferred > 0) {
            dualsense_input_report *in_report = (dualsense_input_report *)buffer;
            dualshock4_input_report out_report = ds_to_ds4_input(in_report);
            ::write(m_out_fd, &out_report, dualshock4_input_report_size);
        }
    }
}

void HidEmulator::hid_get_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *out)
{
    libusb_control_transfer(
        m_device_handle,
        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        HID_REQ_GET_REPORT,
        (HID_FEATURE_REPORT + 1) << 8 | report_number,
        interface_number,
        out,
        len,
        1000
    );
}

void HidEmulator::hid_set_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *in)
{
    libusb_control_transfer(
        m_device_handle,
        LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        HID_REQ_SET_REPORT,
        (HID_FEATURE_REPORT + 1) << 8 | report_number,
        interface_number,
        in,
        len,
        1000
    );
}

void HidEmulator::hid_send_report(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *in)
{
    // HID_OUTPUT_REPORT is over the OUT endpoint
    int written = 0;
    libusb_interrupt_transfer(
        m_device_handle,
        0x03, // OUT endpoint 3
        in,
        len,
        &written,
        1000
    );

    std::cout << "Wrote " << written << " bytes to OUT endpoint\n";
}
