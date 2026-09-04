// The interaction's invariants, asserted over whole sessions.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "BreathField.h"
#include "BreathScene.h"

namespace {

constexpr uint32_t kStepMs = 40;  // a little slower than the device refreshes

SceneInput at(uint32_t nowMs, bool presence = true) {
  SceneInput input;
  input.nowMs = nowMs;
  input.presence = presence;
  input.livePhaseValid = true;
  input.liveBreath = sinf(nowMs * 0.0013f);
  return input;
}

// How different two frames look, measured where it matters: the rendered
// radial intensity profile.
float profileDistance(const BreathField& a, const BreathField& b) {
  float worst = 0.0f;
  for (int r = 0; r <= 250; ++r) {
    const float delta =
        fabsf(a.intensityAt(static_cast<float>(r)) -
              b.intensityAt(static_cast<float>(r)));
    if (delta > worst) worst = delta;
  }
  return worst;
}

const char* name(SceneState state) {
  switch (state) {
    case SceneState::Awakening: return "awakening";
    case SceneState::Resting: return "resting";
    case SceneState::Sleeping: return "sleeping";
    case SceneState::CheckingIn: return "checking-in";
    case SceneState::Countdown: return "countdown";
    case SceneState::Noticing: return "noticing";
    case SceneState::Inviting: return "inviting";
    case SceneState::Guiding: return "guiding";
    case SceneState::Releasing: return "releasing";
  }
  return "?";
}

}  // namespace

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  const SceneConfig config;
  const uint32_t sessionAtMs = 20000;
  // Inviting waits for a tap rather than starting itself; answer it once the
  // ring has had a couple of seconds to settle.
  const uint32_t tapAtMs = sessionAtMs + config.noticeMs + 2000;
  const uint32_t guideTotalMs =
      (config.inhaleMs + config.exhaleMs) * config.breathCycles;
  const uint32_t endMs =
      tapAtMs + guideTotalMs + config.releaseMs + 20000;

  BreathScene scene(config);
  scene.begin(0);

  BreathField previous = scene.output().field;
  float worstSpeed = 0.0f;  // how fast the picture may change in one frame
  float worstExtent = 0.0f;
  std::vector<float> speeds;      // per frame
  std::vector<uint32_t> handovers;  // frame indices where the state changed

  SceneText shownText = SceneText::None;
  float shownOpacity = 0.0f;

  std::string sequence;
  SceneState lastState = scene.state();
  sequence += name(lastState);

  bool requested = false;
  bool tapped = false;
  bool sawShifted = false, sawInvite = false, sawInhale = false,
       sawExhale = false, sawSettled = false;
  uint32_t guidingFromMs = 0;
  std::vector<float> loudestPerCycle(config.breathCycles, 0.0f);

  for (uint32_t now = kStepMs; now <= endMs; now += kStepMs) {
    if (!requested && now >= sessionAtMs) {
      requested = true;
      scene.requestSession(now, true);
    }
    if (!tapped && now >= tapAtMs) {
      tapped = true;
      scene.handleTap(now);
    }
    scene.update(at(now));
    const SceneOutput& out = scene.output();

    // 1. Nothing in the interface may step. Measured on what is drawn, not on
    //    the parameters, so easing mistakes anywhere show up here.
    const float speed = profileDistance(previous, out.field);
    if (speed > worstSpeed) worstSpeed = speed;
    speeds.push_back(speed);

    if (out.state != lastState) {
      if (out.state == SceneState::Guiding) guidingFromMs = now;
      handovers.push_back(static_cast<uint32_t>(speeds.size() - 1));
      lastState = out.state;
      sequence += " -> ";
      sequence += name(lastState);
    }
    previous = out.field;

    // 2. A word is only ever swapped while it is invisible.
    if (out.text != shownText) {
      assert(shownOpacity <= 0.0001f &&
             "text changed while the previous word was still visible");
      shownText = out.text;
    }
    shownOpacity = out.textOpacity;

    // 3. The light stays on the display during a session; only the deliberate
    //    awakening wave and closing release are allowed to wash off the rim.
    if (out.state == SceneState::Noticing ||
        out.state == SceneState::Inviting || out.state == SceneState::Guiding) {
      assert(out.field.extent() <= 233.0f && "the field left the display");
    }
    if (out.field.extent() > worstExtent) worstExtent = out.field.extent();

    // 4. Wherever there is a word, the light steps aside for it.
    if (out.textOpacity > 0.5f) {
      assert(out.field.wellDepth > 0.3f && "text shown with no protection");
    }

    if (out.state == SceneState::Guiding) {
      const uint32_t cycle =
          (now - guidingFromMs) / (config.inhaleMs + config.exhaleMs);
      if (cycle < loudestPerCycle.size() &&
          out.textOpacity > loudestPerCycle[cycle]) {
        loudestPerCycle[cycle] = out.textOpacity;
      }
    }

    switch (out.text) {
      case SceneText::BreathingShifted: sawShifted = true; break;
      case SceneText::BreatheWithMe: sawInvite = true; break;
      case SceneText::Inhale: sawInhale = true; break;
      case SceneText::Exhale: sawExhale = true; break;
      case SceneText::Settled: sawSettled = true; break;
      case SceneText::None: break;
    }
  }

  printf("sequence: %s\n", sequence.c_str());
  // The frame that crosses a state boundary must be no more eventful than the
  // frames either side of it. This is the "it cannot snap" claim, and it holds
  // whether the interface happens to be moving quickly at the time or not.
  float worstRatio = 0.0f;
  for (uint32_t index : handovers) {
    float neighbourhood = 0.0f;
    for (int offset = -6; offset <= 6; ++offset) {
      if (offset == 0) continue;
      const long at = static_cast<long>(index) + offset;
      if (at < 0 || at >= static_cast<long>(speeds.size())) continue;
      if (speeds[at] > neighbourhood) neighbourhood = speeds[at];
    }
    const float ratio = speeds[index] / (neighbourhood + 0.0005f);
    if (ratio > worstRatio) worstRatio = ratio;
  }

  printf("fastest frame: %.4f | busiest handover vs its neighbours: %.2fx | "
         "max extent %.1f px\n",
         worstSpeed, worstRatio, worstExtent);

  // Nothing anywhere may jump.
  assert(worstSpeed < 0.09f && "a frame changed abruptly");
  assert(worstRatio < 1.6f && "a state change was visible as a step");
  assert(sequence ==
             "awakening -> resting -> noticing -> inviting -> guiding -> "
             "releasing -> resting" &&
         "unexpected state flow");
  assert(sawShifted && sawInvite && sawInhale && sawExhale && sawSettled);

  // The words are scaffolding, and scaffolding comes down: two cycles of
  // guidance, one at half, then the light carries it alone.
  printf("word opacity per cycle:");
  for (float loudest : loudestPerCycle) printf(" %.2f", loudest);
  printf("\n");
  assert(loudestPerCycle[0] > 0.95f && loudestPerCycle[1] > 0.95f);
  assert(loudestPerCycle[2] > 0.4f && loudestPerCycle[2] < 0.6f);
  for (size_t cycle = 3; cycle < loudestPerCycle.size(); ++cycle) {
    assert(loudestPerCycle[cycle] == 0.0f && "the words never stop");
  }

  // A session the person started themselves must not claim anything was
  // noticed: it skips straight to the invitation.
  {
    BreathScene manual(config);
    manual.begin(0);
    for (uint32_t now = kStepMs; now <= 6000; now += kStepMs)
      manual.update(at(now));
    manual.requestSession(6000, false);
    manual.update(at(6040));
    assert(manual.state() == SceneState::Inviting);
  }

  // Turning a session down is quiet: a short release and no closing word.
  {
    BreathScene dismissed(config);
    dismissed.begin(0);
    for (uint32_t now = kStepMs; now <= 6000; now += kStepMs)
      dismissed.update(at(now));
    dismissed.requestSession(6000, true);
    dismissed.update(at(6040));
    dismissed.dismiss(8000);
    bool spoke = false;
    for (uint32_t now = 8040; now <= 8040 + config.dismissMs + 2000;
         now += kStepMs) {
      dismissed.update(at(now));
      if (dismissed.output().text != SceneText::None) spoke = true;
    }
    assert(!spoke && "the device commented on being turned down");
    assert(dismissed.state() == SceneState::Resting);
  }

  // Nobody in the room: the object goes quiet, and comes back when they do.
  {
    SceneConfig sleepy = config;
    sleepy.sleepAfterMs = 5000;
    BreathScene idle(sleepy);
    idle.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      idle.update(at(now));
    for (uint32_t now = 3040; now <= 12000; now += kStepMs)
      idle.update(at(now, false));
    assert(idle.state() == SceneState::Sleeping);
    const float asleep = idle.output().field.coreLevel;

    for (uint32_t now = 12040; now <= 20000; now += kStepMs)
      idle.update(at(now, true));
    assert(idle.state() == SceneState::Resting);
    assert(idle.output().field.coreLevel > asleep * 1.5f &&
           "waking should be visible");
  }

  // A gentle warmth precursor to noticing: it tracks the detector's own
  // activation score while resting, stays neutral at baseline, and never
  // touches sleeping (nobody's there to warm the ember for).
  {
    BreathScene warm(config);
    warm.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs) {
      SceneInput input = at(now);
      input.activationScore = 0.0f;
      warm.update(input);
    }
    assert(warm.state() == SceneState::Resting);
    const float atBaseline = warm.output().field.warmth;
    assert(atBaseline < 0.02f && "warmth should sit near neutral at baseline");

    for (uint32_t now = 3040; now <= 8000; now += kStepMs) {
      SceneInput input = at(now);
      input.activationScore = 1.0f;
      warm.update(input);
    }
    const float atActivation = warm.output().field.warmth;
    assert(atActivation > atBaseline + 0.15f &&
           "a sustained rise in activation should visibly warm the ember");

    // Asleep, nobody's there: warmth must not apply regardless of score.
    BreathScene asleepWarm(config);
    asleepWarm.begin(0);
    SceneConfig sleepy = config;
    sleepy.sleepAfterMs = 2000;
    asleepWarm.setConfig(sleepy);
    for (uint32_t now = kStepMs; now <= 1500; now += kStepMs)
      asleepWarm.update(at(now));
    for (uint32_t now = 1540; now <= 6000; now += kStepMs) {
      SceneInput input = at(now, false);
      input.activationScore = 1.0f;
      asleepWarm.update(input);
    }
    assert(asleepWarm.state() == SceneState::Sleeping);
    assert(asleepWarm.output().field.warmth < 0.02f &&
           "sleeping must stay neutral no matter the activation score");
  }

  // Unlike warmth's subliminal nudge, heat is meant to be seen: the ember
  // visibly climbs from neutral through orange toward red as activation
  // approaches the threshold, and stays neutral asleep regardless of score.
  {
    BreathScene hot(config);
    hot.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs) {
      SceneInput input = at(now);
      input.activationScore = 0.0f;
      hot.update(input);
    }
    assert(hot.state() == SceneState::Resting);
    assert(hot.output().field.heat < 0.02f &&
           "heat should sit near neutral at baseline");

    for (uint32_t now = 3040; now <= 8000; now += kStepMs) {
      SceneInput input = at(now);
      input.activationScore = 1.0f;
      hot.update(input);
    }
    assert(hot.output().field.heat > 0.9f &&
           "a sustained rise in activation should visibly heat the ember");

    BreathScene asleepHot(config);
    asleepHot.begin(0);
    SceneConfig sleepy2 = config;
    sleepy2.sleepAfterMs = 2000;
    asleepHot.setConfig(sleepy2);
    for (uint32_t now = kStepMs; now <= 1500; now += kStepMs)
      asleepHot.update(at(now));
    for (uint32_t now = 1540; now <= 6000; now += kStepMs) {
      SceneInput input = at(now, false);
      input.activationScore = 1.0f;
      asleepHot.update(input);
    }
    assert(asleepHot.state() == SceneState::Sleeping);
    assert(asleepHot.output().field.heat < 0.02f &&
           "sleeping must stay neutral no matter the activation score");
  }

  // Whatever heat the ember was carrying releases outward into the noticing
  // bloom rather than starting neutral, then cools as the rim draws inward —
  // and never as a jump, wherever it happens to be in that cool-down.
  {
    BreathScene release(config);
    release.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      release.update(at(now));
    for (uint32_t now = 3040; now <= 8000; now += kStepMs) {
      SceneInput input = at(now);
      input.activationScore = 1.0f;
      release.update(input);
    }
    assert(release.state() == SceneState::Resting);
    const float heatBeforeNotice = release.output().field.heat;
    assert(heatBeforeNotice > 0.9f &&
           "should be hot going into a noticed session");

    release.requestSession(8000, true);
    // heat is a colour-only channel (see BreathLut::build), invisible to
    // profileDistance's intensity comparison — checked directly instead.
    float previousHeat = release.output().field.heat;
    float worstHeatStep = 0.0f;
    for (uint32_t now = 8040; now <= 8000 + config.noticeMs; now += kStepMs) {
      SceneInput input = at(now);
      input.activationScore = 1.0f;  // still elevated; only noticedHeat_ decays
      release.update(input);
      const float step = fabsf(release.output().field.heat - previousHeat);
      if (step > worstHeatStep) worstHeatStep = step;
      previousHeat = release.output().field.heat;
    }
    printf("noticed-heat release: worst frame-to-frame change %.4f\n",
           worstHeatStep);
    assert(worstHeatStep < 0.03f && "the heat release must not jump");
    assert(release.state() == SceneState::Inviting);
    assert(release.output().field.heat < 0.05f &&
           "the carried heat should have cooled by the time the invitation forms");
  }

  // A tap while resting shows a snapshot of your numbers, warms up and cools
  // back down without a jump anywhere in the sequence, and — left alone —
  // quietly returns to rest rather than waiting forever.
  {
    BreathScene checkIn(config);
    checkIn.begin(0);
    BreathField previous = checkIn.output().field;
    float worstStep = 0.0f;

    auto stepAndTrack = [&](uint32_t now, float heartRate, float breathRate) {
      SceneInput input = at(now);
      input.displayHeartRate = heartRate;
      input.displayBreathRate = breathRate;
      checkIn.update(input);
      const float step = profileDistance(previous, checkIn.output().field);
      if (step > worstStep) worstStep = step;
      previous = checkIn.output().field;
    };

    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      stepAndTrack(now, 68.0f, 13.0f);
    assert(checkIn.state() == SceneState::Resting);

    checkIn.handleTap(3000);
    stepAndTrack(3040, 68.0f, 13.0f);
    assert(checkIn.state() == SceneState::CheckingIn);
    assert(checkIn.output().displayHeartRate == 68.0f);
    assert(checkIn.output().displayBreathRate == 13.0f);

    // The numbers are a snapshot: later readings must not change what's
    // shown, or the display would jitter while you're trying to read it.
    for (uint32_t now = 3080; now <= 3600; now += kStepMs)
      stepAndTrack(now, 140.0f, 30.0f);  // a wildly different live reading
    assert(checkIn.output().displayHeartRate == 68.0f &&
           "the check-in display must hold its snapshot, not track live input");

    bool sawBeat = false;
    for (uint32_t now = 3640; now <= 9200; now += kStepMs) {
      stepAndTrack(now, 68.0f, 13.0f);
      if (checkIn.output().heartbeatPulse > 0.5f) sawBeat = true;
    }
    assert(sawBeat && "a plausible heart rate should produce visible beats");
    assert(checkIn.state() == SceneState::Resting &&
           "an unacknowledged check-in should quietly return to rest");
    assert(checkIn.output().heartbeatPulse == 0.0f &&
           "the pulse must not continue once resting again");

    printf("check-in: worst frame-to-frame change %.4f\n", worstStep);
    assert(worstStep < 0.09f && "a frame changed abruptly");
  }

  // Tapping again while checked in counts down 3, 2, 1 and starts a session
  // you asked for yourself — never claiming to have noticed anything.
  {
    BreathScene tapTap(config);
    tapTap.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      tapTap.update(at(now));

    tapTap.handleTap(3000);
    tapTap.update(at(3040));
    assert(tapTap.state() == SceneState::CheckingIn);

    tapTap.handleTap(3040);
    tapTap.update(at(3080));
    assert(tapTap.state() == SceneState::Countdown);

    std::vector<int> numbersSeen;
    int lastNumber = -1;
    bool sawShiftedClaim = false;
    for (uint32_t now = 3120; now <= 3080 + config.countdownStepMs * 3 + 200;
         now += kStepMs) {
      tapTap.update(at(now));
      const int current = tapTap.output().countdownNumber;
      if (current != 0 && current != lastNumber) {
        numbersSeen.push_back(current);
        lastNumber = current;
      }
      if (tapTap.output().text == SceneText::BreathingShifted) {
        sawShiftedClaim = true;
      }
    }
    assert((numbersSeen == std::vector<int>{3, 2, 1}) &&
           "the countdown must count 3, 2, 1 in order");
    assert(!sawShiftedClaim &&
           "a session you asked for must never claim to have noticed anything");
    assert(tapTap.state() == SceneState::Guiding);
  }

  // A tap anywhere else — mid-session, asleep — is not a gesture this device
  // recognises yet, and must be inert rather than guessed at.
  {
    BreathScene ignored(config);
    ignored.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      ignored.update(at(now));
    ignored.requestSession(3000, true);
    ignored.update(at(3040));
    assert(ignored.state() == SceneState::Noticing);
    ignored.handleTap(3040);
    ignored.update(at(3080));
    assert(ignored.state() == SceneState::Noticing &&
           "a tap during an announced session must not be reinterpreted");
  }

  // Dismissing out of a check-in or a countdown returns to rest immediately,
  // and a radar-announced session must never barge in on top of either.
  {
    BreathScene cancel(config);
    cancel.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      cancel.update(at(now));
    cancel.handleTap(3000);
    cancel.update(at(3040));
    assert(cancel.state() == SceneState::CheckingIn);

    cancel.requestSession(3040, true);
    cancel.update(at(3080));
    assert(cancel.state() == SceneState::CheckingIn &&
           "an announced session must wait for the check-in to finish");

    cancel.dismiss(3080);
    cancel.update(at(3120));
    assert(cancel.state() == SceneState::Resting);
  }

  // An early tap — while "breathe with me" is still on screen — must not cut
  // the word off mid-visibility: it's deferred and honoured once the word
  // has genuinely cleared.
  {
    BreathScene early(config);
    early.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      early.update(at(now));
    early.requestSession(3000, true);

    SceneText shownText = SceneText::None;
    float shownOpacity = 0.0f;
    bool tapped = false;
    const uint32_t inviteAtMs = 3000 + config.noticeMs;
    for (uint32_t now = 3040; now <= inviteAtMs + 6000; now += kStepMs) {
      // Tap right as the invitation begins, when "breathe with me" is about
      // to be at its most visible — the worst case for cutting it short.
      if (!tapped && now >= inviteAtMs + 100) {
        tapped = true;
        early.handleTap(now);
      }
      early.update(at(now));
      const SceneOutput& out = early.output();
      if (out.text != shownText) {
        assert(shownOpacity <= 0.0001f &&
               "an early tap cut the invitation off mid-visibility");
        shownText = out.text;
      }
      shownOpacity = out.textOpacity;
    }
    assert(early.state() == SceneState::Guiding &&
           "a deferred tap must still be honoured once the word clears");
  }

  // Left entirely alone, an invitation nobody answers withdraws quietly —
  // no closing word, same as a session turned down — rather than waiting
  // forever or starting itself.
  {
    BreathScene unanswered(config);
    unanswered.begin(0);
    for (uint32_t now = kStepMs; now <= 3000; now += kStepMs)
      unanswered.update(at(now));
    unanswered.requestSession(3000, false);  // straight to Inviting
    unanswered.update(at(3040));
    assert(unanswered.state() == SceneState::Inviting);

    bool settled = false;
    for (uint32_t now = 3080;
         now <= 3040 + config.inviteTimeoutMs + config.releaseMs + 2000;
         now += kStepMs) {
      unanswered.update(at(now));
      if (unanswered.output().text == SceneText::Settled) settled = true;
    }
    assert(!settled && "an unanswered invitation should not comment on itself");
    assert(unanswered.state() == SceneState::Resting &&
           "an unanswered invitation should quietly return to rest");
  }

  printf("breath scene: ok\n");
  return 0;
}
