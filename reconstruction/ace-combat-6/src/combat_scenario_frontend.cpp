#include "ac6/product_runtime.h"
#include "text_parse.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string_view>
#include <unordered_set>

namespace ac6 {
namespace {

template <std::size_t FieldCount>
bool parse_tsv_fields(std::string_view line,
                      std::array<std::string_view, FieldCount>& fields) noexcept {
  std::size_t start = 0;
  for (std::size_t index = 0; index < FieldCount; ++index) {
    if (start > line.size()) return false;
    const std::size_t tab = line.find('\t', start);
    if (index + 1 == FieldCount) {
      if (tab != std::string_view::npos) return false;
      fields[index] = line.substr(start);
    } else {
      if (tab == std::string_view::npos) return false;
      fields[index] = line.substr(start, tab - start);
      start = tab + 1;
    }
    if (fields[index].empty()) return false;
  }
  return true;
}
bool parse_objective_condition(std::string_view text,
                               ObjectiveCondition& condition) noexcept {
  if (text == "manual") {
    condition = ObjectiveCondition::Manual;
    return true;
  }
  if (text == "destroy_unit") {
    condition = ObjectiveCondition::DestroyUnit;
    return true;
  }
  if (text == "protect_unit") {
    condition = ObjectiveCondition::ProtectUnit;
    return true;
  }
  return false;
}
std::uint16_t read_le_u16(const unsigned char* bytes) noexcept {
  return static_cast<std::uint16_t>(bytes[0]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8u);
}
std::uint32_t read_le_u32(const unsigned char* bytes) noexcept {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8u) |
         (static_cast<std::uint32_t>(bytes[2]) << 16u) |
         (static_cast<std::uint32_t>(bytes[3]) << 24u);
}
}

bool UnitRegistry::register_unit(UnitRecord unit) {
  if (unit.id == 0 || unit.asset == 0 || unit.owner == unit.id) return false;
  unit.active = false;
  return units_.emplace(unit.id, unit).second;
}

bool ObjectiveRegistry::add(ObjectiveRecord objective) {
  if (!objective.valid() || objective.state != ObjectiveState::Pending) return false;
  return objectives_.emplace(objective.id, std::move(objective)).second;
}

bool ObjectiveRegistry::activate(std::uint32_t id) noexcept {
  ObjectiveRecord* objective = nullptr;
  const auto it = objectives_.find(id);
  if (it != objectives_.end()) objective = &it->second;
  if (objective == nullptr || objective->state != ObjectiveState::Pending) return false;
  objective->state = ObjectiveState::Active;
  return true;
}

bool ObjectiveRegistry::complete(std::uint32_t id) noexcept {
  const auto it = objectives_.find(id);
  if (it == objectives_.end() || it->second.state != ObjectiveState::Active) return false;
  it->second.state = ObjectiveState::Complete;
  return true;
}

bool ObjectiveRegistry::fail(std::uint32_t id) noexcept {
  const auto it = objectives_.find(id);
  if (it == objectives_.end() || it->second.state == ObjectiveState::Complete ||
      it->second.state == ObjectiveState::Failed) return false;
  it->second.state = ObjectiveState::Failed;
  return true;
}

std::vector<ObjectiveRecord> ObjectiveRegistry::snapshot() const {
  std::vector<ObjectiveRecord> result;
  result.reserve(objectives_.size());
  for (const auto& [id, objective] : objectives_) {
    (void)id;
    result.push_back(objective);
  }
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return left.id < right.id;
  });
  return result;
}

bool ObjectiveRegistry::restore(const std::vector<ObjectiveRecord>& snapshot) noexcept {
  if (snapshot.size() > 1024) return false;
  std::unordered_map<std::uint32_t, ObjectiveRecord> loaded;
  std::uint32_t previous = 0;
  for (const ObjectiveRecord& objective : snapshot) {
    if (!objective.valid() || objective.id <= previous ||
        static_cast<std::uint8_t>(objective.state) >
            static_cast<std::uint8_t>(ObjectiveState::Failed) ||
        !loaded.emplace(objective.id, objective).second) return false;
    previous = objective.id;
  }
  objectives_ = std::move(loaded);
  return true;
}

const ObjectiveRecord* ObjectiveRegistry::find(std::uint32_t id) const noexcept {
  const auto it = objectives_.find(id);
  return it == objectives_.end() ? nullptr : &it->second;
}

bool ObjectiveRegistry::all_required_complete() const noexcept {
  return std::all_of(objectives_.begin(), objectives_.end(), [](const auto& entry) {
    return !entry.second.required || entry.second.state == ObjectiveState::Complete;
  });
}

std::size_t ObjectiveRegistry::completed_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      objectives_.begin(), objectives_.end(), [](const auto& entry) {
        return entry.second.state == ObjectiveState::Complete;
      }));
}

