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
  const uint32_t guideTotalMs =
      (config.inhaleMs + config.exhaleMs) * config.breathCycles;
  const uint32_t endMs = sessionAtMs + config.noticeMs + config.inviteMs +
                         guideTotalMs + config.releaseMs + 20000;

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
  bool sawShifted = false, sawInvite = false, sawInhale = false,
       sawExhale = false, sawSettled = false;
  uint32_t guidingFromMs = 0;
  std::vector<float> loudestPerCycle(config.breathCycles, 0.0f);

  for (uint32_t now = kStepMs; now <= endMs; now += kStepMs) {
    if (!requested && now >= sessionAtMs) {
      requested = true;
      scene.requestSession(now, true);
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

  printf("breath scene: ok\n");
  return 0;
}
