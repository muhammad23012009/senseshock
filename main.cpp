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

#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <cstdint>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/mount.h>
#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include <linux/hid.h>
#include <linux/hidraw.h>

#include <libusb.h>

#include "senseshock.h"
#include "usbhid.h"
#include "structs.h"

static constexpr char ds4_firmware_build_date[] = "Mar 25 2016";
static constexpr char ds4_firmware_build_time[] = "12:00:00";

struct functionfs_descriptor {
    usb_functionfs_descs_head_v2 header;
    __le32 fs_count;
    __le32 hs_count;

    struct {
        usb_interface_descriptor intf;
        hid_descriptor hid;
        usb_endpoint_descriptor_no_audio ep_in;
        usb_endpoint_descriptor_no_audio ep_out;
    } __attribute__((packed)) descs[2];
} __attribute__((packed));

#define STR_INTERFACE_ "Source/Sink"

static const struct {
        struct usb_functionfs_strings_head header;
        struct {
                __le16 code;
                const char str1[sizeof STR_INTERFACE_];
        } __attribute__((packed)) lang0;
} __attribute__((packed)) strings = {
        .header = {
                .magic = FUNCTIONFS_STRINGS_MAGIC,
                .length = (sizeof strings),
                .str_count = (1),
                .lang_count = (1),
        },
        .lang0 = {
                __cpu_to_le16(0x0409), /* en-us */
                STR_INTERFACE_,
        },
};

// Descriptors copied from an actual DualShock 4
unsigned char descs[] = {
    #include "descriptors.inc"
};

DualShockEmulator::~DualShockEmulator()
{
    m_control_thread_running = false;
    m_io_thread_running = false;
    if (m_control_thread.joinable())
        m_control_thread.join();
    if (m_io_thread.joinable())
        m_io_thread.join();
    if (ep0_fd >= 0)
        ::close(ep0_fd);
    if (ep1_in_fd >= 0)
        ::close(ep1_in_fd);
    if (ep2_out_fd >= 0)
        ::close(ep2_out_fd);

    if (m_functionfs_setup) {
        // Detach the virtual USB device
        write_to_file("UDC", "");

        // Remove the strings
        ::rmdir((gadget_path + "/strings/0x409").c_str());

        ::umount("/dev/ffs-hidemu0");
        ::rmdir("/dev/ffs-hidemu0");

        ::remove((gadget_path + "/configs/c.1/ffs.hidemu0").c_str());
        ::rmdir((gadget_path + "/configs/c.1").c_str());

        // Get rid of the functions and configs
        ::rmdir((gadget_path + "/functions/ffs.hidemu0").c_str());

        // And finally, nuke the gadget
        ::rmdir(gadget_path.c_str());
    }

    delete m_hid;
}

void DualShockEmulator::setup_dualsense(HidEmulator *emulator)
{
    // Start the HID emulator
    m_hid = emulator;
    dualsense_setup = true;
}

void DualShockEmulator::dualsense_disconnected()
{
    dualsense_setup = false;
}

int DualShockEmulator::handle_get_report(uint8_t report_id, uint8_t *buffer)
{
    // Handle the reportID's and send appropriate responses

    std::cout << "Handling get_report for report ID: " << std::hex << (int)report_id << std::dec << "\n";

    switch (report_id) {
    case DS4_FEATURE_GYRO_CALIBRATION: {
        std::cout << "Forwarding gyro calibration request to DualSense\n";
        // Time to delegate this to the DualSense.
        m_hid->hid_get_feature(DS_FEATURE_GYRO_CALIBRATION, 3, DS_FEATURE_GYRO_CALIBRATION_LEN, buffer);        
        buffer[0] = DS4_FEATURE_GYRO_CALIBRATION; // Restore report ID
        return DS4_FEATURE_GYRO_CALIBRATION_LEN;
    }

    case DS4_FEATURE_PAIRING_INFO:
        // Copy the MAC address from the DualSense.
        std::cout << "Copying pairing info from the DualSense\n";
        m_hid->hid_get_feature(DS_FEATURE_PAIRING_INFO, 3, DS_FEATURE_PAIRING_INFO_LEN, buffer);
        buffer[0] = DS4_FEATURE_PAIRING_INFO;
        return DS4_FEATURE_PAIRING_INFO_LEN;

    case DS4_FEATURE_HW_FW_VERSION: {
        dualshock_feature_report_firmware ds4_firmware;
        std::memset(&ds4_firmware, 0, sizeof(ds4_firmware));

        ds4_firmware.report_id = DS4_FEATURE_HW_FW_VERSION;

        std::memcpy(ds4_firmware.build_date, ds4_firmware_build_date, sizeof(ds4_firmware_build_date));
        std::memcpy(ds4_firmware.build_time, ds4_firmware_build_time, sizeof(ds4_firmware_build_time));

        // Needed by some games, e.g. Detroit: Become Human sanity-checks hw_version
        ds4_firmware.hw_version_major = 0x0100;
        ds4_firmware.hw_version_minor = 0x6414;
        ds4_firmware.fw_version_major = 0x00000001;
        ds4_firmware.fw_version_minor = 0x7007;

        std::memcpy(buffer, &ds4_firmware, sizeof(ds4_firmware));

        return DS4_FEATURE_HW_FW_VERSION_LEN;
    }
    }

    return 1;
}

