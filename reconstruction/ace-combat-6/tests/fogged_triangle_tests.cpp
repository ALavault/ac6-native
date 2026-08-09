// The fogged textured triangle, checked where the answer is arithmetic.
#include "ac6/demo_flight_view.h"

#include <cstdio>
#include <vector>

namespace {
int failures = 0;
void check(bool c, const char* w) {
  if (!c) { std::printf("FAIL  %s\n", w); ++failures; }
}
ac6::demo::Image canvas(int w, int h) {
  ac6::demo::Image image;
  image.width = w; image.height = h;
  image.rgb.assign(static_cast<std::size_t>(w) * h * 3, 7);
  image.clear_depth();
  return image;
}
}  // namespace

int main() {
  // A 4x4 mid-grey opaque texture.
  std::vector<std::uint32_t> tex(16, 0xFF000000u | (128u << 16) | (128u << 8) | 128u);

  // AT FOG == 0 THE FOGGED PATH IS THE PLAIN PATH, byte for byte. That is the
  // claim in the header, and it is what makes switching callers safe.
  auto plain = canvas(32, 32);
  plain.triangle_textured(2, 2, 10.0F, 0, 0, 29, 2, 10.0F, 1, 0, 15, 29, 10.0F, 0.5F, 1,
                          tex.data(), 4, 4, 0.8F);
  auto fogged = canvas(32, 32);
  fogged.triangle_textured_fogged(2, 2, 10.0F, 0, 0, 29, 2, 10.0F, 1, 0, 15, 29, 10.0F,
                                  0.5F, 1, tex.data(), 4, 4, 0.8F, 0.0F, 200, 10, 10);
  check(plain.rgb == fogged.rgb, "fog 0 is the plain path exactly");

  // AT FOG == 1 every covered pixel is the fog colour, regardless of texel.
  auto full = canvas(32, 32);
  full.triangle_textured_fogged(2, 2, 10.0F, 0, 0, 29, 2, 10.0F, 1, 0, 15, 29, 10.0F,
                                0.5F, 1, tex.data(), 4, 4, 0.8F, 1.0F, 126, 146, 169);
  const std::size_t inside = (10 * 32 + 12) * 3;    // a pixel inside the triangle
  check(full.rgb[inside] == 126 && full.rgb[inside + 1] == 146 &&
            full.rgb[inside + 2] == 169,
        "fog 1 is the fog colour exactly");
  check(full.rgb[0] == 7, "and pixels outside the triangle are untouched");

  // MONOTONE: at fog 0.5 each channel sits between the lit texel and the fog.
  auto half = canvas(32, 32);
  half.triangle_textured_fogged(2, 2, 10.0F, 0, 0, 29, 2, 10.0F, 1, 0, 15, 29, 10.0F,
                                0.5F, 1, tex.data(), 4, 4, 0.8F, 0.5F, 200, 10, 10);
  const int lit = int(128 * 0.8F);
  check(half.rgb[inside] > std::min(lit, 200) - 1 &&
            half.rgb[inside] < std::max(lit, 200) + 1,
        "fog 0.5 lies between the lit texel and the fog colour");

  // THE ALPHA TEST SURVIVES: a transparent texel is skipped, not fogged.
  std::vector<std::uint32_t> clear(16, 0x00FFFFFFu);
  auto skipped = canvas(32, 32);
  skipped.triangle_textured_fogged(2, 2, 10.0F, 0, 0, 29, 2, 10.0F, 1, 0, 15, 29, 10.0F,
                                   0.5F, 1, clear.data(), 4, 4, 0.8F, 0.7F, 126, 146, 169);
  check(skipped.rgb[inside] == 7,
        "a texel below the alpha cutoff is skipped even under fog");

  if (failures == 0) std::printf("fogged triangle OK\n");
  return failures == 0 ? 0 : 1;
}
