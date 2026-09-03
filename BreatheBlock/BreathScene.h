// The state machine and all of the easing. Produces one BreathField plus one
// piece of text per frame.
//
// Pure C++: no LVGL, no Arduino, so the whole interaction can be replayed and
// asserted on a laptop.
#pragma once

#include <stdint.h>

#include "BreathField.h"

enum class SceneState : uint8_t {
  Awakening,
  Resting,
  Sleeping,
  CheckingIn,  // a requested glance at heart rate and breath rate
  Countdown,   // 3, 2, 1 before a self-started session begins
  Noticing,
  Inviting,
  Guiding,
  Releasing,
};

// The interface never composes a sentence at runtime; it picks one of five.
enum class SceneText : uint8_t {
  None,
  BreathingShifted,
  BreatheWithMe,
  Inhale,
  Exhale,
  Settled,
};

struct SceneConfig {
  uint32_t inhaleMs = 4000;
  uint32_t exhaleMs = 6000;
  uint8_t breathCycles = 5;

  uint32_t awakenMs = 2600;
  uint32_t noticeMs = 5200;
  uint32_t inviteMs = 3400;
  // The invitation doesn't start itself: it forms, settles, and waits for a
  // tap. If it waits this long unanswered, it withdraws quietly rather than
  // starting on its own — same as a session that was turned down.
  uint32_t inviteTimeoutMs = 24000;
  uint32_t releaseMs = 4600;
  uint32_t dismissMs = 2000;
  uint32_t sleepAfterMs = 180000;
  // How long a glance at your numbers stays up before it quietly returns to
  // rest, if it isn't tapped a second time.
  uint32_t checkInMs = 6000;
  // "3, 2, 1" — one second each.
  uint32_t countdownStepMs = 1000;

  // Geometry, in pixels on a 466 x 466 display.
  float displayCenter = 233.0f;
  float restCoreRadius = 38.0f;
  float restCoreLevel = 0.16f;
  float sleepCoreRadius = 24.0f;
  float sleepCoreLevel = 0.028f;
  // A gentle warmth precursor to noticing while resting: 0 at your seated
  // baseline, ramping toward this much warmth as the detector's own smoothed
  // activation score approaches its real trigger threshold. Tied to that
  // baseline-relative score rather than raw heart rate, so it only moves
  // with a genuine sustained shift and can't flicker on ordinary noise.
  float restActivationWarmth = 0.30f;
  // The same activation score also grows the ember itself — more obvious
  // than warmth alone — until it's large and bright approaching the
  // trigger threshold, right before a session actually begins.
  float restActivationGrowth = 0.55f;   // extra radius, as a fraction
  float restActivationBrighten = 0.70f;  // extra brightness, as a fraction
  // The ember's size while a glance at your numbers is up: bigger than rest
  // so the moment reads as "look here," calmer than a guided session.
  float checkInCoreRadius = 64.0f;
  float checkInCoreLevel = 0.30f;
  // Countdown counts down inside the same shape the check-in left behind,
  // easing toward the floor of the breath so guidance picks it up smoothly.
  float ringMinRadius = 100.0f;
  float ringMaxRadius = 184.0f;
  // The contour tightens as it fills: these are its released and extended
  // edge softnesses, and the invitation hands over at the released end.
  float ringOuterWidth = 20.0f;   // at the floor of the breath
  float ringOuterWidthFull = 28.0f;
  float ringInnerWidth = 22.0f;   // at the floor of the breath
  float ringInnerWidthFull = 14.0f;
  float horizonRadius = 204.0f;
  float horizonWidth = 22.0f;
  float wellRadius = 84.0f;      // around a single word
  float messageWellRadius = 118.0f;  // around a two- or three-word line

  bool showProgress = true;
};

struct SceneInput {
  uint32_t nowMs = 0;
  bool presence = true;
  bool livePhaseValid = false;
  float liveBreath = 0.0f;  // -1 fully exhaled .. +1 fully inhaled
  // The detector's own smoothed, baseline-relative activation score: 0 at
  // your seated baseline, 1 at the real "sustained shift" trigger threshold.
  // Drives the resting ember's warmth and size — never raw vital signs, so
  // it cannot flicker on an ordinary momentary blip.
  float activationScore = 0.0f;
  // A short, human-legible average — not the detector's own baseline — for
  // the check-in display. Only read at the moment a tap requests it.
  float displayHeartRate = 0.0f;
  float displayBreathRate = 0.0f;
};

