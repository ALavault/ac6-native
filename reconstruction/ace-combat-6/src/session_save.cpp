#include "ac6/product_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <utility>

namespace ac6 {

namespace {

constexpr std::array<char, 8> kMagic{{'A', 'C', '6', 'S', 'E', 'S', 'S', '\0'}};

bool valid_campaign(const CampaignSaveSnapshot& snapshot) noexcept {
  if (snapshot.completed.size() > 1024) return false;
  std::uint32_t previous = 0;
  for (const CampaignSaveSnapshot::Record record : snapshot.completed) {
    if (record.mission_id == 0 || record.mission_id <= previous ||
        static_cast<std::uint8_t>(record.state) >
            static_cast<std::uint8_t>(CampaignMissionState::Failed)) return false;
    previous = record.mission_id;
  }
  return true;
}

bool valid_flight(const RuntimeSnapshot& flight) noexcept {
  return flight.tick != 0 && std::isfinite(flight.position_x) &&
         std::isfinite(flight.position_y) && std::isfinite(flight.position_z) &&
         std::isfinite(flight.pitch) && std::isfinite(flight.roll) &&
         std::isfinite(flight.yaw) && std::isfinite(flight.fixed_accumulator) &&
         flight.fixed_accumulator >= 0.0f && flight.fixed_accumulator < 1.0f / 60.0f;
}

bool valid_radio_playback(const RadioPlaybackSnapshot& playback,
                          std::uint32_t mission_id) noexcept {
  if (static_cast<std::uint8_t>(playback.state) >
      static_cast<std::uint8_t>(RadioPlaybackState::Interrupted)) return false;
  if (playback.state == RadioPlaybackState::Idle) {
    return playback.mission_id == 0 && playback.message_id == 0 && playback.audio_asset == 0 &&
           playback.subtitle_asset == 0 && playback.elapsed_seconds == 0.0f &&
           playback.duration_seconds == 0.0f;
  }
  return playback.mission_id == mission_id && playback.message_id != 0 &&
         playback.audio_asset != 0 && std::isfinite(playback.elapsed_seconds) &&
         std::isfinite(playback.duration_seconds) && playback.duration_seconds > 0.0f &&
         playback.elapsed_seconds >= 0.0f && playback.elapsed_seconds <= playback.duration_seconds;
}

bool valid_checkpoint(const MissionExecution::Checkpoint& checkpoint) noexcept {
  if (checkpoint.mission_id == 0 || !valid_flight(checkpoint.flight) ||
      checkpoint.scenario.mission_id != checkpoint.mission_id ||
      !valid_radio_playback(checkpoint.radio_playback, checkpoint.mission_id) ||
      static_cast<std::uint8_t>(checkpoint.scenario.state) >
          static_cast<std::uint8_t>(ScenarioState::Aborted) || checkpoint.scenario.player == 0 ||
      checkpoint.scenario.objectives.size() > 1024 ||
      checkpoint.scenario.radio_history.size() > 65536 || checkpoint.combat_units.empty() ||
      checkpoint.combat_units.size() > 4096 || checkpoint.sequence.entries.size() > 4096) return false;
  std::uint32_t previous_objective = 0;
  for (const ObjectiveRecord& objective : checkpoint.scenario.objectives) {
    if (!objective.valid() || objective.id <= previous_objective ||
        static_cast<std::uint8_t>(objective.state) >
            static_cast<std::uint8_t>(ObjectiveState::Failed)) return false;
    previous_objective = objective.id;
  }
  for (const std::uint32_t message : checkpoint.scenario.radio_history) {
    if (message == 0) return false;
  }
  EntityId previous_unit = 0;
  for (const CombatUnitState& unit : checkpoint.combat_units) {
    if (!unit.valid() || unit.entity <= previous_unit) return false;
    previous_unit = unit.entity;
  }
  std::uint32_t previous_mission = 0;
  std::uint64_t previous_tick = 0;
  std::uint32_t previous_order = 0;
  for (const MissionSequenceEntrySnapshot& entry : checkpoint.sequence.entries) {
    const MissionSequenceEvent& event = entry.event;
    if (!event.valid() || event.mission_id != checkpoint.mission_id ||
        event.mission_id < previous_mission ||
        (event.mission_id == previous_mission && event.tick < previous_tick) ||
        (event.mission_id == previous_mission && event.tick == previous_tick &&
         event.order <= previous_order)) return false;
    previous_mission = event.mission_id;
    previous_tick = event.tick;
    previous_order = event.order;
  }
  return true;
}

bool valid_session(const SessionSaveSnapshot& snapshot) noexcept {
  return snapshot.mission_id != 0 && valid_flight(snapshot.flight) &&
         valid_campaign(snapshot.campaign) &&
         (!snapshot.checkpoint.has_value() || valid_checkpoint(*snapshot.checkpoint));
}

void write_u32(std::ostream& output, std::uint32_t value) {
  const char bytes[4] = {static_cast<char>(value & 0xffu),
                         static_cast<char>((value >> 8u) & 0xffu),
                         static_cast<char>((value >> 16u) & 0xffu),
                         static_cast<char>((value >> 24u) & 0xffu)};
  output.write(bytes, sizeof(bytes));
}

void write_u64(std::ostream& output, std::uint64_t value) {
  for (unsigned int index = 0; index < 8; ++index) {
    const char byte = static_cast<char>((value >> (index * 8u)) & 0xffu);
    output.write(&byte, 1);
  }
}

void write_f32(std::ostream& output, float value) {
  std::uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  write_u32(output, raw);
}

bool read_u32(std::istream& input, std::uint32_t& value) {
  unsigned char bytes[4]{};
  input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
  if (!input) return false;
  value = static_cast<std::uint32_t>(bytes[0]) |
          (static_cast<std::uint32_t>(bytes[1]) << 8u) |
          (static_cast<std::uint32_t>(bytes[2]) << 16u) |
          (static_cast<std::uint32_t>(bytes[3]) << 24u);
  return true;
}

bool read_u64(std::istream& input, std::uint64_t& value) {
  unsigned char bytes[8]{};
  input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
  if (!input) return false;
  value = 0;
  for (unsigned int index = 0; index < 8; ++index) {
    value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8u);
  }
  return true;
}

bool read_f32(std::istream& input, float& value) {
  std::uint32_t raw = 0;
  if (!read_u32(input, raw)) return false;
  std::memcpy(&value, &raw, sizeof(value));
  return std::isfinite(value);
}

void write_flight(std::ostream& output, const RuntimeSnapshot& flight) {
  write_u64(output, flight.tick);
  write_f32(output, flight.position_x);
  write_f32(output, flight.position_y);
  write_f32(output, flight.position_z);
  write_f32(output, flight.pitch);
  write_f32(output, flight.roll);
  write_f32(output, flight.yaw);
  write_f32(output, flight.fixed_accumulator);
}

bool read_flight(std::istream& input, RuntimeSnapshot& flight);

void write_string(std::ostream& output, const std::string& value) {
  write_u32(output, static_cast<std::uint32_t>(value.size()));
  output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

bool read_string(std::istream& input, std::string& value) {
  std::uint32_t size = 0;
  if (!read_u32(input, size) || size > 65536) return false;
  value.resize(size);
  input.read(value.data(), static_cast<std::streamsize>(size));
  return static_cast<bool>(input);
}

void write_checkpoint(std::ostream& output, const MissionExecution::Checkpoint& checkpoint) {
  write_u32(output, checkpoint.mission_id);
  write_u64(output, checkpoint.failure_tick);
  write_flight(output, checkpoint.flight);
  write_u32(output, static_cast<std::uint32_t>(checkpoint.scenario.state));
  write_u32(output, checkpoint.scenario.player);
  write_u32(output, static_cast<std::uint32_t>(checkpoint.scenario.objectives.size()));
  for (const ObjectiveRecord& objective : checkpoint.scenario.objectives) {
    write_u32(output, objective.id);
    write_string(output, objective.stable_id);
    write_u32(output, objective.required ? 1u : 0u);
    write_u32(output, static_cast<std::uint32_t>(objective.state));
  }
  write_u32(output, static_cast<std::uint32_t>(checkpoint.scenario.radio_history.size()));
  for (const std::uint32_t message : checkpoint.scenario.radio_history) write_u32(output, message);
  write_u32(output, static_cast<std::uint32_t>(checkpoint.combat_units.size()));
  for (const CombatUnitState& unit : checkpoint.combat_units) {
    write_u32(output, unit.entity);
    write_u32(output, unit.faction);
    write_f32(output, unit.position.x);
    write_f32(output, unit.position.y);
    write_f32(output, unit.position.z);
    write_f32(output, unit.health);
    write_f32(output, unit.max_health);
    write_f32(output, unit.collision_radius);
    write_u32(output, unit.active ? 1u : 0u);
  }
  write_u32(output, static_cast<std::uint32_t>(checkpoint.sequence.entries.size()));
  for (const MissionSequenceEntrySnapshot& entry : checkpoint.sequence.entries) {
    write_u32(output, entry.event.mission_id);
    write_u64(output, entry.event.tick);
    write_u32(output, entry.event.order);
    write_u32(output, static_cast<std::uint32_t>(entry.event.type));
    write_u32(output, entry.event.id);
    write_f32(output, entry.event.duration_seconds);
    write_u32(output, entry.published ? 1u : 0u);
  }
  write_u32(output, checkpoint.radio_playback.mission_id);
  write_u32(output, checkpoint.radio_playback.message_id);
  write_u32(output, checkpoint.radio_playback.audio_asset);
  write_u32(output, checkpoint.radio_playback.subtitle_asset);
  write_f32(output, checkpoint.radio_playback.elapsed_seconds);
  write_f32(output, checkpoint.radio_playback.duration_seconds);
  write_u32(output, static_cast<std::uint32_t>(checkpoint.radio_playback.state));
}

bool read_checkpoint(std::istream& input, MissionExecution::Checkpoint& checkpoint,
                     bool has_sequence, bool has_radio) {
  std::uint32_t state = 0;
  std::uint32_t objective_count = 0;
  std::uint32_t radio_count = 0;
  std::uint32_t unit_count = 0;
  std::uint32_t sequence_count = 0;
  if (!read_u32(input, checkpoint.mission_id) || !read_u64(input, checkpoint.failure_tick) ||
      !read_flight(input, checkpoint.flight) ||
      !read_u32(input, state) || !read_u32(input, checkpoint.scenario.player) ||
      !read_u32(input, objective_count) || objective_count > 1024) return false;
  checkpoint.scenario.mission_id = checkpoint.mission_id;
  checkpoint.scenario.state = static_cast<ScenarioState>(state);
  checkpoint.scenario.objectives.reserve(objective_count);
  for (std::uint32_t index = 0; index < objective_count; ++index) {
    ObjectiveRecord objective;
    std::uint32_t required = 0;
    std::uint32_t objective_state = 0;
    if (!read_u32(input, objective.id) || !read_string(input, objective.stable_id) ||
        !read_u32(input, required) || required > 1 || !read_u32(input, objective_state)) return false;
    objective.required = required != 0;
    objective.state = static_cast<ObjectiveState>(objective_state);
    checkpoint.scenario.objectives.push_back(std::move(objective));
  }
  if (!read_u32(input, radio_count) || radio_count > 65536) return false;
  checkpoint.scenario.radio_history.reserve(radio_count);
  for (std::uint32_t index = 0; index < radio_count; ++index) {
    std::uint32_t message = 0;
    if (!read_u32(input, message)) return false;
    checkpoint.scenario.radio_history.push_back(message);
  }
  if (!read_u32(input, unit_count) || unit_count == 0 || unit_count > 4096) return false;
  checkpoint.combat_units.reserve(unit_count);
  for (std::uint32_t index = 0; index < unit_count; ++index) {
    CombatUnitState unit;
    std::uint32_t active = 0;
    if (!read_u32(input, unit.entity) || !read_u32(input, unit.faction) ||
        !read_f32(input, unit.position.x) || !read_f32(input, unit.position.y) ||
        !read_f32(input, unit.position.z) || !read_f32(input, unit.health) ||
        !read_f32(input, unit.max_health) || !read_f32(input, unit.collision_radius) ||
        !read_u32(input, active) || active > 1) return false;
    unit.active = active != 0;
    checkpoint.combat_units.push_back(unit);
  }
  if (has_sequence) {
    if (!read_u32(input, sequence_count) || sequence_count > 4096) return false;
    checkpoint.sequence.entries.reserve(sequence_count);
    for (std::uint32_t index = 0; index < sequence_count; ++index) {
      MissionSequenceEntrySnapshot entry;
      std::uint32_t type = 0;
      std::uint32_t published = 0;
      if (!read_u32(input, entry.event.mission_id) || !read_u64(input, entry.event.tick) ||
          !read_u32(input, entry.event.order) || !read_u32(input, type) ||
          !read_u32(input, entry.event.id) || !read_f32(input, entry.event.duration_seconds) ||
          !read_u32(input, published) || published > 1) return false;
      entry.event.type = static_cast<MissionSequenceEventType>(type);
      entry.published = published != 0;
      checkpoint.sequence.entries.push_back(entry);
    }
  }
  if (has_radio) {
    std::uint32_t state = 0;
    if (!read_u32(input, checkpoint.radio_playback.mission_id) ||
        !read_u32(input, checkpoint.radio_playback.message_id) ||
        !read_u32(input, checkpoint.radio_playback.audio_asset) ||
        !read_u32(input, checkpoint.radio_playback.subtitle_asset) ||
        !read_f32(input, checkpoint.radio_playback.elapsed_seconds) ||
        !read_f32(input, checkpoint.radio_playback.duration_seconds) ||
        !read_u32(input, state)) return false;
    checkpoint.radio_playback.state = static_cast<RadioPlaybackState>(state);
  }
  return valid_checkpoint(checkpoint);
}

bool read_flight(std::istream& input, RuntimeSnapshot& flight) {
  return read_u64(input, flight.tick) && read_f32(input, flight.position_x) &&
         read_f32(input, flight.position_y) && read_f32(input, flight.position_z) &&
         read_f32(input, flight.pitch) && read_f32(input, flight.roll) &&
         read_f32(input, flight.yaw) && read_f32(input, flight.fixed_accumulator) &&
         valid_flight(flight);
}

}  // namespace

bool SessionSaveStore::save(std::uint32_t slot, SessionSaveSnapshot snapshot) {
  if (slot == 0 || !valid_session(snapshot)) return false;
  slots_[slot] = std::move(snapshot);
  return true;
}

const SessionSaveSnapshot* SessionSaveStore::load(std::uint32_t slot) const noexcept {
  const auto it = slots_.find(slot);
  return it == slots_.end() ? nullptr : &it->second;
}

bool SessionSaveStore::write_file(const std::filesystem::path& path) const {
  if (path.empty() || slots_.size() > 1024) return false;
  const std::filesystem::path temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
  write_u32(output, 5);
  write_u32(output, static_cast<std::uint32_t>(slots_.size()));
  std::vector<std::uint32_t> slots;
  slots.reserve(slots_.size());
  for (const auto& [slot, snapshot] : slots_) {
    if (!valid_session(snapshot)) return false;
    slots.push_back(slot);
  }
  std::sort(slots.begin(), slots.end());
  for (const std::uint32_t slot : slots) {
    const SessionSaveSnapshot& snapshot = slots_.at(slot);
    write_u32(output, slot);
    write_u32(output, snapshot.mission_id);
    write_flight(output, snapshot.flight);
    write_u32(output, static_cast<std::uint32_t>(snapshot.campaign.completed.size()));
    for (const CampaignSaveSnapshot::Record record : snapshot.campaign.completed) {
      write_u32(output, record.mission_id);
      write_u32(output, record.objective_mask);
      write_u32(output, static_cast<std::uint32_t>(record.state));
      write_u32(output, record.loadout.aircraft_id);
      write_u32(output, record.loadout.weapon_id);
      write_u32(output, record.loadout.capability_data_valid ? 1u : 0u);
    }
    write_u32(output, snapshot.checkpoint.has_value() ? 1u : 0u);
    if (snapshot.checkpoint.has_value()) write_checkpoint(output, *snapshot.checkpoint);
  }
  if (!output) {
    output.close();
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
  }
  output.close();
  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  return true;
}

bool SessionSaveStore::read_file(const std::filesystem::path& path) {
  if (path.empty()) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  input.read(magic, sizeof(magic));
  if (!input || std::memcmp(magic, kMagic.data(), kMagic.size()) != 0) return false;
  std::uint32_t version = 0;
  std::uint32_t count = 0;
  if (!read_u32(input, version) || !read_u32(input, count) ||
      (version != 1 && version != 2 && version != 3 && version != 4 && version != 5) ||
      count > 1024) {
    return false;
  }
  std::unordered_map<std::uint32_t, SessionSaveSnapshot> loaded;
  for (std::uint32_t index = 0; index < count; ++index) {
    std::uint32_t slot = 0;
    SessionSaveSnapshot snapshot;
    std::uint32_t record_count = 0;
    if (!read_u32(input, slot) || !read_u32(input, snapshot.mission_id) || slot == 0 ||
        loaded.find(slot) != loaded.end() || !read_flight(input, snapshot.flight) ||
        !read_u32(input, record_count) || record_count > 1024) return false;
    snapshot.campaign.completed.reserve(record_count);
    for (std::uint32_t record_index = 0; record_index < record_count; ++record_index) {
      CampaignSaveSnapshot::Record record;
      if (!read_u32(input, record.mission_id) || !read_u32(input, record.objective_mask)) {
        return false;
      }
      if (version >= 5) {
        std::uint32_t state = 0;
        std::uint32_t capability = 0;
        if (!read_u32(input, state) || !read_u32(input, record.loadout.aircraft_id) ||
            !read_u32(input, record.loadout.weapon_id) || !read_u32(input, capability) ||
            capability > 1) return false;
        record.state = static_cast<CampaignMissionState>(state);
        record.loadout.capability_data_valid = capability != 0;
      }
      snapshot.campaign.completed.push_back(record);
    }
    if (version >= 2) {
      std::uint32_t has_checkpoint = 0;
      if (!read_u32(input, has_checkpoint) || has_checkpoint > 1) return false;
      if (has_checkpoint != 0) {
        MissionExecution::Checkpoint checkpoint;
        if (!read_checkpoint(input, checkpoint, version >= 3, version >= 4)) return false;
        snapshot.checkpoint = std::move(checkpoint);
      }
    }
    if (!valid_session(snapshot) || !loaded.emplace(slot, std::move(snapshot)).second) return false;
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof()) return false;
  slots_ = std::move(loaded);
  return true;
}

}  // namespace ac6
