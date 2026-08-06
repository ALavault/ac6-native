#include "test_fixtures.h"

int main() {
  ac6::NativeGeometryDatabase geometry;
  ac6::QualifiedBufferDatabase buffers;
  const ac6::MissionDrawable drawable{1, "terrain", "terrain", 119, 1,
                                      "missing", 3, 3, "qualified"};
  REQUIRE(!geometry.load_verified(drawable, buffers));
  REQUIRE(geometry.find("missing") == nullptr);
  REQUIRE(geometry.decoded("missing") == nullptr);
  return 0;
}
