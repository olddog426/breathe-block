#pragma once

#include <stdint.h>

#include "BreathField.h"

namespace BreatheBlockConfig {

// ---------------------------------------------------------------------------
// What the device is doing today
// ---------------------------------------------------------------------------

// Simulated readings let the whole interface, including the detection path,
// run with no radar attached.
constexpr bool kUseSimulatedRadar = true;

enum class DemoMode : uint8_t {
  Off,     // behave like the product: wait for the radar to notice something
  Tour,    // walk the entire state flow on a loop, for design review
  Manual,  // BOOT button steps through the scenes one at a time
};
constexpr DemoMode kDemoMode = DemoMode::Tour;

// Single-character commands over USB serial (send '?' for the list).
constexpr bool kSerialConsole = true;

// Vital-sign numbers are a bench-testing aid and never part of the product.
constexpr bool kShowVitalsDuringTesting = false;

// ---------------------------------------------------------------------------
// Voice
// ---------------------------------------------------------------------------
// Descriptive, reversible, never diagnostic. Change the product's voice here.
constexpr const char* kTextShifted = "your breathing\nhas shifted";
constexpr const char* kTextInvite = "breathe with me";
constexpr const char* kTextInhale = "inhale";
constexpr const char* kTextExhale = "exhale";
constexpr const char* kTextSettled = "settled";

// ---------------------------------------------------------------------------
// Look
// ---------------------------------------------------------------------------
// kIvory (warm centre, sage halo), kMoonlight (cooler), kEmber (candlelight).
constexpr BreathPalette kPalette = BreathPalettes::kIvory;

// A hairline arc traces the rim during guidance: how much is left, with no
// number and no countdown. Off by default: the contour itself stays a
// complete, continuous circle throughout — nothing ever reads as partial.
constexpr bool kShowSessionProgress = false;

// ---------------------------------------------------------------------------
// Interaction timing
// ---------------------------------------------------------------------------
constexpr uint32_t kInhaleMs = 4000;
constexpr uint32_t kExhaleMs = 6000;
constexpr uint8_t kBreathCycles = 5;
constexpr uint8_t kDemoBreathCycles = 3;  // keeps the tour loop watchable
constexpr uint32_t kSleepAfterMs = 180000;

constexpr uint8_t breathCycles() {
  return kDemoMode == DemoMode::Tour ? kDemoBreathCycles : kBreathCycles;
}

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
// Waveshare ESP32-S3-Touch-AMOLED-1.43 UART connector.
constexpr int kRadarRxPin = 44;  // display RXD <- radar TX0
constexpr int kRadarTxPin = 43;  // display TXD -> radar RX0
constexpr uint32_t kRadarPrimaryBaud = 1382400;
constexpr uint32_t kRadarFallbackBaud = 115200;

// The onboard BOOT button. Short press starts or dismisses a session; in
// DemoMode::Manual it steps to the next scene instead.
constexpr int kManualButtonPin = 0;

// ---------------------------------------------------------------------------
// Noticing
// ---------------------------------------------------------------------------
// Conservative on purpose. This describes sustained activation relative to
// your own seated baseline; it never diagnoses anything.
constexpr uint32_t kCalibrationMs = 90000;
constexpr uint32_t kActivationHoldMs = 75000;
constexpr uint32_t kPromptCooldownMs = 10UL * 60UL * 1000UL;

// With a simulated radar the same thresholds would take minutes to reach, so
// the detection path is compressed for bench work only.
constexpr uint32_t kSimulatedCalibrationMs = 12000;
constexpr uint32_t kSimulatedActivationHoldMs = 8000;
constexpr uint32_t kSimulatedCooldownMs = 40000;

// The simulated body lifts its rate every so often, so the radar → detector →
// interface path can be exercised end to end without a person.
constexpr uint32_t kSimulatedActivationEveryMs = 75000;
constexpr uint32_t kSimulatedActivationLengthMs = 25000;

}  // namespace BreatheBlockConfig
