#include "BreathScene.h"

#include <math.h>

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Time constants for the parameter smoother. Nothing in the interface writes
// a field value directly; everything writes a target and is chased toward it,
// which is why no state change can ever produce a visible step.
constexpr float kGeometryTauMs = 110.0f;
constexpr float kLevelTauMs = 260.0f;
constexpr float kWarmthTauMs = 340.0f;

// Below this level a layer is invisible, so its geometry is teleported to the
// target instead of chased. That is what lets the horizon hand its radius to
// the contour without a visible sweep.
constexpr float kInvisible = 0.02f;

constexpr float kWordFadeMs = 800.0f;
// A real beat of nothing between "inhale" and "exhale", rather than one word
// crossing the other at the turn of the breath.
constexpr float kWordGapMs = 220.0f;
constexpr float kWellDepth = 0.9f;

float clamp01(float value) {
  if (!(value > 0.0f)) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

// Opacity envelope: up over kWordFadeMs from `from`, down to zero at `to`.
// Guaranteed to be exactly zero outside the window, which is what lets the UI
// swap strings without ever cross-dissolving two different words.
float wordEnvelope(float t, float from, float to) {
  if (t <= from || t >= to) return 0.0f;
  const float in = smoothstep(clamp01((t - from) / kWordFadeMs));
  const float out = smoothstep(clamp01((to - t) / kWordFadeMs));
  return in < out ? in : out;
}

void chase(float* value, float target, float k) {
  *value += (target - *value) * k;
}

}  // namespace

float BreathScene::ramp(float t, float from, float to) {
  if (to <= from) return t >= to ? 1.0f : 0.0f;
  return smoothstep(clamp01((t - from) / (to - from)));
}

float BreathScene::mix(float a, float b, float t) { return a + (b - a) * t; }

BreathScene::BreathScene(const SceneConfig& config) : config_(config) {}

void BreathScene::begin(uint32_t nowMs) {
  started_ = true;
  lastUpdateMs_ = nowMs;
  lastPresenceMs_ = nowMs;
  output_ = SceneOutput();
  output_.field.centerX = config_.displayCenter;
  output_.field.centerY = config_.displayCenter;
  output_.field.coreRadius = 5.0f;
  output_.field.coreLevel = 0.0f;
  output_.field.ringRadius = 40.0f;
  output_.field.wellRadius = config_.wellRadius;
  enter(SceneState::Awakening, nowMs);
}

void BreathScene::enter(SceneState state, uint32_t nowMs) {
  if (state == SceneState::Releasing) {
    releaseRingRadius_ = output_.field.ringRadius;
    releaseRingLevel_ = output_.field.ringLevel;
    releaseVeilLevel_ = output_.field.veilLevel;
    releaseRingOuterWidth_ = output_.field.ringOuterWidth;
    releaseRingInnerWidth_ = output_.field.ringInnerWidth;
  }
  state_ = state;
  stateStartedMs_ = nowMs;
}

uint32_t BreathScene::elapsed(uint32_t nowMs) const {
  return static_cast<uint32_t>(nowMs - stateStartedMs_);
}

uint32_t BreathScene::guideTotalMs() const {
  return (config_.inhaleMs + config_.exhaleMs) * config_.breathCycles;
}

bool BreathScene::sessionActive() const {
  return state_ == SceneState::Noticing || state_ == SceneState::Inviting ||
         state_ == SceneState::Guiding || state_ == SceneState::Releasing;
}

bool BreathScene::consumeSessionFinished() {
  const bool value = sessionFinished_;
  sessionFinished_ = false;
  return value;
}

void BreathScene::requestSession(uint32_t nowMs, bool announceShift) {
  if (sessionActive()) return;
  dismissed_ = false;
  progressValue_ = 0.0f;
  enter(announceShift ? SceneState::Noticing : SceneState::Inviting, nowMs);
}

void BreathScene::dismiss(uint32_t nowMs) {
  if (!sessionActive() || state_ == SceneState::Releasing) return;
  dismissed_ = true;
  releaseDurationMs_ = config_.dismissMs;
  enter(SceneState::Releasing, nowMs);
}

void BreathScene::goToSleep(uint32_t nowMs) {
  if (sessionActive()) return;
  enter(SceneState::Sleeping, nowMs);
}

void BreathScene::wake(uint32_t nowMs) {
  if (state_ == SceneState::Sleeping) enter(SceneState::Resting, nowMs);
}

