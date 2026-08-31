#include "BreathingUI.h"

#include <math.h>

namespace {
constexpr uint32_t kBackground = 0x050706;
constexpr uint32_t kSage = 0xB8CCBC;
constexpr uint32_t kSageSoft = 0x91AA98;
constexpr uint32_t kText = 0xE7EDE7;
constexpr uint32_t kMutedText = 0x7E8C80;
constexpr int kCircleY = -8;
constexpr float kSmallCircle = 116.0f;
constexpr float kLargeCircle = 306.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr uint32_t kObservationEndMs = 3200;
constexpr uint32_t kInvitationStartMs = 4100;
constexpr uint32_t kInvitationEndMs = 6300;
constexpr uint32_t kPromptEndMs = 7200;
constexpr uint32_t kPromptTextFadeMs = 850;
constexpr uint32_t kPromptToBreathBlendMs = 900;
}  // namespace

float BreathingUI::clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

float BreathingUI::smooth(float value) {
  value = clamp01(value);
  return 0.5f - 0.5f * cosf(value * kPi);
}

void BreathingUI::begin() {
  screen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(screen_);
  lv_obj_set_style_bg_color(screen_, lv_color_hex(kBackground), 0);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
  lv_screen_load(screen_);

  halo_ = lv_obj_create(screen_);
  lv_obj_remove_style_all(halo_);
  lv_obj_set_style_radius(halo_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(halo_, lv_color_hex(kSageSoft), 0);
  lv_obj_set_style_bg_opa(halo_, 16, 0);

  circle_ = lv_obj_create(screen_);
  lv_obj_remove_style_all(circle_);
  lv_obj_set_style_radius(circle_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(circle_, lv_color_hex(kSage), 0);
  lv_obj_set_style_bg_opa(circle_, 22, 0);
  lv_obj_set_style_border_color(circle_, lv_color_hex(kSage), 0);
  lv_obj_set_style_border_opa(circle_, 185, 0);
  lv_obj_set_style_border_width(circle_, 2, 0);

  edgeRing_ = lv_obj_create(screen_);
  lv_obj_remove_style_all(edgeRing_);
  lv_obj_set_size(edgeRing_, 450, 450);
  lv_obj_align(edgeRing_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_radius(edgeRing_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(edgeRing_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(edgeRing_, lv_color_hex(kSage), 0);
  lv_obj_set_style_border_width(edgeRing_, 3, 0);
  lv_obj_set_style_border_opa(edgeRing_, 0, 0);

  label_ = lv_label_create(screen_);
  lv_obj_set_style_text_color(label_, lv_color_hex(kText), 0);
  lv_obj_set_style_text_opa(label_, 225, 0);
  lv_obj_set_style_text_font(label_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_letter_space(label_, 3, 0);
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(label_, "");
  lv_obj_align(label_, LV_ALIGN_CENTER, 0, kCircleY);

  statusLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_color(statusLabel_, lv_color_hex(kMutedText), 0);
  lv_obj_set_style_text_opa(statusLabel_, 170, 0);
  lv_obj_set_style_text_font(statusLabel_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_letter_space(statusLabel_, 2, 0);
  lv_label_set_text(statusLabel_, "waiting");
  lv_obj_align(statusLabel_, LV_ALIGN_BOTTOM_MID, 0, -54);

  testingLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_color(testingLabel_, lv_color_hex(kMutedText), 0);
  lv_obj_set_style_text_opa(testingLabel_, 120, 0);
  lv_obj_set_style_text_font(testingLabel_, &lv_font_montserrat_14, 0);
  lv_obj_align(testingLabel_, LV_ALIGN_TOP_MID, 0, 52);
  lv_obj_add_flag(testingLabel_, LV_OBJ_FLAG_HIDDEN);

  setCircle(54.0f, 12, 5);
}

void BreathingUI::setMainText(const char* text) {
  lv_label_set_text(label_, text);
  lv_obj_align(label_, LV_ALIGN_CENTER, 0, kCircleY);
}

void BreathingUI::setCircle(float size, uint8_t fillOpacity,
                            uint8_t haloOpacity) {
  currentCircleSize_ = size;
  const int circleSize = static_cast<int>(lroundf(size));
  const int haloSize = circleSize + 42;
  lv_obj_set_size(halo_, haloSize, haloSize);
  lv_obj_align(halo_, LV_ALIGN_CENTER, 0, kCircleY);
  lv_obj_set_style_bg_opa(halo_, haloOpacity, 0);

  lv_obj_set_size(circle_, circleSize, circleSize);
  lv_obj_align(circle_, LV_ALIGN_CENTER, 0, kCircleY);
  lv_obj_set_style_bg_opa(circle_, fillOpacity, 0);
}

void BreathingUI::setAmbientState(BodyState state) {
  if (mode_ == Mode::Ambient) ambientState_ = state;
}

void BreathingUI::setLiveBreathPhase(float phase, bool fresh, uint32_t nowMs) {
  if (!fresh || !isfinite(phase)) return;

  if (!phaseInitialized_) {
    phaseInitialized_ = true;
    phaseCenter_ = phase;
    phaseAmplitude_ = 0.08f;
    liveBreathLevel_ = 0.5f;
  }

  const float delta = phase - phaseCenter_;
  phaseCenter_ += 0.004f * delta;
  phaseAmplitude_ += 0.025f * (fabsf(delta) - phaseAmplitude_);
  if (phaseAmplitude_ < 0.02f) phaseAmplitude_ = 0.02f;

  const float target = clamp01(0.5f + delta / (phaseAmplitude_ * 3.6f));
  liveBreathLevel_ += 0.16f * (target - liveBreathLevel_);
  lastLivePhaseAtMs_ = nowMs;
}

void BreathingUI::setTestingVitals(float heartRate, float breathRate) {
  if (!BreatheBlockConfig::kShowVitalsDuringTesting ||
      mode_ != Mode::Ambient) {
    lv_obj_add_flag(testingLabel_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_clear_flag(testingLabel_, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text_fmt(testingLabel_, "%.0f  ·  %.0f", heartRate, breathRate);
  lv_obj_align(testingLabel_, LV_ALIGN_TOP_MID, 0, 52);
}

void BreathingUI::startSession(uint32_t nowMs) {
  mode_ = Mode::Prompting;
  phase_ = BreathPhase::None;
  promptStartedAtMs_ = nowMs;
  sessionFinished_ = false;
  setMainText("your breathing\nhas shifted");
  lv_obj_set_style_text_opa(label_, 0, 0);
  lv_obj_set_style_border_opa(edgeRing_, 0, 0);
  lv_obj_add_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(testingLabel_, LV_OBJ_FLAG_HIDDEN);
}

void BreathingUI::stopSession() {
  mode_ = Mode::Ambient;
  phase_ = BreathPhase::None;
  lv_obj_clear_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_border_opa(edgeRing_, 0, 0);
  lv_obj_set_style_text_opa(label_, 225, 0);
  setMainText("");
}

bool BreathingUI::consumeSessionFinished() {
  const bool value = sessionFinished_;
  sessionFinished_ = false;
  return value;
}

void BreathingUI::updateAmbient(uint32_t nowMs) {
  const float wave = 0.5f + 0.5f * sinf(nowMs / 1700.0f);
  setMainText("");
  lv_obj_set_style_border_opa(edgeRing_, 0, 0);

  const bool livePhaseAvailable = phaseInitialized_ &&
      static_cast<uint32_t>(nowMs - lastLivePhaseAtMs_) < 750;

  if (livePhaseAvailable &&
      (ambientState_ == BodyState::Watching ||
       ambientState_ == BodyState::Calibrating)) {
    const float breath = smooth(liveBreathLevel_);
    setCircle(72.0f + 112.0f * breath,
              static_cast<uint8_t>(9.0f + 13.0f * breath),
              static_cast<uint8_t>(3.0f + 8.0f * breath));
    lv_label_set_text(statusLabel_, "");
    return;
  }

  switch (ambientState_) {
    case BodyState::WaitingForSignal:
      setCircle(50.0f + 4.0f * wave, 8, 3);
      lv_label_set_text(statusLabel_, "waiting");
      break;
    case BodyState::Calibrating:
      setCircle(66.0f + 10.0f * wave, 13, 5);
      lv_label_set_text(statusLabel_, "settling in");
      break;
    case BodyState::BodyActivated:
      setCircle(88.0f + 8.0f * wave, 18, 8);
      lv_label_set_text(statusLabel_, "a moment");
      break;
    case BodyState::Cooldown:
      setCircle(56.0f + 3.0f * wave, 9, 3);
      lv_label_set_text(statusLabel_, "resting");
      break;
    case BodyState::Watching:
    default:
      setCircle(46.0f + 3.0f * wave, 7, 2);
      lv_label_set_text(statusLabel_, "");
      break;
  }
  lv_obj_align(statusLabel_, LV_ALIGN_BOTTOM_MID, 0, -54);
}

void BreathingUI::updatePrompting(uint32_t nowMs) {
  const uint32_t elapsedMs = nowMs - promptStartedAtMs_;
  const float pulse = 0.5f + 0.5f * sinf(nowMs / 380.0f);
  const float softPulse = smooth(pulse);

  // A quiet inner contour protects the centered message while the outer edge
  // pulse provides the peripheral cue that something is ready for attention.
  setCircle(266.0f + 8.0f * softPulse,
            static_cast<uint8_t>(8.0f + 4.0f * softPulse),
            static_cast<uint8_t>(4.0f + 3.0f * softPulse));

  float edgeFade = 1.0f;
  if (elapsedMs > kInvitationEndMs) {
    edgeFade = 1.0f - clamp01(
        (elapsedMs - kInvitationEndMs) /
        static_cast<float>(kPromptEndMs - kInvitationEndMs));
  }
  lv_obj_set_style_border_opa(
      edgeRing_,
      static_cast<uint8_t>((12.0f + 48.0f * softPulse) * edgeFade), 0);

  const auto textEnvelope = [](uint32_t elapsed, uint32_t start,
                               uint32_t end) -> float {
    if (elapsed < start || elapsed >= end) return 0.0f;
    const uint32_t sinceStart = elapsed - start;
    const uint32_t untilEnd = end - elapsed;
    const uint32_t nearest = sinceStart < untilEnd ? sinceStart : untilEnd;
    return BreathingUI::smooth(
        nearest / static_cast<float>(kPromptTextFadeMs));
  };

  float textOpacity = 0.0f;
  if (elapsedMs < kObservationEndMs) {
    setMainText("your breathing\nhas shifted");
    textOpacity = textEnvelope(elapsedMs, 0, kObservationEndMs);
  } else if (elapsedMs >= kInvitationStartMs &&
             elapsedMs < kInvitationEndMs) {
    setMainText("breathe with me");
    textOpacity = textEnvelope(elapsedMs, kInvitationStartMs,
                               kInvitationEndMs);
  }
  lv_obj_set_style_text_opa(
      label_, static_cast<uint8_t>(225.0f * textOpacity), 0);

  if (elapsedMs >= kPromptEndMs) {
    promptExitCircleSize_ = currentCircleSize_;
    mode_ = Mode::Breathing;
    phase_ = BreathPhase::None;
    sessionStartedAtMs_ = nowMs;
    lv_obj_set_style_border_opa(edgeRing_, 0, 0);
    lv_obj_set_style_text_opa(label_, 0, 0);
    updateBreathing(nowMs);
  }
}

void BreathingUI::updateBreathing(uint32_t nowMs) {
  lv_obj_set_style_border_opa(edgeRing_, 0, 0);
  const uint32_t breathMs = BreatheBlockConfig::kInhaleMs +
                            BreatheBlockConfig::kExhaleMs;
  const uint32_t exerciseMs = breathMs * BreatheBlockConfig::kBreathCycles;
  const uint32_t elapsedMs = nowMs - sessionStartedAtMs_;

  if (elapsedMs >= exerciseMs) {
    if (phase_ != BreathPhase::Settled) {
      phase_ = BreathPhase::Settled;
      setMainText("settled");
    }

    const float t = clamp01((elapsedMs - exerciseMs) /
                            static_cast<float>(BreatheBlockConfig::kSettledMs));
    const uint32_t settledElapsedMs = elapsedMs - exerciseMs;
    constexpr uint32_t kSettledTextFadeMs = 650;
    const uint32_t settledRemainingMs =
        settledElapsedMs < BreatheBlockConfig::kSettledMs
            ? BreatheBlockConfig::kSettledMs - settledElapsedMs
            : 0;
    const float settledFadeIn = smooth(
        settledElapsedMs / static_cast<float>(kSettledTextFadeMs));
    const float settledFadeOut = smooth(
        settledRemainingMs / static_cast<float>(kSettledTextFadeMs));
    const float settledTextOpacity =
        settledFadeIn < settledFadeOut ? settledFadeIn : settledFadeOut;
    setCircle(kSmallCircle - 48.0f * smooth(t),
              static_cast<uint8_t>(24.0f * (1.0f - t)),
              static_cast<uint8_t>(10.0f * (1.0f - t)));
    lv_obj_set_style_text_opa(label_,
                              static_cast<uint8_t>(225.0f * settledTextOpacity),
                              0);

    if (t >= 1.0f) {
      lv_obj_set_style_text_opa(label_, 225, 0);
      mode_ = Mode::Ambient;
      phase_ = BreathPhase::None;
      sessionFinished_ = true;
      lv_obj_clear_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);
      setMainText("");
    }
    return;
  }

  const uint32_t withinBreath = elapsedMs % breathMs;
  float size;
  float energy;
  uint32_t phaseElapsedMs;
  uint32_t phaseDurationMs;

  if (withinBreath < BreatheBlockConfig::kInhaleMs) {
    if (phase_ != BreathPhase::Inhale) {
      phase_ = BreathPhase::Inhale;
      setMainText("inhale");
    }
    const float t = smooth(withinBreath /
                           static_cast<float>(BreatheBlockConfig::kInhaleMs));
    size = kSmallCircle + (kLargeCircle - kSmallCircle) * t;
    energy = t;
    phaseElapsedMs = withinBreath;
    phaseDurationMs = BreatheBlockConfig::kInhaleMs;
  } else {
    if (phase_ != BreathPhase::Exhale) {
      phase_ = BreathPhase::Exhale;
      setMainText("exhale");
    }
    const float t = smooth((withinBreath - BreatheBlockConfig::kInhaleMs) /
                           static_cast<float>(BreatheBlockConfig::kExhaleMs));
    size = kLargeCircle - (kLargeCircle - kSmallCircle) * t;
    energy = 1.0f - t;
    phaseElapsedMs = withinBreath - BreatheBlockConfig::kInhaleMs;
    phaseDurationMs = BreatheBlockConfig::kExhaleMs;
  }

  constexpr uint32_t kTextFadeMs = 700;
  const uint32_t untilPhaseEnd = phaseDurationMs - phaseElapsedMs;
  const uint32_t nearestEdge = phaseElapsedMs < untilPhaseEnd
                                   ? phaseElapsedMs
                                   : untilPhaseEnd;
  const float textFade = smooth(nearestEdge /
                                static_cast<float>(kTextFadeMs));
  lv_obj_set_style_text_opa(
      label_, static_cast<uint8_t>(225.0f * textFade), 0);

  // Preserve momentum across the prompt-to-guidance boundary. The first
  // guided shape starts exactly where the prompt shape ended and settles onto
  // the breathing curve over 900 ms instead of snapping smaller.
  if (elapsedMs < kPromptToBreathBlendMs) {
    const float handoff = smooth(
        elapsedMs / static_cast<float>(kPromptToBreathBlendMs));
    size = promptExitCircleSize_ + (size - promptExitCircleSize_) * handoff;
  }

  setCircle(size, static_cast<uint8_t>(18.0f + 18.0f * energy),
            static_cast<uint8_t>(7.0f + 15.0f * energy));
}

void BreathingUI::update(uint32_t nowMs) {
  switch (mode_) {
    case Mode::Prompting:
      updatePrompting(nowMs);
      break;
    case Mode::Breathing:
      updateBreathing(nowMs);
      break;
    case Mode::Ambient:
    default:
      updateAmbient(nowMs);
      break;
  }
}
