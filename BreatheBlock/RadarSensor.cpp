#include "RadarSensor.h"

#include <math.h>

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
  value.heartRate = 72.0f + 1.8f * sinf(seconds * 0.24f);
  value.breathRate = 14.0f + 0.7f * sinf(seconds * 0.13f + 0.8f);
  value.breathPhase = sinf(seconds * 1.2083f);
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
