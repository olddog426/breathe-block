#pragma once

#include <Arduino.h>

#include "LD6002.h"
#include "VitalSigns.h"

class RadarSensor {
 public:
  RadarSensor(HardwareSerial& serial, bool simulated);

  void begin(int rxPin, int txPin, uint32_t primaryBaud, uint32_t fallbackBaud);
  VitalSigns update(uint32_t nowMs);
  uint32_t activeBaud() const { return activeBaud_; }

  // Bench aid: force the simulated body into (or out of) sustained activation
  // so the detector and the interface can be exercised on demand.
  void setForcedActivation(bool active) { forcedActivation_ = active; }
  bool forcedActivation() const { return forcedActivation_; }
  bool simulated() const { return simulated_; }
  bool hasReceivedData() const { return lastPacketAtMs_ != 0; }

 private:
  VitalSigns simulatedReading(uint32_t nowMs);
  void startSerial(uint32_t baud);

  HardwareSerial& serial_;
  LD6002 parser_;
  bool simulated_;
  int rxPin_ = -1;
  int txPin_ = -1;
  uint32_t primaryBaud_ = 0;
  uint32_t fallbackBaud_ = 0;
  uint32_t activeBaud_ = 0;
  uint32_t startedAtMs_ = 0;
  uint32_t lastPacketAtMs_ = 0;
  bool triedFallback_ = false;
  bool forcedActivation_ = false;
  VitalSigns latest_;
};