void BreathScene::update(const SceneInput& input) {
  if (!started_) begin(input.nowMs);

  float dtMs = static_cast<float>(
      static_cast<uint32_t>(input.nowMs - lastUpdateMs_));
  if (dtMs < 1.0f) dtMs = 1.0f;
  if (dtMs > 200.0f) dtMs = 200.0f;  // never let a stall become a jump
  lastUpdateMs_ = input.nowMs;

  if (input.presence) lastPresenceMs_ = input.nowMs;

  // Timed state transitions.
  switch (state_) {
    case SceneState::Awakening:
      if (elapsed(input.nowMs) >= config_.awakenMs) {
        enter(SceneState::Resting, input.nowMs);
      }
      break;
    case SceneState::Resting:
      if (!input.presence &&
          static_cast<uint32_t>(input.nowMs - lastPresenceMs_) >=
              config_.sleepAfterMs) {
        enter(SceneState::Sleeping, input.nowMs);
      }
      break;
    case SceneState::Sleeping:
      if (input.presence) enter(SceneState::Resting, input.nowMs);
      break;
    case SceneState::Noticing:
      if (elapsed(input.nowMs) >= config_.noticeMs) {
        enter(SceneState::Inviting, input.nowMs);
      }
      break;
    case SceneState::Inviting:
      if (elapsed(input.nowMs) >= config_.inviteMs) {
        enter(SceneState::Guiding, input.nowMs);
      }
      break;
    case SceneState::Guiding:
      if (elapsed(input.nowMs) >= guideTotalMs()) {
        releaseDurationMs_ = config_.releaseMs;
        enter(SceneState::Releasing, input.nowMs);
      }
      break;
    case SceneState::Releasing:
      if (elapsed(input.nowMs) >= releaseDurationMs_) {
        sessionFinished_ = true;
        dismissed_ = false;
        enter(SceneState::Resting, input.nowMs);
      }
      break;
  }

  BreathField target;
  buildTarget(input, &target);
  approach(&output_.field, target, dtMs);

  const float progressK = dtMs / (kLevelTauMs + dtMs);
  chase(&progressOpacity_, output_.progressOpacity, progressK);
  output_.progressOpacity = progressOpacity_;
  output_.progress = progressValue_;
  output_.state = state_;
}

