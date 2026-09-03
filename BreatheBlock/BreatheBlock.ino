// Breathe Block — a quiet, local, USB-powered desk companion.
// Target: Waveshare ESP32-S3-Touch-AMOLED-1.43 (466 x 466 round AMOLED)
//
// LVGL draws text and one hairline arc onto a true-black screen. The light
// field is composited into the flush buffer on its way to the panel, so the
// interface is a lamp rather than a set of widgets. See DESIGN.md.

#define LV_CONF_INCLUDE_SIMPLE

#include <Arduino.h>
#include <lvgl.h>

#include "amoled.h"
#include "AppConfig.h"
#include "BreathingUI.h"
#include "DemoDirector.h"
#include "RadarSensor.h"
#include "StressEngine.h"

namespace {
constexpr int kDisplayEnablePin = 42;
constexpr size_t kInternalDrawRows = 40;
constexpr size_t kPsramDrawRows = 80;

Amoled display;
HardwareSerial radarSerial(1);
RadarSensor radar(radarSerial, BreatheBlockConfig::kUseSimulatedRadar);

StressEngineConfig makeStressConfig() {
  StressEngineConfig value;
  if (BreatheBlockConfig::kUseSimulatedRadar) {
    // Bench timings: the real thresholds would take minutes to reach with a
    // simulated body, which makes the detection path impossible to watch.
    value.calibrationMs = BreatheBlockConfig::kSimulatedCalibrationMs;
    value.activationHoldMs = BreatheBlockConfig::kSimulatedActivationHoldMs;
    value.cooldownMs = BreatheBlockConfig::kSimulatedCooldownMs;
  } else {
    value.calibrationMs = BreatheBlockConfig::kCalibrationMs;
    value.activationHoldMs = BreatheBlockConfig::kActivationHoldMs;
    value.cooldownMs = BreatheBlockConfig::kPromptCooldownMs;
  }
  return value;
}

StressEngine stressEngine(makeStressConfig());
BreathingUI ui;
DemoDirector demo(ui, radar, stressEngine);

lv_display_t* lvDisplay = nullptr;
lv_color_t* drawBuffer1 = nullptr;
lv_color_t* drawBuffer2 = nullptr;
uint32_t lastDebugAtMs = 0;

uint32_t lvMillis() { return millis(); }

void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* pixels) {
  uint16_t* px = reinterpret_cast<uint16_t*>(pixels);

  // The background of every frame: LVGL hands over black plus whatever text it
  // drew, and the light field is added on the way to the panel.
  const uint32_t startedUs = micros();
  ui.blendInto(area, px);
  ui.noteFrame(micros() - startedUs,
               static_cast<uint32_t>(area->x2 - area->x1 + 1) *
                   static_cast<uint32_t>(area->y2 - area->y1 + 1));

  display.drawArea(area->x1, area->y1, area->x2, area->y2, px);
  lv_display_flush_ready(disp);
}

void evenDisplayArea(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_INVALIDATE_AREA) return;
  lv_area_t* area = static_cast<lv_area_t*>(lv_event_get_param(event));
  if (!area) return;
  area->x1 &= ~1;
  area->x2 |= 1;
  area->y1 &= ~1;
  area->y2 |= 1;
}

void fatalBlink(const char* message) {
  while (true) {
    Serial.println(message);
    delay(1000);
  }
}

size_t allocateDrawBuffers() {
  // Internal RAM first: drawArea copies out of these buffers on every flush,
  // and PSRAM reads are the slowest part of that path.
  size_t bytes = DISPLAY_WIDTH * kInternalDrawRows * sizeof(lv_color_t);
  drawBuffer1 = static_cast<lv_color_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  drawBuffer2 = static_cast<lv_color_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (drawBuffer1 && drawBuffer2) return bytes;

  heap_caps_free(drawBuffer1);
  heap_caps_free(drawBuffer2);
  bytes = DISPLAY_WIDTH * kPsramDrawRows * sizeof(lv_color_t);
  drawBuffer1 = static_cast<lv_color_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  drawBuffer2 = static_cast<lv_color_t*>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!drawBuffer1 || !drawBuffer2) fatalBlink("LVGL buffer allocation failed");
  Serial.println("Draw buffers in PSRAM (internal RAM was unavailable)");
  return bytes;
}

void beginDisplay() {
  pinMode(kDisplayEnablePin, OUTPUT);
  digitalWrite(kDisplayEnablePin, HIGH);
  delay(50);

  if (!display.begin()) fatalBlink("Display initialization failed");
  Serial.printf("Display: %s (0x%02X)\n", display.name(), display.ID());

  lv_init();
  lv_tick_set_cb(lvMillis);

  const size_t bufferBytes = allocateDrawBuffers();

  lvDisplay = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_display_set_flush_cb(lvDisplay, displayFlush);
  lv_display_set_buffers(lvDisplay, drawBuffer1, drawBuffer2, bufferBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(lvDisplay, evenDisplayArea, LV_EVENT_INVALIDATE_AREA,
                          nullptr);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(600);
  Serial.println("\nBreathe Block starting");

  beginDisplay();
  ui.begin();
  demo.begin(millis());

  radar.begin(BreatheBlockConfig::kRadarRxPin, BreatheBlockConfig::kRadarTxPin,
              BreatheBlockConfig::kRadarPrimaryBaud,
              BreatheBlockConfig::kRadarFallbackBaud);

  Serial.printf("Radar: %s\n", BreatheBlockConfig::kUseSimulatedRadar
                                   ? "simulated"
                                   : "live");
}

void loop() {
  const uint32_t nowMs = millis();
  const VitalSigns signs = radar.update(nowMs);
  const BodyAssessment assessment = stressEngine.update(signs, nowMs);

  // The one thing the radar is allowed to do to the interface unprompted.
  if (assessment.promptNow && !ui.sessionActive()) {
    Serial.println("[body] sustained relative shift — inviting");
    ui.startSession(nowMs, true);
  }

  ui.setPresence(signs.presence);
  ui.setLiveBreathPhase(signs.breathPhase, signs.breathPhaseFresh, nowMs);
  ui.setTestingVitals(signs.heartRate, signs.breathRate);
  ui.update(nowMs);
  ui.consumeSessionFinished();

  demo.update(nowMs);

  if (static_cast<uint32_t>(nowMs - lastDebugAtMs) >= 5000) {
    lastDebugAtMs = nowMs;
    Serial.printf(
        "%-9s | HR %.1f breath %.1f phase %+.2f | baseline %.1f/%.1f | "
        "load %.2f | baud %lu\n",
        ui.stateName(), signs.heartRate, signs.breathRate, signs.breathPhase,
        assessment.baselineHeartRate, assessment.baselineBreathRate,
        assessment.activationScore,
        static_cast<unsigned long>(radar.activeBaud()));
  }

  lv_timer_handler();
  delay(4);
}
