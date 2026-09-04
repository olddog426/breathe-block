// Objective-C++ wrapper around the firmware's own BreathScene/BreathField —
// the exact same C++ source the device and the browser preview use, not a
// second hand-drawn port. If this ever disagrees with the device, the
// device is right; but there should be nothing here TO disagree, since it's
// the same two .cpp files, referenced (not copied) from ../../../../BreatheBlock.
//
// Swift never sees BreathScene/BreathField directly — only this Obj-C
// surface, which is deliberately small: start/dismiss a session, and render
// the current frame into a pixel buffer once per display tick.
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, BreathPaletteChoice) {
  BreathPaletteIvory = 0,
  BreathPaletteMoonlight = 1,
  BreathPaletteEmber = 2,
};

@interface BreathEngine : NSObject

// Every method below has an explicit NS_SWIFT_NAME: Objective-C's automatic
// Swift-import renaming is a real "won't know until Xcode parses it" risk
// (e.g. whether a NowMs: label gets collapsed), and nothing here can be
// compile-checked outside Xcode — see ios/SETUP.md — so the Swift signature
// is pinned explicitly rather than left to inference.

// nowMs should be a monotonic millisecond clock — CACurrentMediaTime() * 1000
// cast to uint32_t works, same convention the firmware's millis() follows.
// Call this once per display frame; the scene is lazily started on the
// first call, exactly as it is on the device.
- (void)updateWithNowMs:(uint32_t)nowMs
                presence:(BOOL)presence NS_SWIFT_NAME(update(nowMs:presence:));

// A session you started yourself: the app never claims to have noticed
// anything, since there's no sensor behind it yet (see DESIGN.md).
- (void)startSessionWithNowMs:(uint32_t)nowMs NS_SWIFT_NAME(startSession(nowMs:));
- (void)dismissWithNowMs:(uint32_t)nowMs NS_SWIFT_NAME(dismiss(nowMs:));

// Answers a held invitation and begins guiding — the same gesture as a tap
// on the device's glass while it's showing "breathe with me" (see
// DESIGN.md §4: "INVITING doesn't start itself... a tap begins it"). Only
// call this while stateName is "inviting"; from "resting" the identical
// underlying gesture instead starts the device's heart-rate/breath-rate
// check-in (DESIGN.md §4a), which this build has no live numbers to show,
// so callers must gate on state rather than call this unconditionally.
- (void)tapWithNowMs:(uint32_t)nowMs NS_SWIFT_NAME(tap(nowMs:));

// Adjusts pacing only (inhale/exhale length, round count) — everything
// else in the scene's config (geometry, timings for every other state)
// stays whatever it already was. Takes effect on the next state rebuild,
// so it's safe to call any time, including mid-session.
- (void)setBreathingStyleWithInhaleMs:(uint32_t)inhaleMs
                              exhaleMs:(uint32_t)exhaleMs
                                cycles:(uint8_t)cycles
    NS_SWIFT_NAME(setBreathingStyle(inhaleMs:exhaleMs:cycles:));

@property(nonatomic, readonly) BOOL sessionActive;
// True for exactly one updateWithNowMs: call, the frame a guided session
// finishes on its own (not dismissed) — the natural moment to log it.
- (BOOL)consumeSessionFinished NS_SWIFT_NAME(consumeSessionFinished());
@property(nonatomic, readonly, copy) NSString *stateName;
// 0..1 while guiding, for a progress readout if the app ever wants one.
@property(nonatomic, readonly) float progress;

// Renders the current frame's light field into an RGBA8888 buffer,
// width * height * 4 bytes, row-major, top-left origin, fully opaque.
// Matches the same field math the device and browser preview render —
// see BreathField.h. The buffer is cleared to black first; this is a full
// frame, not something to composite onto existing content.
- (void)renderRGBAInto:(uint8_t *)buffer
                  width:(NSInteger)width
                 height:(NSInteger)height
                palette:(BreathPaletteChoice)palette
    NS_SWIFT_NAME(renderRGBA(into:width:height:palette:));

@end

NS_ASSUME_NONNULL_END
