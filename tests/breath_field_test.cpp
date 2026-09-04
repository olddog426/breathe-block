// The renderer's invariants: correct radii, nothing written outside the lit
// disc, and additive light that saturates instead of wrapping.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "BreathField.h"

namespace {

constexpr int kSize = 466;

BreathField guidingField() {
  BreathField field;
  field.centerX = 233.0f;
  field.centerY = 233.0f;
  field.coreLevel = 0.0f;
  field.ringRadius = 150.0f;
  field.ringLevel = 0.5f;
  field.veilLevel = 0.08f;
  return field;
}

}  // namespace

int main() {
  // The scanline walker maintains floor(sqrt(dx^2+dy^2)) by integer addition.
  // Compare it against the honest calculation over the whole display.
  {
    BreathField field;
    field.coreRadius = 400.0f;  // light everywhere, so nothing is skipped
    field.coreLevel = 1.0f;
    BreathLut lut;
    lut.build(field, BreathPalettes::kIvory);

    std::vector<uint16_t> row(kSize);
    for (int y = 0; y < kSize; ++y) {
      memset(row.data(), 0, row.size() * sizeof(uint16_t));
      lut.blendArea(0, y, kSize - 1, y, row.data());
      for (int x = 0; x < kSize; ++x) {
        const int dx = x - 233, dy = y - 233;
        int radius = 0;
        while ((radius + 1) * (radius + 1) <= dx * dx + dy * dy) ++radius;
        const int phase = ((y & 3) << 2) | (x & 3);
        assert(row[x] == lut.colorAt(radius, phase) &&
               "radius walker disagrees with sqrt");
      }
    }
    printf("radius walker matches sqrt across the display\n");
  }

  // Nothing outside the lit disc may be touched: that is what lets the dirty
  // rectangle be exact.
  {
    BreathLut lut;
    lut.build(guidingField(), BreathPalettes::kIvory);
    std::vector<uint16_t> frame(kSize * kSize, 0xDEAD);
    lut.blendArea(0, 0, kSize - 1, kSize - 1, frame.data());

    const int maxR = lut.maxRadius();
    int outside = 0;
    for (int y = 0; y < kSize; ++y) {
      for (int x = 0; x < kSize; ++x) {
        const int dx = x - 233, dy = y - 233;
        if (dx * dx + dy * dy > (maxR + 1) * (maxR + 1) &&
            frame[y * kSize + x] != 0xDEAD) {
          ++outside;
        }
      }
    }
    assert(outside == 0 && "wrote outside the lit disc");
    printf("lit disc bound is exact (maxRadius %d)\n", maxR);
  }

  // The bounding box has to contain every pixel the blend actually writes.
  {
    BreathLut lut;
    lut.build(guidingField(), BreathPalettes::kIvory);
    int x1, y1, x2, y2;
    lut.boundingBox(kSize, kSize, &x1, &y1, &x2, &y2);

    std::vector<uint16_t> frame(kSize * kSize, 0);
    lut.blendArea(0, 0, kSize - 1, kSize - 1, frame.data());
    for (int y = 0; y < kSize; ++y) {
      for (int x = 0; x < kSize; ++x) {
        if (frame[y * kSize + x] == 0) continue;
        assert(x >= x1 && x <= x2 && y >= y1 && y <= y2 &&
               "lit pixel outside the reported bounding box");
      }
    }
    printf("bounding box contains every lit pixel\n");
  }

  // Light adds to whatever LVGL already drew, and clips instead of wrapping.
  {
    BreathField bright = guidingField();
    bright.ringLevel = 1.0f;
    BreathLut lut;
    lut.build(bright, BreathPalettes::kIvory);
    std::vector<uint16_t> row(kSize, 0xFFFF);  // white text everywhere
    lut.blendArea(0, 233, kSize - 1, 233, row.data());
    for (int x = 0; x < kSize; ++x) {
      assert(row[x] == 0xFFFF && "adding light to white must stay white");
    }
    printf("additive blend saturates\n");
  }

  // Byte-swapped output is the same picture, byte for byte reversed.
  {
    BreathLut plain, swapped;
    plain.build(guidingField(), BreathPalettes::kIvory);
    swapped.setSwappedBytes(true);
    swapped.build(guidingField(), BreathPalettes::kIvory);

    std::vector<uint16_t> a(kSize, 0), b(kSize, 0);
    plain.blendArea(0, 233, kSize - 1, 233, a.data());
    swapped.blendArea(0, 233, kSize - 1, 233, b.data());
    for (int x = 0; x < kSize; ++x) {
      const uint16_t expected =
          static_cast<uint16_t>((a[x] << 8) | (a[x] >> 8));
      assert(b[x] == expected && "byte-swapped output diverged");
    }
    printf("byte-swapped output matches\n");
  }

  // An empty field lights nothing at all, so a sleeping device can skip work.
  {
    BreathField dark;
    dark.coreLevel = 0.0f;
    BreathLut lut;
    lut.build(dark, BreathPalettes::kIvory);
    assert(lut.empty());
  }

  // Heat is meant to be seen: at full heat the ember's own colour should
  // read as red (redder than green or blue), clearly past a simple warm
  // tint, regardless of which palette is active.
  {
    BreathField ember;
    ember.coreRadius = 40.0f;
    ember.coreLevel = 0.3f;
    ember.heat = 1.0f;
    BreathLut lut;
    lut.build(ember, BreathPalettes::kIvory);
    const uint16_t color = lut.colorAt(2, 0);
    const int r5 = (color >> 11) & 0x1F, g6 = (color >> 5) & 0x3F,
              b5 = color & 0x1F;
    // Compare on a common 0..31 scale so the 5/6-bit channel split doesn't
    // bias the comparison.
    const int g5 = g6 / 2;
    assert(r5 > g5 + 2 && r5 > b5 + 2 &&
           "full heat should read as red, not just a warm tint");
  }

  printf("breath field: ok\n");
  return 0;
}