void BreathScene::buildTarget(const SceneInput& input, BreathField* target) {
  const float t = static_cast<float>(elapsed(input.nowMs));
  const float now = static_cast<float>(input.nowMs);
  const SceneConfig& c = config_;

  target->ringOuterWidth = c.ringOuterWidth;
  target->ringInnerWidth = c.ringInnerWidth;
  target->horizonWidth = c.horizonWidth;
  target->horizonRadius = c.horizonRadius;
  target->ringRadius = output_.field.ringRadius;

  SceneText text = SceneText::None;
  float textOpacity = 0.0f;
  float wellRadius = c.wellRadius;
  float wantsProgress = 0.0f;
  float driftTarget = 1.0f;

  switch (state_) {
    case SceneState::Awakening: {
      // A spark, an outward wave that leaves, then the ember. Once, ever.
      const float open = ramp(t, 0.0f, 700.0f);
      const float settle = ramp(t, 1200.0f, static_cast<float>(c.awakenMs));
      target->coreRadius = mix(mix(5.0f, 46.0f, open), c.restCoreRadius, settle);
      target->coreLevel =
          mix(mix(0.0f, 0.50f, ramp(t, 0.0f, 420.0f)), c.restCoreLevel, settle);

      const float wave = ramp(t, 520.0f, 2200.0f);
      target->ringRadius = mix(40.0f, 214.0f, wave);
      // The leading edge stays soft as the wave accelerates, so the gesture
      // reads as light spreading rather than as a ring being redrawn.
      target->ringOuterWidth = mix(26.0f, 48.0f, wave);
      target->ringInnerWidth = mix(20.0f, 34.0f, wave);
      target->ringLevel = 0.30f * ramp(t, 460.0f, 820.0f) * (1.0f - wave);
      break;
    }

    case SceneState::Resting:
    case SceneState::Sleeping: {
      const bool asleep = state_ == SceneState::Sleeping;
      const float baseRadius = asleep ? c.sleepCoreRadius : c.restCoreRadius;
      const float baseLevel = asleep ? c.sleepCoreLevel : c.restCoreLevel;

      // Idle rhythm, deliberately slower than a person breathes so it reads as
      // the object idling rather than as the object tracking you.
      const float idlePeriodMs = asleep ? 26000.0f : 16000.0f;
      const float idle = sinf(now * (2.0f * kPi / idlePeriodMs));
      const float breath =
          mix(idle, input.liveBreath, asleep ? 0.0f : liveWeight_);

      // Awake, the pulse is meant to be caught at a glance; asleep it stays
      // closer to imperceptible, since nobody's there to breathe with.
      const float radiusDepth = asleep ? 0.16f : 0.32f;
      const float levelDepth = asleep ? 0.20f : 0.40f;
      target->coreRadius = baseRadius * (1.0f + radiusDepth * breath);
      target->coreLevel = baseLevel * (1.0f + levelDepth * breath);

      // A gentle warmth precursor to noticing. Continuity, not an alert: by
      // the time a session actually starts, the ember has already been
      // quietly responding.
      target->warmth = asleep ? 0.0f
                              : c.restActivationWarmth *
                                    clamp01(input.activationScore);
      break;
    }

    case SceneState::Noticing: {
      // The rim notices first: this is meant to be caught in the corner of the
      // eye, then drawn inward toward the centre where the words are.
      const float bloom = ramp(t, 0.0f, 1100.0f);
      const float draw = ramp(t, 900.0f, 4200.0f);
      const float fade = ramp(t, 4000.0f, static_cast<float>(c.noticeMs));
      target->horizonRadius = mix(c.horizonRadius, 172.0f, draw);
      target->horizonWidth = mix(c.horizonWidth, 30.0f, draw);
      target->horizonLevel = 0.30f * bloom * (1.0f - fade);
      target->coreRadius = c.restCoreRadius;
      target->coreLevel = c.restCoreLevel * (1.0f - 0.55f * bloom);

      text = SceneText::BreathingShifted;
      textOpacity = wordEnvelope(t, 900.0f, 4300.0f);
      wellRadius = c.messageWellRadius;
      driftTarget = 0.0f;
      break;
    }

    case SceneState::Inviting: {
      // The contour forms exactly where the horizon stopped, then settles to
      // the floor of the breath so the first inhale starts from rest.
      const float form = ramp(t, 0.0f, 900.0f);
      const float settle = ramp(t, 1900.0f, static_cast<float>(c.inviteMs));
      target->ringRadius = mix(mix(172.0f, 148.0f, form), c.ringMinRadius,
                               settle);
      target->ringLevel = 0.42f * form;
      target->veilLevel = 0.055f * form;
      // Hand over to guidance at exactly the edge softness the floor of the
      // breath uses, so the first inhale continues the same shape.
      target->ringInnerWidth = c.ringInnerWidth;
      target->ringOuterWidth = c.ringOuterWidth;
      target->coreLevel = 0.0f;

      text = SceneText::BreatheWithMe;
      textOpacity = wordEnvelope(t, 500.0f, 3000.0f);
      wellRadius = c.messageWellRadius;
      driftTarget = 0.0f;
      break;
    }

    case SceneState::Guiding: {
      const uint32_t cycleMs = c.inhaleMs + c.exhaleMs;
      const uint32_t elapsedMs = elapsed(input.nowMs);
      const uint32_t cycleIndex = elapsedMs / cycleMs;
      const uint32_t within = elapsedMs % cycleMs;

      float extension;   // 0 fully released, 1 fully extended
      float phaseTime;
      float phaseLength;
      bool inhaling = within < c.inhaleMs;
      if (inhaling) {
        phaseTime = static_cast<float>(within);
        phaseLength = static_cast<float>(c.inhaleMs);
        // Full extension is reached at 88% of the inhale: the last half second
        // is still. The stillness at the turn is what makes the pacing feel
        // human rather than metronomic.
        extension = smoothstep(clamp01(phaseTime / (phaseLength * 0.88f)));
      } else {
        phaseTime = static_cast<float>(within - c.inhaleMs);
        phaseLength = static_cast<float>(c.exhaleMs);
        extension =
            1.0f - smoothstep(clamp01(phaseTime / (phaseLength * 0.92f)));
      }

      target->ringRadius = mix(c.ringMinRadius, c.ringMaxRadius, extension);
      // The contour tightens as it fills and softens as it releases, so the
      // quality of the edge carries the breath as well as its size does.
      target->ringInnerWidth =
          mix(c.ringInnerWidth, c.ringInnerWidthFull, extension);
      target->ringOuterWidth =
          mix(c.ringOuterWidth, c.ringOuterWidthFull, extension);
      // Never entirely static, even at the turn of the breath.
      const float shimmer = 1.0f + 0.025f * sinf(now * 0.0011f);
      target->ringLevel = (0.40f + 0.16f * extension) * shimmer;
      target->veilLevel = 0.050f + 0.065f * extension;
      target->coreLevel = 0.0f;
      target->warmth = -0.18f + 0.50f * extension;

      // The words are scaffolding: full for two cycles, half for one, then the
      // light carries it alone.
      float scaffold = 0.0f;
      if (cycleIndex <= 1) {
        scaffold = 1.0f;
      } else if (cycleIndex == 2) {
        scaffold = 0.5f;
      }
      text = inhaling ? SceneText::Inhale : SceneText::Exhale;
      textOpacity =
          scaffold * wordEnvelope(phaseTime, 0.0f, phaseLength - kWordGapMs);

      progressValue_ = clamp01(static_cast<float>(elapsedMs) /
                               static_cast<float>(guideTotalMs()));
      wantsProgress = c.showProgress ? 1.0f : 0.0f;
      driftTarget = 0.0f;
      break;
    }

    case SceneState::Releasing: {
      const float duration = static_cast<float>(releaseDurationMs_);
      const float go = ramp(t, 0.0f, duration * 0.62f);
      target->ringRadius = mix(releaseRingRadius_, 208.0f, go);
      target->ringOuterWidth = mix(releaseRingOuterWidth_, 54.0f, go);
      target->ringInnerWidth = mix(releaseRingInnerWidth_, 40.0f, go);
      target->ringLevel =
          mix(releaseRingLevel_, 0.0f, ramp(t, 0.0f, duration * 0.70f));
      target->veilLevel =
          mix(releaseVeilLevel_, 0.0f, ramp(t, 0.0f, duration * 0.45f));
      target->coreRadius = c.restCoreRadius;
      target->coreLevel =
          c.restCoreLevel * ramp(t, duration * 0.45f, duration);

      // A session that was turned down gets no closing remark.
      if (!dismissed_) {
        text = SceneText::Settled;
        textOpacity = wordEnvelope(t, 600.0f, 3200.0f);
      }
      driftTarget = 0.0f;
      break;
    }
  }

  // The light steps aside wherever a word is.
  target->wellRadius = wellRadius;
  target->wellDepth = kWellDepth * textOpacity;

  // Slow wander: burn-in care, and it keeps the object from looking pinned.
  // It eases away entirely during a session so the ring stays concentric with
  // the words.
  const float driftK = 1.0f / (1.0f + 260.0f / 40.0f);
  driftAmount_ += (driftTarget - driftAmount_) * driftK * 0.25f;
  target->centerX =
      c.displayCenter + driftAmount_ * 5.0f * sinf(now * 0.00017f);
  target->centerY =
      c.displayCenter + driftAmount_ * 4.0f * sinf(now * 0.000119f + 1.1f);

  // Fade between the idle rhythm and the person's own, so gaining or losing
  // the radar signal is never a visible event.
  const float liveTarget =
      (input.livePhaseValid && state_ == SceneState::Resting) ? 1.0f : 0.0f;
  liveWeight_ += (liveTarget - liveWeight_) * 0.02f;

  output_.text = text;
  output_.textOpacity = textOpacity;
  output_.progressOpacity = wantsProgress;
}

