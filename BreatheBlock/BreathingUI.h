#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "AppConfig.h"
#include "BreathField.h"
#include "BreathScene.h"

// The LVGL side of the interface. LVGL draws only text and the progress
// hairline onto a true-black screen; the light field is composited into the
// flush buffer by blendInto(), so any area LVGL chooses to repaint comes out
// correct without an offscreen canvas.
class BreathingUI {
 public:
  void begin();
  void update(uint32_t nowMs);

  // Called from the display flush callback, once per flushed sub-area.
  void blendInto(const lv_area_t* area, uint16_t* pixels);

  void setPresence(bool present) { presence_ = present; }
  void setLiveBreathPhase(float phase, bool fresh, uint32_t nowMs);
  void setTestingVitals(float heartRate, float breathRate);
  void setPalette(const BreathPalette& palette);
  const BreathPalette& palette() const { return palette_; }

  void startSession(uint32_t nowMs, bool announceShift);
  void dismiss(uint32_t nowMs);
  void goToSleep(uint32_t nowMs) { scene_.goToSleep(nowMs); }
  void wake(uint32_t nowMs) { scene_.wake(nowMs); }

  bool sessionActive() const { return scene_.sessionActive(); }
  bool consumeSessionFinished() { return scene_.consumeSessionFinished(); }
  SceneState state() const { return scene_.state(); }
  const char* stateName() const;

  BreathScene& scene() { return scene_; }

  // Frame instrumentation, printed by the serial console with 'f'.
  void noteFrame(uint32_t blendMicros, uint32_t pixels);
  void printStats() const;

 private:
  void applyText(const SceneOutput& out);
  void applyProgress(const SceneOutput& out);
  void invalidateGlow(bool fieldChanged);

  BreathScene scene_;
  BreathLut lut_;
  BreathField lastField_;
  BreathPalette palette_ = BreatheBlockConfig::kPalette;

  lv_obj_t* screen_ = nullptr;
  lv_obj_t* label_ = nullptr;
  lv_obj_t* arc_ = nullptr;
  lv_obj_t* vitalsLabel_ = nullptr;

  lv_area_t previousBox_ = {0, 0, -1, -1};
  SceneText shownText_ = SceneText::None;
  uint8_t shownTextOpacity_ = 0;
  uint8_t shownArcOpacity_ = 0;
  int16_t shownArcValue_ = -1;

  float liveBreath_ = 0.0f;
  float phaseCenter_ = 0.0f;
  float phaseAmplitude_ = 0.08f;
  uint32_t lastLivePhaseMs_ = 0;
  bool phaseInitialised_ = false;
  bool presence_ = true;
  bool firstFrame_ = true;

  uint32_t frames_ = 0;
  uint32_t blendMicrosTotal_ = 0;
  uint32_t blendMicrosPeak_ = 0;
  uint32_t pixelsTotal_ = 0;
  uint32_t statsSinceMs_ = 0;
};
