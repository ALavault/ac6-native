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
  replay.difficulty = ac6::retail::RetailDifficulty::Ace;
  replay.loadout = {1, 1, true};
  replay.content_index_sha256.fill(0x5a);
  replay.frames = {{1200, -2300, 3400, 200, 0x0010},
                   {-100, 0, 90, 255, 0x0000}};
  replay.final_tick = replay.frames.size();
  replay.final_digest = replay.input_digest();
  replay.checkpoints = {{1, replay.input_digest(1)}};
  return replay;
}

}  // namespace

int main() {
  const char* path = "ac6-test-retail-session.ac6rply";
  const char* bad_path = "ac6-test-retail-session-bad.ac6rply";
  const char* legacy_path = "ac6-test-retail-session-v1.ac6rply";
  const char* legacy_v2_path = "ac6-test-retail-session-v2.ac6rply";
  const auto original = fixture();
  int failures = 0;
  failures += check(original.valid(), "fixture is valid");
  failures += check(original.write_file(path), "atomic replay write succeeds");

  ac6::retail::RetailSessionReplay loaded;
  failures += check(loaded.read_file(path), "replay round-trips");
  failures += check(loaded.version == original.version &&
                        loaded.mission_id == original.mission_id &&
                        loaded.difficulty == original.difficulty &&
                        loaded.loadout == original.loadout &&
                        loaded.content_index_sha256 == original.content_index_sha256 &&
                        loaded.frames == original.frames &&
                        loaded.random_seed == original.random_seed &&
                        loaded.final_tick == original.final_tick &&
                        loaded.final_digest == original.final_digest &&
                        loaded.checkpoints == original.checkpoints,
                    "round-trip preserves metadata, identity and frames");

  {
    std::ofstream output(bad_path, std::ios::binary | std::ios::trunc);
    output << "not-a-retail-replay";
  }
  failures += check(!loaded.read_file(bad_path), "bad magic is rejected");
  failures += check(loaded.mission_id == original.mission_id,
                    "failed read does not destroy previous state");

  {
    std::ofstream output(legacy_path, std::ios::binary | std::ios::trunc);
    auto u32 = [&output](std::uint32_t value) {
      const char bytes[4]{static_cast<char>(value), static_cast<char>(value >> 8u),
                          static_cast<char>(value >> 16u), static_cast<char>(value >> 24u)};
      output.write(bytes, sizeof(bytes));
    };
    output.write("AC6RTPLY\0", 9);
    u32(1);  // v1 had no difficulty field; read migration supplies Normal.
    u32(1);
    u32(1);
    u32(1);
    u32(1);
    const std::array<unsigned char, 32> digest{};
    output.write(reinterpret_cast<const char*>(digest.data()), digest.size());
    u32(1);
    const std::array<unsigned char, 9> frame{0, 0, 0, 0, 0, 0, 0, 0, 0};
    output.write(reinterpret_cast<const char*>(frame.data()), frame.size());
  }
  // Make the legacy digest non-zero without changing the fixture identity
  // used by the current-format checks above.
  {
    std::fstream legacy(legacy_path, std::ios::binary | std::ios::in | std::ios::out);
    legacy.seekp(9 + 4 * 5);
    const char byte = static_cast<char>(0x5a);
    legacy.write(&byte, 1);
  }
  failures += check(loaded.read_file(legacy_path), "v1 replay migrates");
  failures += check(loaded.version == ac6::retail::RetailSessionReplay::kCurrentVersion &&
                        loaded.difficulty == ac6::retail::RetailDifficulty::Normal &&
                        loaded.final_tick == loaded.frames.size() &&
                        loaded.final_digest == loaded.input_digest() &&
                        loaded.checkpoints.empty(),
                    "v1 replay migrates deterministic metadata");

  {
    std::ofstream output(legacy_v2_path, std::ios::binary | std::ios::trunc);
    auto u32 = [&output](std::uint32_t value) {
      const char bytes[4]{static_cast<char>(value), static_cast<char>(value >> 8u),
                          static_cast<char>(value >> 16u), static_cast<char>(value >> 24u)};
      output.write(bytes, sizeof(bytes));
    };
    output.write("AC6RTPLY\0", 9);
    u32(2);  // v2 carries difficulty but no replay metadata.
    u32(1);
    u32(4);
    u32(1);
    u32(1);
    u32(1);
    std::array<unsigned char, 32> qualified_digest{};
    qualified_digest.fill(0x5a);
    output.write(reinterpret_cast<const char*>(qualified_digest.data()),
                 qualified_digest.size());
    u32(1);
    const std::array<unsigned char, 9> frame{0, 0, 0, 0, 0, 0, 0, 0, 0};
    output.write(reinterpret_cast<const char*>(frame.data()), frame.size());
  }
  failures += check(loaded.read_file(legacy_v2_path), "v2 replay migrates");
  failures += check(loaded.difficulty == ac6::retail::RetailDifficulty::Ace &&
                        loaded.final_tick == loaded.frames.size() &&
                        loaded.final_digest == loaded.input_digest(),
                    "v2 replay preserves difficulty during migration");

  auto invalid = original;
  invalid.content_index_sha256.fill(0);
  failures += check(!invalid.valid() && !invalid.write_file(path),
                    "missing cache identity is rejected");
  invalid = original;
  invalid.loadout.capability_data_valid = false;
  failures += check(!invalid.valid() && !invalid.write_file(path),
                    "unqualified loadout is rejected");
  invalid = original;
  invalid.final_digest[0] ^= 0xffu;
  failures += check(!invalid.valid() && !invalid.write_file(path),
                    "wrong final digest is rejected");
  invalid = original;
  invalid.checkpoints[0].input_digest.fill(0);
  failures += check(!invalid.valid() && !invalid.write_file(path),
                    "wrong checkpoint digest is rejected");

  std::remove(path);
  std::remove(bad_path);
  std::remove(legacy_path);
  std::remove(legacy_v2_path);
  return failures == 0 ? 0 : 1;
}
