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

/* Responsible for emulating the HID protocol */
class HidEmulator
{
public:
    HidEmulator() {}
    HidEmulator(libusb_device *device, int output_fd);
    ~HidEmulator();

    void parse_hid_packet();

    void hid_get_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *out);
    void hid_set_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *in);

    void hid_send_report(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *in);

private:
    libusb_device_handle *m_device_handle;

    int m_out_fd = -1;

    bool m_hid_thread_running = false;
    std::thread m_hid_thread;
    std::function<void()> m_callback;
};
