#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "BreathingUI.h"
#include "RadarSensor.h"
#include "StressEngine.h"

// Everything needed to review and exercise the interface without a radar,
// and eventually without a person: a looping tour of the whole state flow, a
// button that steps through it by hand, and a one-character serial console.
class DemoDirector {
 public:
  DemoDirector(BreathingUI& ui, RadarSensor& radar, StressEngine& engine);

  void begin(uint32_t nowMs);
  void update(uint32_t nowMs);

 private:
  enum class TourStep : uint8_t {
    Settle,
    NoticedSession,
    Breathe,
    ManualSession,
    Drift,
    Sleep,
    Wake,
    Count,
  };

  void handleButton(uint32_t nowMs);
  void handleSerial(uint32_t nowMs);
  void runTour(uint32_t nowMs);
  void nextPalette();
  void printHelp() const;
  void announce(const char* what) const;

  BreathingUI& ui_;
  RadarSensor& radar_;
  StressEngine& engine_;

  TourStep step_ = TourStep::Settle;
  uint32_t stepAtMs_ = 0;
  bool tourRunning_ = false;
  bool buttonDown_ = false;
  uint32_t buttonPressedAtMs_ = 0;
  uint8_t paletteIndex_ = 0;
};
