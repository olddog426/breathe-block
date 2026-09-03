#include "BreathField.h"

#include <math.h>

namespace {

constexpr float kIntensityCeiling = 1.0f;

float clamp01(float value) {
  if (!(value > 0.0f)) return 0.0f;  // also catches NaN
  if (value > 1.0f) return 1.0f;
  return value;
}

float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

// A bell with compact support: 1 at the centre, exactly 0 at |d| >= width,
// smooth at both ends. Cheaper than a Gaussian and, more usefully, it gives
// the field a hard outer bound so dirty rectangles can be exact.
float bell(float d, float width) {
  if (width <= 0.0f) return 0.0f;
  const float t = 1.0f - fabsf(d) / width;
  if (t <= 0.0f) return 0.0f;
  return smoothstep(t);
}

int isqrtFloor(int value) {
  if (value <= 0) return 0;
  int root = static_cast<int>(sqrtf(static_cast<float>(value)));
  while (root > 0 && root * root > value) --root;
  while ((root + 1) * (root + 1) <= value) ++root;
  return root;
}

// 4x4 ordered dither. RGB565 gives 5-6 bits per channel, which bands badly
// across a slow gradient; four extra bits of ordered noise removes it.
const uint8_t kBayer[16] = {0,  8,  2,  10, 12, 4, 14, 6,
                            3,  11, 1,  9,  15, 7, 13, 5};

uint16_t pack565(int r5, int g6, int b5) {
  return static_cast<uint16_t>((r5 << 11) | (g6 << 5) | b5);
}

uint16_t swapBytes(uint16_t value) {
  return static_cast<uint16_t>((value << 8) | (value >> 8));
}

// Saturating per-channel add, on natively ordered RGB565.
uint16_t addSaturating(uint16_t a, uint16_t b) {
  uint32_t r = (a & 0xF800u) + (b & 0xF800u);
  uint32_t g = (a & 0x07E0u) + (b & 0x07E0u);
  uint32_t bl = (a & 0x001Fu) + (b & 0x001Fu);
  if (r > 0xF800u) r = 0xF800u;
  if (g > 0x07E0u) g = 0x07E0u;
  if (bl > 0x001Fu) bl = 0x001Fu;
  return static_cast<uint16_t>(r | g | bl);
}

}  // namespace

float BreathField::intensityAt(float r) const {
  float value = coreLevel * bell(r, coreRadius);

  const float ringDelta = r - ringRadius;
  value += ringLevel * bell(ringDelta, ringDelta < 0.0f ? ringInnerWidth
                                                        : ringOuterWidth);

  if (veilLevel > 0.0f && r < ringRadius) {
    value += veilLevel * smoothstep(clamp01((ringRadius - r) / veilFeather));
  }

  value += horizonLevel * bell(r - horizonRadius, horizonWidth);

  if (wellDepth > 0.0f) {
    value *= 1.0f - wellDepth * bell(r, wellRadius);
  }
  return value > kIntensityCeiling ? kIntensityCeiling : value;
}

float BreathField::extent() const {
  float extent = 0.0f;
  // A level below one RGB565 step is not a visible layer, and treating it as
  // one would keep the dirty rectangle at full screen forever.
  constexpr float kVisible = 0.0009f;
  if (coreLevel > kVisible) extent = coreRadius;
  if (ringLevel > kVisible || veilLevel > kVisible) {
    const float reach = ringRadius + ringOuterWidth;
    if (reach > extent) extent = reach;
  }
  if (horizonLevel > kVisible) {
    const float reach = horizonRadius + horizonWidth;
    if (reach > extent) extent = reach;
  }
  return extent;
}

bool BreathField::matches(const BreathField& other) const {
  const float geometry[] = {centerX,      centerY,      coreRadius,
                            ringRadius,   ringOuterWidth, ringInnerWidth,
                            veilFeather,  horizonRadius, horizonWidth,
                            wellRadius};
  const float otherGeometry[] = {other.centerX,        other.centerY,
                                 other.coreRadius,     other.ringRadius,
                                 other.ringOuterWidth, other.ringInnerWidth,
                                 other.veilFeather,    other.horizonRadius,
                                 other.horizonWidth,   other.wellRadius};
  for (unsigned i = 0; i < sizeof(geometry) / sizeof(geometry[0]); ++i) {
    if (fabsf(geometry[i] - otherGeometry[i]) > 0.35f) return false;
  }

  const float levels[] = {coreLevel, ringLevel, veilLevel, horizonLevel,
                          wellDepth, warmth};
  const float otherLevels[] = {other.coreLevel,    other.ringLevel,
                               other.veilLevel,    other.horizonLevel,
                               other.wellDepth,    other.warmth};
  for (unsigned i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
    if (fabsf(levels[i] - otherLevels[i]) > 0.0015f) return false;
  }
  return true;
}

