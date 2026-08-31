#pragma once

#include <stdint.h>

#include "VitalSigns.h"

enum class BodyState : uint8_t {
  WaitingForSignal,
  Calibrating,
  Watching,
  BodyActivated,
  Cooldown,
};

struct StressEngineConfig {
  uint32_t calibrationMs = 90000;
  uint32_t activationHoldMs = 75000;
  uint32_t cooldownMs = 600000;
  float heartRiseBpm = 12.0f;
  float breathRisePerMin = 4.0f;
  float activationThreshold = 1.0f;
};

struct BodyAssessment {
  BodyState state = BodyState::WaitingForSignal;
  bool promptNow = false;
  bool baselineReady = false;
  bool movementQuiet = true;
  float activationScore = 0.0f;
  float baselineHeartRate = 0.0f;
  float baselineBreathRate = 0.0f;
};

class StressEngine {
 public:
  explicit StressEngine(const StressEngineConfig& config = StressEngineConfig());

  BodyAssessment update(const VitalSigns& signs, uint32_t nowMs);
  void noteSessionStarted(uint32_t nowMs);
  void reset();

 private:
  static bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs);
  static float clamp(float value, float low, float high);
  static bool plausibleHeartRate(float value);
  static bool plausibleBreathRate(float value);
  static bool plausibleDistance(float value);

  StressEngineConfig config_;
  bool calibrationStarted_ = false;
  bool baselineReady_ = false;
  uint32_t calibrationStartedAtMs_ = 0;
  uint32_t lastValidAtMs_ = 0;
  uint32_t activationStartedAtMs_ = 0;
  uint32_t cooldownStartedAtMs_ = 0;
  uint16_t baselineSamples_ = 0;
  float baselineHeartRate_ = 0.0f;
  float baselineBreathRate_ = 0.0f;
  float distanceEma_ = 0.0f;
  bool activationTiming_ = false;
  bool cooldownActive_ = false;
};
