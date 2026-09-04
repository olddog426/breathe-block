// Runs the *device* interface — real LVGL, real Montserrat text, real dirty
// rectangles, real flush-time compositing — on a laptop, and dumps frames.
//
// Because the host framebuffer is only ever written by the flush callback,
// anything wrong with the invalidation logic shows up immediately as smearing.
//
//   git clone --depth 1 -b v9.2.2 https://github.com/lvgl/lvgl
//   make -C tools host_ui LVGL_DIR=../lvgl
//   mkdir -p out && tools/host_ui out && python3 tools/contact_sheet.py out
#include <lvgl.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../BreatheBlock/BreathingUI.h"
#include "../BreatheBlock/DemoDirector.h"

#include <chrono>

HostSerial Serial;
uint32_t hostNowMs = 0;

uint32_t hostMicros() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<uint32_t>(
      duration_cast<microseconds>(steady_clock::now() - start).count());
}

namespace {

constexpr int kSize = 466;
constexpr int kCenter = 233;
constexpr int kDrawRows = 40;
constexpr uint32_t kStepMs = 25;  // LV_DEF_REFR_PERIOD is 33 ms

std::vector<uint16_t> framebuffer(kSize * kSize, 0);
BreathingUI ui;

uint32_t hostTick() { return hostNowMs; }

void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
  uint16_t* px = reinterpret_cast<uint16_t*>(pixels);
  const uint32_t startedUs = micros();
  ui.blendInto(area, px);
  ui.noteFrame(micros() - startedUs,
               static_cast<uint32_t>(area->x2 - area->x1 + 1) *
                   static_cast<uint32_t>(area->y2 - area->y1 + 1));

  const int width = area->x2 - area->x1 + 1;
  for (int y = area->y1; y <= area->y2; ++y) {
    memcpy(&framebuffer[y * kSize + area->x1], px + (y - area->y1) * width,
           width * sizeof(uint16_t));
  }
  lv_display_flush_ready(display);
}

void evenArea(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_INVALIDATE_AREA) return;
  lv_area_t* area = static_cast<lv_area_t*>(lv_event_get_param(event));
  if (!area) return;
  area->x1 &= ~1;
  area->x2 |= 1;
  area->y1 &= ~1;
  area->y2 |= 1;
}

void writePpm(const std::string& path) {
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) return;
  fprintf(file, "P6\n%d %d\n255\n", kSize, kSize);
  std::vector<unsigned char> row(kSize * 3);
  for (int y = 0; y < kSize; ++y) {
    for (int x = 0; x < kSize; ++x) {
      const int dx = x - kCenter, dy = y - kCenter;
      uint16_t value = framebuffer[y * kSize + x];
#if defined(LV_COLOR_16_SWAP) && LV_COLOR_16_SWAP
      // The framebuffer holds what the panel would receive.
      value = static_cast<uint16_t>((value << 8) | (value >> 8));
#endif
      if (dx * dx + dy * dy > kCenter * kCenter) value = 0;
      row[x * 3 + 0] = static_cast<unsigned char>(((value >> 11) & 0x1F) * 255 / 31);
      row[x * 3 + 1] = static_cast<unsigned char>(((value >> 5) & 0x3F) * 255 / 63);
      row[x * 3 + 2] = static_cast<unsigned char>((value & 0x1F) * 255 / 31);
    }
    fwrite(row.data(), 1, row.size(), file);
  }
  fclose(file);
}

struct Moment {
  uint32_t atMs;
  const char* label;
};

}  // namespace