void BreathLut::build(const BreathField& field, const BreathPalette& palette) {
  centerX_ = static_cast<int>(lroundf(field.centerX));
  centerY_ = static_cast<int>(lroundf(field.centerY));

  int extent = static_cast<int>(ceilf(field.extent())) + 1;
  if (extent < 0) extent = 0;
  if (extent > kMaxRadius) extent = kMaxRadius;

  // Faint light picks up the halo colour, strong light the core colour. The
  // hue therefore moves with brightness, which is what real scattered light
  // does and what keeps the glow from looking like a tinted sticker.
  const float warm = field.warmth;
  const float coreR = palette.coreR + 8.0f * warm;
  const float coreG = palette.coreG;
  const float coreB = palette.coreB - 10.0f * warm;
  const float haloR = palette.haloR + 12.0f * warm;
  const float haloG = palette.haloG;
  const float haloB = palette.haloB - 12.0f * warm;

  int lastLit = -1;
  for (int r = 0; r <= extent; ++r) {
    const float intensity = field.intensityAt(static_cast<float>(r));
    if (intensity <= 0.0007f) {
      uint16_t* row = &table_[r << 4];
      for (int p = 0; p < 16; ++p) row[p] = 0;
      continue;
    }
    lastLit = r;

    const float mix = clamp01(intensity * 1.15f);
    const float red = (haloR + (coreR - haloR) * mix) * intensity;
    const float green = (haloG + (coreG - haloG) * mix) * intensity;
    const float blue = (haloB + (coreB - haloB) * mix) * intensity;

    // Four fractional bits below the 5/6 bit channel, so the dither has
    // somewhere to work.
    int redFixed = static_cast<int>(red * 2.0078f);
    int greenFixed = static_cast<int>(green * 4.0157f);
    int blueFixed = static_cast<int>(blue * 2.0078f);
    if (redFixed < 0) redFixed = 0;
    if (greenFixed < 0) greenFixed = 0;
    if (blueFixed < 0) blueFixed = 0;

    uint16_t* row = &table_[r << 4];
    for (int p = 0; p < 16; ++p) {
      const int bias = kBayer[p];
      int r5 = (redFixed + bias) >> 4;
      int g6 = (greenFixed + bias) >> 4;
      int b5 = (blueFixed + bias) >> 4;
      if (r5 > 31) r5 = 31;
      if (g6 > 63) g6 = 63;
      if (b5 > 31) b5 = 31;
      const uint16_t packed = pack565(r5, g6, b5);
      row[p] = swapped_ ? swapBytes(packed) : packed;
    }
  }

  maxRadius_ = lastLit;
}

void BreathLut::boundingBox(int w, int h, int* x1, int* y1, int* x2,
                            int* y2) const {
  if (maxRadius_ < 0) {
    *x1 = *y1 = 0;
    *x2 = *y2 = -1;
    return;
  }
  int left = centerX_ - maxRadius_;
  int top = centerY_ - maxRadius_;
  int right = centerX_ + maxRadius_;
  int bottom = centerY_ + maxRadius_;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > w - 1) right = w - 1;
  if (bottom > h - 1) bottom = h - 1;
  *x1 = left;
  *y1 = top;
  *x2 = right;
  *y2 = bottom;
}

void BreathLut::blendRow(int y, int x1, int x2, uint16_t* row) const {
  const int dy = y - centerY_;
  const int dy2 = dy * dy;
  const int maxR2 = maxRadius_ * maxRadius_;
  if (dy2 > maxR2) return;

  const int halfSpan = isqrtFloor(maxR2 - dy2);
  int startX = centerX_ - halfSpan;
  int endX = centerX_ + halfSpan;
  if (startX < x1) startX = x1;
  if (endX > x2) endX = x2;
  if (startX > endX) return;

  int dx = startX - centerX_;
  int r2 = dy2 + dx * dx;
  int r = isqrtFloor(r2);
  int rSquared = r * r;
  int nextSquared = rSquared + 2 * r + 1;

  const int phaseBase = (y & 3) << 2;
  uint16_t* pixel = row + (startX - x1);

  for (int x = startX; x <= endX; ++x) {
    // r tracks floor(sqrt(r2)) by addition only; r2 itself is incremental.
    while (r > 0 && r2 < rSquared) {
      nextSquared = rSquared;
      rSquared -= 2 * r - 1;
      --r;
    }
    while (r2 >= nextSquared) {
      rSquared = nextSquared;
      ++r;
      nextSquared = rSquared + 2 * r + 1;
    }

    const uint16_t glow = table_[(r << 4) | (phaseBase | (x & 3))];
    if (glow) {
      const uint16_t under = *pixel;
      if (under == 0) {
        *pixel = glow;
      } else if (swapped_) {
        *pixel = swapBytes(addSaturating(swapBytes(under), swapBytes(glow)));
      } else {
        *pixel = addSaturating(under, glow);
      }
    }

    r2 += 2 * (x - centerX_) + 1;
    ++pixel;
  }
}

void BreathLut::blendArea(int x1, int y1, int x2, int y2,
                          uint16_t* pixels) const {
  if (maxRadius_ <= 0 || x2 < x1 || y2 < y1) return;
  const int stride = x2 - x1 + 1;
  for (int y = y1; y <= y2; ++y) {
    blendRow(y, x1, x2, pixels + static_cast<long>(y - y1) * stride);
  }
}
