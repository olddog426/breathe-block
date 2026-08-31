#include "StressEngine.h"

#include <math.h>

StressEngine::StressEngine(const StressEngineConfig& config) : config_(config) {}

void StressEngine::reset() {
  calibrationStarted_ = false;
  baselineReady_ = false;
  calibrationStartedAtMs_ = 0;
  lastValidAtMs_ = 0;
  activationStartedAtMs_ = 0;
  cooldownStartedAtMs_ = 0;
  baselineSamples_ = 0;
  baselineHeartRate_ = 0.0f;
  baselineBreathRate_ = 0.0f;
  distanceEma_ = 0.0f;
  activationTiming_ = false;
  cooldownActive_ = false;
}

bool StressEngine::elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs) {
  return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}

float StressEngine::clamp(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

bool StressEngine::plausibleHeartRate(float value) {
  return isfinite(value) && value >= 38.0f && value <= 190.0f;
}

bool StressEngine::plausibleBreathRate(float value) {
  return isfinite(value) && value >= 5.0f && value <= 45.0f;
}

bool StressEngine::plausibleDistance(float value) {
  return isfinite(value) && value >= 15.0f && value <= 180.0f;
}

void StressEngine::noteSessionStarted(uint32_t nowMs) {
  cooldownActive_ = true;
  cooldownStartedAtMs_ = nowMs;
  activationTiming_ = false;
}

BodyAssessment StressEngine::update(const VitalSigns& signs, uint32_t nowMs) {
  BodyAssessment result;
  result.baselineReady = baselineReady_;
  result.baselineHeartRate = baselineHeartRate_;
  result.baselineBreathRate = baselineBreathRate_;

  if (cooldownActive_ && elapsed(nowMs, cooldownStartedAtMs_, config_.cooldownMs)) {
    cooldownActive_ = false;
  }

  const bool valid = signs.presence && plausibleHeartRate(signs.heartRate) &&
                     plausibleBreathRate(signs.breathRate);

  if (signs.fresh && valid) {
    lastValidAtMs_ = nowMs;

    if (!calibrationStarted_) {
      calibrationStarted_ = true;
      calibrationStartedAtMs_ = nowMs;
      baselineHeartRate_ = signs.heartRate;
      baselineBreathRate_ = signs.breathRate;
      baselineSamples_ = 1;
    } else if (!baselineReady_) {
      const float alpha = 0.08f;
      baselineHeartRate_ += alpha * (signs.heartRate - baselineHeartRate_);
      baselineBreathRate_ += alpha * (signs.breathRate - baselineBreathRate_);
      ++baselineSamples_;

      if (baselineSamples_ >= 20 &&
          elapsed(nowMs, calibrationStartedAtMs_, config_.calibrationMs)) {
        baselineReady_ = true;
      }
    }
  }

  if (lastValidAtMs_ == 0 || elapsed(nowMs, lastValidAtMs_, 6000)) {
    result.state = BodyState::WaitingForSignal;
    activationTiming_ = false;
    return result;
  }

  if (!baselineReady_) {
    result.state = BodyState::Calibrating;
    result.baselineHeartRate = baselineHeartRate_;
    result.baselineBreathRate = baselineBreathRate_;
    return result;
  }

  result.baselineReady = true;
  result.baselineHeartRate = baselineHeartRate_;
  result.baselineBreathRate = baselineBreathRate_;

  if (signs.fresh && plausibleDistance(signs.distanceCm)) {
    if (distanceEma_ <= 0.0f) distanceEma_ = signs.distanceCm;
    const float distanceChange = fabsf(signs.distanceCm - distanceEma_);
    result.movementQuiet = distanceChange < 7.0f;
    distanceEma_ += 0.15f * (signs.distanceCm - distanceEma_);
  }

  const float heartLoad = clamp(
      (signs.heartRate - baselineHeartRate_) / config_.heartRiseBpm, 0.0f, 2.0f);
  const float breathLoad = clamp(
      (signs.breathRate - baselineBreathRate_) / config_.breathRisePerMin, 0.0f, 2.0f);
  result.activationScore = 0.65f * heartLoad + 0.35f * breathLoad;

  if (cooldownActive_) {
    result.state = BodyState::Cooldown;
    return result;
  }

  const bool activated = result.activationScore >= config_.activationThreshold &&
                         result.movementQuiet;

  if (activated) {
    result.state = BodyState::BodyActivated;
    if (!activationTiming_) {
      activationTiming_ = true;
      activationStartedAtMs_ = nowMs;
    } else if (elapsed(nowMs, activationStartedAtMs_, config_.activationHoldMs)) {
      result.promptNow = true;
      noteSessionStarted(nowMs);
      result.state = BodyState::Cooldown;
    }
  } else {
    activationTiming_ = false;
    result.state = BodyState::Watching;

    // Allow the seated baseline to follow slow, genuinely calm changes while
    // resisting short spikes that should remain visible to the detector.
    if (signs.fresh && result.activationScore < 0.35f) {
      const float alpha = 0.004f;
      baselineHeartRate_ += alpha * (signs.heartRate - baselineHeartRate_);
      baselineBreathRate_ += alpha * (signs.breathRate - baselineBreathRate_);
    }
  }

  return result;
}
