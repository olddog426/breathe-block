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
  // Fast for this test, which is about the calibrate/activate/cooldown
  // flow, not the averaging itself — that gets its own check below.
  config.activationSmoothingMs = 200;

  StressEngine engine(config);

  BodyAssessment assessment;
  for (uint32_t now = 100; now <= 2500; now += 100) {
    assessment = engine.update(sample(66.0f, 12.0f), now);
  }
  assert(assessment.baselineReady);
  assert(assessment.state == BodyState::Watching);

  // A single elevated reading isn't enough on its own — the score is driven
  // by an averaged heart/breath rate, not the instant one.
  assessment = engine.update(sample(82.0f, 18.0f), 2600);
  assert(assessment.state != BodyState::BodyActivated);

  // Held elevated, it does eventually cross the threshold and, once that
  // holds for activationHoldMs, prompts.
  uint32_t promptAtMs = 0;
  for (uint32_t now = 2700; now <= 4200; now += 100) {
    assessment = engine.update(sample(82.0f, 18.0f), now);
    if (assessment.promptNow) {
      promptAtMs = now;
      break;
    }
  }
  assert(promptAtMs != 0);
  assert(assessment.state == BodyState::Cooldown);

  assessment = engine.update(sample(82.0f, 18.0f), promptAtMs + 300);
  assert(!assessment.promptNow);
  assert(assessment.state == BodyState::Cooldown);

  assessment = engine.update(sample(66.0f, 12.0f), promptAtMs + 3100);
  assert(assessment.state == BodyState::Watching);

  VitalSigns missing = sample(66.0f, 12.0f);
  missing.presence = false;
  missing.fresh = false;
  assessment = engine.update(missing, promptAtMs + 10000);
  assert(assessment.state == BodyState::WaitingForSignal);

  // A brief, noisy radar packet — one bad reading right back to baseline —
  // must not be enough to swing the score toward activation at all, at the
  // real (not test-shortened) averaging window.
  {
    StressEngineConfig noisyConfig;
    noisyConfig.calibrationMs = 2000;
    StressEngine noisyEngine(noisyConfig);

    BodyAssessment noisy;
    for (uint32_t now = 100; now <= 2500; now += 100) {
      noisy = noisyEngine.update(sample(66.0f, 12.0f), now);
    }
    assert(noisy.baselineReady);

    noisy = noisyEngine.update(sample(120.0f, 30.0f), 2600);
    assert(noisy.activationScore < 0.1f);
    assert(noisy.state != BodyState::BodyActivated);

    noisy = noisyEngine.update(sample(66.0f, 12.0f), 2700);
    assert(noisy.activationScore < 0.1f);
    assert(noisy.state == BodyState::Watching);
  }

  std::cout << "StressEngine tests passed\n";
  return 0;
}