std::size_t ObjectiveRegistry::failed_count() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      objectives_.begin(), objectives_.end(), [](const auto& entry) {
        return entry.second.state == ObjectiveState::Failed;
      }));
}

bool MissionObjectiveDatabase::add(MissionObjectiveDefinition definition) {
  if (definition.mission_id == 0 || !definition.objective.valid() ||
      definition.objective.state != ObjectiveState::Pending) return false;
  for (const MissionObjectiveDefinition& existing : objectives_) {
    if (existing.mission_id == definition.mission_id &&
        existing.objective.id == definition.objective.id) return false;
  }
  objectives_.push_back(std::move(definition));
  return true;
}

bool MissionObjectiveDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionObjectiveDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string_view, 6> fields{};
    std::size_t start = 0;
    std::size_t field_count = 0;
    while (field_count < fields.size()) {
      const std::size_t tab = line.find('\t', start);
      if (tab == std::string::npos) {
        fields[field_count++] = std::string_view(line).substr(start);
        break;
      }
      fields[field_count++] = std::string_view(line).substr(start, tab - start);
      start = tab + 1;
    }
    if ((field_count != 4 && field_count != 6) || line.find('\t', start) != std::string::npos) {
      return false;
    }
    MissionObjectiveDefinition definition;
    bool required = false;
    if (!detail::parse_u32(fields[0], definition.mission_id) ||
        !detail::parse_u32(fields[1], definition.objective.id) ||
        fields[2].empty() ||
        !detail::parse_bool01(fields[3], required)) return false;
    definition.objective.stable_id = std::string(fields[2]);
    definition.objective.required = required;
    if (field_count == 6) {
      if (!parse_objective_condition(fields[4], definition.objective.condition) ||
          !detail::parse_u32(fields[5], definition.objective.target_entity)) return false;
    }
    if (!loaded.add(std::move(definition))) return false;
  }
  objectives_ = std::move(loaded.objectives_);
  return true;
}

std::vector<const ObjectiveRecord*> MissionObjectiveDatabase::find_by_mission(
    std::uint32_t mission_id) const {
  std::vector<const ObjectiveRecord*> result;
  for (const MissionObjectiveDefinition& definition : objectives_) {
    if (definition.mission_id == mission_id) result.push_back(&definition.objective);
  }
  return result;
}

bool RadioMessageDatabase::add(RadioMessageDefinition message) {
  if (!message.valid()) return false;
  if (find(message.mission_id, message.id) != nullptr) return false;
  messages_.push_back(std::move(message));
  return true;
}

bool RadioMessageDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  RadioMessageDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::size_t tabs[5]{};
    std::size_t previous = 0;
    bool malformed = false;
    for (std::size_t i = 0; i < 5; ++i) {
      tabs[i] = line.find('\t', previous);
      if (tabs[i] == std::string::npos) { malformed = true; break; }
      previous = tabs[i] + 1;
    }
    if (malformed || line.find('\t', previous) != std::string::npos) return false;
    RadioMessageDefinition message;
    if (!detail::parse_u32(std::string_view(line).substr(0, tabs[0]), message.mission_id) ||
        !detail::parse_u32(std::string_view(line).substr(tabs[0] + 1, tabs[1] - tabs[0] - 1), message.id) ||
        (message.stable_id = line.substr(tabs[1] + 1, tabs[2] - tabs[1] - 1)).empty() ||
        (message.speaker = line.substr(tabs[2] + 1, tabs[3] - tabs[2] - 1)).empty() ||
        !detail::parse_u32(std::string_view(line).substr(tabs[3] + 1, tabs[4] - tabs[3] - 1), message.audio_asset) ||
        !detail::parse_u32(std::string_view(line).substr(tabs[4] + 1), message.subtitle_asset) ||
        !loaded.add(std::move(message))) return false;
  }
  messages_ = std::move(loaded.messages_);
  return true;
}

const RadioMessageDefinition* RadioMessageDatabase::find(std::uint32_t mission_id,
                                                          std::uint32_t id) const noexcept {
  for (const RadioMessageDefinition& message : messages_) {
    if (message.mission_id == mission_id && message.id == id) return &message;
  }
  return nullptr;
}

bool RadioPlaybackService::start(const RadioMessageDatabase& messages, std::uint32_t mission_id,
                                  std::uint32_t message_id, float duration_seconds) noexcept {
  const RadioMessageDefinition* message = messages.find(mission_id, message_id);
  if (message == nullptr || !std::isfinite(duration_seconds) || duration_seconds <= 0.0f ||
      playing()) return false;
  snapshot_ = {mission_id, message_id, message->audio_asset, message->subtitle_asset,
               0.0f, duration_seconds, RadioPlaybackState::Playing};
  return true;
}

