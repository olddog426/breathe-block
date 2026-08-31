#include <cassert>
#include <cstdint>
#include <iostream>

#include "StressEngine.h"

namespace {
VitalSigns sample(float heart, float breath, float distance = 70.0f) {
  VitalSigns signs;
  signs.heartRate = heart;
  signs.breathRate = breath;
  signs.distanceCm = distance;
  signs.presence = true;
  signs.fresh = true;
  return signs;
}
}  // namespace

int main() {
  StressEngineConfig config;
  config.calibrationMs = 2000;
  config.activationHoldMs = 1000;
  config.cooldownMs = 3000;

  StressEngine engine(config);

  BodyAssessment assessment;
  for (uint32_t now = 100; now <= 2500; now += 100) {
    assessment = engine.update(sample(66.0f, 12.0f), now);
  }
  assert(assessment.baselineReady);
  assert(assessment.state == BodyState::Watching);

  assessment = engine.update(sample(82.0f, 18.0f), 2600);
  assert(assessment.state == BodyState::BodyActivated);
  assert(!assessment.promptNow);

  assessment = engine.update(sample(82.0f, 18.0f), 3700);
  assert(assessment.promptNow);
  assert(assessment.state == BodyState::Cooldown);

  assessment = engine.update(sample(82.0f, 18.0f), 4000);
  assert(!assessment.promptNow);
  assert(assessment.state == BodyState::Cooldown);

  assessment = engine.update(sample(66.0f, 12.0f), 6800);
  assert(assessment.state == BodyState::Watching);

  VitalSigns missing = sample(66.0f, 12.0f);
  missing.presence = false;
  missing.fresh = false;
  assessment = engine.update(missing, 13000);
  assert(assessment.state == BodyState::WaitingForSignal);

  std::cout << "StressEngine tests passed\n";
  return 0;
}
