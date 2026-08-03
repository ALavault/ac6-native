#include "../ac6_display_defaults.h"

#include <cassert>

int main() {
  static_assert(ac6::display::kWidth * 9 == ac6::display::kHeight * 16);
  assert(ac6::display::kWidth == 1280);
  assert(ac6::display::kHeight == 720);
  assert(ac6::display::kResolutionPreset == "720p");
  return 0;
}