bool RadioPlaybackService::tick(float fixed_dt) noexcept {
  if (!playing()) return false;
  if (!(fixed_dt > 0.0f) || !std::isfinite(fixed_dt)) fixed_dt = 1.0f / 60.0f;
  snapshot_.elapsed_seconds = std::min(snapshot_.duration_seconds,
                                       snapshot_.elapsed_seconds + std::min(fixed_dt, 0.25f));
  if (snapshot_.elapsed_seconds >= snapshot_.duration_seconds) {
    snapshot_.state = RadioPlaybackState::Complete;
  }
  return true;
}

bool RadioPlaybackService::finish() noexcept {
  if (!playing()) return false;
  snapshot_.elapsed_seconds = snapshot_.duration_seconds;
  snapshot_.state = RadioPlaybackState::Complete;
  return true;
}

bool RadioPlaybackService::interrupt() noexcept {
  if (!playing()) return false;
  snapshot_.state = RadioPlaybackState::Interrupted;
  return true;
}

bool RadioPlaybackService::restore(RadioPlaybackSnapshot snapshot) noexcept {
  if (static_cast<std::uint8_t>(snapshot.state) >
      static_cast<std::uint8_t>(RadioPlaybackState::Interrupted)) return false;
  if (snapshot.state == RadioPlaybackState::Idle) {
    if (snapshot.mission_id != 0 || snapshot.message_id != 0 || snapshot.audio_asset != 0 ||
        snapshot.subtitle_asset != 0 || snapshot.elapsed_seconds != 0.0f ||
        snapshot.duration_seconds != 0.0f) return false;
  } else if (snapshot.mission_id == 0 || snapshot.message_id == 0 || snapshot.audio_asset == 0 ||
             !std::isfinite(snapshot.elapsed_seconds) || !std::isfinite(snapshot.duration_seconds) ||
             snapshot.duration_seconds <= 0.0f || snapshot.elapsed_seconds < 0.0f ||
             snapshot.elapsed_seconds > snapshot.duration_seconds) {
    return false;
  }
  snapshot_ = snapshot;
  return true;
}

void RadioPlaybackService::reset() noexcept {
  snapshot_ = {};
}

bool MissionSequenceEvent::valid() const noexcept {
  const auto event_type = static_cast<std::uint8_t>(type);
  if (mission_id == 0 || tick == 0 || order == 0 || id == 0 || event_type >
      static_cast<std::uint8_t>(MissionSequenceEventType::PlayRadio)) return false;
  if (event_type == static_cast<std::uint8_t>(MissionSequenceEventType::PlayRadio)) {
    return std::isfinite(duration_seconds) && duration_seconds > 0.0f;
  }
  return duration_seconds == 0.0f;
}

bool MissionSequenceDirector::add(MissionSequenceEvent event) {
  if (!event.valid()) return false;
  for (const Entry& entry : entries_) {
    if (entry.event.mission_id == event.mission_id && entry.event.tick == event.tick &&
        entry.event.order == event.order) return false;
  }
  entries_.push_back({event, false});
  std::sort(entries_.begin(), entries_.end(), [](const Entry& left, const Entry& right) {
    if (left.event.mission_id != right.event.mission_id) {
      return left.event.mission_id < right.event.mission_id;
    }
    if (left.event.tick != right.event.tick) return left.event.tick < right.event.tick;
    return left.event.order < right.event.order;
  });
  return true;
}

bool MissionSequenceDirector::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionSequenceDirector loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string_view, 6> fields{};
    std::size_t start = 0;
    std::size_t field_count = 0;
    while (field_count < fields.size()) {
      const std::size_t tab = line.find('\t', start);
      if (tab == std::string::npos) {
        fields[field_count++] = std::string_view(line).substr(start);
        break;
      }
      fields[field_count++] = std::string_view(line).substr(start, tab - start);
      start = tab + 1;
    }
    if (field_count != fields.size() || line.find('\t', start) != std::string::npos) return false;
    MissionSequenceEvent event;
    if (!detail::parse_u32(fields[0], event.mission_id) || !detail::parse_u64(fields[1], event.tick) ||
        !detail::parse_u32(fields[2], event.order) || !detail::parse_u32(fields[4], event.id) ||
        !detail::parse_f32(fields[5], event.duration_seconds)) return false;
    if (fields[3] == "activate_objective") {
      event.type = MissionSequenceEventType::ActivateObjective;
    } else if (fields[3] == "complete_objective") {
      event.type = MissionSequenceEventType::CompleteObjective;
    } else if (fields[3] == "fail_objective") {
      event.type = MissionSequenceEventType::FailObjective;
    } else if (fields[3] == "play_radio") {
      event.type = MissionSequenceEventType::PlayRadio;
    } else {
      return false;
    }
    if (!loaded.add(event)) return false;
  }
  if (loaded.entries_.empty()) return false;
  entries_ = std::move(loaded.entries_);
  return true;
}

