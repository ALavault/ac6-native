#include "ac6/retail_session_replay.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>

namespace {

int check(bool condition, const char* message) {
  if (!condition) std::cerr << "FAIL " << message << '\n';
  return condition ? 0 : 1;
}

ac6::retail::RetailSessionReplay fixture() {
  ac6::retail::RetailSessionReplay replay;
  replay.mission_id = 1;
  replay.loadout = {1, 1, true};
  replay.content_index_sha256.fill(0x5a);
  replay.frames = {{1200, -2300, 3400, 200, 0x0010},
                   {-100, 0, 90, 255, 0x0000}};
  return replay;
}

}  // namespace

int main() {
  const char* path = "ac6-test-retail-session.ac6rply";
  const char* bad_path = "ac6-test-retail-session-bad.ac6rply";
  const auto original = fixture();
  int failures = 0;
  failures += check(original.valid(), "fixture is valid");
  failures += check(original.write_file(path), "atomic replay write succeeds");

  ac6::retail::RetailSessionReplay loaded;
  failures += check(loaded.read_file(path), "replay round-trips");
  failures += check(loaded.version == original.version &&
                        loaded.mission_id == original.mission_id &&
                        loaded.loadout == original.loadout &&
                        loaded.content_index_sha256 == original.content_index_sha256 &&
                        loaded.frames == original.frames,
                    "round-trip preserves identity and frames");

  {
    std::ofstream output(bad_path, std::ios::binary | std::ios::trunc);
    output << "not-a-retail-replay";
  }
  failures += check(!loaded.read_file(bad_path), "bad magic is rejected");
  failures += check(loaded.mission_id == original.mission_id,
                    "failed read does not destroy previous state");

  auto invalid = original;
  invalid.content_index_sha256.fill(0);
  failures += check(!invalid.valid() && !invalid.write_file(path),
                    "missing cache identity is rejected");
  invalid = original;
  invalid.loadout.capability_data_valid = false;
  failures += check(!invalid.valid() && !invalid.write_file(path),
                    "unqualified loadout is rejected");

  std::remove(path);
  std::remove(bad_path);
  return failures == 0 ? 0 : 1;
}
