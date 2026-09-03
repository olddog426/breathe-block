// Renders the real BreathField/BreathScene code to PPM frames on a laptop, so
// the interface can be reviewed and regression-checked with no ESP32 and no
// radar attached.
//
//   make -C tools preview && tools/preview out/
//
// Writes frame PPMs plus index.txt, which carries the text and progress state
// for each frame (see tools/contact_sheet.py).
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../BreatheBlock/BreathField.h"
#include "../BreatheBlock/BreathScene.h"

namespace {

constexpr int kSize = 466;
constexpr int kCenter = 233;
constexpr uint32_t kStepMs = 40;

const char* textName(SceneText text) {
  switch (text) {
    case SceneText::BreathingShifted: return "your breathing|has shifted";
    case SceneText::BreatheWithMe: return "breathe with me";
    case SceneText::Inhale: return "inhale";
    case SceneText::Exhale: return "exhale";
    case SceneText::Settled: return "settled";
    case SceneText::None: default: return "";
  }
}

const char* stateName(SceneState state) {
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

struct Moment {
  uint32_t atMs;
  const char* label;
};

void writePpm(const std::string& path, const uint16_t* pixels) {
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    fprintf(stderr, "cannot write %s\n", path.c_str());
    return;
  }
  fprintf(file, "P6\n%d %d\n255\n", kSize, kSize);
  std::vector<unsigned char> row(kSize * 3);
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      const int dx = x - kCenter;
      const int dy = y - kCenter;
      uint16_t value = pixels[y * kSize + x];
      if (dx * dx + dy * dy > kCenter * kCenter) value = 0;  // round bezel
      const int r5 = (value >> 11) & 0x1F;
      const int g6 = (value >> 5) & 0x3F;
      const int b5 = value & 0x1F;
      row[x * 3 + 0] = static_cast<unsigned char>((r5 * 255 + 15) / 31);
      row[x * 3 + 1] = static_cast<unsigned char>((g6 * 255 + 31) / 63);
      row[x * 3 + 2] = static_cast<unsigned char>((b5 * 255 + 15) / 31);
    }
    fwrite(row.data(), 1, row.size(), file);
  }
  fclose(file);
}

}  // namespace

int main(int argc, char** argv) {
  const std::string outDir = argc > 1 ? argv[1] : "out";

  SceneConfig config;
  BreathScene scene(config);
  BreathLut lut;
  std::vector<uint16_t> frame(kSize * kSize);

  const uint32_t sessionAtMs = 6000;
  const std::vector<Moment> moments = {
      {400, "awakening-spark"},   {1200, "awakening-wave"},
      {4000, "resting"},          {sessionAtMs - 300, "resting-warm"},
      {6700, "noticing-bloom"},
      {8200, "noticing-words"},   {10600, "noticing-inward"},
      {12000, "inviting"},        {14200, "inviting-settle"},
      {15600, "guide-inhale"},    {18000, "guide-full"},
      {19600, "guide-exhale"},    {23800, "guide-floor"},
      {35600, "guide-half-words"},{48000, "guide-wordless"},
      {57000, "guide-late"},      {66000, "releasing"},
      {67600, "releasing-word"},  {72000, "resting-after"},
  };

  size_t next = 0;
  FILE* index = fopen((outDir + "/index.txt").c_str(), "w");
  if (!index) {
    fprintf(stderr, "cannot write index in %s (does the directory exist?)\n",
            outDir.c_str());
    return 1;
  }

  scene.begin(0);
  bool requested = false;
  for (uint32_t now = 0; now <= 80000 && next < moments.size();
       now += kStepMs) {
    if (!requested && now >= sessionAtMs) {
      requested = true;
      scene.requestSession(now, true);
    }

    SceneInput input;
    input.nowMs = now;
    input.presence = true;
    input.livePhaseValid = true;
    input.liveBreath = sinf(now * 0.0013f);
    // Stays at baseline through the "resting" sample, then a sustained shift
    // builds toward the session, so "resting-warm" shows the ember's
    // continuity precursor rather than a mid-ramp value at both samples.
    float activation = (static_cast<float>(now) - 4400.0f) / 1400.0f;
    if (activation < 0.0f) activation = 0.0f;
    if (activation > 1.0f) activation = 1.0f;
    input.activationScore = now < sessionAtMs ? activation : 0.0f;
    scene.update(input);

    if (now >= moments[next].atMs) {
      const SceneOutput& out = scene.output();
      lut.build(out.field, BreathPalettes::kIvory);
      memset(frame.data(), 0, frame.size() * sizeof(uint16_t));
      lut.blendArea(0, 0, kSize - 1, kSize - 1, frame.data());

      char name[64];
      snprintf(name, sizeof(name), "%02zu-%s.ppm", next, moments[next].label);
      writePpm(outDir + "/" + name, frame.data());
      fprintf(index, "%s\t%s\t%s\t%s\t%.4f\t%.4f\t%.4f\n", name,
              moments[next].label, stateName(out.state), textName(out.text),
              out.textOpacity, out.progress, out.progressOpacity);
      printf("%-18s %-10s r=%6.1f level=%.3f extent=%5.1f text=%.2f\n",
             moments[next].label, stateName(out.state), out.field.ringRadius,
             out.field.ringLevel, out.field.extent(), out.textOpacity);
      ++next;
    }
  }

  fclose(index);
  printf("wrote %zu frames to %s\n", next, outDir.c_str());
  return 0;
}