int main(int argc, char** argv) {
  const std::string outDir = argc > 1 ? argv[1] : "out";

  lv_init();
  lv_tick_set_cb(hostTick);

  // lv_color_t is three bytes in LVGL 9, so an array of it is byte-aligned;
  // the RGB565 render buffer has to be aligned by hand.
  alignas(4) static uint8_t buffer1[kSize * kDrawRows * 2];
  alignas(4) static uint8_t buffer2[kSize * kDrawRows * 2];
  lv_display_t* display = lv_display_create(kSize, kSize);
  lv_display_set_flush_cb(display, flush);
  lv_display_set_buffers(display, buffer1, buffer2, sizeof(buffer1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(display, evenArea, LV_EVENT_INVALIDATE_AREA, nullptr);

  ui.begin();

  // Placed relative to the configured session so the sheet stays right
  // whatever AppConfig says about cycle count and pacing.
  const SceneConfig& cfg = ui.scene().config();
  // A slow, gradual climb rather than a quick ramp, so the sheet can show
  // it actually building rather than jumping from calm to hot.
  constexpr uint32_t kHeatRampStartMs = 2900;   // just after awakening settles
  constexpr uint32_t kHeatRampDurationMs = 4000;
  const uint32_t heatRampEndMs = kHeatRampStartMs + kHeatRampDurationMs;
  const uint32_t sessionAtMs = heatRampEndMs + 1200;  // hold for full convergence
  const uint32_t guideAtMs = sessionAtMs + cfg.noticeMs + cfg.inviteMs;
  const uint32_t cycleMs = cfg.inhaleMs + cfg.exhaleMs;
  const uint32_t guideEndMs = guideAtMs + cycleMs * cfg.breathCycles;
  const uint32_t lastCycle = cfg.breathCycles - 1;
  // A tap-tap check-in-and-countdown, exercised well after the noticed
  // session above has fully released so the two don't overlap.
  const uint32_t restingAfterMs = guideEndMs + cfg.releaseMs + 3000;
  const uint32_t firstTapAtMs = restingAfterMs + 1500;
  const uint32_t secondTapAtMs = firstTapAtMs + 2500;
  const std::vector<Moment> moments = {
      {400, "awakening-spark"},
      {1200, "awakening-wave"},
      {kHeatRampStartMs - 200, "resting"},
      {kHeatRampStartMs + kHeatRampDurationMs / 4, "resting-rising-25"},
      {kHeatRampStartMs + kHeatRampDurationMs / 2, "resting-rising-50"},
      {kHeatRampStartMs + 3 * kHeatRampDurationMs / 4, "resting-rising-75"},
      {sessionAtMs - 100, "resting-warm"},
      {sessionAtMs + 50, "noticing-hot"},
      {sessionAtMs + 700, "noticing-bloom"},
      {sessionAtMs + 2200, "noticing-words"},
      {sessionAtMs + 4600, "noticing-inward"},
      {sessionAtMs + cfg.noticeMs + 800, "inviting"},
      {sessionAtMs + cfg.noticeMs + 3000, "inviting-settle"},
      {guideAtMs + 1400, "guide-inhale"},
      {guideAtMs + cfg.inhaleMs - 200, "guide-full"},
      {guideAtMs + cfg.inhaleMs + 1400, "guide-exhale"},
      {guideAtMs + cycleMs - 400, "guide-floor"},
      {guideAtMs + 2 * cycleMs + 1400, "guide-half-words"},
      {guideAtMs + lastCycle * cycleMs + cfg.inhaleMs - 200, "guide-wordless"},
      {guideEndMs - 1200, "guide-late"},
      {guideEndMs + 1400, "releasing"},
      {guideEndMs + 2600, "releasing-word"},
      {guideEndMs + cfg.releaseMs + 3000, "resting-after"},
      {firstTapAtMs + 900, "checking-in"},
      {secondTapAtMs + 300, "countdown-3"},
      {secondTapAtMs + 1300, "countdown-2"},
      {secondTapAtMs + 2300, "countdown-1"},
      {secondTapAtMs + 3000 + 1400, "guide-from-tap"},
  };

  FILE* index = fopen((outDir + "/index.txt").c_str(), "w");
  if (!index) {
    fprintf(stderr, "cannot write index in %s\n", outDir.c_str());
    return 1;
  }

  size_t next = 0;
  bool requested = false;
  bool tappedIntoGuiding = false;
  bool tappedFirst = false;
  bool tappedSecond = false;
  const uint32_t endMs = moments.back().atMs + 2000;
  for (hostNowMs = 0; hostNowMs <= endMs && next < moments.size();
       hostNowMs += kStepMs) {
    if (!requested && hostNowMs >= sessionAtMs) {
      requested = true;
      ui.startSession(hostNowMs, true);
    }
    // Inviting waits for a tap rather than starting itself; answer it once
    // the ring has had time to settle.
    if (!tappedIntoGuiding && hostNowMs >= guideAtMs) {
      tappedIntoGuiding = true;
      ui.handleTap(hostNowMs);
    }
    // A plausible steady reading, so the check-in demo frame has numbers to
    // show and the heartbeat pulse has a real rhythm.
    ui.setVitals(68.0f, 13.0f, hostNowMs);
    if (!tappedFirst && hostNowMs >= firstTapAtMs) {
      tappedFirst = true;
      ui.handleTap(hostNowMs);
    }
    if (!tappedSecond && hostNowMs >= secondTapAtMs) {
      tappedSecond = true;
      ui.handleTap(hostNowMs);
    }
    ui.setPresence(true);
    // Stays at baseline through the "resting" sample, then a slow, gradual
    // climb through the rising-25/50/75 samples so the sheet can show it
    // actually building, with enough hold time before sessionAtMs for the
    // smoothed heat to fully converge rather than catching it mid-chase
    // right as the session starts.
    float activation = (static_cast<float>(hostNowMs) -
                        static_cast<float>(kHeatRampStartMs)) /
                       static_cast<float>(kHeatRampDurationMs);
    if (activation < 0.0f) activation = 0.0f;
    if (activation > 1.0f) activation = 1.0f;
    ui.setActivation(hostNowMs < sessionAtMs ? activation : 0.0f);
    ui.setLiveBreathPhase(sinf(hostNowMs * 0.0013f), true, hostNowMs);
    ui.update(hostNowMs);
    lv_timer_handler();

    if (hostNowMs >= moments[next].atMs) {
      char name[64];
      snprintf(name, sizeof(name), "%02zu-%s.ppm", next, moments[next].label);
      writePpm(outDir + "/" + name);
      // Text and the arc are drawn by LVGL here, so the index carries none.
      fprintf(index, "%s\t%s\t%s\t\t0\t0\t0\n", name, moments[next].label,
              ui.stateName());
      printf("%-18s %s\n", moments[next].label, ui.stateName());
      ++next;
    }
  }

  fclose(index);
  ui.printStats();
  printf("wrote %zu frames to %s\n", next, outDir.c_str());
  return 0;
}
