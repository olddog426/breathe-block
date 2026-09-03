#include "RadarSensor.h"

#include <math.h>

#include "AppConfig.h"

RadarSensor::RadarSensor(HardwareSerial& serial, bool simulated)
    : serial_(serial), parser_(serial), simulated_(simulated) {}

void RadarSensor::begin(int rxPin, int txPin, uint32_t primaryBaud,
                        uint32_t fallbackBaud) {
  rxPin_ = rxPin;
  txPin_ = txPin;
  primaryBaud_ = primaryBaud;
  fallbackBaud_ = fallbackBaud;
  startedAtMs_ = millis();
  if (!simulated_) startSerial(primaryBaud_);
}

void RadarSensor::startSerial(uint32_t baud) {
  serial_.end();
  delay(20);
  serial_.begin(baud, SERIAL_8N1, rxPin_, txPin_);
  activeBaud_ = baud;
  startedAtMs_ = millis();
}

VitalSigns RadarSensor::simulatedReading(uint32_t nowMs) {
  VitalSigns value;
  const float seconds = nowMs / 1000.0f;

  // A simulated body that occasionally lifts, so the radar -> detector ->
  // interface path can be watched end to end with nobody in the room.
  float activation = forcedActivation_ ? 1.0f : 0.0f;
  if (!forcedActivation_ && BreatheBlockConfig::kSimulatedActivationEveryMs) {
    const uint32_t phase =
        nowMs % BreatheBlockConfig::kSimulatedActivationEveryMs;
    const uint32_t length = BreatheBlockConfig::kSimulatedActivationLengthMs;
    if (phase < length) {
      // Ease in and out so the detector sees a sustained change, not a step.
      const float t = static_cast<float>(phase) / static_cast<float>(length);
      activation = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
      if (activation > 1.0f) activation = 1.0f;
    }
  }

  value.heartRate =
      72.0f + 1.8f * sinf(seconds * 0.24f) + 17.0f * activation;
  value.breathRate =
      14.0f + 0.7f * sinf(seconds * 0.13f + 0.8f) + 6.0f * activation;
  // Breathing speeds up a little as well, which is what the ambient ember
  // quietly follows.
  value.breathPhase = sinf(seconds * (1.2083f + 0.45f * activation));
  value.distanceCm = 72.0f + 0.6f * sinf(seconds * 0.08f);
  value.presence = true;
  value.fresh = true;
  value.breathPhaseFresh = true;
  value.observedAtMs = nowMs;
  return value;
}

VitalSigns RadarSensor::update(uint32_t nowMs) {
  if (simulated_) return simulatedReading(nowMs);

  latest_.fresh = false;
  latest_.breathPhaseFresh = false;
  parser_.update();
  bool packetReceived = false;

  if (parser_.hasNewBreathPhase()) {
    const float value = parser_.getBreathPhase();
    if (isfinite(value)) {
      latest_.breathPhase = value;
      latest_.breathPhaseFresh = true;
    }
    parser_.clearBreathPhaseFlag();
    packetReceived = true;
  }

  if (parser_.hasNewHeartRate()) {
    const float value = parser_.getHeartRate();
    if (isfinite(value) && value > 0.0f) latest_.heartRate = value;
    parser_.clearHeartRateFlag();
    latest_.fresh = true;
    packetReceived = true;
  }
  if (parser_.hasNewBreathRate()) {
    const float value = parser_.getBreathRate();
    if (isfinite(value) && value > 0.0f) latest_.breathRate = value;
    parser_.clearBreathRateFlag();
    latest_.fresh = true;
    packetReceived = true;
  }
  if (parser_.hasNewDistance()) {
    const float value = parser_.getDistance();
    if (isfinite(value) && value > 0.0f) latest_.distanceCm = value;
    parser_.clearDistanceFlag();
    latest_.fresh = true;
    packetReceived = true;
  }

  if (packetReceived) {
    lastPacketAtMs_ = nowMs;
    latest_.observedAtMs = nowMs;
  }
  latest_.presence = lastPacketAtMs_ != 0 &&
                     static_cast<uint32_t>(nowMs - lastPacketAtMs_) < 5000;

  // A small number of LD6002 units ship at 115200 rather than the documented
  // 1,382,400 baud. Try that once automatically if no valid packet arrives.
  if (!triedFallback_ && lastPacketAtMs_ == 0 &&
      static_cast<uint32_t>(nowMs - startedAtMs_) > 6000) {
    triedFallback_ = true;
    startSerial(fallbackBaud_);
  }

  return latest_;
}
