#pragma once

#include <Arduino.h>

// Polls the board's FT3168 capacitive touch controller over I2C and turns
// its raw press/release stream into a single, deliberate gesture: a tap.
//
// The FT3168 follows the common FocalTech FT5x06-family register layout
// (TD_STATUS at 0x02, then a 4-byte burst of X/Y at 0x03-0x06 per touch
// point) used across most of that vendor's capacitive controllers. This has
// not been checked against the FT3168's own datasheet line by line — if taps
// never register on real hardware, this register map is the first thing to
// verify with a logic analyzer or a known-good vendor example.
class TouchSensor {
 public:
  void begin(int sdaPin, int sclPin, uint8_t address, uint32_t freqHz);

  // Returns true exactly once, on the frame a complete tap is recognised: a
  // press and release close together, without much drift in between. Safe to
  // call every loop iteration; a single I2C read failure is treated as "no
  // tap" rather than let a communication hiccup strand a gesture mid-press.
  bool pollTap(uint32_t nowMs);

 private:
  enum class Phase : uint8_t { Idle, Down };

  bool readRegisters(uint8_t startReg, uint8_t* buffer, size_t length);

  uint8_t address_ = 0;
  Phase phase_ = Phase::Idle;
  int16_t startX_ = 0;
  int16_t startY_ = 0;
  int16_t maxDriftPx_ = 0;
  uint32_t downAtMs_ = 0;
};