bool InputBinding::valid() const noexcept {
  return button_mask != 0 && static_cast<std::uint8_t>(event) <=
      static_cast<std::uint8_t>(EventType::Abort);
}

bool InputMappingDatabase::add(InputBinding binding) {
  if (!binding.valid() || std::find_if(bindings_.begin(), bindings_.end(),
                                       [binding](const InputBinding& existing) {
                                         return existing.button_mask == binding.button_mask;
                                       }) != bindings_.end()) {
    return false;
  }
  bindings_.push_back(binding);
  return true;
}

bool InputMappingDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  InputMappingDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    const std::string_view view(line);
    if (view.empty() || view.front() == '#') continue;
    const auto tab = view.find('\t');
    if (tab == std::string_view::npos) return false;
    std::uint32_t mask{};
    if (!detail::parse_u32(view.substr(0, tab), mask) || mask > 0xFFFFu) return false;
    const auto action = view.substr(tab + 1);
    EventType event{};
    if (action == "start_mission") event = EventType::StartMission;
    else if (action == "pause") event = EventType::Pause;
    else if (action == "resume") event = EventType::Resume;
    else if (action == "complete") event = EventType::Complete;
    else if (action == "abort") event = EventType::Abort;
    else return false;
    if (!loaded.add({static_cast<std::uint16_t>(mask), event})) return false;
  }
  if (loaded.bindings_.empty()) return false;
  bindings_ = std::move(loaded.bindings_);
  return true;
}

const InputBinding* InputMappingDatabase::resolve(std::uint16_t buttons) const noexcept {
  for (const InputBinding& binding : bindings_) {
    if (binding.button_mask == buttons) return &binding;
  }
  const InputBinding* best = nullptr;
  unsigned best_bits = 0;
  for (const InputBinding& binding : bindings_) {
    if (binding.button_mask == 0 || (buttons & binding.button_mask) != binding.button_mask) continue;
    unsigned bits = 0;
    for (std::uint16_t mask = binding.button_mask; mask != 0; mask >>= 1u) {
      bits += static_cast<unsigned>(mask & 1u);
    }
    if (best == nullptr || bits > best_bits) {
      best = &binding;
      best_bits = bits;
    }
  }
  if (best != nullptr) return best;
  return nullptr;
}

bool ReplayLog::write_file(const std::filesystem::path& path) const {
  if (path.empty() || frames_.size() > 1000000u) return false;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  const auto write_u16 = [&output](std::uint16_t value) {
    const unsigned char bytes[2] = {static_cast<unsigned char>(value & 0xffu),
                                    static_cast<unsigned char>((value >> 8u) & 0xffu)};
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
  };
  const auto write_u32 = [&output](std::uint32_t value) {
    const unsigned char bytes[4] = {static_cast<unsigned char>(value & 0xffu),
                                    static_cast<unsigned char>((value >> 8u) & 0xffu),
                                    static_cast<unsigned char>((value >> 16u) & 0xffu),
                                    static_cast<unsigned char>((value >> 24u) & 0xffu)};
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
  };
  output.write("AC6RPLY\0", 8);
  write_u32(1u);
  write_u32(static_cast<std::uint32_t>(frames_.size()));
  for (const InputFrame frame : frames_) {
    write_u16(static_cast<std::uint16_t>(frame.pitch));
    write_u16(static_cast<std::uint16_t>(frame.roll));
    write_u16(static_cast<std::uint16_t>(frame.yaw));
    output.put(static_cast<char>(frame.throttle));
    write_u16(frame.buttons);
  }
  return static_cast<bool>(output);
}

bool ReplayLog::read_file(const std::filesystem::path& path) {
  if (path.empty()) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  input.read(magic, sizeof(magic));
  if (!input || std::memcmp(magic, "AC6RPLY\0", 8) != 0) return false;
  unsigned char version_bytes[4]{};
  unsigned char count_bytes[4]{};
  input.read(reinterpret_cast<char*>(version_bytes), sizeof(version_bytes));
  input.read(reinterpret_cast<char*>(count_bytes), sizeof(count_bytes));
  if (!input) return false;
  const std::uint32_t version = read_le_u32(version_bytes);
  const std::uint32_t count = read_le_u32(count_bytes);
  if (version != 1u || count > 1000000u) return false;
  std::vector<InputFrame> loaded;
  loaded.reserve(count);
  unsigned char bytes[7]{};
  for (std::uint32_t i = 0; i < count; ++i) {
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (!input) return false;
    InputFrame frame;
    std::uint16_t raw_pitch = read_le_u16(bytes);
    std::uint16_t raw_roll = read_le_u16(bytes + 2);
    std::uint16_t raw_yaw = read_le_u16(bytes + 4);
    std::memcpy(&frame.pitch, &raw_pitch, sizeof(frame.pitch));
    std::memcpy(&frame.roll, &raw_roll, sizeof(frame.roll));
    std::memcpy(&frame.yaw, &raw_yaw, sizeof(frame.yaw));
    frame.throttle = bytes[6];
    unsigned char button_bytes[2]{};
    input.read(reinterpret_cast<char*>(button_bytes), sizeof(button_bytes));
    if (!input) return false;
    frame.buttons = read_le_u16(button_bytes);
    loaded.push_back(frame);
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof()) return false;
  frames_ = std::move(loaded);
  return true;
}