void BreathScene::approach(BreathField* current, const BreathField& target,
                           float dtMs) {
  const float geometryK = dtMs / (kGeometryTauMs + dtMs);
  const float levelK = dtMs / (kLevelTauMs + dtMs);
  const float warmthK = dtMs / (kWarmthTauMs + dtMs);

  chase(&current->centerX, target.centerX, geometryK);
  chase(&current->centerY, target.centerY, geometryK);

  // An invisible layer is repositioned rather than moved: that is how the
  // horizon can hand its radius to the contour with no sweep between them.
  if (current->coreLevel < kInvisible) {
    current->coreRadius = target.coreRadius;
  } else {
    chase(&current->coreRadius, target.coreRadius, geometryK);
  }
  if (current->ringLevel < kInvisible && current->veilLevel < kInvisible) {
    current->ringRadius = target.ringRadius;
    current->ringOuterWidth = target.ringOuterWidth;
    current->ringInnerWidth = target.ringInnerWidth;
  } else {
    chase(&current->ringRadius, target.ringRadius, geometryK);
    chase(&current->ringOuterWidth, target.ringOuterWidth, geometryK);
    chase(&current->ringInnerWidth, target.ringInnerWidth, geometryK);
  }
  if (current->horizonLevel < kInvisible) {
    current->horizonRadius = target.horizonRadius;
    current->horizonWidth = target.horizonWidth;
  } else {
    chase(&current->horizonRadius, target.horizonRadius, geometryK);
    chase(&current->horizonWidth, target.horizonWidth, geometryK);
  }
  chase(&current->wellRadius, target.wellRadius, geometryK);
  current->veilFeather = target.veilFeather;

  chase(&current->coreLevel, target.coreLevel, levelK);
  chase(&current->ringLevel, target.ringLevel, levelK);
  chase(&current->veilLevel, target.veilLevel, levelK);
  chase(&current->horizonLevel, target.horizonLevel, levelK);
  // The well is driven straight from the text envelope, which is already a
  // smooth ramp. Smoothing it a second time would make the light step aside
  // after the word had already appeared.
  current->wellDepth = target.wellDepth;
  chase(&current->warmth, target.warmth, warmthK);
}
