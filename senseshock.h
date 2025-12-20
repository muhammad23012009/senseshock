#pragma once

#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include <linux/hid.h>
#include <libusb.h>

#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <fcntl.h>
#include <unistd.h>

#include "hid.h"

#define DS_FEATURE_GYRO_CALIBRATION 0x05
#define DS_FEATURE_GYRO_CALIBRATION_LEN    41

#define DS_FEATURE_PAIRING_INFO     0x09
#define DS_FEATURE_PAIRING_INFO_LEN     20

#define DS4_FEATURE_GYRO_CALIBRATION 0x02
#define DS4_FEATURE_GYRO_CALIBRATION_LEN    37

#define DS4_FEATURE_PAIRING_INFO     0x12
#define DS4_FEATURE_PAIRING_INFO_LEN     16

#define DS4_FEATURE_HW_FW_VERSION    0xA3
#define DS4_FEATURE_HW_FW_VERSION_LEN    49

const std::string gadget_path = "/sys/kernel/config/usb_gadget/g99/";

class HidEmulator;

class DualShockEmulator
{
public:
    DualShockEmulator() {}
    ~DualShockEmulator();

    void setup_ep0();
    int init_ep();

    void setup_dualsense(HidEmulator *emulator);
    void dualsense_disconnected();

    int handle_get_report(uint8_t report_id, uint8_t *buffer);
    int handle_set_report(uint8_t *buffer, size_t length);

    void handle_control_request(void);
    void handle_setup_request(usb_ctrlrequest* setup);

    void do_io();

    int get_in_fd() const { return ep1_in_fd; }
    HidEmulator *get_hid() const { return m_hid; }

    std::atomic<bool> dualsense_setup = false;

private:
    void write_to_file(const std::string& path, const std::string& value) {
        int fd = ::open((gadget_path + path).c_str(), O_WRONLY);
        if (fd < 0) {
            std::cerr << "Failed to open " << path << " for writing\n";
            return;
        }
        ::write(fd, value.c_str(), value.size());
        ::close(fd);
    }

    int ep0_fd;

    // EP1 IN
    int ep1_in_fd = -1;
    // EP2 OUT
    int ep2_out_fd = -1;

    std::thread m_control_thread;
    bool m_control_thread_running = false;

    std::thread m_io_thread;
    bool m_io_thread_running = false;

    bool m_functionfs_setup = false;

    HidEmulator *m_hid = nullptr;

    usb_endpoint_descriptor_no_audio m_ep_in = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DIR_IN | 1, // IN endpoint 1
        .bmAttributes = USB_ENDPOINT_XFER_INT,
        .wMaxPacketSize = 512,
        .bInterval = 4, // 4ms as DualShock reports
    };

    usb_endpoint_descriptor_no_audio m_ep_out = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_DIR_OUT | 2, // OUT endpoint 2
        .bmAttributes = USB_ENDPOINT_XFER_INT,
        .wMaxPacketSize = 512,
        .bInterval = 4, // 4ms as DualShock reports
    };

    /* TODO: Implement the gyro interface too */
    usb_interface_descriptor m_hid_interface = {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = 0x03, // HID class
        .bInterfaceSubClass = 0x00,
        .bInterfaceProtocol = 0x00,
        .iInterface = 1,
    };

    hid_descriptor m_hid_desc = {
        .bLength = sizeof(hid_descriptor),
        .bDescriptorType = HID_DT_HID, // HID
        .bcdHID = __cpu_to_le16(0x0111), // HID 1.11
        .bCountryCode = 0x00,
        .bNumDescriptors = 1,
        .rpt_desc = {
            .bDescriptorType = HID_DT_REPORT,
            .wDescriptorLength = __cpu_to_le16(507), // Report descriptor size
        },
    };
};
