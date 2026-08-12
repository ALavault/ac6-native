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
  const std::vector<std::uint32_t> source(64u * 32u, 0xFF102030u);
  const std::vector<float> source_depth(64u * 32u, 120.0F);
  REQUIRE(target.blit_argb32(64, 32, source, source_depth, 24000.0F));
  std::vector<std::uint8_t> rgba;
  REQUIRE(target.copy_rgba8(rgba));
  REQUIRE(rgba[0] == 0x10u && rgba[1] == 0x20u && rgba[2] == 0x30u &&
          rgba[3] == 0xFFu);
  const auto* rgba_storage = rgba.data();
  const std::size_t rgba_capacity = rgba.capacity();
  REQUIRE(target.copy_rgba8(rgba));
  REQUIRE(rgba.data() == rgba_storage);
  REQUIRE(rgba.capacity() == rgba_capacity);
  REQUIRE(target.readback().depth_coverage == source.size());
  REQUIRE(!target.blit_argb32(32, 64, source, source_depth, 24000.0F));
  return 0;
}
