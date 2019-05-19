#pragma once

#include <stdint.h>


#define kMaxInputReportSizeBytes  64

// Report IDs.
static const uint8_t kReportIdOutput01 = 0x01;
static const uint8_t kReportIdOutput10 = 0x10;
static const uint8_t kReportIdInput21 = 0x21;
static const uint8_t kReportIdInput30 = 0x30;
static const uint8_t kUsbReportIdOutput80 = 0x80;
static const uint8_t kUsbReportIdInput81 = 0x81;

// Sub-types of the 0x80 output report, used for initialization.
static const uint8_t kSubTypeRequestMac = 0x01;
static const uint8_t kSubTypeHandshake = 0x02;
static const uint8_t kSubTypeBaudRate = 0x03;
static const uint8_t kSubTypeDisableUsbTimeout = 0x04;
static const uint8_t kSubTypeEnableUsbTimeout = 0x05;

// UART subcommands.
static const uint8_t kSubCommandSetInputReportMode = 0x03;
static const uint8_t kSubCommandReadSpi = 0x10;
static const uint8_t kSubCommandSetPlayerLights = 0x30;
static const uint8_t kSubCommand33 = 0x33;
static const uint8_t kSubCommandSetHomeLight = 0x38;
static const uint8_t kSubCommandEnableImu = 0x40;
static const uint8_t kSubCommandSetImuSensitivity = 0x41;
static const uint8_t kSubCommandEnableVibration = 0x48;

// SPI memory regions.
static const uint16_t kSpiImuCalibrationAddress = 0x6020;
static const uint32_t kSpiImuCalibrationSize = 24;
static const uint16_t kSpiAnalogStickCalibrationAddress = 0x603d;
static const uint32_t kSpiAnalogStickCalibrationSize = 18;
static const uint16_t kSpiImuHorizontalOffsetsAddress = 0x6080;
static const uint32_t kSpiImuHorizontalOffsetsSize = 6;
static const uint16_t kSpiAnalogStickParametersAddress = 0x6086;
static const uint32_t kSpiAnalogStickParametersSize = 18;

// Byte index for the first byte of subcommand data in 0x80 output reports.
static const uint32_t kSubCommandDataOffset = 11;
// Byte index for the first byte of SPI data in SPI read responses.
#define kSpiDataOffset 20

// Values for the |device_type| field reported in the MAC reply.
static const uint8_t kUsbDeviceTypeChargingGripNoDevice = 0x00;
static const uint8_t kUsbDeviceTypeChargingGripJoyConL = 0x01;
static const uint8_t kUsbDeviceTypeChargingGripJoyConR = 0x02;
static const uint8_t kUsbDeviceTypeProController = 0x03;

static const uint32_t kMaxRetryCount = 3;

static const uint32_t kMaxVibrationEffectDurationMillis = 100;

// Initialization parameters.
static const uint8_t kGyroSensitivity2000Dps = 0x03;
static const uint8_t kAccelerometerSensitivity8G = 0x00;
static const uint8_t kGyroPerformance208Hz = 0x01;
static const uint8_t kAccelerometerFilterBandwidth100Hz = 0x01;
static const uint8_t kPlayerLightPattern1 = 0x01;

// Parameters for the "strong" and "weak" components of the dual-rumble effect.
static const double kVibrationFrequencyStrongRumble = 141.0;
static const double kVibrationFrequencyWeakRumble = 182.0;
static const double kVibrationAmplitudeStrongRumbleMax = 0.9;
static const double kVibrationAmplitudeWeakRumbleMax = 0.1;

static const int kVibrationFrequencyHzMin = 41;
static const int kVibrationFrequencyHzMax = 1253;
static const int kVibrationAmplitudeMax = 1000;

#pragma pack(1)
struct ControllerData {
  uint8_t timestamp;
  uint8_t battery_level : 4;
  uint8_t connection_info : 4;
  // btn right - 8bit
  uint8_t button_y : 1;
  uint8_t button_x : 1;
  uint8_t button_b : 1;
  uint8_t button_a : 1;
  uint8_t button_right_sr : 1;
  uint8_t button_right_sl : 1;
  uint8_t button_r : 1;
  uint8_t button_zr : 1;
  // btn common - 8bit
  uint8_t button_minus : 1;
  uint8_t button_plus : 1;
  uint8_t button_thumb_r : 1;
  uint8_t button_thumb_l : 1;
  uint8_t button_home : 1;
  uint8_t button_capture : 1;
  uint8_t dummy : 1;
  uint8_t charging_grip : 1;
  // btn left - 8 bit
  uint8_t dpad_down : 1;
  uint8_t dpad_up : 1;
  uint8_t dpad_right : 1;
  uint8_t dpad_left : 1;
  uint8_t button_left_sr : 1;
  uint8_t button_left_sl : 1;
  uint8_t button_l : 1;
  uint8_t button_zl : 1;
  // analog - 48
  uint8_t analog[6];
  uint8_t vibrator_input_report;
};

// In standard full input report mode, controller data is reported with IMU data
// in reports with ID 0x30.
#pragma pack(1)
struct ControllerDataReport {
  struct ControllerData controller_data;  // 12 bytes
  uint8_t imu_data[36];
  uint8_t padding[kMaxInputReportSizeBytes - 49 /* 36 + 12 +1(reportid ?)?*/];
};

// Responses to SPI read requests are sent in reports with ID 0x21. These
// reports also include controller data.
#pragma pack(1)
struct SpiReadReport {
  struct ControllerData controller_data;  // 12 bytes
  uint8_t subcommand_ack;          // 0x90
  uint8_t subcommand;              // 0x10
  uint8_t addrl;
  uint8_t addrh;
  uint8_t padding[2];  // 0x00 0x00
  uint8_t length;
  uint8_t spi_data[kMaxInputReportSizeBytes - kSpiDataOffset];
};
