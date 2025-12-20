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

#include "usbhid.h"
#include "structs.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <iostream>

// Opens a raw USB interface and exchanges HID packets
UsbHidEmulator::UsbHidEmulator(DualShockEmulator *emulator):
  HidEmulator(emulator)
{
}

UsbHidEmulator::~UsbHidEmulator()
{
    if (m_device_handle) {
        libusb_release_interface(m_device_handle, 3);
        libusb_close(m_device_handle);
    }

    libusb_exit(NULL);

    m_device_handle = nullptr;
}

void UsbHidEmulator::start(int output_fd)
{
    m_out_fd = output_fd;

    if (!libusb_open(m_device, &m_device_handle)) {
        // Successfully opened
        std::cout << "Opened libusb device for HID emulation\n";
        libusb_set_auto_detach_kernel_driver(m_device_handle, 1);
        libusb_claim_interface(m_device_handle, 3); // HID interface is at interface 3
    } else {
        m_device_handle = nullptr;
    }
}

void UsbHidEmulator::hid_get_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *out)
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

void UsbHidEmulator::hid_set_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *in)
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

void UsbHidEmulator::hid_send_report(dualsense_output_report_common report)
{
    // HID_OUTPUT_REPORT is over the OUT endpoint
    dualsense_output_report_usb out = {0};
    std::memcpy(&out.common, &report, sizeof(dualsense_output_report_common));
    out.report_id = 0x02;

    int written = 0;
    libusb_interrupt_transfer(
        m_device_handle,
        0x03, // OUT endpoint 3
        (uint8_t*)&out,
        sizeof(out),
        &written,
        1000
    );
}

static libusb_hotplug_callback_handle s_cb_handle;
static int usb_hotplug_cb(libusb_context *ctx, libusb_device *device,
                    libusb_hotplug_event event, void *user_data)
{
    UsbHidEmulator *emulator = static_cast<UsbHidEmulator *>(user_data);

    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(device, &desc);

        // Sony DualSense VID/PID
        if (desc.idVendor == 0x054c && desc.idProduct == 0x0ce6) {
            emulator->set_device(device);
            emulator->get_emulator()->setup_ep0();
            emulator->get_emulator()->setup_dualsense(emulator);
        }
    } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        // Device disconnected
        std::cout << "DualSense disconnected\n";
        libusb_hotplug_deregister_callback(NULL, s_cb_handle);
        emulator->get_emulator()->dualsense_disconnected();
        return 1;
    }

    return 0;
}

// Starts an asynchronous search for a compatible USB device
void UsbHidEmulator::search_for_device()
{
    libusb_init_context(NULL, NULL, 0);

    if (!libusb_hotplug_register_callback(NULL, 
            LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
            LIBUSB_HOTPLUG_ENUMERATE,
            0x054c, 0x0ce6, LIBUSB_HOTPLUG_MATCH_ANY,
            usb_hotplug_cb, this, &s_cb_handle)) {
        std::cout << "Registered hotplug callback\n";
    }
}

void UsbHidEmulator::stop_device_search()
{
}

static uint8_t s_buffer[64];
static int s_transferred = 0;
static struct timeval s_timeval = {0, 0};

void UsbHidEmulator::process_device_events()
{
    libusb_handle_events_timeout(nullptr, &s_timeval);

    if (!m_device_handle)
        return;

    int ret = libusb_interrupt_transfer(
                m_device_handle,
                0x84, // IN endpoint 4
                s_buffer,
                sizeof(s_buffer),
                &s_transferred,
                1000
            );

    if (ret == 0 && s_transferred > 0) {
        dualsense_input_report *in_report = (dualsense_input_report *)s_buffer;
        dualshock4_input_report out_report = ds_to_ds4_input(in_report);
        ::write(m_out_fd, &out_report, dualshock4_input_report_size);
    }
}