bool UnitRegistry::activate(EntityId id) noexcept {
  const auto it = units_.find(id);
  if (it == units_.end()) return false;
  it->second.active = true;
  return true;
}

bool UnitRegistry::deactivate(EntityId id) noexcept {
  const auto it = units_.find(id);
  if (it == units_.end() || !it->second.active) return false;
  it->second.active = false;
  return true;
}

bool UnitRegistry::set_active(EntityId id, bool active) noexcept {
  const auto it = units_.find(id);
  if (it == units_.end()) return false;
  it->second.active = active;
  return true;
}

std::size_t UnitRegistry::active_count() const noexcept {
  std::size_t count = 0;
  for (const auto& [id, unit] : units_) {
    (void)id;
    count += unit.active ? 1u : 0u;
  }
  return count;
}

const UnitRecord* UnitRegistry::find(EntityId id) const noexcept {
  const auto it = units_.find(id);
  return it == units_.end() ? nullptr : &it->second;
}

std::vector<UnitRecord> UnitRegistry::snapshot() const {
  std::vector<UnitRecord> result;
  result.reserve(units_.size());
  for (const auto& [id, unit] : units_) {
    (void)id;
    result.push_back(unit);
  }
  std::sort(result.begin(), result.end(), [](const UnitRecord& left, const UnitRecord& right) {
    return left.id < right.id;
  });
  return result;
}

bool UnitRegistry::restore(const std::vector<UnitRecord>& snapshot) noexcept {
  if (snapshot.size() > 4096) return false;
  std::unordered_map<EntityId, UnitRecord> loaded;
  EntityId previous = 0;
  for (const UnitRecord& unit : snapshot) {
    if (unit.id == 0 || unit.asset == 0 || unit.owner == unit.id || unit.id <= previous ||
        !loaded.emplace(unit.id, unit).second) return false;
    previous = unit.id;
  }
  units_ = std::move(loaded);
  return true;
}

bool CombatUnitState::valid() const noexcept {
  return entity != 0 && faction != 0 && std::isfinite(position.x) &&
         std::isfinite(position.y) && std::isfinite(position.z) &&
         std::isfinite(health) && std::isfinite(max_health) && max_health > 0.0f &&
         health >= 0.0f && health <= max_health && (!active || health > 0.0f) &&
         std::isfinite(collision_radius) &&
         collision_radius > 0.0f;
}

bool WeaponDefinition::valid() const noexcept {
  return id != 0 && std::isfinite(damage) && damage > 0.0f &&
         std::isfinite(projectile_speed) && projectile_speed > 0.0f &&
         std::isfinite(cooldown) && cooldown >= 0.0f && std::isfinite(max_range) &&
         max_range > 0.0f;
}

namespace {

float combat_distance_squared(CombatVector a, CombatVector b) noexcept {
  const float x = a.x - b.x;
  const float y = a.y - b.y;
  const float z = a.z - b.z;
  return x * x + y * y + z * z;
}

}  // namespace

bool CombatWorld::add_unit(CombatUnitState unit) {
  if (!unit.valid() || unit.active == false || this->unit(unit.entity) != nullptr) return false;
  units_.push_back(unit);
  return true;
}

bool CombatWorld::add_weapon(WeaponDefinition weapon) {
  if (!weapon.valid() || std::any_of(weapons_.begin(), weapons_.end(), [&](const auto& existing) {
        return existing.definition.id == weapon.id;
      })) return false;
  weapons_.push_back({weapon, 0.0f});
  return true;
}

bool CombatWorld::lock_target(EntityId owner, EntityId target) noexcept {
  const CombatUnitState* source = unit(owner);
  const CombatUnitState* destination = unit(target);
  if (source == nullptr || destination == nullptr || !source->active || !destination->active ||
      owner == target || source->faction == destination->faction) return false;
  locks_[owner] = target;
  return true;
}

EntityId CombatWorld::locked_target(EntityId owner) const noexcept {
  const auto it = locks_.find(owner);
  return it == locks_.end() ? 0 : it->second;
}

