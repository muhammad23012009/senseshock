#pragma once

#include "hid.h"
#include "senseshock.h"

class UsbHidEmulator : public HidEmulator
{
public:
    UsbHidEmulator(DualShockEmulator *emulator);
    ~UsbHidEmulator();

    HidType type() const override { return HID_USB; }

    void start(int output_fd) override;

    void hid_get_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *out) override;
    void hid_set_feature(uint8_t report_number, uint8_t interface_number, uint8_t len, uint8_t *in) override;

    void hid_send_report(dualsense_output_report_common report) override;

    void search_for_device() override;
    void stop_device_search() override;
    void process_device_events() override;

    void set_device(libusb_device *device) { m_device = device; }

private:
    libusb_device *m_device;
    libusb_device_handle *m_device_handle;
};
