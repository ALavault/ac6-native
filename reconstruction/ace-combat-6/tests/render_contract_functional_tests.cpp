#include "test_fixtures.h"

int main() {
  ac6::NativeRenderTarget target;
  REQUIRE(!target.resize(0, 32));
  REQUIRE(target.resize(32, 18));
  REQUIRE(target.clear(0x11223344u, 1.0f));
  std::vector<std::uint8_t> pixels;
  REQUIRE(target.copy_rgba8(pixels));
  REQUIRE(target.readback().width == 32 && target.readback().height == 18);

  ac6::NativeGeometryDatabase geometry;
  const ac6::MissionDrawable missing{1, "missing", "terrain", 9, 1, "missing", 1, 1,
                                     "missing"};
  ac6::QualifiedBufferDatabase buffers;
  REQUIRE(!geometry.load_verified(missing, buffers));
  return 0;
}
