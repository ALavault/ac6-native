#include "ac6/product_runtime.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>

namespace {
void require(bool condition, const char* expression, int line) {
  if (!condition) {
    std::fprintf(stderr, "REQUIRE failed at line %d: %s\n", line, expression);
    std::abort();
  }
}
#define REQUIRE(value) require((value), #value, __LINE__)
}

int main() {
  ac6::SessionSaveSnapshot snapshot{};
  snapshot.mission_id = 1;
  snapshot.flight = {120, 1.0f, 2.0f, 3.0f, 0.1f, -0.2f, 0.3f, 0.001f};
  snapshot.campaign.completed.push_back({1, 1});
  ac6::MissionExecution::Checkpoint mission_checkpoint;
  mission_checkpoint.mission_id = 1;
  mission_checkpoint.failure_tick = 600;
  mission_checkpoint.flight = snapshot.flight;
  mission_checkpoint.scenario = {
      1, ac6::ScenarioState::Gameplay, 4097,
      {{1, "intercept_primary", true, ac6::ObjectiveState::Active}}, {10}};
  mission_checkpoint.combat_units = {
      {4097, 1, {0.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true},
      {4098, 2, {20.0f, 0.0f, 0.0f}, 40.0f, 100.0f, 1.0f, true}};
  mission_checkpoint.sequence.entries = {
      {ac6::MissionSequenceEvent{1, 1, 1,
                                 ac6::MissionSequenceEventType::ActivateObjective, 1, 0.0f}, true},
      {ac6::MissionSequenceEvent{1, 2, 1,
                                 ac6::MissionSequenceEventType::PlayRadio, 10, 0.25f}, false}};
  snapshot.checkpoint = mission_checkpoint;
  ac6::SessionSaveStore store;
  REQUIRE(store.save(2, snapshot));
  REQUIRE(!store.save(0, snapshot));
  const char* path = "ac6-test-session-save.ac6s";
  REQUIRE(store.write_file(path));

  ac6::SessionSaveStore loaded;
  REQUIRE(loaded.read_file(path));
  REQUIRE(loaded.load(2) != nullptr && *loaded.load(2) == snapshot);

  ac6::CampaignProgression campaign;
  REQUIRE(campaign.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(campaign.finalize());
  REQUIRE(campaign.restore(loaded.load(2)->campaign));
  REQUIRE(campaign.status(1)->state == ac6::CampaignMissionState::Completed);

  const char* bad_path = "ac6-test-bad-session-save.ac6s";
  { std::ofstream output(bad_path, std::ios::binary); output << "bad"; }
  REQUIRE(!loaded.read_file(bad_path));
  REQUIRE(loaded.load(2) != nullptr && *loaded.load(2) == snapshot);

  const char* v1_path = "ac6-test-session-save-v1.ac6s";
  auto write_u32 = [](std::ofstream& output, std::uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xffu),
                           static_cast<char>((value >> 8u) & 0xffu),
                           static_cast<char>((value >> 16u) & 0xffu),
                           static_cast<char>((value >> 24u) & 0xffu)};
    output.write(bytes, sizeof(bytes));
  };
  auto write_u64 = [&](std::ofstream& output, std::uint64_t value) {
    for (unsigned int i = 0; i < 8; ++i) {
      const char byte = static_cast<char>((value >> (i * 8u)) & 0xffu);
      output.write(&byte, 1);
    }
  };
  auto write_f32 = [&](std::ofstream& output, float value) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    write_u32(output, raw);
  };
  {
    std::ofstream output(v1_path, std::ios::binary);
    output.write("AC6SESS\0", 8);
    write_u32(output, 1);
    write_u32(output, 1);
    write_u32(output, 3);
    write_u32(output, 1);
    write_u64(output, 120);
    for (const float value : {1.0f, 2.0f, 3.0f, 0.1f, -0.2f, 0.3f, 0.001f}) {
      write_f32(output, value);
    }
    write_u32(output, 0);
  }
  REQUIRE(loaded.read_file(v1_path));
  REQUIRE(loaded.load(3) != nullptr && !loaded.load(3)->checkpoint.has_value());
  std::remove(path);
  std::remove(bad_path);
  std::remove(v1_path);
  return 0;
}
