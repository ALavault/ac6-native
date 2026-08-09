// The map's post-process, checked at points where the answer is arithmetic.
#include "ac6/demo_flight_view.h"

#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(bool c, const char* w) {
  if (!c) { std::printf("FAIL  %s\n", w); ++failures; }
}
ac6::demo::Image flat(int w, int h, std::uint8_t v) {
  ac6::demo::Image image;
  image.width = w; image.height = h;
  image.rgb.assign(static_cast<std::size_t>(w) * h * 3, v);
  return image;
}
}  // namespace

int main() {
  using ac6::demo::MapsetPost;

  // LEVELS ALONE, at the centre pixel so the vignette does not touch it.
  {
    MapsetPost post;
    post.bloom = false;
    post.vignette = false;
    auto image = flat(9, 9, 128);
    ac6::demo::apply_mapset_post(image, post);
    const std::size_t mid = (4 * 9 + 4) * 3;
    for (int c = 0; c < 3; ++c) {
      const float t = std::pow((128.0F - post.in_min[c]) /
                                   (post.in_max[c] - post.in_min[c]),
                               1.0F / post.in_gamma[c]);
      const float want = post.out_min[c] + (post.out_max[c] - post.out_min[c]) * t;
      check(std::abs(int(image.rgb[mid + c]) - int(want)) <= 1,
            "the level curve is the file's numbers, channel by channel");
    }
    // The channels differ, because the gammas do: 0.9 / 1.0 / 1.1.
    check(image.rgb[mid] != image.rgb[mid + 2],
          "a grey input does NOT stay grey -- the per-channel gamma is real");
  }

  // Input at or below In.Min clamps to Out.Min; at or above In.Max to Out.Max.
  {
    MapsetPost post;
    post.bloom = false;
    post.vignette = false;
    auto dark = flat(3, 3, 0);
    ac6::demo::apply_mapset_post(dark, post);
    check(dark.rgb[(1 * 3 + 1) * 3] == 5, "black floors at Out.Min = 5");
    auto light = flat(3, 3, 255);
    ac6::demo::apply_mapset_post(light, post);
    check(light.rgb[(1 * 3 + 1) * 3] == 235, "white ceils at Out.Max = 235");
  }

  // THE VIGNETTE darkens the corner and leaves the centre alone.
  {
    MapsetPost post;
    post.bloom = false;
    post.level_correction = false;
    auto image = flat(41, 41, 200);
    ac6::demo::apply_mapset_post(image, post);
    const std::size_t centre = (20 * 41 + 20) * 3;
    check(image.rgb[centre] == 200, "the centre is untouched");
    check(image.rgb[0] < 200, "and the corner is darker");
    // Inside fRadiusRatio nothing changes: that is what the number means.
    const int inside = static_cast<int>(20 * 0.6F);
    check(image.rgb[((20) * 41 + 20 + inside) * 3] == 200,
          "and everything inside fRadiusRatio is untouched");
  }

  // BLOOM only lifts pixels above the bright-pass threshold.
  {
    MapsetPost post;
    post.level_correction = false;
    post.vignette = false;
    auto dim = flat(21, 21, 40);            // luma 40 < 0.4 * 255
    const auto before = dim.rgb;
    ac6::demo::apply_mapset_post(dim, post);
    check(dim.rgb == before, "a frame below the threshold is unchanged");
    auto bright = flat(21, 21, 250);
    ac6::demo::apply_mapset_post(bright, post);
    check(bright.rgb[(10 * 21 + 10) * 3] >= 250, "a bright frame is lifted");
  }

  if (failures == 0) std::printf("mapset post OK\n");
  return failures == 0 ? 0 : 1;
}
