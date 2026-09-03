#include "DemoDirector.h"

namespace {

constexpr uint32_t kLongPressMs = 700;

const BreathPalette kPaletteChoices[] = {
    BreathPalettes::kIvory,
    BreathPalettes::kMoonlight,
    BreathPalettes::kEmber,
};
const char* const kPaletteNames[] = {"ivory", "moonlight", "ember"};
constexpr uint8_t kPaletteCount = 3;

}  // namespace

DemoDirector::DemoDirector(BreathingUI& ui, RadarSensor& radar,
                           StressEngine& engine)
    : ui_(ui), radar_(radar), engine_(engine) {}

void DemoDirector::begin(uint32_t nowMs) {
  pinMode(BreatheBlockConfig::kManualButtonPin, INPUT_PULLUP);
  stepAtMs_ = nowMs;
  tourRunning_ = BreatheBlockConfig::kDemoMode ==
                 BreatheBlockConfig::DemoMode::Tour;
  if (BreatheBlockConfig::kSerialConsole) printHelp();
}

void DemoDirector::announce(const char* what) const {
  Serial.printf("[demo] %s\n", what);
}

void DemoDirector::printHelp() const {
  Serial.println(
      "\nBreathe Block console\n"
      "  n  a noticed session (starts with \"your breathing has shifted\")\n"
      "  m  a session you started yourself (skips that line)\n"
      "  c  simulate a tap (no touch hardware needed) — twice: check in,\n"
      "     then a 3-2-1 into breathing\n"
      "  x  dismiss the current session\n"
      "  r  return to the quiet state\n"
      "  s  sleep      w  wake\n"
      "  b  hold the simulated body in sustained activation (toggle)\n"
      "  t  run the looping tour of every state (toggle)\n"
      "  p  next palette      f  frame timings      ?  this list");
}

void DemoDirector::nextPalette() {
  paletteIndex_ = static_cast<uint8_t>((paletteIndex_ + 1) % kPaletteCount);
  ui_.setPalette(kPaletteChoices[paletteIndex_]);
  Serial.printf("[demo] palette: %s\n", kPaletteNames[paletteIndex_]);
}

void DemoDirector::handleButton(uint32_t nowMs) {
  const bool down = digitalRead(BreatheBlockConfig::kManualButtonPin) == LOW;

  if (down && !buttonDown_) {
    buttonPressedAtMs_ = nowMs;
  } else if (!down && buttonDown_) {
    const bool longPress =
        static_cast<uint32_t>(nowMs - buttonPressedAtMs_) >= kLongPressMs;

    if (BreatheBlockConfig::kDemoMode ==
            BreatheBlockConfig::DemoMode::Manual &&
        !longPress) {
      // Step through the tour by hand, one scene per press.
      tourRunning_ = false;
      stepAtMs_ = nowMs;
      step_ = static_cast<TourStep>(
          (static_cast<uint8_t>(step_) + 1) %
          static_cast<uint8_t>(TourStep::Count));
      runTour(nowMs);
    } else if (ui_.sessionActive()) {
      announce("dismissed");
      ui_.dismiss(nowMs);
    } else {
      // A session you started yourself: the device does not claim to have
      // noticed anything.
      announce("session started from the button");
      ui_.startSession(nowMs, false);
      engine_.noteSessionStarted(nowMs);
    }
  }
  buttonDown_ = down;
}

void DemoDirector::handleSerial(uint32_t nowMs) {
  while (Serial.available() > 0) {
    const int key = Serial.read();
    switch (key) {
      case 'n':
        announce("noticed session");
        ui_.startSession(nowMs, true);
        engine_.noteSessionStarted(nowMs);
        break;
      case 'm':
        announce("self-started session");
        ui_.startSession(nowMs, false);
        engine_.noteSessionStarted(nowMs);
        break;
      case 'c':
        announce("simulated tap");
        ui_.handleTap(nowMs);
        break;
      case 'x':
        announce("dismissed");
        ui_.dismiss(nowMs);
        break;
      case 'r':
        announce("quiet");
        ui_.dismiss(nowMs);
        ui_.wake(nowMs);
        break;
      case 's':
        announce("sleeping");
        ui_.goToSleep(nowMs);
        break;
      case 'w':
        announce("awake");
        ui_.wake(nowMs);
        break;
      case 'b':
        radar_.setForcedActivation(!radar_.forcedActivation());
        Serial.printf("[demo] simulated activation %s\n",
                      radar_.forcedActivation() ? "held on" : "released");
        break;
      case 't':
        tourRunning_ = !tourRunning_;
        stepAtMs_ = nowMs;
        Serial.printf("[demo] tour %s\n", tourRunning_ ? "running" : "stopped");
        break;
      case 'p':
        nextPalette();
        break;
      case 'f':
        ui_.printStats();
        break;
      case '?':
      case 'h':
        printHelp();
        break;
      default:
        break;
    }
  }
}

void DemoDirector::runTour(uint32_t nowMs) {
  switch (step_) {
    case TourStep::Settle:
      ui_.wake(nowMs);
      announce("tour: the quiet state");
      break;
    case TourStep::NoticedSession:
      announce("tour: a noticed session");
      ui_.startSession(nowMs, true);
      engine_.noteSessionStarted(nowMs);
      // Inviting waits for a tap; the tour answers its own a couple of
      // seconds after the invitation would have settled.
      pendingTapAtMs_ =
          nowMs + ui_.scene().config().noticeMs + 2000;
      break;
    case TourStep::Breathe:
      announce("tour: back to quiet");
      break;
    case TourStep::ManualSession:
      announce("tour: a session you started yourself");
      ui_.startSession(nowMs, false);
      engine_.noteSessionStarted(nowMs);
      pendingTapAtMs_ = nowMs + 2000;
      break;
    case TourStep::Drift:
      announce("tour: quiet again");
      break;
    case TourStep::Sleep:
      announce("tour: nobody there");
      ui_.goToSleep(nowMs);
      break;
    case TourStep::Wake:
      announce("tour: somebody there");
      ui_.wake(nowMs);
      break;
    case TourStep::Count:
      break;
  }
}

void DemoDirector::update(uint32_t nowMs) {
  handleButton(nowMs);
  if (BreatheBlockConfig::kSerialConsole) handleSerial(nowMs);
  if (pendingTapAtMs_ != 0 && nowMs >= pendingTapAtMs_) {
    pendingTapAtMs_ = 0;
    // Only if the invitation is still the one waiting — it may already have
    // been dismissed (BOOT button, or the tour stopped) by the time this
    // fires, and a tap means something different in every other state.
    if (ui_.state() == SceneState::Inviting) ui_.handleTap(nowMs);
  }
  if (!tourRunning_) return;

  // A session always plays out in full; the tour only fills the gaps between.
  if (ui_.sessionActive()) {
    stepAtMs_ = nowMs;
    return;
  }

  static constexpr uint32_t kGapMs[] = {6000, 500, 4000, 500, 4000, 7000, 5000};
  const uint8_t index = static_cast<uint8_t>(step_);
  if (static_cast<uint32_t>(nowMs - stepAtMs_) < kGapMs[index]) return;

  stepAtMs_ = nowMs;
  step_ = static_cast<TourStep>(
      (index + 1) % static_cast<uint8_t>(TourStep::Count));
  runTour(nowMs);
}