bool CombatWorld::fire(EntityId owner, std::uint32_t weapon_id) noexcept {
  const CombatUnitState* source = unit(owner);
  const EntityId target_id = locked_target(owner);
  const CombatUnitState* target = unit(target_id);
  auto weapon = std::find_if(weapons_.begin(), weapons_.end(), [&](const auto& candidate) {
    return candidate.definition.id == weapon_id;
  });
  if (source == nullptr || target == nullptr || weapon == weapons_.end() || !source->active ||
      !target->active || source->faction == target->faction ||
      weapon->cooldown_remaining > 0.0f || next_projectile_id_ == 0) return false;
  CombatVector direction{target->position.x - source->position.x,
                         target->position.y - source->position.y,
                         target->position.z - source->position.z};
  const float distance_squared = combat_distance_squared(source->position, target->position);
  if (!std::isfinite(distance_squared) || distance_squared <= 0.000001f) return false;
  const float distance = std::sqrt(distance_squared);
  if (distance > weapon->definition.max_range) return false;
  const float inverse_distance = 1.0f / distance;
  direction.x *= inverse_distance;
  direction.y *= inverse_distance;
  direction.z *= inverse_distance;
  projectiles_.push_back({next_projectile_id_++, owner, target_id, source->position,
                          {direction.x * weapon->definition.projectile_speed,
                           direction.y * weapon->definition.projectile_speed,
                           direction.z * weapon->definition.projectile_speed},
                          weapon->definition.damage, weapon->definition.max_range, true});
  weapon->cooldown_remaining = weapon->definition.cooldown;
  return true;
}

bool CombatWorld::apply_damage(EntityId target, float damage) noexcept {
  if (!std::isfinite(damage) || damage <= 0.0f) return false;
  for (auto& unit : units_) {
    if (unit.entity != target || !unit.active) continue;
    unit.health = std::max(0.0f, unit.health - damage);
    unit.active = unit.health > 0.0f;
    ++damage_events_;
    return true;
  }
  return false;
}

bool CombatWorld::deactivate_unit(EntityId entity) noexcept {
  for (auto& unit : units_) {
    if (unit.entity != entity || !unit.active) continue;
    unit.active = false;
    for (auto& projectile : projectiles_) {
      if (projectile.owner == entity || projectile.target == entity) projectile.active = false;
    }
    locks_.erase(entity);
    return true;
  }
  return false;
}

void CombatWorld::tick(float fixed_dt) noexcept {
  if (!(fixed_dt > 0.0f) || !std::isfinite(fixed_dt)) fixed_dt = 1.0f / 60.0f;
  fixed_dt = std::min(fixed_dt, 0.25f);
  for (auto& weapon : weapons_) {
    weapon.cooldown_remaining = std::max(0.0f, weapon.cooldown_remaining - fixed_dt);
  }
  for (auto& projectile : projectiles_) {
    if (!projectile.active) continue;
    const CombatVector previous_position = projectile.position;
    projectile.position.x += projectile.velocity.x * fixed_dt;
    projectile.position.y += projectile.velocity.y * fixed_dt;
    projectile.position.z += projectile.velocity.z * fixed_dt;
    projectile.remaining_range -= std::sqrt(combat_distance_squared(
        {0.0f, 0.0f, 0.0f},
        {projectile.velocity.x * fixed_dt, projectile.velocity.y * fixed_dt,
         projectile.velocity.z * fixed_dt}));
    const CombatUnitState* target = unit(projectile.target);
    if (target == nullptr || !target->active || projectile.remaining_range <= 0.0f ||
        !std::isfinite(projectile.remaining_range)) {
      projectile.active = false;
      continue;
    }
    const float hit_radius = target->collision_radius + 0.25f;
    // Projectiles are discrete state, but collision is a swept test.  A fast
    // round must not tunnel through a target between two fixed ticks.
    const CombatVector segment{
        projectile.position.x - previous_position.x,
        projectile.position.y - previous_position.y,
        projectile.position.z - previous_position.z};
    const CombatVector to_target{
        target->position.x - previous_position.x,
        target->position.y - previous_position.y,
        target->position.z - previous_position.z};
    const float segment_length_squared =
        segment.x * segment.x + segment.y * segment.y + segment.z * segment.z;
    float segment_fraction = 0.0f;
    if (segment_length_squared > 0.0f && std::isfinite(segment_length_squared)) {
      segment_fraction = (to_target.x * segment.x + to_target.y * segment.y +
                          to_target.z * segment.z) / segment_length_squared;
      segment_fraction = std::clamp(segment_fraction, 0.0f, 1.0f);
    }
    const CombatVector closest{
        previous_position.x + segment.x * segment_fraction,
        previous_position.y + segment.y * segment_fraction,
        previous_position.z + segment.z * segment_fraction};
    if (combat_distance_squared(closest, target->position) <= hit_radius * hit_radius) {
      (void)apply_damage(projectile.target, projectile.damage);
      projectile.active = false;
    }
  }
}

