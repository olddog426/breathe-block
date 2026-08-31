// Breathe Block — quiet, local, USB-powered desk companion.
// Target: Waveshare ESP32-S3-Touch-AMOLED-1.43 (466 x 466 AMOLED)

#define LV_CONF_INCLUDE_SIMPLE

#include <Arduino.h>
#include <lvgl.h>

#include "amoled.h"
#include "AppConfig.h"
#include "BreathingUI.h"
#include "RadarSensor.h"
#include "StressEngine.h"

namespace {
constexpr int kDisplayEnablePin = 42;
constexpr size_t kDrawRows = 80;
constexpr size_t kDrawBufferBytes =
    DISPLAY_WIDTH * kDrawRows * sizeof(lv_color_t);

Amoled display;
HardwareSerial radarSerial(1);
RadarSensor radar(radarSerial, BreatheBlockConfig::kUseSimulatedRadar);

StressEngineConfig makeStressConfig() {
  StressEngineConfig value;
  value.calibrationMs = BreatheBlockConfig::kCalibrationMs;
  value.activationHoldMs = BreatheBlockConfig::kActivationHoldMs;
  value.cooldownMs = BreatheBlockConfig::kPromptCooldownMs;
  return value;
}

StressEngine stressEngine(makeStressConfig());
BreathingUI ui;
lv_display_t* lvDisplay = nullptr;
lv_color_t* drawBuffer1 = nullptr;
lv_color_t* drawBuffer2 = nullptr;

bool demoStarted = false;
bool previousButtonDown = false;
uint32_t lastDebugAtMs = 0;

uint32_t lvMillis() { return millis(); }

void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* pixels) {
  display.drawArea(area->x1, area->y1, area->x2, area->y2,
                   reinterpret_cast<uint16_t*>(pixels));
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

void beginDisplay() {
  pinMode(kDisplayEnablePin, OUTPUT);
  digitalWrite(kDisplayEnablePin, HIGH);
  delay(50);

  if (!display.begin()) fatalBlink("Display initialization failed");
  Serial.printf("Display: %s (0x%02X)\n", display.name(), display.ID());

  lv_init();
  lv_tick_set_cb(lvMillis);

  drawBuffer1 = static_cast<lv_color_t*>(
      heap_caps_malloc(kDrawBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  drawBuffer2 = static_cast<lv_color_t*>(
      heap_caps_malloc(kDrawBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!drawBuffer1 || !drawBuffer2) fatalBlink("LVGL buffer allocation failed");

  lvDisplay = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_display_set_flush_cb(lvDisplay, displayFlush);
  lv_display_set_buffers(lvDisplay, drawBuffer1, drawBuffer2, kDrawBufferBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_add_event_cb(lvDisplay, evenDisplayArea,
                          LV_EVENT_INVALIDATE_AREA, nullptr);
}

void handleManualButton(uint32_t nowMs) {
  const bool buttonDown = digitalRead(BreatheBlockConfig::kManualButtonPin) == LOW;
  if (buttonDown && !previousButtonDown) {
    if (ui.sessionActive()) {
      ui.stopSession();
    } else {
      ui.startSession(nowMs);
      stressEngine.noteSessionStarted(nowMs);
    }
  }
  previousButtonDown = buttonDown;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(600);
  Serial.println("\nBreathe Block starting");

  pinMode(BreatheBlockConfig::kManualButtonPin, INPUT_PULLUP);
  beginDisplay();
  ui.begin();

  radar.begin(BreatheBlockConfig::kRadarRxPin,
              BreatheBlockConfig::kRadarTxPin,
              BreatheBlockConfig::kRadarPrimaryBaud,
              BreatheBlockConfig::kRadarFallbackBaud);

  Serial.printf("Radar mode: %s\n",
                BreatheBlockConfig::kUseSimulatedRadar ? "simulated" : "live");
}

void loop() {
  const uint32_t nowMs = millis();
  const VitalSigns signs = radar.update(nowMs);
  const BodyAssessment assessment = stressEngine.update(signs, nowMs);

  if (assessment.promptNow && !ui.sessionActive()) ui.startSession(nowMs);

  if (BreatheBlockConfig::kAutoStartVisualDemo && !demoStarted && nowMs > 2200) {
    demoStarted = true;
    ui.startSession(nowMs);
  }

  handleManualButton(nowMs);
  ui.setAmbientState(assessment.state);
  ui.setLiveBreathPhase(signs.breathPhase, signs.breathPhaseFresh, nowMs);
  ui.setTestingVitals(signs.heartRate, signs.breathRate);
  ui.update(nowMs);
  ui.consumeSessionFinished();

  if (static_cast<uint32_t>(nowMs - lastDebugAtMs) >= 2000) {
    lastDebugAtMs = nowMs;
    Serial.printf(
        "HR %.1f | breath %.1f | phase %.3f | distance %.1f | baseline %.1f/%.1f | "
        "load %.2f | baud %lu\n",
        signs.heartRate, signs.breathRate, signs.breathPhase, signs.distanceCm,
        assessment.baselineHeartRate, assessment.baselineBreathRate,
        assessment.activationScore,
        static_cast<unsigned long>(radar.activeBaud()));
  }

  lv_timer_handler();
  delay(5);
}
