#pragma once

#include <stdint.h>

namespace BreatheBlockConfig {

// Start here while the hardware is in transit. The visual will run with
// gentle simulated readings and automatically begin one breathing session.
constexpr bool kUseSimulatedRadar = true;
constexpr bool kAutoStartVisualDemo = true;

// The finished product stays quiet: vital-sign numbers are available only
// when intentionally enabled for bench testing.
constexpr bool kShowVitalsDuringTesting = false;

// Waveshare ESP32-S3-Touch-AMOLED-1.43 UART connector.
constexpr int kRadarRxPin = 44;  // Display RXD <- radar TX0
constexpr int kRadarTxPin = 43;  // Display TXD -> radar RX0
constexpr uint32_t kRadarPrimaryBaud = 1382400;
constexpr uint32_t kRadarFallbackBaud = 115200;

// The onboard BOOT button manually starts/stops a breathing session.
constexpr int kManualButtonPin = 0;

// Calm interaction timing.
constexpr uint32_t kInhaleMs = 4000;
constexpr uint32_t kExhaleMs = 6000;
constexpr uint8_t kBreathCycles = 5;
constexpr uint32_t kSettledMs = 3000;

// Conservative first-pass physiology logic. This never diagnoses stress;
// it only notices sustained activation relative to the seated baseline.
constexpr uint32_t kCalibrationMs = 90000;
constexpr uint32_t kActivationHoldMs = 75000;
constexpr uint32_t kPromptCooldownMs = 10UL * 60UL * 1000UL;

}  // namespace BreatheBlockConfig