struct SceneOutput {
  BreathField field;
  SceneState state = SceneState::Awakening;
  SceneText text = SceneText::None;
  float textOpacity = 0.0f;   // 0..1
  float progress = 0.0f;      // 0..1 through the guided session
  float progressOpacity = 0.0f;

  // While checking in: the numbers to show (snapshot at the moment the tap
  // requested them, so they hold still while you read them) and how visible
  // they are.
  float displayHeartRate = 0.0f;
  float displayBreathRate = 0.0f;
  float vitalsOpacity = 0.0f;
  // A brightening pulse timed to each detected heartbeat, shown only while
  // checking in.
  float heartbeatPulse = 0.0f;

  // While counting down: which number (3, 2, 1; 0 = none) and how visible.
  uint8_t countdownNumber = 0;
  float countdownOpacity = 0.0f;
};

class BreathScene {
 public:
  explicit BreathScene(const SceneConfig& config = SceneConfig());

  void begin(uint32_t nowMs);
  void update(const SceneInput& input);

  const SceneOutput& output() const { return output_; }
  SceneState state() const { return state_; }

  // announceShift = false for a session the person started themselves. The
  // device must not claim to have noticed something when it has not.
  void requestSession(uint32_t nowMs, bool announceShift);
  void dismiss(uint32_t nowMs);

  // A single tap does the next natural thing: resting -> a glance at your
  // numbers; while that's up -> a 3-2-1 into a session you started yourself.
  // Anywhere else, a tap is not a gesture this device recognises yet, and is
  // ignored rather than guessed at.
  void handleTap(uint32_t nowMs);
  void goToSleep(uint32_t nowMs);
  void wake(uint32_t nowMs);

  bool sessionActive() const;
  // Checking in or counting down: not a session (nothing was noticed, no
  // announcement), but still a moment the device is in the middle of —
  // a new request should wait, and a dismiss should be able to cancel it.
  bool interactive() const;
  bool consumeSessionFinished();

  const SceneConfig& config() const { return config_; }
  void setConfig(const SceneConfig& config) { config_ = config; }

  static float ramp(float t, float from, float to);
  static float mix(float a, float b, float t);

 private:
  void enter(SceneState state, uint32_t nowMs);
  void beginGuidingFromInvite(uint32_t nowMs);
  uint32_t elapsed(uint32_t nowMs) const;
  void buildTarget(const SceneInput& input, BreathField* target);
  void approach(BreathField* current, const BreathField& target, float dtMs);
  uint32_t guideTotalMs() const;

  SceneConfig config_;
  SceneOutput output_;
  SceneState state_ = SceneState::Awakening;
  uint32_t stateStartedMs_ = 0;
  uint32_t lastUpdateMs_ = 0;
  uint32_t lastPresenceMs_ = 0;
  uint32_t releaseDurationMs_ = 4600;
  float releaseRingRadius_ = 100.0f;
  float releaseRingLevel_ = 0.0f;
  float releaseVeilLevel_ = 0.0f;
  float releaseRingOuterWidth_ = 24.0f;
  float releaseRingInnerWidth_ = 17.0f;
  float liveWeight_ = 0.0f;
  float driftAmount_ = 1.0f;
  float progressOpacity_ = 0.0f;
  float progressValue_ = 0.0f;
  // Snapshot at the moment a tap requested the check-in, held steady while
  // it's shown rather than left to drift as you read it.
  float checkInHeartRate_ = 0.0f;
  float checkInBreathRate_ = 0.0f;
  // The latest values offered to update(), so handleTap() has something to
  // snapshot without needing its own copy of SceneInput.
  float lastDisplayHeartRate_ = 0.0f;
  float lastDisplayBreathRate_ = 0.0f;
  // A tap answering the invitation before "breathe with me" has safely
  // cleared can't take effect immediately — see handleTap — so it waits
  // here and is honoured the moment the word is gone.
  bool tapPending_ = false;
  bool started_ = false;
  bool dismissed_ = false;
  bool sessionFinished_ = false;
};
