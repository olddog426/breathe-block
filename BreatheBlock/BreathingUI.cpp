#include "BreathingUI.h"

#include <math.h>

namespace {

constexpr int kDisplaySize = 466;
constexpr int kArcSize = 448;      // outer radius 224, clear of the contour
constexpr uint8_t kTextOpacity = 232;
constexpr uint8_t kArcOpacity = 46;   // a hairline whisper, not a progress bar
constexpr uint32_t kArcColor = 0xC9D6CE;
constexpr uint32_t kTextColor = 0xF2F5F1;

const char* textFor(SceneText text) {
  switch (text) {
    case SceneText::BreathingShifted: return BreatheBlockConfig::kTextShifted;
    case SceneText::BreatheWithMe: return BreatheBlockConfig::kTextInvite;
    case SceneText::Inhale: return BreatheBlockConfig::kTextInhale;
    case SceneText::Exhale: return BreatheBlockConfig::kTextExhale;
    case SceneText::Settled: return BreatheBlockConfig::kTextSettled;
    case SceneText::None: default: return "";
  }
}

// A single word gets the larger size; a phrase gets the smaller one so it
// stays inside the contour.
bool isSingleWord(SceneText text) {
  return text == SceneText::Inhale || text == SceneText::Exhale ||
         text == SceneText::Settled;
}

lv_area_t unionOf(const lv_area_t& a, const lv_area_t& b) {
  const bool aEmpty = a.x2 < a.x1 || a.y2 < a.y1;
  const bool bEmpty = b.x2 < b.x1 || b.y2 < b.y1;
  if (aEmpty) return b;
  if (bEmpty) return a;
  lv_area_t out;
  out.x1 = a.x1 < b.x1 ? a.x1 : b.x1;
  out.y1 = a.y1 < b.y1 ? a.y1 : b.y1;
  out.x2 = a.x2 > b.x2 ? a.x2 : b.x2;
  out.y2 = a.y2 > b.y2 ? a.y2 : b.y2;
  return out;
}

}  // namespace

