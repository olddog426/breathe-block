#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "AppConfig.h"
#include "StressEngine.h"

class BreathingUI {
 public:
  void begin();
  void update(uint32_t nowMs);
  void setAmbientState(BodyState state);
  void setLiveBreathPhase(float phase, bool fresh, uint32_t nowMs);
  void setTestingVitals(float heartRate, float breathRate);
  void startSession(uint32_t nowMs);
  void stopSession();
  bool sessionActive() const { return mode_ != Mode::Ambient; }
  bool consumeSessionFinished();

 private:
  enum class Mode : uint8_t { Ambient, Prompting, Breathing };
  enum class BreathPhase : uint8_t { None, Inhale, Exhale, Settled };

  static float smooth(float value);
  static float clamp01(float value);
  void updateAmbient(uint32_t nowMs);
  void updatePrompting(uint32_t nowMs);
  void updateBreathing(uint32_t nowMs);
  void setCircle(float size, uint8_t fillOpacity, uint8_t haloOpacity);
  void setMainText(const char* text);

  lv_obj_t* screen_ = nullptr;
  lv_obj_t* halo_ = nullptr;
  lv_obj_t* circle_ = nullptr;
  lv_obj_t* edgeRing_ = nullptr;
  lv_obj_t* label_ = nullptr;
  lv_obj_t* statusLabel_ = nullptr;
  lv_obj_t* testingLabel_ = nullptr;
  Mode mode_ = Mode::Ambient;
  BreathPhase phase_ = BreathPhase::None;
  BodyState ambientState_ = BodyState::WaitingForSignal;
  uint32_t sessionStartedAtMs_ = 0;
  uint32_t promptStartedAtMs_ = 0;
  uint32_t lastLivePhaseAtMs_ = 0;
  float phaseCenter_ = 0.0f;
  float phaseAmplitude_ = 0.08f;
  float liveBreathLevel_ = 0.5f;
  float currentCircleSize_ = 54.0f;
  float promptExitCircleSize_ = 266.0f;
  bool phaseInitialized_ = false;
  bool sessionFinished_ = false;
};