int DualShockEmulator::handle_set_report(uint8_t *buffer, size_t length)
{
    dualshock4_output_report *in_report = (dualshock4_output_report *)buffer;
    dualsense_output_report_common out_report;
    std::memset(&out_report, 0, sizeof(dualsense_output_report_common));

    /* Map the flags first and foremost. */
    if (in_report->valid_flag0 & 0x01) { // Motor update
        out_report.valid_flag0 |= (1 << 0) | (1 << 1); // Vibration v0
        out_report.motor_right = in_report->motor_right;
        out_report.motor_left = in_report->motor_left;
    }

    if (in_report->valid_flag0 & 0x02) { // Lightbar update
        out_report.valid_flag1 |= (1 << 2); // Lightbar update
        out_report.lightbar_red = in_report->lightbar_red;
        out_report.lightbar_green = in_report->lightbar_green;
        out_report.lightbar_blue = in_report->lightbar_blue;
    }

    m_hid->hid_send_report(out_report);

    return 0;
}

void DualShockEmulator::setup_ep0(void)
{
    uint8_t buffer[2048];
    uint8_t *p = buffer;
    int ret;

    // Create the gadget
    ::mkdir(gadget_path.c_str(), 0755);

    write_to_file("idVendor", "0x054c"); // Sony Corp
    write_to_file("idProduct", "0x09cc"); // DualShock 4
    write_to_file("bcdDevice", "0x0100");
    write_to_file("bcdUSB", "0x0200");
    write_to_file("bDeviceClass", "0x00");
    write_to_file("bDeviceSubClass", "0x00");
    write_to_file("bDeviceProtocol", "0x00");
    write_to_file("bMaxPacketSize0", "255");

    ::mkdir((gadget_path + "/strings/0x409").c_str(), 0755);
    write_to_file("strings/0x409/manufacturer", "Sony Interactive Entertainment");
    write_to_file("strings/0x409/product", "Wireless Controller");
    write_to_file("strings/0x409/serialnumber", "DS4EMU001");

    /* Setup a FunctionFS now. */
    if ((ret = ::mkdir((gadget_path + "/functions/ffs.hidemu0").c_str(), 0755)) < 0) {
        std::cerr << "Failed to create functionfs! " << errno << '\n';
        return;
    }
    ::mkdir((gadget_path + "/configs/c.1").c_str(), 0755);
    if ((ret = ::symlink((gadget_path + "/functions/ffs.hidemu0").c_str(), (gadget_path + "/configs/c.1/ffs.hidemu0").c_str())) < 0) {
        std::cerr << "Failed to link function to config " << errno << "\n";
        return;
    }
    ::mkdir("/dev/ffs-hidemu0", 0755);
    ::mount("hidemu0", "/dev/ffs-hidemu0", "functionfs", 0, NULL);

    /* Now we're ready to open ep0 */
    ep0_fd = ::open("/dev/ffs-hidemu0/ep0", O_RDWR|O_SYNC);
    if (ep0_fd < 0) {
        std::cerr << "Failed to open gadgetfs at " << gadget_path << "\n";
        return;
    }

    functionfs_descriptor descriptors = {
        .header = {
            .magic = htole32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
            .length = htole32(sizeof(descriptors)),
            .flags = FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC,
        },
        .fs_count = htole32(4),
        .hs_count = htole32(4),
        .descs = {
            // FS Descriptors
            {
                .intf = m_hid_interface,
                .hid = m_hid_desc,
                .ep_in = m_ep_in,
                .ep_out = m_ep_out,
            },
            // HS Descriptors
            {
                .intf = m_hid_interface,
                .hid = m_hid_desc,
                .ep_in = m_ep_in,
                .ep_out = m_ep_out,
            },
        },
    };

    /* Copy all the structs to the buffer. */
    if ((ret = ::write(ep0_fd, &descriptors, sizeof(descriptors))) < 0) {
        std::cerr << "Failed to write descriptors to ep0\n";
        return;
    }

    if ((ret = ::write(ep0_fd, &strings, sizeof(strings))) < 0) {
        std::cerr << "Failed to write strings to ep0 " << errno << "\n";
        return;
    }

    write_to_file("UDC", "dummy_udc.0");

    m_functionfs_setup = true;

    m_control_thread_running = true;
    m_control_thread = std::thread(&DualShockEmulator::handle_control_request, this);
    m_control_thread.detach();

    /* Initialize endpoints and start I/O thread */
    init_ep();
    m_io_thread_running = true;
    m_io_thread = std::thread(&DualShockEmulator::do_io, this);
    m_io_thread.detach();
}