const CombatUnitState* CombatWorld::unit(EntityId entity) const noexcept {
  for (const auto& candidate : units_) {
    if (candidate.entity == entity) return &candidate;
  }
  return nullptr;
}

std::vector<CombatUnitState> CombatWorld::snapshot_units() const {
  std::vector<CombatUnitState> result = units_;
  std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
    return left.entity < right.entity;
  });
  return result;
}

bool CombatWorld::restore_units(const std::vector<CombatUnitState>& units) noexcept {
  if (units.size() > 4096) return false;
  std::vector<CombatUnitState> loaded;
  loaded.reserve(units.size());
  EntityId previous = 0;
  for (const CombatUnitState& candidate : units) {
    if (!candidate.valid() || candidate.entity <= previous) return false;
    loaded.push_back(candidate);
    previous = candidate.entity;
  }
  units_ = std::move(loaded);
  projectiles_.clear();
  locks_.clear();
  return true;
}

std::size_t CombatWorld::active_units() const noexcept {
  return static_cast<std::size_t>(std::count_if(units_.begin(), units_.end(),
      [](const auto& unit) { return unit.active && unit.health > 0.0f; }));
}

std::size_t CombatWorld::active_projectiles() const noexcept {
  return static_cast<std::size_t>(std::count_if(projectiles_.begin(), projectiles_.end(),
      [](const auto& projectile) { return projectile.active; }));
}

void CombatWorld::clear() noexcept {
  units_.clear();
  weapons_.clear();
  projectiles_.clear();
  locks_.clear();
  next_projectile_id_ = 1;
  damage_events_ = 0;
}

bool MissionWaveSpawn::valid() const noexcept {
  return mission_id != 0 && spawn_tick != 0 && unit.id != 0 && unit.owner != 0 &&
         unit.asset != 0 && combat.entity == unit.id && combat.faction == unit.owner &&
         combat.active && combat.valid();
}

bool MissionWaveDirector::add(MissionWaveSpawn spawn) {
  if (!spawn.valid()) return false;
  for (const Entry& entry : entries_) {
    if (entry.spawn.mission_id == spawn.mission_id && entry.spawn.unit.id == spawn.unit.id) {
      return false;
    }
  }
  entries_.push_back({std::move(spawn), false});
  std::sort(entries_.begin(), entries_.end(), [](const Entry& left, const Entry& right) {
    if (left.spawn.mission_id != right.spawn.mission_id) {
      return left.spawn.mission_id < right.spawn.mission_id;
    }
    if (left.spawn.spawn_tick != right.spawn.spawn_tick) {
      return left.spawn.spawn_tick < right.spawn.spawn_tick;
    }
    return left.spawn.unit.id < right.spawn.unit.id;
  });
  return true;
}

bool MissionWaveDirector::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionWaveDirector loaded;
  bool has_entry = false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string_view, 12> fields{};
    if (!parse_tsv_fields(std::string_view(line), fields)) return false;
    MissionWaveSpawn spawn;
    std::uint32_t unit_id = 0;
    std::uint32_t owner = 0;
    AssetId asset = 0;
    std::uint32_t faction = 0;
    if (!detail::parse_u32(fields[0], spawn.mission_id) ||
        !detail::parse_u64(fields[1], spawn.spawn_tick) || !detail::parse_u32(fields[2], unit_id) ||
        !detail::parse_u32(fields[3], owner) || !detail::parse_u32(fields[4], asset) ||
        !detail::parse_u32(fields[5], faction) || !detail::parse_f32(fields[6], spawn.combat.position.x) ||
        !detail::parse_f32(fields[7], spawn.combat.position.y) ||
        !detail::parse_f32(fields[8], spawn.combat.position.z) ||
        !detail::parse_f32(fields[9], spawn.combat.health) ||
        !detail::parse_f32(fields[10], spawn.combat.max_health) ||
        !detail::parse_f32(fields[11], spawn.combat.collision_radius)) return false;
    spawn.unit = {unit_id, owner, asset, false};
    spawn.combat.entity = unit_id;
    spawn.combat.faction = faction;
    spawn.combat.active = true;
    if (!loaded.add(std::move(spawn))) return false;
    has_entry = true;
  }
  if (!has_entry) return false;
  entries_ = std::move(loaded.entries_);
  return true;
}

MissionWaveSnapshot MissionWaveDirector::snapshot() const {
  MissionWaveSnapshot result;
  result.entries.reserve(entries_.size());
  for (const Entry& entry : entries_) {
    result.entries.push_back({entry.spawn, entry.published});
  }
  return result;
}

