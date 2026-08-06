#include "test_fixtures.h"

int main() {
  ac6::NativeRenderTarget target;
  REQUIRE(target.resize(64, 32));
  REQUIRE(target.clear(0, 1.0f));
  REQUIRE(target.draw_hud_rect(4, 4, 12, 12, 0xFFFFFFFFu));
  const auto readback = target.readback();
  REQUIRE(readback.color_coverage > 0);
  std::vector<std::uint32_t> object_ids;
  REQUIRE(target.copy_object_id(object_ids));
  REQUIRE(object_ids.size() == 64u * 32u);
  return 0;
}