int DualShockEmulator::init_ep()
{
    ep1_in_fd = ::open("/dev/ffs-hidemu0/ep1", O_RDWR);
    ep2_out_fd = ::open("/dev/ffs-hidemu0/ep2", O_RDWR);

    return 0;
}

void DualShockEmulator::handle_control_request(void)
{
    int ret, i;
    fd_set read_set;
    struct usb_functionfs_event event;

    while (m_control_thread_running)
    {
        FD_ZERO(&read_set);
        FD_SET(ep0_fd, &read_set);

        ::select(ep0_fd+1, &read_set, NULL, NULL, NULL);

        ret = ::read(ep0_fd, &event, sizeof(event));
        if (ret < 0) {
            return;
        }

        switch (event.type)
        {
        case FUNCTIONFS_ENABLE:
        case FUNCTIONFS_DISABLE:
            break;
        case FUNCTIONFS_SETUP:
            handle_setup_request(&event.u.setup);
            break;
        }
    }
}

void DualShockEmulator::handle_setup_request(usb_ctrlrequest* setup)
{
    int status;
    uint8_t buffer[512];

    switch (setup->bRequestType)
    {
    case (USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE):
        switch (setup->bRequest)
        {
        case USB_REQ_GET_DESCRIPTOR:
            if ((setup->wValue >> 8) == HID_DT_REPORT)
            {
                ::write(ep0_fd, descs, 507);
                return;
            }
        }
    case (USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE):
        switch (setup->bRequest)
        {
        case HID_REQ_GET_REPORT:
            // Send empty report
            status = handle_get_report((setup->wValue & 0xff), buffer);
            ::write(ep0_fd, buffer, status);
            return;
        }

        break;
    case (USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE):
        switch (setup->bRequest)
        {
        case HID_REQ_SET_IDLE:
            // ACK
            status = ::read(ep0_fd, &status, 0);
            buffer[0] = 0;
            ::write(ep0_fd, buffer, 1);
            return;
        case HID_REQ_SET_REPORT:
            // This is most likely wrong, but it works so I don't care :D
            status = ::read(ep0_fd, buffer, setup->wLength);
            m_hid->hid_set_feature(buffer[0], 3, setup->wLength, buffer);
            return;
        }
    }
}

void DualShockEmulator::do_io(void)
{
    uint8_t buffer[64];
    int ret;

    while (m_io_thread_running)
    {
        // Read from OUT endpoint
        ret = ::read(ep2_out_fd, buffer, sizeof(buffer));
        if (ret > 0) {
            handle_set_report(buffer, ret);
        }
    }
}

static bool s_running = true;
void signal_handler(int signum)
{
    std::cout << "Signal " << signum << " received, stopping...\n";
    s_running = false;
}

int main(void)
{
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        std::cerr << "Failed to set SIGINT handler\n";
        return 1;
    }

    DualShockEmulator *emulator = new DualShockEmulator();
    UsbHidEmulator *usb_hid = new UsbHidEmulator(emulator);

    usb_hid->search_for_device();

    while (!emulator->dualsense_setup && s_running) {
        // Process events for connection methods
        usb_hid->process_device_events();
    }

    if (!emulator->get_hid()) {
        delete usb_hid;
        delete emulator;

        return 1;
    }

    if (emulator->get_hid()->type() == HidEmulator::HID_USB) {
        std::cout << "DualSense connected over USB\n";
    }

    // Keep the main thread alive
    std::cout << "Starting DualShock Emulator\n";
    emulator->init_ep();

    emulator->get_hid()->start(emulator->get_in_fd());

    while (emulator->dualsense_setup && s_running) {
        emulator->get_hid()->process_device_events();
    }

    std::cout << "Exiting emulator\n";

    delete emulator;

    return 0;
}
