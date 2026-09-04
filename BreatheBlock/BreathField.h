// The light field: everything the screen shows apart from text.
//
// Deliberately free of LVGL and Arduino so the renderer can be unit tested and
// rendered to images on a laptop (see tools/preview.cpp).
#pragma once

#include <stdint.h>

// A palette is two colours: what bright light looks like and what faint light
// looks like. Intensity interpolates between them, which is what makes the
// glow read as scattered light rather than as a flat tinted shape.
struct BreathPalette {
  uint8_t coreR, coreG, coreB;  // high intensity
  uint8_t haloR, haloG, haloB;  // low intensity
};

namespace BreathPalettes {
// Warm ivory centre falling to a pale sage-teal halo. The default.
constexpr BreathPalette kIvory{255, 246, 232, 122, 172, 160};
// Cooler and more nocturnal.
constexpr BreathPalette kMoonlight{236, 242, 255, 108, 140, 186};
// Warmer, closer to candlelight.
constexpr BreathPalette kEmber{255, 234, 206, 178, 126, 96};
}  // namespace BreathPalettes

// One frame of the field, in pixels and 0..1 levels. Every member is
// continuous in time; nothing in the system is allowed to step.
struct BreathField {
  float centerX = 233.0f;
  float centerY = 233.0f;

  // Ember: a solid bloom at the centre.
  float coreRadius = 30.0f;
  float coreLevel = 0.0f;

  // Contour: the ring whose radius is the breath.
  float ringRadius = 120.0f;
  float ringOuterWidth = 26.0f;  // falloff outside the ring
  float ringInnerWidth = 17.0f;  // falloff inside it (crisper)
  float ringLevel = 0.0f;

  // Veil: the faint fill inside the contour.
  float veilFeather = 74.0f;
  float veilLevel = 0.0f;

  // Horizon: the rim ring used only while noticing.
  float horizonRadius = 210.0f;
  float horizonWidth = 22.0f;
  float horizonLevel = 0.0f;

  // Text well: attenuates the field inside this radius so a word stays
  // legible. Depth 1.0 removes the light entirely.
  float wellRadius = 84.0f;
  float wellDepth = 0.0f;

  // -1 cools the palette, +1 warms it. Used to shade inhale against exhale.
  float warmth = 0.0f;

  // -1..1: a temperature, not a fraction. -1 is the ember's own resting
  // colour, a deliberately cool and calming one; 0 is the palette's plain
  // colour, untouched (the default everywhere outside Resting); +1 is the
  // detector's activation score at its trigger threshold. Unlike warmth's
  // subliminal breathing shade, this is meant to be seen — the ember
  // visibly cooling toward calm at your seated baseline and climbing
  // through orange toward red as activation approaches the point a session
  // would begin. Zero (not -1) outside Resting/Sleeping — Noticing carries
  // a fading positive snapshot outward instead, never the calm colour.
  float heat = 0.0f;

  // Intensity of the field at a distance r from the centre, before colour.
  float intensityAt(float r) const;

  // Radius beyond which intensityAt() is exactly zero. Profiles have compact
  // support, so this is a hard bound, not an approximation.
  float extent() const;

  // True when the two fields would produce a visually identical frame.
  bool matches(const BreathField& other) const;
};

// A frame's worth of precomputed colour, indexed by integer radius and by
// ordered-dither phase. ~11 KB; keep one instance and rebuild it per frame.
class BreathLut {
 public:
  static constexpr int kMaxRadius = 340;  // centre to corner of 466x466 is 330

  // Some LVGL builds hand back RGB565 with the bytes swapped. Detect it once
  // from LVGL itself and tell the LUT; everything downstream stays correct.
  void setSwappedBytes(bool swapped) { swapped_ = swapped; }

  void build(const BreathField& field, const BreathPalette& palette);

  int centerX() const { return centerX_; }
  int centerY() const { return centerY_; }
  int maxRadius() const { return maxRadius_; }
  bool empty() const { return maxRadius_ <= 0; }

  // The packed colour this frame gives an integer radius under one of the 16
  // ordered-dither phases. Exposed so the scanline walker can be checked
  // against an honest square root.
  uint16_t colorAt(int radius, int phase) const {
    if (radius < 0 || radius > kMaxRadius) return 0;
    return table_[(radius << 4) | (phase & 15)];
  }

  // Bounding box of the lit area, clipped to a w x h display.
  void boundingBox(int w, int h, int* x1, int* y1, int* x2, int* y2) const;

  // Add the field into an already rendered RGB565 area. `pixels` is the top
  // left of the inclusive rectangle (x1,y1)-(x2,y2) in screen coordinates,
  // with a row stride of (x2 - x1 + 1) pixels.
  void blendArea(int x1, int y1, int x2, int y2, uint16_t* pixels) const;

 private:
  void blendRow(int y, int x1, int x2, uint16_t* row) const;

  uint16_t table_[(kMaxRadius + 1) * 16] = {0};
  int centerX_ = 233;
  int centerY_ = 233;
  int maxRadius_ = 0;
  bool swapped_ = false;
};
