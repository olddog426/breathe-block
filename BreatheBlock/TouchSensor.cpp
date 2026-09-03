#include "TouchSensor.h"

#include <math.h>

#include <Wire.h>

namespace {

// FT5x06-family registers (see the header for the caveat on FT3168 itself).
constexpr uint8_t kRegTdStatus = 0x02;   // bits 3:0 = number of touch points
constexpr uint8_t kRegTouch1XH = 0x03;   // 4-byte burst: XH, XL, YH, YL

// A tap, not a hold and not noise: held for a plausible finger-press
// duration, close to where it started.
constexpr uint32_t kMinTapMs = 15;
constexpr uint32_t kMaxTapMs = 700;
constexpr int16_t kMaxDriftPx = 40;

}  // namespace

void TouchSensor::begin(int sdaPin, int sclPin, uint8_t address,
                        uint32_t freqHz) {
  address_ = address;
  Wire.begin(sdaPin, sclPin, freqHz);
}

bool TouchSensor::readRegisters(uint8_t startReg, uint8_t* buffer,
                                size_t length) {
  Wire.beginTransmission(address_);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start

  if (Wire.requestFrom(static_cast<int>(address_), static_cast<int>(length)) !=
      static_cast<int>(length)) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) buffer[i] = Wire.read();
  return true;
}

bool TouchSensor::pollTap(uint32_t nowMs) {
  uint8_t status = 0;
  if (!readRegisters(kRegTdStatus, &status, 1)) return false;
  const bool touching = (status & 0x0F) != 0;

  if (phase_ == Phase::Idle) {
    if (!touching) return false;

    uint8_t point[4] = {0, 0, 0, 0};
    if (!readRegisters(kRegTouch1XH, point, sizeof(point))) return false;
    startX_ = static_cast<int16_t>(((point[0] & 0x0F) << 8) | point[1]);
    startY_ = static_cast<int16_t>(((point[2] & 0x0F) << 8) | point[3]);
    maxDriftPx_ = 0;
    downAtMs_ = nowMs;
    phase_ = Phase::Down;
    return false;
  }

  // Phase::Down.
  if (touching) {
    uint8_t point[4] = {0, 0, 0, 0};
    if (readRegisters(kRegTouch1XH, point, sizeof(point))) {
      const int16_t x = static_cast<int16_t>(((point[0] & 0x0F) << 8) | point[1]);
      const int16_t y = static_cast<int16_t>(((point[2] & 0x0F) << 8) | point[3]);
      const int16_t dx = static_cast<int16_t>(x - startX_);
      const int16_t dy = static_cast<int16_t>(y - startY_);
      const int16_t drift =
          static_cast<int16_t>(lroundf(sqrtf(static_cast<float>(dx) * dx +
                                             static_cast<float>(dy) * dy)));
      if (drift > maxDriftPx_) maxDriftPx_ = drift;
    }
    return false;
  }

  // Released: decide whether what just happened was a tap.
  phase_ = Phase::Idle;
  const uint32_t heldMs = static_cast<uint32_t>(nowMs - downAtMs_);
  return heldMs >= kMinTapMs && heldMs <= kMaxTapMs &&
         maxDriftPx_ <= kMaxDriftPx;
}