void BreathingUI::begin() {
  SceneConfig config;
  config.inhaleMs = BreatheBlockConfig::kInhaleMs;
  config.exhaleMs = BreatheBlockConfig::kExhaleMs;
  config.breathCycles = BreatheBlockConfig::breathCycles();
  config.sleepAfterMs = BreatheBlockConfig::kSleepAfterMs;
  config.showProgress = BreatheBlockConfig::kShowSessionProgress;
  scene_.setConfig(config);

  screen_ = lv_obj_create(nullptr);
  lv_obj_remove_style_all(screen_);
  // True black: on AMOLED these pixels are off, which is both the calmest
  // background and the cheapest, and it lets the glow compositor take a fast
  // path on every untouched pixel.
  lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
  lv_screen_load(screen_);

  arc_ = lv_arc_create(screen_);
  lv_obj_set_size(arc_, kArcSize, kArcSize);
  lv_obj_center(arc_);
  lv_obj_set_style_pad_all(arc_, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_opa(arc_, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc_, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(arc_, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_pad_all(arc_, 0, LV_PART_KNOB);
  lv_obj_set_style_arc_width(arc_, 2, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc_, true, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, lv_color_hex(kArcColor), LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(arc_, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_arc_set_rotation(arc_, 270);
  lv_arc_set_bg_angles(arc_, 0, 360);
  lv_arc_set_range(arc_, 0, 1000);
  lv_arc_set_value(arc_, 0);

  label_ = lv_label_create(screen_);
  lv_obj_set_style_text_color(label_, lv_color_hex(kTextColor), 0);
  lv_obj_set_style_text_font(label_, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_letter_space(label_, 5, 0);
  lv_obj_set_style_text_line_space(label_, 14, 0);
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_opa(label_, LV_OPA_TRANSP, 0);
  lv_label_set_text(label_, "");
  lv_obj_center(label_);

  vitalsLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_color(vitalsLabel_, lv_color_hex(0x6C7A70), 0);
  lv_obj_set_style_text_font(vitalsLabel_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_opa(vitalsLabel_, LV_OPA_TRANSP, 0);
  lv_label_set_text(vitalsLabel_, "");
  lv_obj_align(vitalsLabel_, LV_ALIGN_TOP_MID, 0, 56);

  // With LV_COLOR_16_SWAP set, lv_refr.c byte-swaps the render buffer on its
  // way into flush_cb, so the light field has to be packed the same way.
  // Mirrors LVGL's own condition exactly (see call_flush_cb in lv_refr.c).
#if defined(LV_COLOR_16_SWAP) && LV_COLOR_16_SWAP
  lut_.setSwappedBytes(true);
#else
  lut_.setSwappedBytes(false);
#endif
}

const char* BreathingUI::stateName() const {
  switch (scene_.state()) {
    case SceneState::Awakening: return "awakening";
    case SceneState::Resting: return "resting";
    case SceneState::Sleeping: return "sleeping";
    case SceneState::Noticing: return "noticing";
    case SceneState::Inviting: return "inviting";
    case SceneState::Guiding: return "guiding";
    case SceneState::Releasing: return "releasing";
  }
  return "?";
}

void BreathingUI::setLiveBreathPhase(float phase, bool fresh, uint32_t nowMs) {
  if (!fresh || !isfinite(phase)) return;

  if (!phaseInitialised_) {
    phaseInitialised_ = true;
    phaseCenter_ = phase;
    phaseAmplitude_ = 0.08f;
    liveBreath_ = 0.0f;
  }

  const float delta = phase - phaseCenter_;
  phaseCenter_ += 0.004f * delta;
  phaseAmplitude_ += 0.025f * (fabsf(delta) - phaseAmplitude_);
  if (phaseAmplitude_ < 0.02f) phaseAmplitude_ = 0.02f;

  float target = delta / (phaseAmplitude_ * 2.2f);
  if (target > 1.0f) target = 1.0f;
  if (target < -1.0f) target = -1.0f;
  liveBreath_ += 0.16f * (target - liveBreath_);
  lastLivePhaseMs_ = nowMs;
}

void BreathingUI::setTestingVitals(float heartRate, float breathRate) {
  if (!BreatheBlockConfig::kShowVitalsDuringTesting ||
      scene_.state() != SceneState::Resting) {
    lv_obj_set_style_text_opa(vitalsLabel_, LV_OPA_TRANSP, 0);
    return;
  }
  lv_label_set_text_fmt(vitalsLabel_, "%.0f  ·  %.0f", heartRate, breathRate);
  lv_obj_set_style_text_opa(vitalsLabel_, 90, 0);
  lv_obj_align(vitalsLabel_, LV_ALIGN_TOP_MID, 0, 56);
}

void BreathingUI::setPalette(const BreathPalette& palette) {
  palette_ = palette;
  firstFrame_ = true;  // force a LUT rebuild on the next frame
}

void BreathingUI::startSession(uint32_t nowMs, bool announceShift) {
  scene_.requestSession(nowMs, announceShift);
}

void BreathingUI::dismiss(uint32_t nowMs) { scene_.dismiss(nowMs); }

void BreathingUI::update(uint32_t nowMs) {
  SceneInput input;
  input.nowMs = nowMs;
  input.presence = presence_;
  input.livePhaseValid =
      phaseInitialised_ &&
      static_cast<uint32_t>(nowMs - lastLivePhaseMs_) < 900;
  input.liveBreath = liveBreath_;
  scene_.update(input);

  const SceneOutput& out = scene_.output();
  const bool fieldChanged = firstFrame_ || !out.field.matches(lastField_);
  if (fieldChanged) {
    lut_.build(out.field, palette_);
    lastField_ = out.field;
  }

  applyText(out);
  applyProgress(out);
  invalidateGlow(fieldChanged);
  firstFrame_ = false;
}

void BreathingUI::applyText(const SceneOutput& out) {
  uint8_t opacity =
      static_cast<uint8_t>(kTextOpacity * (out.textOpacity < 0.0f
                                               ? 0.0f
                                               : (out.textOpacity > 1.0f
                                                      ? 1.0f
                                                      : out.textOpacity)));

  // A string only ever changes while it is invisible, so two different words
  // are never cross-dissolved into each other.
  if (out.text != shownText_) {
    if (opacity == 0) {
      shownText_ = out.text;
      lv_obj_set_style_text_font(
          label_,
          isSingleWord(out.text) ? &lv_font_montserrat_28
                                 : &lv_font_montserrat_24,
          0);
      lv_label_set_text(label_, textFor(out.text));
      lv_obj_center(label_);
    } else {
      opacity = 0;  // hold the old word invisible until the swap can happen
    }
  }

  if (opacity != shownTextOpacity_) {
    shownTextOpacity_ = opacity;
    lv_obj_set_style_text_opa(label_, opacity, 0);
  }
}

void BreathingUI::applyProgress(const SceneOutput& out) {
  float opacityScale = out.progressOpacity;
  if (opacityScale < 0.0f) opacityScale = 0.0f;
  if (opacityScale > 1.0f) opacityScale = 1.0f;
  const uint8_t opacity = static_cast<uint8_t>(kArcOpacity * opacityScale);
  if (opacity != shownArcOpacity_) {
    shownArcOpacity_ = opacity;
    lv_obj_set_style_arc_opa(arc_, opacity, LV_PART_INDICATOR);
  }

  const int16_t value = static_cast<int16_t>(out.progress * 1000.0f);
  if (value != shownArcValue_) {
    shownArcValue_ = value;
    lv_arc_set_value(arc_, value);
  }
}

void BreathingUI::invalidateGlow(bool fieldChanged) {
  lv_area_t box;
  if (lut_.maxRadius() > 0) {
    int x1, y1, x2, y2;
    lut_.boundingBox(kDisplaySize, kDisplaySize, &x1, &y1, &x2, &y2);
    box.x1 = static_cast<int32_t>(x1);
    box.y1 = static_cast<int32_t>(y1);
    box.x2 = static_cast<int32_t>(x2);
    box.y2 = static_cast<int32_t>(y2);
  } else {
    box.x1 = 0;
    box.y1 = 0;
    box.x2 = -1;
    box.y2 = -1;
  }

  // Repaint what the glow covers now plus what it covered last frame, so a
  // shrinking field always leaves black behind it.
  if (fieldChanged) {
    lv_area_t dirty = unionOf(previousBox_, box);
    if (dirty.x2 >= dirty.x1 && dirty.y2 >= dirty.y1) {
      lv_obj_invalidate_area(screen_, &dirty);
    }
  }
  previousBox_ = box;
}

void BreathingUI::blendInto(const lv_area_t* area, uint16_t* pixels) {
  lut_.blendArea(static_cast<int>(area->x1), static_cast<int>(area->y1),
                 static_cast<int>(area->x2), static_cast<int>(area->y2),
                 pixels);
}

void BreathingUI::noteFrame(uint32_t blendMicros, uint32_t pixels) {
  ++frames_;
  blendMicrosTotal_ += blendMicros;
  pixelsTotal_ += pixels;
  if (blendMicros > blendMicrosPeak_) blendMicrosPeak_ = blendMicros;
}

void BreathingUI::printStats() const {
  const uint32_t nowMs = millis();
  const uint32_t windowMs = nowMs - statsSinceMs_;
  Serial.printf(
      "frames %lu | blend avg %lu us, peak %lu us | avg %lu px/flush | %s\n",
      static_cast<unsigned long>(frames_),
      static_cast<unsigned long>(frames_ ? blendMicrosTotal_ / frames_ : 0),
      static_cast<unsigned long>(blendMicrosPeak_),
      static_cast<unsigned long>(frames_ ? pixelsTotal_ / frames_ : 0),
      stateName());
  (void)windowMs;
}
