#import "BreathEngine.h"

// The firmware's own source, referenced (not copied) from the repo root —
// see the note in BreathEngine.h. If Xcode can't find these two headers,
// the file references weren't added the way ios/SETUP.md describes.
#include "../../../../../BreatheBlock/BreathField.h"
#include "../../../../../BreatheBlock/BreathScene.h"

#include <cstring>
#include <vector>

namespace {

// Matches the device's own 466 x 466 round display; see board_config.h.
// The engine always renders at this resolution and lets the view scale it,
// so the field math (radii, well depths, all in pixels) matches the
// firmware and the browser preview exactly rather than being re-tuned per
// screen size.
constexpr int kFieldSize = 466;

const BreathPalette& PaletteFor(BreathPaletteChoice choice) {
  switch (choice) {
    case BreathPaletteMoonlight:
      return BreathPalettes::kMoonlight;
    case BreathPaletteEmber:
      return BreathPalettes::kEmber;
    case BreathPaletteIvory:
    default:
      return BreathPalettes::kIvory;
  }
}

NSString* NameForState(SceneState state) {
  switch (state) {
    case SceneState::Awakening: return @"awakening";
    case SceneState::Resting: return @"resting";
    case SceneState::Sleeping: return @"sleeping";
    case SceneState::CheckingIn: return @"checking-in";
    case SceneState::Countdown: return @"countdown";
    case SceneState::Noticing: return @"noticing";
    case SceneState::Inviting: return @"inviting";
    case SceneState::Guiding: return @"guiding";
    case SceneState::Releasing: return @"releasing";
  }
  return @"?";
}

}  // namespace

@implementation BreathEngine {
  BreathScene _scene;
  BreathLut _lut;
  std::vector<uint16_t> _rgb565;  // kFieldSize x kFieldSize scratch buffer
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _rgb565.resize(static_cast<size_t>(kFieldSize) * kFieldSize, 0);
  }
  return self;
}

- (void)updateWithNowMs:(uint32_t)nowMs presence:(BOOL)presence {
  SceneInput input;
  input.nowMs = nowMs;
  input.presence = presence;
  // No sensor feeding this yet — see DESIGN.md's "no numbers unasked"
  // principle and the phase-1 scope note in ios/SETUP.md. livePhaseValid
  // stays false, so the ember's idle breath is the same gentle sine the
  // device itself falls back to with no radar signal.
  input.livePhaseValid = false;
  input.liveBreath = 0.0f;
  input.activationScore = 0.0f;
  input.displayHeartRate = 0.0f;
  input.displayBreathRate = 0.0f;
  _scene.update(input);
}

- (void)startSessionWithNowMs:(uint32_t)nowMs {
  // announceShift is always false here: with no sensor behind this build,
  // the app must never claim to have noticed a shift it didn't detect.
  _scene.requestSession(nowMs, false);
}

- (void)dismissWithNowMs:(uint32_t)nowMs {
  _scene.dismiss(nowMs);
}

- (void)tapWithNowMs:(uint32_t)nowMs {
  _scene.handleTap(nowMs);
}

- (BOOL)sessionActive {
  return _scene.sessionActive() ? YES : NO;
}

- (BOOL)consumeSessionFinished {
  return _scene.consumeSessionFinished() ? YES : NO;
}

- (NSString*)stateName {
  return NameForState(_scene.state());
}

- (float)progress {
  return _scene.output().progress;
}

- (void)renderRGBAInto:(uint8_t*)buffer
                  width:(NSInteger)width
                 height:(NSInteger)height
                palette:(BreathPaletteChoice)palette {
  if (width != kFieldSize || height != kFieldSize) {
    // The caller is expected to render at the field's native resolution and
    // let the view scale the result — see the kFieldSize note above. Fail
    // loudly in debug rather than silently drawing the wrong thing.
    NSCAssert(NO, @"BreathEngine renders at %d x %d only", kFieldSize, kFieldSize);
    std::memset(buffer, 0, static_cast<size_t>(width) * height * 4);
    return;
  }

  _lut.build(_scene.output().field, PaletteFor(palette));
  std::fill(_rgb565.begin(), _rgb565.end(), static_cast<uint16_t>(0));
  _lut.blendArea(0, 0, kFieldSize - 1, kFieldSize - 1, _rgb565.data());

  // RGB565 -> RGBA8888, expanding each channel back toward 8 bits the same
  // way the browser preview's own byte-swap-aware unpacking does.
  for (int i = 0; i < kFieldSize * kFieldSize; ++i) {
    const uint16_t packed = _rgb565[static_cast<size_t>(i)];
    const uint8_t r5 = (packed >> 11) & 0x1F;
    const uint8_t g6 = (packed >> 5) & 0x3F;
    const uint8_t b5 = packed & 0x1F;
    uint8_t* out = buffer + static_cast<size_t>(i) * 4;
    out[0] = static_cast<uint8_t>((r5 * 255 + 15) / 31);
    out[1] = static_cast<uint8_t>((g6 * 255 + 31) / 63);
    out[2] = static_cast<uint8_t>((b5 * 255 + 15) / 31);
    out[3] = 255;
  }
}

@end
