#pragma once

#include "ac6/product_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace ac6_test {

inline void require(bool condition, const char* expression, const char* file, int line) {
  if (!condition) {
    std::fprintf(stderr, "REQUIRE failed at %s:%d: %s\n", file, line, expression);
    std::abort();
  }
}

inline std::uint64_t fnv64(std::string_view bytes) {
  std::uint64_t hash = 1469598103934665603ull;
  for (unsigned char byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

inline ac6::MissionCatalog catalog_fixture() {
  ac6::MissionCatalog catalog;
  require(catalog.add({1, ac6::MissionFamily::AirIntercept, {9, 119, 165, 199, 210}}),
          "catalog fixture", __FILE__, __LINE__);
  return catalog;
}

inline ac6::MissionLaunchDefinition launch_fixture() {
  ac6::MissionLaunchDefinition launch;
  launch.mission_id = 1;
  launch.player_entity = 4097;
  launch.units = {{4097, 1, 9}, {4098, 1, 119}};
  launch.weapons = {{7, 60.0f, 20.0f, 0.25f, 100.0f}};
  return launch;
}

}  // namespace ac6_test

#define REQUIRE(condition) \
  ::ac6_test::require(static_cast<bool>(condition), #condition, __FILE__, __LINE__)