bool MissionWaveDirector::restore(const MissionWaveSnapshot& snapshot) noexcept {
  if (snapshot.entries.size() > 4096) return false;
  MissionWaveDirector loaded;
  for (std::size_t index = 0; index < snapshot.entries.size(); ++index) {
    const MissionWaveEntrySnapshot& candidate = snapshot.entries[index];
    if (!candidate.spawn.valid() ||
        (index != 0 &&
         (candidate.spawn.mission_id < snapshot.entries[index - 1].spawn.mission_id ||
          (candidate.spawn.mission_id == snapshot.entries[index - 1].spawn.mission_id &&
           (candidate.spawn.spawn_tick < snapshot.entries[index - 1].spawn.spawn_tick ||
            (candidate.spawn.spawn_tick == snapshot.entries[index - 1].spawn.spawn_tick &&
             candidate.spawn.unit.id <= snapshot.entries[index - 1].spawn.unit.id)))))) {
      return false;
    }
    for (const Entry& existing : loaded.entries_) {
      if (existing.spawn.mission_id == candidate.spawn.mission_id &&
          existing.spawn.unit.id == candidate.spawn.unit.id) return false;
    }
    loaded.entries_.push_back({candidate.spawn, candidate.published});
  }
  entries_ = std::move(loaded.entries_);
  return true;
}

bool MissionWaveDirector::spawn_due(std::uint32_t mission_id, std::uint64_t tick,
                                    UnitRegistry& units, CombatWorld& combat) noexcept {
  UnitRegistry staged_units = units;
  CombatWorld staged_combat = combat;
  std::vector<std::size_t> due;
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    const Entry& entry = entries_[index];
    if (entry.spawn.mission_id == mission_id && !entry.published &&
        entry.spawn.spawn_tick <= tick) due.push_back(index);
  }
  for (const std::size_t index : due) {
    const MissionWaveSpawn& spawn = entries_[index].spawn;
    if (!staged_units.register_unit(spawn.unit) || !staged_units.activate(spawn.unit.id) ||
        !staged_combat.add_unit(spawn.combat)) return false;
  }
  units = std::move(staged_units);
  combat = std::move(staged_combat);
  for (const std::size_t index : due) entries_[index].published = true;
  return true;
}

bool MissionWaveDirector::despawn(EntityId entity, UnitRegistry& units,
                                  CombatWorld& combat) noexcept {
  UnitRegistry staged_units = units;
  CombatWorld staged_combat = combat;
  if (!staged_units.deactivate(entity) || !staged_combat.deactivate_unit(entity)) return false;
  units = std::move(staged_units);
  combat = std::move(staged_combat);
  return true;
}

std::size_t MissionWaveDirector::pending(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.spawn.mission_id == mission_id && !entry.published;
      }));
}

std::size_t MissionWaveDirector::spawned(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.spawn.mission_id == mission_id && entry.published;
      }));
}

void MissionWaveDirector::reset() noexcept {
  for (Entry& entry : entries_) entry.published = false;
}

bool MissionAiDirector::add(MissionAiRule rule) {
  if (!rule.valid() || std::find(rules_.begin(), rules_.end(), rule) != rules_.end()) return false;
  rules_.push_back(rule);
  return true;
}

bool MissionAiDirector::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionAiDirector loaded;
  bool has_rule = false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string_view, 6> fields{};
    if (!parse_tsv_fields(std::string_view(line), fields)) return false;
    MissionAiRule rule;
    if (!detail::parse_u32(fields[0], rule.mission_id) || !detail::parse_u64(fields[1], rule.first_tick) ||
        !detail::parse_u64(fields[2], rule.period_ticks) || !detail::parse_u32(fields[3], rule.entity) ||
        !detail::parse_u32(fields[4], rule.target) || !detail::parse_u32(fields[5], rule.weapon_id) ||
        !loaded.add(rule)) return false;
    has_rule = true;
  }
  if (!has_rule) return false;
  rules_ = std::move(loaded.rules_);
  return true;
}

bool MissionAiDirector::dispatch_due(std::uint32_t mission_id, std::uint64_t tick,
                                     CombatWorld& combat) noexcept {
  for (const MissionAiRule& rule : rules_) {
    if (rule.mission_id != mission_id || tick < rule.first_tick ||
        (tick - rule.first_tick) % rule.period_ticks != 0) continue;
    const CombatUnitState* source = combat.unit(rule.entity);
    const CombatUnitState* target = combat.unit(rule.target);
    if (source == nullptr || target == nullptr || !source->active || !target->active) continue;
    if (!combat.lock_target(rule.entity, rule.target)) return false;
    (void)combat.fire(rule.entity, rule.weapon_id);
  }
  return true;
}

std::size_t MissionAiDirector::active(std::uint32_t mission_id, std::uint64_t tick) const noexcept {
  return static_cast<std::size_t>(std::count_if(rules_.begin(), rules_.end(),
      [mission_id, tick](const MissionAiRule& rule) {
        return rule.mission_id == mission_id && tick >= rule.first_tick &&
               (tick - rule.first_tick) % rule.period_ticks == 0;
      }));
}

}  // namespace ac6
