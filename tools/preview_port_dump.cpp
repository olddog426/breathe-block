// Prints the firmware's scene as CSV, for tools/check_preview_port.mjs to
// compare the browser preview against.
#include <cstdio>

#include "../BreatheBlock/BreathScene.h"

int main() {
  BreathScene scene;
  scene.begin(0);
  bool requested = false;
  bool tapped = false;
  for (uint32_t now = 40; now <= 90000; now += 40) {
    if (!requested && now >= 20000) {
      requested = true;
      scene.requestSession(now, true);
    }
    // Inviting waits for a tap rather than starting itself; answer it once
    // the ring has had a couple of seconds to settle, so the timeline still
    // exercises Guiding and Releasing too.
    if (!tapped && now >= 27200) {
      tapped = true;
      scene.handleTap(now);
    }
    SceneInput input;
    input.nowMs = now;
    input.presence = true;
    scene.update(input);
    if (now % 400 == 0) {
      const BreathField& field = scene.output().field;
      printf("%u,%.3f,%.4f,%.4f,%.4f,%.4f\n", now, field.ringRadius,
             field.ringLevel, field.coreLevel, field.horizonLevel,
             scene.output().textOpacity);
    }
  }
  return 0;
}
