#include "ac6/product_runtime.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>
#include <unordered_set>

namespace ac6 {

namespace {
bool parse_u32(std::string_view text, std::uint32_t& value) noexcept;
bool parse_bool01(std::string_view text, bool& value) noexcept;
std::uint16_t read_le_u16(const unsigned char* bytes) noexcept;
std::uint32_t read_le_u32(const unsigned char* bytes) noexcept;
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
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    if (first == std::string::npos || second == std::string::npos || third == std::string::npos ||
        line.find('\t', third + 1) != std::string::npos) return false;
    MissionObjectiveDefinition definition;
    bool required = false;
    if (!parse_u32(std::string_view(line).substr(0, first), definition.mission_id) ||
        !parse_u32(std::string_view(line).substr(first + 1, second - first - 1), definition.objective.id) ||
        (definition.objective.stable_id = line.substr(second + 1, third - second - 1)).empty() ||
        !parse_bool01(std::string_view(line).substr(third + 1), required)) return false;
    definition.objective.required = required;
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
    if (!parse_u32(std::string_view(line).substr(0, tabs[0]), message.mission_id) ||
        !parse_u32(std::string_view(line).substr(tabs[0] + 1, tabs[1] - tabs[0] - 1), message.id) ||
        (message.stable_id = line.substr(tabs[1] + 1, tabs[2] - tabs[1] - 1)).empty() ||
        (message.speaker = line.substr(tabs[2] + 1, tabs[3] - tabs[2] - 1)).empty() ||
        !parse_u32(std::string_view(line).substr(tabs[3] + 1, tabs[4] - tabs[3] - 1), message.audio_asset) ||
        !parse_u32(std::string_view(line).substr(tabs[4] + 1), message.subtitle_asset) ||
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
    if (!parse_u32(view.substr(0, tab), mask) || mask > 0xFFFFu) return false;
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
    if (combat_distance_squared(projectile.position, target->position) <= hit_radius * hit_radius) {
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
         combat.valid();
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

namespace {

bool parse_u32(std::string_view text, std::uint32_t& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_u64(std::string_view text, std::uint64_t& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_f32(std::string_view text, float& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
         std::isfinite(value);
}

bool parse_bool01(std::string_view text, bool& value) noexcept {
  if (text == "0") {
    value = false;
    return true;
  }
  if (text == "1") {
    value = true;
    return true;
  }
  return false;
}

bool parse_hex_u32(std::string_view text, std::uint32_t& value) noexcept {
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    text.remove_prefix(2);
  }
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_hex_u64(std::string_view text, std::uint64_t& value) noexcept {
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    text.remove_prefix(2);
  }
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

std::uint32_t stable_hash32(std::string_view text) noexcept {
  std::uint32_t hash = 2166136261u;
  for (const char byte : text) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 16777619u;
  }
  return hash;
}

bool file_fnv64(const std::filesystem::path& path, std::uint64_t& hash) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::array<char, 64 * 1024> bytes{};
  std::uint64_t value = 1469598103934665603ull;
  while (input) {
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const std::streamsize count = input.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      value ^= static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);
      value *= 1099511628211ull;
    }
  }
  if (!input.eof()) return false;
  hash = value;
  return true;
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

float read_le_f32(const unsigned char* bytes) noexcept {
  const std::uint32_t raw = read_le_u32(bytes);
  float value = 0.0f;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

bool read_exact_at(std::ifstream& input, std::uint64_t offset, unsigned char* bytes,
                   std::size_t size) {
  if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
    return false;
  }
  input.clear();
  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!input) return false;
  input.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(size));
  return input.good();
}

struct Vec3 {
  float x{};
  float y{};
  float z{};
};

struct ScreenPoint {
  std::uint32_t x{};
  std::uint32_t y{};
  float depth{};
};

float dot(Vec3 a, Vec3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

bool normalize(Vec3& value) noexcept {
  const float length_squared = dot(value, value);
  if (!std::isfinite(length_squared) || length_squared <= 0.000001f) return false;
  const float inverse_length = 1.0f / std::sqrt(length_squared);
  value.x *= inverse_length;
  value.y *= inverse_length;
  value.z *= inverse_length;
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct NativeCameraProjection {
  Vec3 origin;
  Vec3 right;
  Vec3 up;
  Vec3 forward;
  float aspect{};
  float focal{};
};

bool make_projection(const WorldFrame& frame, std::uint32_t width, std::uint32_t height,
                     NativeCameraProjection& projection) noexcept {
  if (width == 0 || height == 0 ||
      !std::isfinite(frame.camera_x) || !std::isfinite(frame.camera_y) ||
      !std::isfinite(frame.camera_z) || !std::isfinite(frame.camera_target_x) ||
      !std::isfinite(frame.camera_target_y) || !std::isfinite(frame.camera_target_z)) {
    return false;
  }
  projection.origin = {frame.camera_x, frame.camera_y, frame.camera_z};
  projection.forward = {frame.camera_target_x - frame.camera_x,
                        frame.camera_target_y - frame.camera_y,
                        frame.camera_target_z - frame.camera_z};
  if (!normalize(projection.forward)) return false;
  projection.right = cross(projection.forward, {0.0f, 1.0f, 0.0f});
  if (!normalize(projection.right)) {
    projection.right = cross(projection.forward, {0.0f, 0.0f, 1.0f});
    if (!normalize(projection.right)) return false;
  }
  projection.up = cross(projection.right, projection.forward);
  if (!normalize(projection.up)) return false;
  projection.aspect = static_cast<float>(width) / static_cast<float>(height);
  projection.focal = 1.7320508075688772f;
  return std::isfinite(projection.aspect) && projection.aspect > 0.0f;
}

bool project_clip_point(const MissionCameraDefinition& camera, Vec3 world,
                        std::uint32_t width, std::uint32_t height,
                        ScreenPoint& screen, bool clip_to_viewport = true) noexcept {
  const auto& m = camera.clip_rows;
  const float x = camera.column_major
      ? m[0] * world.x + m[4] * world.y + m[8] * world.z + m[12]
      : m[0] * world.x + m[1] * world.y + m[2] * world.z + m[3];
  const float y = camera.column_major
      ? m[1] * world.x + m[5] * world.y + m[9] * world.z + m[13]
      : m[4] * world.x + m[5] * world.y + m[6] * world.z + m[7];
  const float z = camera.column_major
      ? m[2] * world.x + m[6] * world.y + m[10] * world.z + m[14]
      : m[8] * world.x + m[9] * world.y + m[10] * world.z + m[11];
  const float w = camera.column_major
      ? m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15]
      : m[12] * world.x + m[13] * world.y + m[14] * world.z + m[15];
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      !std::isfinite(w) || w <= 0.000001f) return false;
  const float ndc_x = x / w;
  const float ndc_y = y / w;
  const float ndc_z = z / w;
  if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) || !std::isfinite(ndc_z)) return false;
  if (clip_to_viewport && (ndc_x < -1.0f || ndc_x > 1.0f || ndc_y < -1.0f || ndc_y > 1.0f ||
                           ndc_z < -1.0f || ndc_z > 1.0f)) return false;
  const float safe_x = std::clamp(ndc_x, -1.0f, 1.0f);
  const float safe_y = std::clamp(ndc_y, -1.0f, 1.0f);
  screen.x = std::min(width - 1u, static_cast<std::uint32_t>((safe_x * 0.5f + 0.5f) * width));
  screen.y = std::min(height - 1u, static_cast<std::uint32_t>((0.5f - safe_y * 0.5f) * height));
  // c218–c221 are Xenos-style homogeneous rows (depth range -1..1),
  // whereas the native depth plane is normalized to [0,1].
  screen.depth = ndc_z * 0.5f + 0.5f;
  return true;
}

bool project_point(const NativeCameraProjection& projection, Vec3 world,
                   std::uint32_t width, std::uint32_t height,
                   ScreenPoint& screen, bool clip_to_viewport = true) noexcept {
  const Vec3 relative{world.x - projection.origin.x, world.y - projection.origin.y,
                      world.z - projection.origin.z};
  const float view_x = dot(relative, projection.right);
  const float view_y = dot(relative, projection.up);
  const float view_z = dot(relative, projection.forward);
  constexpr float near_plane = 0.1f;
  if (!std::isfinite(view_x) || !std::isfinite(view_y) || !std::isfinite(view_z) ||
      view_z <= near_plane) {
    return false;
  }
  const float ndc_x = (view_x * projection.focal) / (view_z * projection.aspect);
  const float ndc_y = (view_y * projection.focal) / view_z;
  if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) ||
      (clip_to_viewport && (ndc_x < -1.0f || ndc_x > 1.0f || ndc_y < -1.0f || ndc_y > 1.0f))) {
    return false;
  }
  const float safe_x = std::clamp(ndc_x, -1.0f, 1.0f);
  const float safe_y = std::clamp(ndc_y, -1.0f, 1.0f);
  screen.x = std::min(width - 1u,
                      static_cast<std::uint32_t>((safe_x * 0.5f + 0.5f) *
                                                 static_cast<float>(width)));
  screen.y = std::min(height - 1u,
                      static_cast<std::uint32_t>((0.5f - safe_y * 0.5f) *
                                                 static_cast<float>(height)));
  screen.depth = view_z;
  return true;
}

MissionFamily parse_family(std::string_view family) noexcept {
  if (family == "air_intercept") return MissionFamily::AirIntercept;
  if (family == "strike") return MissionFamily::Strike;
  if (family == "escort") return MissionFamily::Escort;
  return MissionFamily::Unknown;
}

bool parse_asset_ids(std::string_view text, std::vector<AssetId>& asset_ids) {
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    AssetId id{};
    if (token.empty() || !parse_u32(token, id) || id == 0 ||
        std::find(asset_ids.begin(), asset_ids.end(), id) != asset_ids.end()) {
      return false;
    }
    asset_ids.push_back(id);
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return !asset_ids.empty();
}

bool parse_units(std::string_view text, std::vector<UnitRecord>& units) {
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    const auto first = token.find(':');
    const auto second = first == std::string_view::npos ? std::string_view::npos :
        token.find(':', first + 1);
    if (first == std::string_view::npos || second == std::string_view::npos ||
        token.find(':', second + 1) != std::string_view::npos) {
      return false;
    }
    UnitRecord unit;
    if (!parse_u32(token.substr(0, first), unit.id) ||
        !parse_u32(token.substr(first + 1, second - first - 1), unit.owner) ||
        !parse_u32(token.substr(second + 1), unit.asset)) {
      return false;
    }
    unit.active = false;
    if (unit.id == 0 || unit.asset == 0 || unit.owner == unit.id ||
        std::find_if(units.begin(), units.end(), [unit](const UnitRecord& existing) {
          return existing.id == unit.id;
        }) != units.end()) {
      return false;
    }
    units.push_back(unit);
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return !units.empty();
}

}  // namespace

bool MissionCatalog::add(MissionDefinition definition) {
  if (definition.id == 0 || definition.family == MissionFamily::Unknown ||
      definition.asset_ids.empty()) return false;
  for (std::size_t i = 0; i < definition.asset_ids.size(); ++i) {
    if (definition.asset_ids[i] == 0) return false;
    if (std::find(definition.asset_ids.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                  definition.asset_ids.end(), definition.asset_ids[i]) != definition.asset_ids.end()) {
      return false;
    }
  }
  return missions_.emplace(definition.id, std::move(definition)).second;
}

bool MissionCatalog::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionCatalog loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        line.find('\t', second + 1) != std::string::npos) {
      return false;
    }
    std::uint32_t mission_id{};
    const auto id_text = std::string_view(line).substr(0, first);
    if (!parse_u32(id_text, mission_id)) return false;
    std::vector<AssetId> asset_ids;
    const auto assets_text = std::string_view(line).substr(second + 1);
    if (!parse_asset_ids(assets_text, asset_ids)) return false;
    if (!loaded.add({mission_id, parse_family(std::string_view(line).substr(first + 1, second - first - 1)),
                     std::move(asset_ids)})) {
      return false;
    }
  }
  missions_ = std::move(loaded.missions_);
  return true;
}

const MissionDefinition* MissionCatalog::find(std::uint32_t id) const noexcept {
  const auto it = missions_.find(id);
  return it == missions_.end() ? nullptr : &it->second;
}

MissionScenario::MissionScenario(const MissionDefinition& definition) : mission_id_(definition.id) {}

bool MissionScenario::bind_player(const UnitRegistry& units, EntityId entity) noexcept {
  const UnitRecord* unit = units.find(entity);
  if (unit == nullptr || !unit->active) return false;
  player_ = entity;
  return true;
}

bool MissionScenario::dispatch(Event event) noexcept {
  switch (event.type) {
    case EventType::StartMission:
      if (state_ != ScenarioState::Loading && state_ != ScenarioState::Briefing) return false;
      if (player_ != 0 && event.subject != player_) return false;
      state_ = ScenarioState::Gameplay;
      return true;
    case EventType::Pause:
      if (state_ != ScenarioState::Gameplay) return false;
      state_ = ScenarioState::Paused;
      return true;
    case EventType::Resume:
      if (state_ != ScenarioState::Paused) return false;
      state_ = ScenarioState::Gameplay;
      return true;
    case EventType::Complete:
      if (state_ != ScenarioState::Gameplay && state_ != ScenarioState::Paused) return false;
      if (!objectives_.all_required_complete()) return false;
      state_ = ScenarioState::Complete;
      return true;
    case EventType::Abort:
      if (state_ == ScenarioState::Complete || state_ == ScenarioState::Aborted) return false;
      state_ = ScenarioState::Aborted;
      return true;
  }
  return false;
}

bool MissionScenario::complete_objective(std::uint32_t id) noexcept {
  return objectives_.complete(id);
}

bool MissionScenario::add_objective(ObjectiveRecord objective) {
  return objectives_.add(std::move(objective));
}

bool MissionScenario::activate_objective(std::uint32_t id) noexcept {
  return objectives_.activate(id);
}

bool MissionScenario::fail_objective(std::uint32_t id) noexcept {
  if (!objectives_.fail(id)) return false;
  state_ = ScenarioState::Aborted;
  return true;
}

bool MissionScenario::dispatch_radio(const RadioMessageDatabase& messages,
                                     std::uint32_t id) noexcept {
  if (state_ != ScenarioState::Gameplay && state_ != ScenarioState::Paused) return false;
  if (messages.find(mission_id_, id) == nullptr) return false;
  radio_history_.push_back(id);
  return true;
}

std::optional<std::uint32_t> MissionScenario::objective_index(std::uint32_t id) const noexcept {
  const std::vector<ObjectiveRecord> records = objectives_.snapshot();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (records[index].id == id) return static_cast<std::uint32_t>(index);
  }
  return std::nullopt;
}

MissionDebrief MissionScenario::debrief() const {
  MissionDebrief result;
  result.mission_id = mission_id_;
  result.outcome = state_ == ScenarioState::Complete
                       ? MissionOutcome::Success
                       : (state_ == ScenarioState::Aborted ? MissionOutcome::Failure
                                                            : MissionOutcome::InProgress);
  result.objective_count = static_cast<std::uint32_t>(objectives_.size());
  result.completed_objectives = static_cast<std::uint32_t>(objectives_.completed_count());
  result.failed_objectives = static_cast<std::uint32_t>(objectives_.failed_count());
  result.radio_history = radio_history_;
  return result;
}

MissionScenarioSnapshot MissionScenario::snapshot() const {
  return {mission_id_, state_, player_, objectives_.snapshot(), radio_history_};
}

bool MissionScenario::restore(const MissionScenarioSnapshot& snapshot) noexcept {
  if (snapshot.mission_id != mission_id_ ||
      static_cast<std::uint8_t>(snapshot.state) >
          static_cast<std::uint8_t>(ScenarioState::Aborted) ||
      snapshot.radio_history.size() > 65536) return false;
  for (const std::uint32_t message : snapshot.radio_history) {
    if (message == 0) return false;
  }
  ObjectiveRegistry loaded;
  if (!loaded.restore(snapshot.objectives)) return false;
  objectives_ = std::move(loaded);
  state_ = snapshot.state;
  player_ = snapshot.player;
  radio_history_ = snapshot.radio_history;
  return true;
}

bool MissionScenario::dispatch_buttons(const InputMappingDatabase& mappings,
                                       std::uint16_t buttons, EntityId subject) noexcept {
  const InputBinding* binding = mappings.resolve(buttons);
  return binding != nullptr && dispatch({binding->event, subject});
}

bool FrontendController::advance() noexcept {
  switch (state_) {
    case FrontendState::Title: state_ = FrontendState::NewGame; break;
    case FrontendState::NewGame:
      if (campaign_ != nullptr &&
          (selected_mission_ == 0 || !campaign_->enter_briefing(selected_mission_))) return false;
      state_ = FrontendState::Briefing;
      break;
    case FrontendState::Briefing: state_ = FrontendState::Hangar; break;
    case FrontendState::Hangar:
      if (campaign_ != nullptr &&
          (selected_mission_ == 0 || !campaign_->begin(selected_mission_))) return false;
      state_ = FrontendState::Loading;
      break;
    case FrontendState::Loading: state_ = FrontendState::Mission; break;
    case FrontendState::Mission: return false;
  }
  return true;
}

bool FrontendController::configure(FrontendSettings settings) noexcept {
  if (state_ != FrontendState::Title || !settings.valid()) return false;
  settings_ = settings;
  return true;
}

bool FrontendController::dispatch(Event event) noexcept {
  switch (event.type) {
    case EventType::StartMission:
      return advance();
    case EventType::Abort:
      state_ = FrontendState::Title;
      selected_mission_ = 0;
      return true;
    default:
      return false;
  }
}

bool FrontendController::dispatch_buttons(const InputMappingDatabase& mappings,
                                          std::uint16_t buttons) noexcept {
  const InputBinding* binding = mappings.resolve(buttons);
  return binding != nullptr && dispatch({binding->event, 0});
}

bool FrontendController::select_mission(const MissionCatalog& catalog,
                                        std::uint32_t mission_id) noexcept {
  if (catalog.find(mission_id) == nullptr ||
      (campaign_ != nullptr && !campaign_->is_available(mission_id))) return false;
  selected_mission_ = mission_id;
  return true;
}

bool FrontendController::set_loadout(CampaignLoadout loadout) noexcept {
  return campaign_ != nullptr && state_ == FrontendState::Hangar && selected_mission_ != 0 &&
         campaign_->set_loadout(selected_mission_, loadout);
}

const MissionDefinition* FrontendController::mission_definition(
    const MissionCatalog& catalog) const noexcept {
  if (state_ != FrontendState::Mission || selected_mission_ == 0) return nullptr;
  return catalog.find(selected_mission_);
}

bool FrontendController::launch_selected(const MissionCatalog& catalog,
                                         const MissionLaunchDatabase& launches,
                                         MissionExecution& execution) const noexcept {
  if (state_ != FrontendState::Mission || selected_mission_ == 0 || execution.launched()) {
    return false;
  }
  const MissionDefinition* definition = catalog.find(selected_mission_);
  const MissionLaunchDefinition* launch = launches.find(selected_mission_);
  return definition != nullptr && launch != nullptr && definition->id == selected_mission_ &&
         execution.launch(*launch);
}

bool FrontendController::enter_debrief(const MissionExecution& execution) noexcept {
  if (state_ != FrontendState::Mission || selected_mission_ == 0 || !execution.launched()) {
    return false;
  }
  const MissionDebrief result = execution.debrief();
  if (result.mission_id != selected_mission_ || result.outcome == MissionOutcome::InProgress) {
    return false;
  }
  if (campaign_ != nullptr) {
    const CampaignMissionStatus* status = campaign_->status(selected_mission_);
    const CampaignMissionState expected = result.outcome == MissionOutcome::Success
        ? CampaignMissionState::Completed : CampaignMissionState::Failed;
    if (status == nullptr || status->state != expected) return false;
  }
  debrief_ = result;
  state_ = FrontendState::Debrief;
  return true;
}

bool FrontendController::return_to_campaign() noexcept {
  if (state_ != FrontendState::Debrief || !debrief_.has_value()) return false;
  state_ = FrontendState::NewGame;
  selected_mission_ = 0;
  debrief_.reset();
  return true;
}

bool SaveStore::save(std::uint32_t slot, RuntimeSnapshot snapshot) {
  if (slot == 0 || snapshot.tick == 0 || !std::isfinite(snapshot.position_x) ||
      !std::isfinite(snapshot.position_y) || !std::isfinite(snapshot.position_z) ||
      !std::isfinite(snapshot.pitch) || !std::isfinite(snapshot.roll) ||
      !std::isfinite(snapshot.yaw) || !std::isfinite(snapshot.fixed_accumulator) ||
      snapshot.fixed_accumulator < 0.0f || snapshot.fixed_accumulator >= 1.0f / 60.0f) return false;
  slots_[slot] = snapshot;
  return true;
}

const RuntimeSnapshot* SaveStore::load(std::uint32_t slot) const noexcept {
  const auto it = slots_.find(slot);
  return it == slots_.end() ? nullptr : &it->second;
}

bool SaveStore::write_file(const std::filesystem::path& path) const {
  if (path.empty() || slots_.size() > 1024u) return false;
  std::vector<std::uint32_t> slot_ids;
  slot_ids.reserve(slots_.size());
  for (const auto& [slot, snapshot] : slots_) {
    (void)snapshot;
    if (slot == 0) return false;
    slot_ids.push_back(slot);
  }
  std::sort(slot_ids.begin(), slot_ids.end());
  std::filesystem::path temporary = path;
  temporary += ".tmp";
  std::error_code cleanup_error;
  std::filesystem::remove(temporary, cleanup_error);
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) {
    std::filesystem::remove(temporary, cleanup_error);
    return false;
  }
  const auto write_u32 = [&output](std::uint32_t value) {
    const unsigned char bytes[4] = {static_cast<unsigned char>(value & 0xffu),
                                    static_cast<unsigned char>((value >> 8u) & 0xffu),
                                    static_cast<unsigned char>((value >> 16u) & 0xffu),
                                    static_cast<unsigned char>((value >> 24u) & 0xffu)};
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
  };
  const auto write_u64 = [&output](std::uint64_t value) {
    unsigned char bytes[8]{};
    for (std::size_t i = 0; i < sizeof(bytes); ++i) bytes[i] =
        static_cast<unsigned char>((value >> (i * 8u)) & 0xffu);
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
  };
  output.write("AC6SAVE\0", 8);
  write_u32(3u);
  write_u32(static_cast<std::uint32_t>(slot_ids.size()));
  for (const std::uint32_t slot : slot_ids) {
    const RuntimeSnapshot& snapshot = slots_.at(slot);
    write_u32(slot);
    write_u64(snapshot.tick);
    std::uint32_t raw{};
    std::memcpy(&raw, &snapshot.position_x, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.position_y, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.position_z, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.pitch, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.roll, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.yaw, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.fixed_accumulator, sizeof(raw)); write_u32(raw);
  }
  const bool written = static_cast<bool>(output);
  output.close();
  if (!written) {
    std::filesystem::remove(temporary, cleanup_error);
    return false;
  }
  std::error_code rename_error;
  std::filesystem::rename(temporary, path, rename_error);
  if (rename_error) {
    std::filesystem::remove(temporary, cleanup_error);
    return false;
  }
  return true;
}

bool SaveStore::read_file(const std::filesystem::path& path) {
  if (path.empty()) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  input.read(magic, sizeof(magic));
  if (!input || std::memcmp(magic, "AC6SAVE\0", 8) != 0) return false;
  unsigned char header[8]{};
  input.read(reinterpret_cast<char*>(header), sizeof(header));
  if (!input || (read_le_u32(header) != 1u && read_le_u32(header) != 2u &&
                 read_le_u32(header) != 3u) ||
      read_le_u32(header + 4) > 1024u) return false;
  const std::uint32_t version = read_le_u32(header);
  const std::uint32_t count = read_le_u32(header + 4);
  std::unordered_map<std::uint32_t, RuntimeSnapshot> loaded;
  for (std::uint32_t i = 0; i < count; ++i) {
    constexpr std::size_t kV1RecordSize = 24;
    constexpr std::size_t kV2RecordSize = 36;
    constexpr std::size_t kV3RecordSize = 40;
    std::array<unsigned char, kV3RecordSize> record{};
    const std::size_t record_size = version == 1u ? kV1RecordSize :
                                    (version == 2u ? kV2RecordSize : kV3RecordSize);
    input.read(reinterpret_cast<char*>(record.data()), static_cast<std::streamsize>(record_size));
    if (!input) return false;
    const std::uint32_t slot = read_le_u32(record.data());
    if (slot == 0 || loaded.find(slot) != loaded.end()) return false;
    const std::uint64_t tick = [&record]() {
      std::uint64_t value = 0;
      for (std::size_t j = 0; j < 8; ++j) value |= static_cast<std::uint64_t>(record[4 + j]) << (j * 8u);
      return value;
    }();
    RuntimeSnapshot snapshot{tick, 0.0f, 0.0f, 0.0f};
    const auto read_float = [&record](std::size_t offset, float& value) {
      const std::uint32_t raw = read_le_u32(record.data() + offset);
      std::memcpy(&value, &raw, sizeof(value));
      return std::isfinite(value);
    };
    if (!read_float(12, snapshot.position_x) || !read_float(16, snapshot.position_y) ||
        !read_float(20, snapshot.position_z) || snapshot.tick == 0) return false;
    if (version == 2u && (!read_float(24, snapshot.pitch) || !read_float(28, snapshot.roll) ||
                          !read_float(32, snapshot.yaw))) return false;
    if (version == 3u && (!read_float(24, snapshot.pitch) || !read_float(28, snapshot.roll) ||
                          !read_float(32, snapshot.yaw) || !read_float(36, snapshot.fixed_accumulator) ||
                          snapshot.fixed_accumulator < 0.0f || snapshot.fixed_accumulator >= 1.0f / 60.0f)) return false;
    loaded.emplace(slot, snapshot);
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof()) return false;
  slots_ = std::move(loaded);
  return true;
}

bool MissionAssetDatabase::add(AssetRecord record) {
  if (record.id == 0 || record.relative_path.empty() || record.sha256.empty()) {
    return false;
  }
  return records_.emplace(record.id, std::move(record)).second;
}

bool MissionAssetDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionAssetDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    if (first == std::string::npos || second == std::string::npos) return false;
    AssetId id{};
    const auto number = std::string_view(line).substr(0, first);
    if (!parse_u32(number, id) ||
        !loaded.add({id, line.substr(first + 1, second - first - 1), line.substr(second + 1)})) {
      return false;
    }
  }
  records_ = std::move(loaded.records_);
  return true;
}

bool MissionAssetDatabase::load_qualified_manifest(const std::filesystem::path& manifest) {
  MissionAssetDatabase loaded;
  if (!loaded.load_manifest(manifest)) return false;
  for (const auto& [id, record] : loaded.records_) {
    (void)id;
    if (record.sha256.size() != 64 ||
        !std::all_of(record.sha256.begin(), record.sha256.end(), [](char value) {
          return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
                 (value >= 'A' && value <= 'F');
        })) {
      return false;
    }
  }
  records_ = std::move(loaded.records_);
  return true;
}

const AssetRecord* MissionAssetDatabase::resolve(AssetId id) const noexcept {
  const auto it = records_.find(id);
  return it == records_.end() ? nullptr : &it->second;
}

bool MissionLaunchDatabase::add(MissionLaunchDefinition definition) {
  if (definition.mission_id == 0 || definition.player_entity == 0 || definition.units.empty()) {
    return false;
  }
  bool has_player = false;
  for (std::size_t i = 0; i < definition.units.size(); ++i) {
    const UnitRecord& unit = definition.units[i];
    if (unit.id == 0 || unit.asset == 0 || unit.owner == unit.id) return false;
    if (unit.id == definition.player_entity) has_player = true;
    if (std::find_if(definition.units.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                     definition.units.end(), [unit](const UnitRecord& existing) {
                       return existing.id == unit.id;
                     }) != definition.units.end()) {
      return false;
    }
  }
  if (!has_player) return false;
  return launches_.emplace(definition.mission_id, std::move(definition)).second;
}

bool MissionLaunchDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionLaunchDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        line.find('\t', second + 1) != std::string::npos) {
      return false;
    }
    MissionLaunchDefinition definition;
    if (!parse_u32(std::string_view(line).substr(0, first), definition.mission_id) ||
        !parse_u32(std::string_view(line).substr(first + 1, second - first - 1),
                   definition.player_entity) ||
        !parse_units(std::string_view(line).substr(second + 1), definition.units) ||
        !loaded.add(std::move(definition))) {
      return false;
    }
  }
  launches_ = std::move(loaded.launches_);
  return true;
}

const MissionLaunchDefinition* MissionLaunchDatabase::find(
    std::uint32_t mission_id) const noexcept {
  const auto it = launches_.find(mission_id);
  return it == launches_.end() ? nullptr : &it->second;
}

bool MissionManifestLoader::load_paths(const std::filesystem::path& manifest,
                                       MissionManifestPaths& paths) const {
  if (manifest.empty()) return false;
  std::ifstream input(manifest);
  if (!input) return false;
  MissionManifestPaths loaded;
  const std::filesystem::path root = manifest.parent_path();
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto tab = line.find('\t');
    if (tab == std::string::npos || line.find('\t', tab + 1) != std::string::npos) return false;
    const std::string key = line.substr(0, tab);
    const std::string value = line.substr(tab + 1);
    if (value.empty()) return false;
    std::filesystem::path resolved(value);
    if (resolved.is_relative()) resolved = root / resolved;
    if (key == "campaign" && loaded.campaign.empty()) loaded.campaign = resolved;
    else if (key == "catalog" && loaded.catalog.empty()) loaded.catalog = resolved;
    else if (key == "assets" && loaded.assets.empty()) loaded.assets = resolved;
    else if (key == "launches" && loaded.launches.empty()) loaded.launches = resolved;
    else if (key == "input" && loaded.input.empty()) loaded.input = resolved;
    else if (key == "controls" && loaded.controls.empty()) loaded.controls = resolved;
    else if (key == "objectives" && loaded.objectives.empty()) loaded.objectives = resolved;
    else if (key == "radios" && loaded.radios.empty()) loaded.radios = resolved;
    else if (key == "render" && loaded.render.empty()) loaded.render = resolved;
    else if (key == "drawables" && loaded.drawables.empty()) loaded.drawables = resolved;
    else if (key == "transforms" && loaded.transforms.empty()) loaded.transforms = resolved;
    else if (key == "materials" && loaded.materials.empty()) loaded.materials = resolved;
    else if (key == "textures" && loaded.textures.empty()) loaded.textures = resolved;
    else if (key == "shaders" && loaded.shaders.empty()) loaded.shaders = resolved;
    else if (key == "targets" && loaded.targets.empty()) loaded.targets = resolved;
    else if (key == "passes" && loaded.passes.empty()) loaded.passes = resolved;
    else if (key == "resolves" && loaded.resolves.empty()) loaded.resolves = resolved;
    else if (key == "buffers" && loaded.buffers.empty()) loaded.buffers = resolved;
    else if (key == "camera" && loaded.camera.empty()) loaded.camera = resolved;
    else return false;
  }
  if (!loaded.valid()) return false;
  paths = std::move(loaded);
  return true;
}

bool MissionManifestLoader::load_campaign(const std::filesystem::path& manifest,
                                           CampaignProgression& campaign) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || paths.campaign.empty()) return false;
  CampaignProgression loaded;
  if (!loaded.load_manifest(paths.campaign)) return false;
  campaign = std::move(loaded);
  return true;
}

bool MissionManifestLoader::load_runtime(const std::filesystem::path& manifest,
                                         MissionCatalog& catalog,
                                         MissionAssetDatabase& assets,
                                         MissionLaunchDatabase& launches) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths)) return false;
  MissionCatalog loaded_catalog;
  MissionAssetDatabase loaded_assets;
  MissionLaunchDatabase loaded_launches;
  if (!loaded_catalog.load_manifest(paths.catalog) ||
      !loaded_assets.load_qualified_manifest(paths.assets) ||
      !loaded_launches.load_manifest(paths.launches)) return false;
  if (!paths.input.empty()) {
    InputMappingDatabase loaded_input;
    if (!loaded_input.load_manifest(paths.input)) return false;
  }
  if (!paths.objectives.empty()) {
    MissionObjectiveDatabase loaded_objectives;
    if (!loaded_objectives.load_manifest(paths.objectives)) return false;
  }
  if (!paths.radios.empty()) {
    RadioMessageDatabase loaded_radios;
    if (!loaded_radios.load_manifest(paths.radios)) return false;
  }
  if (!paths.campaign.empty()) {
    CampaignProgression loaded_campaign;
    if (!loaded_campaign.load_manifest(paths.campaign)) return false;
  }
  catalog = std::move(loaded_catalog);
  assets = std::move(loaded_assets);
  launches = std::move(loaded_launches);
  return true;
}

bool MissionManifestLoader::load_input(const std::filesystem::path& manifest,
                                       InputMappingDatabase& input) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || paths.input.empty()) return false;
  InputMappingDatabase loaded;
  if (!loaded.load_manifest(paths.input)) return false;
  input = std::move(loaded);
  return true;
}

bool MissionCameraDefinition::valid() const noexcept {
  if (mission_id == 0) return false;
  for (const float value : clip_rows) if (!std::isfinite(value)) return false;
  return true;
}

bool MissionCameraDatabase::add(MissionCameraDefinition definition) {
  if (!definition.valid() || find(definition.mission_id) != nullptr) return false;
  cameras_.push_back(std::move(definition));
  return true;
}

bool MissionCameraDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionCameraDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string_view, 20> fields{};
    std::size_t start = 0;
    std::size_t field_count = 0;
    while (field_count < fields.size()) {
      const std::size_t tab = line.find('\t', start);
      if (tab == std::string::npos) {
        fields[field_count++] = std::string_view(line).substr(start);
        break;
      } else {
        fields[field_count++] = std::string_view(line).substr(start, tab - start);
        start = tab + 1;
      }
    }
    if (field_count < 17 || field_count > 19) return false;
    if (field_count == fields.size() && line.find('\t', start) != std::string::npos) return false;
    MissionCameraDefinition definition;
    if (!parse_u32(fields[0], definition.mission_id)) return false;
    for (std::size_t i = 0; i < definition.clip_rows.size(); ++i) {
      if (!parse_f32(fields[i + 1], definition.clip_rows[i])) return false;
    }
    for (std::size_t field = 17; field < field_count; ++field) {
      if (fields[field] == "qualified") definition.qualified = true;
      else if (fields[field] == "column_major") definition.column_major = true;
      else return false;
    }
    if (!loaded.add(std::move(definition))) return false;
  }
  cameras_ = std::move(loaded.cameras_);
  return !cameras_.empty();
}

const MissionCameraDefinition* MissionCameraDatabase::find(std::uint32_t mission_id) const noexcept {
  const auto it = std::find_if(cameras_.begin(), cameras_.end(),
                               [mission_id](const MissionCameraDefinition& camera) {
                                 return camera.mission_id == mission_id;
                               });
  return it == cameras_.end() ? nullptr : &*it;
}

bool MissionManifestLoader::load_camera(const std::filesystem::path& manifest,
                                        MissionCameraDatabase& cameras) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || paths.camera.empty()) return false;
  MissionCameraDatabase loaded;
  if (!loaded.load_manifest(paths.camera)) return false;
  cameras = std::move(loaded);
  return true;
}

bool MissionManifestLoader::load_render(const std::filesystem::path& manifest,
                                        MissionRenderDatabase& render,
                                        MissionDrawableDatabase& drawables,
                                        MissionTransformDatabase& transforms,
                                        MissionMaterialDatabase& materials,
                                        MissionTextureDatabase& textures,
                                        ShaderPermutationDatabase& shaders,
                                        MissionRenderTargetDatabase& targets,
                                        MissionRenderPassDatabase& passes,
                                        MissionRenderResolveDatabase& resolves,
                                        QualifiedBufferDatabase& buffers) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || !paths.render_valid()) return false;
  MissionRenderDatabase loaded_render;
  MissionDrawableDatabase loaded_drawables;
  MissionTransformDatabase loaded_transforms;
  MissionMaterialDatabase loaded_materials;
  MissionTextureDatabase loaded_textures;
  ShaderPermutationDatabase loaded_shaders;
  MissionRenderTargetDatabase loaded_targets;
  MissionRenderPassDatabase loaded_passes;
  MissionRenderResolveDatabase loaded_resolves;
  QualifiedBufferDatabase loaded_buffers;
  if (!loaded_render.load_manifest(paths.render) ||
      !loaded_drawables.load_manifest(paths.drawables) ||
      !loaded_transforms.load_manifest(paths.transforms) ||
      !loaded_materials.load_manifest(paths.materials) ||
      !loaded_textures.load_manifest(paths.textures) ||
      !loaded_shaders.load_manifest(paths.shaders) ||
      !loaded_targets.load_manifest(paths.targets) ||
      !loaded_passes.load_manifest(paths.passes) ||
      !loaded_resolves.load_manifest(paths.resolves) ||
      !loaded_buffers.load_manifest(paths.buffers)) return false;
  render = std::move(loaded_render);
  drawables = std::move(loaded_drawables);
  transforms = std::move(loaded_transforms);
  materials = std::move(loaded_materials);
  textures = std::move(loaded_textures);
  shaders = std::move(loaded_shaders);
  targets = std::move(loaded_targets);
  passes = std::move(loaded_passes);
  resolves = std::move(loaded_resolves);
  buffers = std::move(loaded_buffers);
  return true;
}

bool MissionManifestLoader::load_render(const std::filesystem::path& manifest,
                                        MissionRenderDatabase& render,
                                        MissionDrawableDatabase& drawables,
                                        MissionTransformDatabase& transforms,
                                        MissionMaterialDatabase& materials,
                                        MissionTextureDatabase& textures,
                                        ShaderPermutationDatabase& shaders,
                                        MissionRenderTargetDatabase& targets,
                                        MissionRenderPassDatabase& passes,
                                        MissionRenderResolveDatabase& resolves,
                                        QualifiedBufferDatabase& buffers,
                                        NativeGeometryDatabase& geometries) const {
  MissionRenderDatabase loaded_render;
  MissionDrawableDatabase loaded_drawables;
  MissionTransformDatabase loaded_transforms;
  MissionMaterialDatabase loaded_materials;
  MissionTextureDatabase loaded_textures;
  ShaderPermutationDatabase loaded_shaders;
  MissionRenderTargetDatabase loaded_targets;
  MissionRenderPassDatabase loaded_passes;
  MissionRenderResolveDatabase loaded_resolves;
  QualifiedBufferDatabase loaded_buffers;
  if (!load_render(manifest, loaded_render, loaded_drawables, loaded_transforms,
                   loaded_materials, loaded_textures, loaded_shaders, loaded_targets,
                   loaded_passes, loaded_resolves, loaded_buffers)) return false;
  NativeGeometryDatabase loaded_geometries;
  std::unordered_set<std::string> loaded_buffer_ids;
  for (const auto& [mission_id, definition] : loaded_render.definitions()) {
    for (const AssetId asset : definition.asset_ids) {
      for (const MissionDrawable* drawable : loaded_drawables.find_by_asset(mission_id, asset)) {
        if (drawable == nullptr || !loaded_buffer_ids.insert(drawable->buffer_id).second) continue;
        if (!loaded_buffers.verify(drawable->buffer_id) ||
            !loaded_geometries.load_verified(*drawable, loaded_buffers)) return false;
      }
    }
  }
  render = std::move(loaded_render);
  drawables = std::move(loaded_drawables);
  transforms = std::move(loaded_transforms);
  materials = std::move(loaded_materials);
  textures = std::move(loaded_textures);
  shaders = std::move(loaded_shaders);
  targets = std::move(loaded_targets);
  passes = std::move(loaded_passes);
  resolves = std::move(loaded_resolves);
  buffers = std::move(loaded_buffers);
  geometries = std::move(loaded_geometries);
  return true;
}

bool configure_mission_launch(const MissionLaunchDefinition& launch, UnitRegistry& units,
                              MissionScenario& scenario) noexcept {
  if (launch.mission_id == 0 || launch.mission_id != scenario.mission_id() ||
      launch.player_entity == 0 || launch.units.empty()) {
    return false;
  }
  for (UnitRecord unit : launch.units) {
    if (!units.register_unit(unit) || !units.activate(unit.id)) return false;
  }
  return scenario.bind_player(units, launch.player_entity);
}

bool MissionRenderDatabase::add(MissionRenderDefinition definition) {
  if (definition.mission_id == 0 || definition.asset_ids.empty()) return false;
  for (std::size_t i = 0; i < definition.asset_ids.size(); ++i) {
    if (definition.asset_ids[i] == 0) return false;
    if (std::find(definition.asset_ids.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                  definition.asset_ids.end(), definition.asset_ids[i]) != definition.asset_ids.end()) {
      return false;
    }
  }
  return renders_.emplace(definition.mission_id, std::move(definition)).second;
}

bool MissionRenderDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    if (first == std::string::npos || line.find('\t', first + 1) != std::string::npos) {
      return false;
    }
    MissionRenderDefinition definition;
    if (!parse_u32(std::string_view(line).substr(0, first), definition.mission_id) ||
        !parse_asset_ids(std::string_view(line).substr(first + 1), definition.asset_ids) ||
        !add(std::move(definition))) {
      return false;
    }
  }
  return true;
}

const MissionRenderDefinition* MissionRenderDatabase::find(
    std::uint32_t mission_id) const noexcept {
  const auto it = renders_.find(mission_id);
  return it == renders_.end() ? nullptr : &it->second;
}

bool MissionDrawableDatabase::add(MissionDrawable drawable) {
  if (drawable.mission_id == 0 || drawable.stable_id.empty() || drawable.kind.empty() ||
      drawable.asset == 0 || drawable.primitive_count == 0 ||
      !drawable.has_buffer_contract()) {
    return false;
  }
  if (find(drawable.mission_id, drawable.stable_id) != nullptr) return false;
  drawables_.push_back(drawable);
  return true;
}

bool MissionDrawableDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    const auto eighth = seventh == std::string::npos ? std::string::npos : line.find('\t', seventh + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        seventh == std::string::npos || eighth == std::string::npos ||
        line.find('\t', eighth + 1) != std::string::npos) {
      return false;
    }
    MissionDrawable drawable;
    if (!parse_u32(std::string_view(line).substr(0, first), drawable.mission_id) ||
        (drawable.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        (drawable.kind = line.substr(second + 1, third - second - 1)).empty() ||
        !parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1), drawable.asset) ||
        !parse_u32(std::string_view(line).substr(fourth + 1, fifth - fourth - 1), drawable.primitive_count) ||
        (drawable.buffer_id = line.substr(fifth + 1, sixth - fifth - 1)).empty() ||
        !parse_u32(std::string_view(line).substr(sixth + 1, seventh - sixth - 1), drawable.vertex_count) ||
        !parse_u32(std::string_view(line).substr(seventh + 1, eighth - seventh - 1), drawable.index_count) ||
        (drawable.content_hash = line.substr(eighth + 1)).empty() ||
        !add(drawable)) {
      return false;
    }
  }
  return true;
}

const MissionDrawable* MissionDrawableDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(drawables_.begin(), drawables_.end(),
                               [mission_id, &stable_id](const MissionDrawable& drawable) {
                                 return drawable.mission_id == mission_id &&
                                        drawable.stable_id == stable_id;
                               });
  return it == drawables_.end() ? nullptr : &*it;
}

std::vector<const MissionDrawable*> MissionDrawableDatabase::find_by_asset(
    std::uint32_t mission_id, AssetId asset) const {
  std::vector<const MissionDrawable*> result;
  for (const MissionDrawable& drawable : drawables_) {
    if (drawable.mission_id == mission_id && drawable.asset == asset) {
      result.push_back(&drawable);
    }
  }
  return result;
}

bool MissionDrawableTransform::valid() const noexcept {
  return mission_id != 0 && !stable_id.empty() &&
         std::isfinite(translate_x) && std::isfinite(translate_y) &&
         std::isfinite(translate_z) && std::isfinite(scale_x) &&
         std::isfinite(scale_y) && std::isfinite(scale_z) &&
         scale_x > 0.0f && scale_y > 0.0f && scale_z > 0.0f;
}

bool MissionTransformDatabase::add(MissionDrawableTransform transform) {
  if (!transform.valid() || find(transform.mission_id, transform.stable_id) != nullptr) {
    return false;
  }
  transforms_.push_back(std::move(transform));
  return true;
}

bool MissionTransformDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        seventh == std::string::npos || line.find('\t', seventh + 1) != std::string::npos) {
      return false;
    }
    MissionDrawableTransform transform;
    if (!parse_u32(std::string_view(line).substr(0, first), transform.mission_id) ||
        (transform.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        !parse_f32(std::string_view(line).substr(second + 1, third - second - 1),
                   transform.translate_x) ||
        !parse_f32(std::string_view(line).substr(third + 1, fourth - third - 1),
                   transform.translate_y) ||
        !parse_f32(std::string_view(line).substr(fourth + 1, fifth - fourth - 1),
                   transform.translate_z) ||
        !parse_f32(std::string_view(line).substr(fifth + 1, sixth - fifth - 1),
                   transform.scale_x) ||
        !parse_f32(std::string_view(line).substr(sixth + 1, seventh - sixth - 1),
                   transform.scale_y) ||
        !parse_f32(std::string_view(line).substr(seventh + 1), transform.scale_z) ||
        !add(std::move(transform))) {
      return false;
    }
  }
  return true;
}

const MissionDrawableTransform* MissionTransformDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(transforms_.begin(), transforms_.end(),
                               [mission_id, &stable_id](const MissionDrawableTransform& transform) {
                                 return transform.mission_id == mission_id &&
                                        transform.stable_id == stable_id;
                               });
  return it == transforms_.end() ? nullptr : &*it;
}

bool MissionMaterial::valid() const noexcept {
  return mission_id != 0 && !stable_id.empty() && !shader_permutation.empty() &&
         (blend_mode == "opaque" || blend_mode == "alpha" || blend_mode == "additive") &&
         ((base_color >> 24u) != 0u);
}

bool MissionMaterialDatabase::add(MissionMaterial material) {
  if (!material.valid() || find(material.mission_id, material.stable_id) != nullptr) {
    return false;
  }
  materials_.push_back(std::move(material));
  return true;
}

bool MissionMaterialDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        (seventh != std::string::npos && line.find('\t', seventh + 1) != std::string::npos)) {
      return false;
    }
    MissionMaterial material;
    if (!parse_u32(std::string_view(line).substr(0, first), material.mission_id) ||
        (material.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        (material.shader_permutation = line.substr(second + 1, third - second - 1)).empty() ||
        !parse_bool01(std::string_view(line).substr(third + 1, fourth - third - 1),
                      material.depth_test) ||
        !parse_bool01(std::string_view(line).substr(fourth + 1, fifth - fourth - 1),
                      material.depth_write) ||
        (material.blend_mode = line.substr(fifth + 1, sixth - fifth - 1)).empty() ||
        !parse_hex_u32(std::string_view(line).substr(sixth + 1,
                                                      seventh == std::string::npos ?
                                                          std::string::npos : seventh - sixth - 1),
                       material.base_color) ||
        (seventh != std::string::npos &&
         (!parse_u64(std::string_view(line).substr(seventh + 1), material.mate_id) ||
          material.mate_id == 0)) ||
        !add(std::move(material))) {
      return false;
    }
  }
  return true;
}

const MissionMaterial* MissionMaterialDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(materials_.begin(), materials_.end(),
                               [mission_id, &stable_id](const MissionMaterial& material) {
                                 return material.mission_id == mission_id &&
                                        material.stable_id == stable_id;
                               });
  return it == materials_.end() ? nullptr : &*it;
}

bool MissionTextureBinding::valid() const noexcept {
  return mission_id != 0 && !stable_id.empty() && !texture_id.empty() &&
         (sampler_filter == "nearest" || sampler_filter == "linear") &&
         (sampler_address == "wrap" || sampler_address == "clamp") &&
         content_hash != 0;
}

bool MissionTextureDatabase::add(MissionTextureBinding texture) {
  if (!texture.valid() || find(texture.mission_id, texture.stable_id) != nullptr) {
    return false;
  }
  textures_.push_back(std::move(texture));
  return true;
}

bool MissionTextureDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos) {
      return false;
    }
    MissionTextureBinding texture;
    if (!parse_u32(std::string_view(line).substr(0, first), texture.mission_id) ||
        (texture.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        (texture.texture_id = line.substr(second + 1, third - second - 1)).empty() ||
        (texture.sampler_filter = line.substr(third + 1, fourth - third - 1)).empty() ||
        (texture.sampler_address = line.substr(fourth + 1, fifth - fourth - 1)).empty() ||
        !parse_hex_u64(std::string_view(line).substr(fifth + 1,
                                                       sixth == std::string::npos ?
                                                           std::string::npos : sixth - fifth - 1),
                       texture.content_hash)) {
      return false;
    }
    if (sixth != std::string::npos) {
      const auto seventh = line.find('\t', sixth + 1);
      if (seventh == std::string::npos ||
          (texture.source_path = line.substr(sixth + 1, seventh - sixth - 1)).empty() ||
          !parse_u64(std::string_view(line).substr(seventh + 1,
                                                   line.find('\t', seventh + 1) == std::string::npos ?
                                                       std::string::npos : line.find('\t', seventh + 1) - seventh - 1),
                     texture.source_size) ||
          texture.source_size == 0) return false;
      const auto eighth = line.find('\t', seventh + 1);
      if (eighth != std::string::npos) {
        const auto ninth = line.find('\t', eighth + 1);
        const auto tenth = ninth == std::string::npos ? std::string::npos : line.find('\t', ninth + 1);
        const auto eleventh = tenth == std::string::npos ? std::string::npos : line.find('\t', tenth + 1);
        if (ninth == std::string::npos || tenth == std::string::npos ||
            !parse_u32(std::string_view(line).substr(eighth + 1, ninth - eighth - 1), texture.source_width) ||
            !parse_u32(std::string_view(line).substr(ninth + 1, tenth - ninth - 1), texture.source_height) ||
            !parse_u32(std::string_view(line).substr(tenth + 1,
                                                     eleventh == std::string::npos ?
                                                         std::string::npos : eleventh - tenth - 1),
                       texture.source_format) ||
            texture.source_width == 0 || texture.source_height == 0 || texture.source_format == 0) return false;
        if (eleventh != std::string::npos &&
            (!parse_u64(std::string_view(line).substr(eleventh + 1), texture.gidx) || texture.gidx == 0)) return false;
      }
      if (texture.source_path.is_relative()) texture.source_path = manifest.parent_path() / texture.source_path;
      std::error_code error;
      if (!std::filesystem::is_regular_file(texture.source_path, error) || error ||
          std::filesystem::file_size(texture.source_path, error) != texture.source_size || error) {
        return false;
      }
      std::uint64_t source_hash = 0;
      if (!file_fnv64(texture.source_path, source_hash) || source_hash != texture.content_hash) {
        return false;
      }
      if (texture.source_path.extension() == ".ppm") {
        std::ifstream image(texture.source_path, std::ios::binary);
        std::string magic;
        std::uint32_t width = 0, height = 0, max_value = 0;
        image >> magic >> width >> height >> max_value;
        if (!image || magic != "P6" || width == 0 || height == 0 || max_value != 255) return false;
        image.get();
        std::vector<unsigned char> bytes(static_cast<std::size_t>(width) * height * 3u);
        image.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!image) return false;
        Image decoded{width, height, {}};
        decoded.pixels.resize(static_cast<std::size_t>(width) * height);
        for (std::size_t pixel = 0; pixel < decoded.pixels.size(); ++pixel) {
          decoded.pixels[pixel] = 0xFF000000u |
              (static_cast<std::uint32_t>(bytes[pixel * 3u]) << 16u) |
              (static_cast<std::uint32_t>(bytes[pixel * 3u + 1u]) << 8u) |
              static_cast<std::uint32_t>(bytes[pixel * 3u + 2u]);
        }
        images_[std::to_string(texture.mission_id) + ":" + texture.stable_id] = std::move(decoded);
      }
      if (texture.source_width != 0) {
        std::array<unsigned char, 0x28> header{};
        std::ifstream source(texture.source_path, std::ios::binary);
        source.read(reinterpret_cast<char*>(header.data()),
                    static_cast<std::streamsize>(header.size()));
        if (!source || std::memcmp(header.data(), "NTXR", 4) != 0) return false;
        const auto be16 = [&header](std::size_t offset) {
          return static_cast<std::uint32_t>(header[offset]) << 8u |
                 static_cast<std::uint32_t>(header[offset + 1]);
        };
        const auto be_format = [&header](std::size_t offset) {
          return static_cast<std::uint32_t>(header[offset]) << 8u |
                 static_cast<std::uint32_t>(header[offset + 1]);
        };
        if (be16(0x24) != texture.source_width || be16(0x26) != texture.source_height ||
            be_format(0x04) != texture.source_format) return false;
      }
    }
    if (!add(std::move(texture))) return false;
  }
  return true;
}

const MissionTextureBinding* MissionTextureDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(textures_.begin(), textures_.end(),
                               [mission_id, &stable_id](const MissionTextureBinding& texture) {
                                 return texture.mission_id == mission_id &&
                                        texture.stable_id == stable_id;
                               });
  return it == textures_.end() ? nullptr : &*it;
}

bool MissionTextureDatabase::sample(std::uint32_t mission_id, const std::string& stable_id,
                                    float u, float v, std::uint32_t& rgba) const noexcept {
  const auto it = images_.find(std::to_string(mission_id) + ":" + stable_id);
  const MissionTextureBinding* binding = find(mission_id, stable_id);
  if (it == images_.end() || binding == nullptr || it->second.width == 0 ||
      it->second.height == 0 || !std::isfinite(u) || !std::isfinite(v)) return false;
  if (binding->sampler_address == "clamp") {
    u = std::clamp(u, 0.0f, std::nextafter(1.0f, 0.0f));
    v = std::clamp(v, 0.0f, std::nextafter(1.0f, 0.0f));
  } else {
    u -= std::floor(u);
    v -= std::floor(v);
  }
  const std::uint32_t x = std::min(it->second.width - 1u,
                                   static_cast<std::uint32_t>(u * it->second.width));
  const std::uint32_t y = std::min(it->second.height - 1u,
                                   static_cast<std::uint32_t>(v * it->second.height));
  if (binding->sampler_filter == "nearest") {
    rgba = it->second.pixels[static_cast<std::size_t>(y) * it->second.width + x];
    return true;
  }
  const float px = binding->sampler_address == "clamp" ? u * (it->second.width - 1u) : u * it->second.width;
  const float py = binding->sampler_address == "clamp" ? v * (it->second.height - 1u) : v * it->second.height;
  const std::uint32_t x0 = static_cast<std::uint32_t>(std::floor(px)) % it->second.width;
  const std::uint32_t y0 = static_cast<std::uint32_t>(std::floor(py)) % it->second.height;
  const std::uint32_t x1 = binding->sampler_address == "clamp" ?
      std::min(x0 + 1u, it->second.width - 1u) : (x0 + 1u) % it->second.width;
  const std::uint32_t y1 = binding->sampler_address == "clamp" ?
      std::min(y0 + 1u, it->second.height - 1u) : (y0 + 1u) % it->second.height;
  const float fx = px - std::floor(px), fy = py - std::floor(py);
  const auto fetch = [&it](std::uint32_t sx, std::uint32_t sy) {
    return it->second.pixels[static_cast<std::size_t>(sy) * it->second.width + sx];
  };
  const std::uint32_t c00 = fetch(x0, y0), c10 = fetch(x1, y0);
  const std::uint32_t c01 = fetch(x0, y1), c11 = fetch(x1, y1);
  std::uint32_t result = 0xFF000000u;
  for (unsigned channel = 0; channel < 3; ++channel) {
    const unsigned shift = 16u - channel * 8u;
    const float top = static_cast<float>((c00 >> shift) & 0xFFu) * (1.0f - fx) +
                      static_cast<float>((c10 >> shift) & 0xFFu) * fx;
    const float bottom = static_cast<float>((c01 >> shift) & 0xFFu) * (1.0f - fx) +
                         static_cast<float>((c11 >> shift) & 0xFFu) * fx;
    result |= static_cast<std::uint32_t>(std::clamp(top * (1.0f - fy) + bottom * fy,
                                                    0.0f, 255.0f)) << shift;
  }
  rgba = result;
  return true;
}

bool ShaderPermutation::valid() const noexcept {
  return !id.empty() && !vertex_layout.empty() && texture_fetches != 0 &&
         constant_count != 0 &&
         (render_target_format == "rgba8" || render_target_format == "rgba16f" ||
          render_target_format == "d24s8");
}

bool ShaderPermutationDatabase::add(ShaderPermutation permutation) {
  if (!permutation.valid() || find(permutation.id) != nullptr) {
    return false;
  }
  permutations_.push_back(std::move(permutation));
  return true;
}

bool ShaderPermutationDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        line.find('\t', fourth + 1) != std::string::npos) {
      return false;
    }
    ShaderPermutation permutation;
    permutation.id = line.substr(0, first);
    permutation.vertex_layout = line.substr(first + 1, second - first - 1);
    if (!parse_u32(std::string_view(line).substr(second + 1, third - second - 1),
                   permutation.texture_fetches) ||
        !parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1),
                   permutation.constant_count) ||
        (permutation.render_target_format = line.substr(fourth + 1)).empty() ||
        !add(std::move(permutation))) {
      return false;
    }
  }
  return true;
}

const ShaderPermutation* ShaderPermutationDatabase::find(const std::string& id) const noexcept {
  const auto it = std::find_if(permutations_.begin(), permutations_.end(),
                               [&id](const ShaderPermutation& permutation) {
                                 return permutation.id == id;
                               });
  return it == permutations_.end() ? nullptr : &*it;
}

bool MissionRenderTargetDefinition::valid() const noexcept {
  constexpr std::uint32_t max_dimension = 4096;
  const bool supported_sample_count =
      sample_count == 1 || sample_count == 2 || sample_count == 4 || sample_count == 8;
  return mission_id != 0 &&
         (target_id == "world_color" || target_id == "present" || target_id == "main_color") &&
         width != 0 && height != 0 &&
         width <= max_dimension && height <= max_dimension &&
         static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) <=
             16u * 1024u * 1024u &&
         supported_sample_count &&
         (color_format == "rgba8" || color_format == "rgba16f") &&
         (depth_format == "none" || depth_format == "d24s8") &&
         (depth_enabled == (depth_format != "none"));
}

bool MissionRenderTargetDatabase::add(MissionRenderTargetDefinition definition) {
  if (!definition.valid() || find(definition.mission_id, definition.target_id) != nullptr) return false;
  targets_.push_back(std::move(definition));
  return true;
}

bool MissionRenderTargetDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        seventh == std::string::npos || line.find('\t', seventh + 1) != std::string::npos) {
      return false;
    }
    MissionRenderTargetDefinition definition;
    if (!parse_u32(std::string_view(line).substr(0, first), definition.mission_id) ||
        (definition.target_id = line.substr(first + 1, second - first - 1)).empty() ||
        !parse_u32(std::string_view(line).substr(second + 1, third - second - 1),
                   definition.width) ||
        !parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1),
                   definition.height) ||
        !parse_u32(std::string_view(line).substr(fourth + 1, fifth - fourth - 1),
                   definition.sample_count) ||
        (definition.color_format = line.substr(fifth + 1, sixth - fifth - 1)).empty() ||
        (definition.depth_format = line.substr(sixth + 1, seventh - sixth - 1)).empty() ||
        !parse_bool01(std::string_view(line).substr(seventh + 1), definition.depth_enabled) ||
        !add(std::move(definition))) {
      return false;
    }
  }
  return true;
}

const MissionRenderTargetDefinition* MissionRenderTargetDatabase::find(
    std::uint32_t mission_id) const noexcept {
  return find(mission_id, "main_color");
}

const MissionRenderTargetDefinition* MissionRenderTargetDatabase::find(
    std::uint32_t mission_id, const std::string& target_id) const noexcept {
  const auto it = std::find_if(targets_.begin(), targets_.end(),
                               [mission_id, &target_id](const MissionRenderTargetDefinition& target) {
                                 return target.mission_id == mission_id &&
                                        target.target_id == target_id;
                               });
  return it == targets_.end() ? nullptr : &*it;
}

bool MissionRenderPass::valid() const noexcept {
  return mission_id != 0 && !pass_id.empty() && order != 0 &&
         (color_target == "main_color" || color_target == "world_color") &&
         (depth_target == "main_depth" || depth_target == "none") &&
         std::isfinite(clear_depth) && clear_depth >= 0.0f && clear_depth <= 1.0f;
}

bool MissionRenderPassDatabase::add(MissionRenderPass pass) {
  if (!pass.valid() || find(pass.mission_id, pass.pass_id) != nullptr) return false;
  passes_.push_back(std::move(pass));
  return true;
}

bool MissionRenderPassDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        line.find('\t', sixth + 1) != std::string::npos) {
      return false;
    }
    MissionRenderPass pass;
    if (!parse_u32(std::string_view(line).substr(0, first), pass.mission_id) ||
        (pass.pass_id = line.substr(first + 1, second - first - 1)).empty() ||
        !parse_u32(std::string_view(line).substr(second + 1, third - second - 1), pass.order) ||
        (pass.color_target = line.substr(third + 1, fourth - third - 1)).empty() ||
        (pass.depth_target = line.substr(fourth + 1, fifth - fourth - 1)).empty() ||
        !parse_hex_u32(std::string_view(line).substr(fifth + 1, sixth - fifth - 1),
                       pass.clear_color) ||
        !parse_f32(std::string_view(line).substr(sixth + 1), pass.clear_depth) ||
        !add(std::move(pass))) {
      return false;
    }
  }
  return true;
}

const MissionRenderPass* MissionRenderPassDatabase::find(
    std::uint32_t mission_id, const std::string& pass_id) const noexcept {
  const auto it = std::find_if(passes_.begin(), passes_.end(),
                               [mission_id, &pass_id](const MissionRenderPass& pass) {
                                 return pass.mission_id == mission_id && pass.pass_id == pass_id;
                               });
  return it == passes_.end() ? nullptr : &*it;
}

bool MissionRenderResolve::valid() const noexcept {
  return mission_id != 0 && source_pass == "world" &&
         (source_target == "main_color" || source_target == "world_color") &&
         destination_target == "present" &&
         (mode == "copy" || mode == "tonemap" || mode == "linear" || mode == "msaa_resolve");
}

bool MissionRenderResolveDatabase::add(MissionRenderResolve resolve) {
  if (!resolve.valid() || find(resolve.mission_id, resolve.source_pass) != nullptr) return false;
  resolves_.push_back(std::move(resolve));
  return true;
}

bool MissionRenderResolveDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        line.find('\t', fourth + 1) != std::string::npos) {
      return false;
    }
    MissionRenderResolve resolve;
    if (!parse_u32(std::string_view(line).substr(0, first), resolve.mission_id) ||
        (resolve.source_pass = line.substr(first + 1, second - first - 1)).empty() ||
        (resolve.source_target = line.substr(second + 1, third - second - 1)).empty() ||
        (resolve.destination_target = line.substr(third + 1, fourth - third - 1)).empty() ||
        (resolve.mode = line.substr(fourth + 1)).empty() ||
        !add(std::move(resolve))) {
      return false;
    }
  }
  return true;
}

const MissionRenderResolve* MissionRenderResolveDatabase::find(
    std::uint32_t mission_id, const std::string& source_pass) const noexcept {
  const auto it = std::find_if(resolves_.begin(), resolves_.end(),
                               [mission_id, &source_pass](const MissionRenderResolve& resolve) {
                                 return resolve.mission_id == mission_id &&
                                        resolve.source_pass == source_pass;
                               });
  return it == resolves_.end() ? nullptr : &*it;
}

bool QualifiedBufferDatabase::add(QualifiedBufferRecord record) {
  if (record.buffer_id.empty() || record.path.empty() || record.byte_size == 0 ||
      record.fnv64 == 0 || find(record.buffer_id) != nullptr) {
    return false;
  }
  record.verified = false;
  buffers_.push_back(std::move(record));
  return true;
}

bool QualifiedBufferDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || line.find('\t', third + 1) != std::string::npos) {
      return false;
    }
    QualifiedBufferRecord record;
    record.buffer_id = line.substr(0, first);
    record.path = line.substr(first + 1, second - first - 1);
    if (record.path.is_relative()) record.path = manifest.parent_path() / record.path;
    if (!parse_u64(std::string_view(line).substr(second + 1, third - second - 1),
                   record.byte_size) ||
        !parse_u64(std::string_view(line).substr(third + 1), record.fnv64) ||
        !add(std::move(record))) {
      return false;
    }
  }
  return true;
}

bool QualifiedBufferDatabase::verify(const std::string& buffer_id) {
  QualifiedBufferRecord* record = nullptr;
  for (QualifiedBufferRecord& candidate : buffers_) {
    if (candidate.buffer_id == buffer_id) {
      record = &candidate;
      break;
    }
  }
  if (record == nullptr) return false;
  std::ifstream input(record->path, std::ios::binary);
  if (!input) return false;
  std::uint64_t hash = 1469598103934665603ull;
  std::uint64_t size = 0;
  char byte = 0;
  while (input.get(byte)) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ull;
    ++size;
  }
  record->verified = size == record->byte_size && hash == record->fnv64;
  return record->verified;
}

bool QualifiedBufferDatabase::has_verified(const std::string& buffer_id) const noexcept {
  const QualifiedBufferRecord* record = find(buffer_id);
  return record != nullptr && record->verified;
}

const QualifiedBufferRecord* QualifiedBufferDatabase::find(
    const std::string& buffer_id) const noexcept {
  const auto it = std::find_if(buffers_.begin(), buffers_.end(),
                               [&buffer_id](const QualifiedBufferRecord& record) {
                                 return record.buffer_id == buffer_id;
                               });
  return it == buffers_.end() ? nullptr : &*it;
}

bool NativeGeometryDatabase::load_verified(const MissionDrawable& drawable,
                                           const QualifiedBufferDatabase& buffers) {
  if (!drawable.has_buffer_contract() || !buffers.has_verified(drawable.buffer_id) ||
      find(drawable.buffer_id) != nullptr) {
    return false;
  }
  const QualifiedBufferRecord* record = buffers.find(drawable.buffer_id);
  if (record == nullptr) return false;
  std::ifstream input(record->path, std::ios::binary);
  if (!input) return false;
  // Retail NDXR slices are binary, big-endian records. The text form below is
  // retained for deterministic unit fixtures, but a qualified retail slice
  // must be decoded without rewriting its bytes into a synthetic container.
  {
    std::vector<unsigned char> raw;
    if (record->byte_size >= 0x30 && record->byte_size <= 256u * 1024u * 1024u) {
      raw.resize(static_cast<std::size_t>(record->byte_size));
      input.clear();
      input.seekg(0, std::ios::beg);
      input.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
      const auto be16 = [&raw](std::size_t offset, std::uint16_t& value) noexcept {
        if (offset > raw.size() || raw.size() - offset < 2u) return false;
        value = static_cast<std::uint16_t>(raw[offset] << 8u | raw[offset + 1u]);
        return true;
      };
      const auto be32 = [&raw](std::size_t offset, std::uint32_t& value) noexcept {
        if (offset > raw.size() || raw.size() - offset < 4u) return false;
        value = (static_cast<std::uint32_t>(raw[offset]) << 24u) |
                (static_cast<std::uint32_t>(raw[offset + 1u]) << 16u) |
                (static_cast<std::uint32_t>(raw[offset + 2u]) << 8u) |
                static_cast<std::uint32_t>(raw[offset + 3u]);
        return true;
      };
      const auto bef32 = [&be32](std::size_t offset, float& value) noexcept {
        std::uint32_t bits = 0;
        if (!be32(offset, bits)) return false;
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value);
      };
      if (raw.size() >= 0x30 && std::memcmp(raw.data(), "NDXR", 4) == 0 && raw[4] == 0) {
        std::uint32_t declared_size = 0, header_size = 0, polygon_size = 0;
        std::uint32_t vertex_size = 0, additional_size = 0;
        std::uint16_t object_count = 0;
        if (!be32(4, declared_size) || declared_size != raw.size() || !be16(0x0a, object_count) ||
            object_count == 0 || object_count > 100000 || !be32(0x10, header_size) ||
            !be32(0x14, polygon_size) || !be32(0x18, vertex_size) ||
            !be32(0x1c, additional_size)) return false;
        const std::size_t object_table = 0x30u;
        const std::size_t polygon_descriptors = object_table +
            static_cast<std::size_t>(object_count) * 0x30u;
        if (polygon_descriptors > raw.size() || header_size > raw.size() - 0x30u) return false;
        std::uint32_t polygon_count = 0;
        for (std::uint32_t object = 0; object < object_count; ++object) {
          std::uint16_t count = 0;
          if (!be16(object_table + static_cast<std::size_t>(object) * 0x30u + 0x2au, count) ||
              polygon_count > 100000u - count) return false;
          polygon_count += count;
        }
        const std::size_t polygon_descriptor_end = polygon_descriptors +
            static_cast<std::size_t>(polygon_count) * 0x30u;
        const std::size_t polygon_base = 0x30u + static_cast<std::size_t>(header_size);
        if (polygon_descriptor_end > polygon_base || polygon_base > raw.size() ||
            polygon_size > raw.size() - polygon_base) return false;
        const std::size_t vertex_base = polygon_base + polygon_size;
        if (vertex_base > raw.size() || vertex_size > raw.size() - vertex_base ||
            additional_size > raw.size() - vertex_base - vertex_size) return false;

        struct Polygon { std::uint32_t index_offset{}, vertex_offset{}; std::uint16_t vertex_count{}, index_count{}, format{}; };
        std::vector<Polygon> polygons;
        polygons.reserve(polygon_count);
        std::uint32_t vertex_stride = 0;
        std::uint64_t max_vertex_end = 0;
        std::uint64_t total_indices = 0;
        for (std::uint32_t polygon = 0; polygon < polygon_count; ++polygon) {
          const std::size_t offset = polygon_descriptors + static_cast<std::size_t>(polygon) * 0x30u;
          Polygon value;
          if (!be32(offset, value.index_offset) || !be32(offset + 4u, value.vertex_offset) ||
              !be16(offset + 0x0cu, value.vertex_count) || !be16(offset + 0x0eu, value.format) ||
              !be16(offset + 0x20u, value.index_count) ||
              value.index_offset > polygon_size ||
              static_cast<std::uint64_t>(value.index_count) * 2u > polygon_size - value.index_offset) {
            return false;
          }
          const std::uint32_t stride = value.format == 0x0611u ? 28u :
                                       value.format == 0x0613u ? 32u :
                                       value.format == 0x0711u ? 44u :
                                       value.format == 0x0721u ? 52u : 0u;
          if (stride == 0 || (vertex_stride != 0 && vertex_stride != stride)) return false;
          vertex_stride = stride;
          if (value.vertex_offset % stride != 0 || value.vertex_offset > vertex_size ||
              static_cast<std::uint64_t>(value.vertex_count) * stride > vertex_size - value.vertex_offset ||
              total_indices > std::numeric_limits<std::uint32_t>::max() - value.index_count) return false;
          total_indices += value.index_count;
          max_vertex_end = std::max(max_vertex_end,
              static_cast<std::uint64_t>(value.vertex_offset) +
              static_cast<std::uint64_t>(value.vertex_count) * stride);
          polygons.push_back(value);
        }
        if (vertex_stride == 0 || max_vertex_end == 0 ||
            (max_vertex_end + vertex_stride - 1u) / vertex_stride > std::numeric_limits<std::uint32_t>::max() ||
            total_indices > std::numeric_limits<std::uint32_t>::max() ||
            drawable.vertex_count != vertex_size / vertex_stride ||
            drawable.index_count != total_indices || drawable.primitive_count != polygon_count) {
          return false;
        }
        NativeGeometryMetadata metadata;
        metadata.buffer_id = drawable.buffer_id;
        metadata.source_format = "NDXR_BE";
        metadata.vertex_count = static_cast<std::uint32_t>((max_vertex_end + vertex_stride - 1u) / vertex_stride);
        metadata.index_count = static_cast<std::uint32_t>(total_indices);
        metadata.primitive_count = polygon_count;
        metadata.vertex_section_count = metadata.vertex_count;
        metadata.index_section_count = metadata.index_count;
        metadata.polygon_descriptor_count = polygon_count;
        metadata.vertex_stride = vertex_stride;
        metadata.index_size = 2;
        metadata.vertex_byte_size = vertex_size;
        metadata.index_byte_size = total_indices * 2u;
        DecodedGeometry decoded;
        bool has_primitive_restart = false;
        decoded.buffer_id = drawable.buffer_id;
        decoded.vertices.reserve(metadata.vertex_count);
        for (std::uint32_t vertex = 0; vertex < metadata.vertex_count; ++vertex) {
          const std::size_t offset = vertex_base + static_cast<std::size_t>(vertex) * vertex_stride;
          DecodedVertex value;
          if (!bef32(offset, value.x) || !bef32(offset + 4u, value.y) ||
              !bef32(offset + 8u, value.z)) return false;
          const std::size_t uv_offset = vertex_stride == 28u ? 16u : 20u;
          if (vertex_stride >= 28u) {
            std::uint32_t u_bits = 0, v_bits = 0;
            if (!be32(offset + uv_offset, u_bits) || !be32(offset + uv_offset + 4u, v_bits)) return false;
            std::memcpy(&value.u, &u_bits, sizeof(value.u));
            std::memcpy(&value.v, &v_bits, sizeof(value.v));
          }
          if (!std::isfinite(value.u) || !std::isfinite(value.v)) value.u = value.v = 0.0f;
          if (!decoded.bounds.valid) decoded.bounds = {value.x, value.y, value.z, value.x, value.y, value.z, true};
          else {
            decoded.bounds.min_x = std::min(decoded.bounds.min_x, value.x);
            decoded.bounds.min_y = std::min(decoded.bounds.min_y, value.y);
            decoded.bounds.min_z = std::min(decoded.bounds.min_z, value.z);
            decoded.bounds.max_x = std::max(decoded.bounds.max_x, value.x);
            decoded.bounds.max_y = std::max(decoded.bounds.max_y, value.y);
            decoded.bounds.max_z = std::max(decoded.bounds.max_z, value.z);
          }
          decoded.vertices.push_back(value);
        }
        decoded.indices.reserve(static_cast<std::size_t>(total_indices) + polygons.size());
        for (std::size_t polygon_index = 0; polygon_index < polygons.size(); ++polygon_index) {
          const Polygon& polygon = polygons[polygon_index];
          // Each retail NDXR polygon is an independent triangle strip.  The
          // container stores polygon boundaries in descriptors rather than
          // injecting restart indices, so preserve that ownership explicitly
          // in the decoded stream before concatenating polygons.
          if (polygon_index != 0) {
            decoded.indices.push_back(std::numeric_limits<std::uint32_t>::max());
            has_primitive_restart = true;
          }
          const std::uint32_t vertex_base_index = polygon.vertex_offset / vertex_stride;
          for (std::uint32_t index = 0; index < polygon.index_count; ++index) {
            std::uint16_t local = 0;
            if (!be16(polygon_base + polygon.index_offset + static_cast<std::size_t>(index) * 2u, local)) return false;
            if (local == 0xffffu) {
              // Preserve the restart boundary: retail NDXR polygons are
              // triangle strips, not a flat triangle-list stream.
              has_primitive_restart = true;
              decoded.indices.push_back(std::numeric_limits<std::uint32_t>::max());
              continue;
            }
            if (local >= polygon.vertex_count || vertex_base_index + local >= metadata.vertex_count) return false;
            decoded.indices.push_back(vertex_base_index + local);
          }
        }
        if (!decoded.bounds.valid || decoded.indices.empty()) return false;
        metadata.topology = NativeIndexTopology::TriangleStripRestart;
        geometries_.push_back(std::move(metadata));
        decoded_.push_back(std::move(decoded));
        return true;
      }
    }
  }
  input.clear();
  input.seekg(0, std::ios::beg);
  if (!input) return false;
  std::string line;
  if (!std::getline(input, line)) return false;
  const auto first = line.find('\t');
  const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
  const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
  const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
  if (first == std::string::npos || second == std::string::npos ||
      third == std::string::npos || fourth == std::string::npos ||
      line.find('\t', fourth + 1) != std::string::npos ||
      line.substr(0, first) != "NDXR" ||
      line.substr(first + 1, second - first - 1) != "1") {
    return false;
  }
  NativeGeometryMetadata metadata;
  metadata.buffer_id = drawable.buffer_id;
  metadata.source_format = "NDXR";
  if (!parse_u32(std::string_view(line).substr(second + 1, third - second - 1),
                 metadata.vertex_count) ||
      !parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1),
                 metadata.index_count) ||
      !parse_u32(std::string_view(line).substr(fourth + 1), metadata.primitive_count)) {
    return false;
  }
  if (metadata.vertex_count != drawable.vertex_count ||
      metadata.index_count != drawable.index_count ||
      metadata.primitive_count != drawable.primitive_count) {
    return false;
  }
  std::uint64_t payload_byte_size = 0;
  std::uint64_t payload_start_offset = 0;
  bool saw_payload = false;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    if (line == "DATA") {
      const auto payload_start = input.tellg();
      if (payload_start < 0 || record->byte_size < static_cast<std::uint64_t>(payload_start)) {
        return false;
      }
      payload_start_offset = static_cast<std::uint64_t>(payload_start);
      payload_byte_size = record->byte_size - static_cast<std::uint64_t>(payload_start);
      saw_payload = true;
      break;
    }
    const auto section_first = line.find('\t');
    const auto section_second = section_first == std::string::npos ? std::string::npos :
        line.find('\t', section_first + 1);
    if (section_first == std::string::npos || section_second == std::string::npos ||
        line.find('\t', section_second + 1) != std::string::npos) {
      return false;
    }
    const auto section = line.substr(0, section_first);
    std::uint32_t count = 0;
    std::uint32_t stride_or_flags = 0;
    if (!parse_u32(std::string_view(line).substr(section_first + 1,
                                                 section_second - section_first - 1), count) ||
        !parse_u32(std::string_view(line).substr(section_second + 1), stride_or_flags)) {
      return false;
    }
    if (section == "VTX") {
      if (metadata.vertex_section_count != 0 || count != metadata.vertex_count ||
          stride_or_flags == 0) {
        return false;
      }
      metadata.vertex_section_count = count;
      metadata.vertex_stride = stride_or_flags;
    } else if (section == "IDX") {
      if (metadata.index_section_count != 0 || count != metadata.index_count ||
          (stride_or_flags != 2 && stride_or_flags != 4)) {
        return false;
      }
      metadata.index_section_count = count;
      metadata.index_size = stride_or_flags;
    } else if (section == "POLY") {
      if (metadata.polygon_descriptor_count != 0 || count != metadata.primitive_count) {
        return false;
      }
      metadata.polygon_descriptor_count = count;
    } else {
      return false;
    }
  }
  if (metadata.vertex_section_count != metadata.vertex_count ||
      metadata.index_section_count != metadata.index_count ||
      metadata.polygon_descriptor_count != metadata.primitive_count || !saw_payload) {
    return false;
  }
  metadata.vertex_byte_size = static_cast<std::uint64_t>(metadata.vertex_count) *
                              static_cast<std::uint64_t>(metadata.vertex_stride);
  metadata.index_byte_size = static_cast<std::uint64_t>(metadata.index_count) *
                             static_cast<std::uint64_t>(metadata.index_size);
  if (metadata.vertex_byte_size == 0 || metadata.index_byte_size == 0 ||
      metadata.vertex_byte_size + metadata.index_byte_size > payload_byte_size) {
    return false;
  }
  if (metadata.vertex_stride < 12) return false;

  DecodedGeometry decoded;
  decoded.buffer_id = drawable.buffer_id;
  // A qualified slice is the complete drawable contract, not a preview. Keep
  // one explicit allocation guard, then decode every declared vertex/index so
  // the native renderer can submit the same topology as the oracle.
  constexpr std::uint32_t max_decoded_vertices = 1'000'000;
  constexpr std::uint32_t max_decoded_indices = 4'000'000;
  if (metadata.vertex_count > max_decoded_vertices || metadata.index_count > max_decoded_indices) {
    return false;
  }
  const std::uint32_t vertex_samples = metadata.vertex_count;
  const std::uint32_t index_samples = metadata.index_count;
  decoded.vertices.reserve(vertex_samples);
  decoded.indices.reserve(index_samples);

  for (std::uint32_t i = 0; i < vertex_samples; ++i) {
    unsigned char bytes[12]{};
    const std::uint64_t offset = payload_start_offset +
        static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(metadata.vertex_stride);
    if (!read_exact_at(input, offset, bytes, sizeof(bytes))) return false;
    const DecodedVertex vertex{read_le_f32(bytes), read_le_f32(bytes + 4), read_le_f32(bytes + 8)};
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
      return false;
    }
    if (!decoded.bounds.valid) {
      decoded.bounds = {vertex.x, vertex.y, vertex.z, vertex.x, vertex.y, vertex.z, true};
    } else {
      decoded.bounds.min_x = std::min(decoded.bounds.min_x, vertex.x);
      decoded.bounds.min_y = std::min(decoded.bounds.min_y, vertex.y);
      decoded.bounds.min_z = std::min(decoded.bounds.min_z, vertex.z);
      decoded.bounds.max_x = std::max(decoded.bounds.max_x, vertex.x);
      decoded.bounds.max_y = std::max(decoded.bounds.max_y, vertex.y);
      decoded.bounds.max_z = std::max(decoded.bounds.max_z, vertex.z);
    }
    decoded.vertices.push_back(vertex);
  }

  const std::uint64_t index_stream_offset = payload_start_offset + metadata.vertex_byte_size;
  for (std::uint32_t i = 0; i < index_samples; ++i) {
    unsigned char bytes[4]{};
    const std::uint64_t offset = index_stream_offset +
        static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(metadata.index_size);
    if (!read_exact_at(input, offset, bytes, metadata.index_size)) return false;
    const std::uint32_t index = metadata.index_size == 2 ? read_le_u16(bytes) : read_le_u32(bytes);
    if (index >= metadata.vertex_count) return false;
    decoded.indices.push_back(index);
  }
  if (!decoded.bounds.valid) return false;

  geometries_.push_back(std::move(metadata));
  decoded_.push_back(std::move(decoded));
  return true;
}

const NativeGeometryMetadata* NativeGeometryDatabase::find(
    const std::string& buffer_id) const noexcept {
  const auto it = std::find_if(geometries_.begin(), geometries_.end(),
                               [&buffer_id](const NativeGeometryMetadata& metadata) {
                                 return metadata.buffer_id == buffer_id;
                               });
  return it == geometries_.end() ? nullptr : &*it;
}

const DecodedGeometry* NativeGeometryDatabase::decoded(
    const std::string& buffer_id) const noexcept {
  const auto it = std::find_if(decoded_.begin(), decoded_.end(),
                               [&buffer_id](const DecodedGeometry& decoded) {
                                 return decoded.buffer_id == buffer_id;
                               });
  return it == decoded_.end() ? nullptr : &*it;
}

bool NativeRenderTarget::resize(std::uint32_t width, std::uint32_t height) {
  constexpr std::uint32_t max_dimension = 4096;
  if (width == 0 || height == 0 || width > max_dimension || height > max_dimension) {
    return false;
  }
  const std::uint64_t pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  if (pixels > 16u * 1024u * 1024u) return false;
  width_ = width;
  height_ = height;
  color_.assign(static_cast<std::size_t>(pixels), 0);
  depth_.assign(static_cast<std::size_t>(pixels), 1.0f);
  geometry_calls_ = 0;
  raster_triangles_ = 0;
  raster_writes_ = 0;
  return true;
}

bool NativeRenderTarget::clear(std::uint32_t color, float depth) {
  if (width_ == 0 || height_ == 0 || color_.empty() || depth_.empty() || !std::isfinite(depth)) {
    return false;
  }
  std::fill(color_.begin(), color_.end(), color);
  std::fill(depth_.begin(), depth_.end(), depth);
  geometry_calls_ = 0;
  raster_triangles_ = 0;
  raster_writes_ = 0;
  return true;
}

bool NativeRenderTarget::mark_world_asset(const WorldFrame& frame, AssetId asset,
                                          std::uint32_t ordinal) noexcept {
  if (width_ == 0 || height_ == 0 || color_.empty() || depth_.empty() || asset == 0 ||
      !frame.mission_ready) {
    return false;
  }
  const std::uint64_t seed = static_cast<std::uint64_t>(asset) * 2654435761ull +
                             static_cast<std::uint64_t>(frame.mission_id) * 2246822519ull +
                             static_cast<std::uint64_t>(ordinal) * 3266489917ull;
  const std::uint32_t x = static_cast<std::uint32_t>(seed % width_);
  const std::uint32_t y = static_cast<std::uint32_t>((seed / width_) % height_);
  const std::size_t index = static_cast<std::size_t>(y) * width_ + x;
  color_[index] = 0xFF000000u | ((asset & 0xFFu) << 16) |
                  ((frame.mission_id & 0xFFu) << 8) | (ordinal & 0xFFu);
  depth_[index] = 1.0f / static_cast<float>(ordinal + 2u);
  return true;
}

bool NativeRenderTarget::draw_world_asset(const WorldFrame& frame,
                                          const MissionDrawable& drawable,
                                          std::uint32_t ordinal) noexcept {
  if (drawable.mission_id != frame.mission_id || drawable.stable_id.empty() ||
      drawable.kind.empty() || drawable.asset == 0 ||
      drawable.primitive_count == 0 || !drawable.has_buffer_contract()) {
    return false;
  }
  const std::uint32_t samples = std::min<std::uint32_t>(drawable.primitive_count, 64u);
  for (std::uint32_t sample = 0; sample < samples; ++sample) {
    if (!mark_world_asset(frame, drawable.asset, ordinal * 131u + sample)) return false;
  }
  return true;
}

bool NativeRenderTarget::draw_world_geometry(const WorldFrame& frame,
                                             const MissionDrawable& drawable,
                                             const NativeGeometryMetadata& geometry,
                                             const DecodedGeometry& decoded,
                                             const MissionDrawableTransform& transform,
                                             const MissionMaterial& material,
                                             const MissionTextureBinding& texture,
                                             const ShaderPermutation& shader,
                                             const MissionRenderTargetDefinition& render_target,
                                             const MissionRenderTargetDefinition& destination_target,
                                             const MissionRenderPass& pass,
                                             const MissionRenderResolve& resolve,
                                             const MissionCameraDefinition* camera,
                                             const MissionTextureDatabase* texture_database,
                                             std::uint32_t ordinal) noexcept {
  const bool msaa_resolve = resolve.mode == "msaa_resolve";
  const bool sample_contract_valid =
      msaa_resolve ? (render_target.sample_count > 1 && destination_target.sample_count == 1)
                   : (resolve.mode == "tonemap" ? destination_target.sample_count == 1
                                                : render_target.sample_count == destination_target.sample_count);
  if (width_ == 0 || height_ == 0 || color_.empty() || depth_.empty() ||
      !frame.mission_ready ||
      drawable.mission_id != frame.mission_id || drawable.stable_id.empty() ||
      drawable.kind.empty() || drawable.asset == 0 ||
      drawable.primitive_count == 0 || !drawable.has_buffer_contract() ||
      !transform.valid() || transform.mission_id != drawable.mission_id ||
      transform.stable_id != drawable.stable_id ||
      !material.valid() || material.mission_id != drawable.mission_id ||
      material.stable_id != drawable.stable_id ||
      !texture.valid() || texture.mission_id != drawable.mission_id ||
      texture.stable_id != drawable.stable_id ||
      !shader.valid() || shader.id != material.shader_permutation ||
      !render_target.valid() || render_target.mission_id != frame.mission_id ||
      render_target.target_id != pass.color_target ||
      render_target.width != width_ || render_target.height != height_ ||
      render_target.color_format != shader.render_target_format ||
      (render_target.depth_enabled && render_target.depth_format != "d24s8") ||
      (!render_target.depth_enabled && (material.depth_test || material.depth_write)) ||
      !destination_target.valid() || destination_target.mission_id != frame.mission_id ||
      destination_target.target_id != resolve.destination_target ||
      destination_target.width != width_ || destination_target.height != height_ ||
      destination_target.color_format != render_target.color_format ||
      destination_target.depth_enabled || destination_target.depth_format != "none" ||
      !sample_contract_valid ||
      !pass.valid() || pass.mission_id != frame.mission_id || pass.pass_id != "world" ||
      (render_target.depth_enabled ? pass.depth_target != "main_depth" : pass.depth_target != "none") ||
      !resolve.valid() || resolve.mission_id != frame.mission_id ||
      resolve.source_pass != pass.pass_id || resolve.source_target != pass.color_target ||
      resolve.destination_target != "present" ||
      geometry.buffer_id != drawable.buffer_id ||
      decoded.buffer_id != drawable.buffer_id ||
      geometry.vertex_count != drawable.vertex_count ||
      geometry.index_count != drawable.index_count ||
      geometry.primitive_count != drawable.primitive_count ||
      geometry.vertex_section_count != drawable.vertex_count ||
      geometry.index_section_count != drawable.index_count ||
      geometry.polygon_descriptor_count != drawable.primitive_count ||
      geometry.vertex_stride == 0 || geometry.index_size == 0 ||
      geometry.vertex_byte_size == 0 || geometry.index_byte_size == 0 ||
      decoded.vertices.empty() || decoded.indices.empty() || !decoded.bounds.valid ||
      !std::isfinite(decoded.bounds.min_x) || !std::isfinite(decoded.bounds.min_y) ||
      !std::isfinite(decoded.bounds.min_z) || !std::isfinite(decoded.bounds.max_x) ||
      !std::isfinite(decoded.bounds.max_y) || !std::isfinite(decoded.bounds.max_z) ||
      decoded.bounds.min_x > decoded.bounds.max_x ||
      decoded.bounds.min_y > decoded.bounds.max_y ||
      decoded.bounds.min_z > decoded.bounds.max_z) {
    return false;
  }

  NativeCameraProjection projection;
  if (camera == nullptr && !make_projection(frame, width_, height_, projection)) return false;
  const auto project = [camera, &projection, this](Vec3 world, ScreenPoint& screen) noexcept {
    return camera != nullptr ? project_clip_point(*camera, world, width_, height_, screen)
                             : project_point(projection, world, width_, height_, screen);
  };
  const auto project_clamped = [camera, &projection, this](Vec3 world,
                                                            ScreenPoint& screen) noexcept {
    return camera != nullptr ? project_clip_point(*camera, world, width_, height_, screen, false)
                             : project_point(projection, world, width_, height_, screen, false);
  };

  const auto shade = [&material, &texture, &shader, &pass, &resolve, &render_target,
                      &destination_target, texture_database](std::uint32_t salt,
                                                              float u, float v) noexcept {
    const std::uint32_t texture_salt =
        static_cast<std::uint32_t>(texture.content_hash) ^
        static_cast<std::uint32_t>(texture.content_hash >> 32u) ^
        static_cast<std::uint32_t>(texture.gidx) ^
        static_cast<std::uint32_t>(texture.gidx >> 32u) ^
        (texture.sampler_filter == "linear" ? 0x13579BDFu : 0x2468ACE0u) ^
        (texture.sampler_address == "wrap" ? 0x10203040u : 0x50607080u);
    const std::uint32_t shader_salt =
        stable_hash32(shader.id) ^ stable_hash32(shader.vertex_layout) ^
        (shader.texture_fetches * 0x01010101u) ^
        (shader.constant_count * 0x00010001u) ^
        stable_hash32(shader.render_target_format) ^
        stable_hash32(pass.pass_id) ^ (pass.order * 0x11111111u) ^
        stable_hash32(pass.color_target) ^ stable_hash32(pass.depth_target) ^
        pass.clear_color ^ stable_hash32(resolve.source_target) ^
        stable_hash32(resolve.destination_target) ^ stable_hash32(resolve.mode) ^
        (render_target.sample_count * 0x01020408u) ^
        (destination_target.sample_count * 0x08040201u);
    const std::uint32_t material_salt = static_cast<std::uint32_t>(material.mate_id) ^
                                        static_cast<std::uint32_t>(material.mate_id >> 32u);
    std::uint32_t sampled = 0;
    const bool has_sample = texture_database != nullptr &&
        texture_database->sample(texture.mission_id, texture.stable_id, u, v, sampled);
    const std::uint32_t rgb = (has_sample ? sampled : material.base_color) & 0x00FFFFFFu;
    // Preserve the qualified texture signal.  The old diagnostic XOR/shift
    // path made every surface nearly black and hid useful retail albedo;
    // identity salts now only provide a bounded, deterministic lighting
    // variation around the source color.
    const std::uint32_t identity = salt ^ texture_salt ^ shader_salt ^ material_salt;
    const float gain = 0.78f + static_cast<float>(identity & 0x3Fu) / 256.0f;
    const auto modulate = [gain](std::uint32_t value) {
      return static_cast<std::uint32_t>(std::clamp(value * gain, 0.0f, 255.0f));
    };
    return (material.base_color & 0xFF000000u) |
           (modulate((rgb >> 16u) & 0xFFu) << 16u) |
           (modulate((rgb >> 8u) & 0xFFu) << 8u) |
           modulate(rgb & 0xFFu);
  };
  ++geometry_calls_;
  const auto write_projected = [this, &material, &shade](ScreenPoint screen,
                                                         std::uint32_t salt,
                                                         float u, float v) noexcept {
    const std::size_t pixel = static_cast<std::size_t>(screen.y) * width_ + screen.x;
    const float depth_value = std::max(0.0f, std::min(0.999999f, screen.depth / 4096.0f));
    if (material.depth_test && depth_value >= depth_[pixel]) return false;
    const std::uint32_t shaded = shade(salt, u, v);
    if (material.blend_mode == "opaque") {
      color_[pixel] = shaded;
    } else if (material.blend_mode == "alpha") {
      const std::uint32_t dst = color_[pixel];
      const std::uint32_t src_a = (shaded >> 24u) & 0xFFu;
      const std::uint32_t inv_a = 255u - src_a;
      const std::uint32_t src_r = (shaded >> 16u) & 0xFFu;
      const std::uint32_t src_g = (shaded >> 8u) & 0xFFu;
      const std::uint32_t src_b = shaded & 0xFFu;
      const std::uint32_t dst_r = (dst >> 16u) & 0xFFu;
      const std::uint32_t dst_g = (dst >> 8u) & 0xFFu;
      const std::uint32_t dst_b = dst & 0xFFu;
      color_[pixel] = 0xFF000000u |
                      (((src_r * src_a + dst_r * inv_a) / 255u) << 16u) |
                      (((src_g * src_a + dst_g * inv_a) / 255u) << 8u) |
                      ((src_b * src_a + dst_b * inv_a) / 255u);
    } else {
      const std::uint32_t dst = color_[pixel];
      const std::uint32_t r = std::min(255u, ((dst >> 16u) & 0xFFu) + ((shaded >> 16u) & 0xFFu));
      const std::uint32_t g = std::min(255u, ((dst >> 8u) & 0xFFu) + ((shaded >> 8u) & 0xFFu));
      const std::uint32_t b = std::min(255u, (dst & 0xFFu) + (shaded & 0xFFu));
      color_[pixel] = 0xFF000000u | (r << 16u) | (g << 8u) | b;
    }
    if (material.depth_write) depth_[pixel] = depth_value;
    ++raster_writes_;
    return true;
  };

  const auto to_world = [&transform](const DecodedVertex& vertex) noexcept {
    return Vec3{vertex.x * transform.scale_x + transform.translate_x,
                vertex.y * transform.scale_y + transform.translate_y,
                vertex.z * transform.scale_z + transform.translate_z};
  };

  std::uint32_t projected_samples = 0;
  for (std::uint32_t i = 0; i < decoded.vertices.size(); ++i) {
    const DecodedVertex& vertex = decoded.vertices[i];
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
      return false;
    }
    const Vec3 world = to_world(vertex);
    if (!std::isfinite(world.x) || !std::isfinite(world.y) || !std::isfinite(world.z)) {
      return false;
    }
    ScreenPoint screen;
    if (!project(world, screen)) continue;
    if (write_projected(screen, drawable.asset ^ ordinal ^ i ^
                                    static_cast<std::uint32_t>(std::abs(static_cast<int>(world.y))),
                        vertex.u, vertex.v)) {
      ++projected_samples;
    }
  }

  for (std::uint32_t i = 0; i < decoded.indices.size(); ++i) {
    const std::uint32_t index = decoded.indices[i];
    if (index == std::numeric_limits<std::uint32_t>::max()) continue;
    if (index >= geometry.vertex_count) return false;
    if (index >= decoded.vertices.size()) continue;
    const Vec3 world = to_world(decoded.vertices[index]);
    ScreenPoint screen;
    if (!project(world, screen)) continue;
    if (write_projected(screen, drawable.asset ^ (index << 8u) ^ i ^ ordinal,
                        decoded.vertices[index].u, decoded.vertices[index].v)) {
      ++projected_samples;
    }
  }

  // Submit indexed triangles. Real NDXR polygons are triangle strips with
  // primitive-restart markers; older developer fixtures remain triangle lists.
  const auto raster_triangle = [&](std::uint32_t ia, std::uint32_t ib,
                                   std::uint32_t ic, std::uint32_t salt) {
    if (ia >= decoded.vertices.size() || ib >= decoded.vertices.size() ||
        ic >= decoded.vertices.size()) return false;
    ScreenPoint a{}, b{}, c{};
    const bool projected_a = project(to_world(decoded.vertices[ia]), a);
    const bool projected_b = project(to_world(decoded.vertices[ib]), b);
    const bool projected_c = project(to_world(decoded.vertices[ic]), c);
    if (!projected_a || !projected_b || !projected_c) {
      // Keep triangles intersecting the viewport. Full homogeneous clipping
      // is deferred until the qualified Xenos camera contract is available;
      // clamping here avoids dropping every large retail terrain triangle.
      if (!project_clamped(to_world(decoded.vertices[ia]), a) ||
          !project_clamped(to_world(decoded.vertices[ib]), b) ||
          !project_clamped(to_world(decoded.vertices[ic]), c)) return true;
    }
    if (!std::isfinite(a.depth) || !std::isfinite(b.depth) || !std::isfinite(c.depth)) {
      return true;
    }
    const float area = (static_cast<float>(b.x) - a.x) * (static_cast<float>(c.y) - a.y) -
                       (static_cast<float>(b.y) - a.y) * (static_cast<float>(c.x) - a.x);
    if (!std::isfinite(area) || std::abs(area) < 0.5f) return true;
    ++raster_triangles_;
    const std::uint32_t min_x = std::min({a.x, b.x, c.x});
    const std::uint32_t max_x = std::max({a.x, b.x, c.x});
    const std::uint32_t min_y = std::min({a.y, b.y, c.y});
    const std::uint32_t max_y = std::max({a.y, b.y, c.y});
    const auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
      return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    };
    const float inverse_area = 1.0f / area;
    for (std::uint32_t y = min_y; y <= max_y; ++y) {
      for (std::uint32_t x = min_x; x <= max_x; ++x) {
        const float px = static_cast<float>(x) + 0.5f;
        const float py = static_cast<float>(y) + 0.5f;
        const float wa = edge(static_cast<float>(b.x), static_cast<float>(b.y),
                              static_cast<float>(c.x), static_cast<float>(c.y), px, py) * inverse_area;
        const float wb = edge(static_cast<float>(c.x), static_cast<float>(c.y),
                              static_cast<float>(a.x), static_cast<float>(a.y), px, py) * inverse_area;
        const float wc = edge(static_cast<float>(a.x), static_cast<float>(a.y),
                              static_cast<float>(b.x), static_cast<float>(b.y), px, py) * inverse_area;
        if (wa < -0.001f || wb < -0.001f || wc < -0.001f) continue;
        const ScreenPoint pixel{x, y, wa * a.depth + wb * b.depth + wc * c.depth};
        const float u = wa * decoded.vertices[ia].u + wb * decoded.vertices[ib].u +
                        wc * decoded.vertices[ic].u;
        const float v = wa * decoded.vertices[ia].v + wb * decoded.vertices[ib].v +
                        wc * decoded.vertices[ic].v;
        if (write_projected(pixel, drawable.asset ^ ordinal ^ salt, u, v)) ++projected_samples;
      }
    }
    return true;
  };
  if (geometry.topology == NativeIndexTopology::TriangleStripRestart) {
    std::array<std::uint32_t, 3> strip{};
    std::size_t strip_count = 0;
    for (std::size_t i = 0; i <= decoded.indices.size(); ++i) {
      const bool restart = i == decoded.indices.size() ||
          decoded.indices[i] == std::numeric_limits<std::uint32_t>::max();
      if (restart) {
        strip_count = 0;
        continue;
      }
      strip[strip_count++ % 3u] = decoded.indices[i];
      if (strip_count >= 3u) {
        std::uint32_t a = strip[(strip_count - 3u) % 3u];
        std::uint32_t b = strip[(strip_count - 2u) % 3u];
        const std::uint32_t c = strip[(strip_count - 1u) % 3u];
        if ((strip_count & 1u) == 0u) std::swap(a, b);
        if (!raster_triangle(a, b, c, static_cast<std::uint32_t>(i))) return false;
      }
    }
  } else {
    for (std::uint32_t i = 0; i + 2u < decoded.indices.size(); i += 3u) {
      const std::uint32_t ia = decoded.indices[i];
      const std::uint32_t ib = decoded.indices[i + 1u];
      const std::uint32_t ic = decoded.indices[i + 2u];
      if (ia == std::numeric_limits<std::uint32_t>::max() ||
          ib == std::numeric_limits<std::uint32_t>::max() ||
          ic == std::numeric_limits<std::uint32_t>::max()) continue;
      if (!raster_triangle(ia, ib, ic, i)) return false;
    }
  }

  const float world_min_x = decoded.bounds.min_x * transform.scale_x + transform.translate_x;
  const float world_min_y = decoded.bounds.min_y * transform.scale_y + transform.translate_y;
  const float world_min_z = decoded.bounds.min_z * transform.scale_z + transform.translate_z;
  const float world_max_x = decoded.bounds.max_x * transform.scale_x + transform.translate_x;
  const float world_max_y = decoded.bounds.max_y * transform.scale_y + transform.translate_y;
  const float world_max_z = decoded.bounds.max_z * transform.scale_z + transform.translate_z;
  if (!std::isfinite(world_min_x) || !std::isfinite(world_min_y) ||
      !std::isfinite(world_min_z) || !std::isfinite(world_max_x) ||
      !std::isfinite(world_max_y) || !std::isfinite(world_max_z) ||
      world_min_x > world_max_x || world_min_y > world_max_y || world_min_z > world_max_z) {
    return false;
  }
  const float span_x = std::max(1.0f, world_max_x - world_min_x);
  const float span_z = std::max(1.0f, world_max_z - world_min_z);
  ScreenPoint bounds_screen_0;
  ScreenPoint bounds_screen_1;
  const bool projected_bound_0 = project({world_min_x, world_min_y, world_min_z}, bounds_screen_0);
  const bool projected_bound_1 = project({world_max_x, world_max_y, world_max_z}, bounds_screen_1);
  if (projected_bound_0) {
    if (write_projected(bounds_screen_0, drawable.asset ^ ordinal ^
                                         static_cast<std::uint32_t>(span_x + span_z), 0.0f, 0.0f)) {
      ++projected_samples;
    }
  }
  if (projected_bound_1) {
    if (write_projected(bounds_screen_1, drawable.asset ^ ordinal ^
                                         static_cast<std::uint32_t>(
                                             std::abs(static_cast<int>(world_max_y - world_min_y))), 0.0f, 0.0f)) {
      ++projected_samples;
    }
  }

  // A valid draw may be completely clipped by the qualified camera. Treat it
  // as a successful no-op; rejecting the whole frame here would make a
  // multi-draw scene fail merely because one LOD is outside the frustum.
  (void)projected_samples;
  return true;
}

RenderReadback NativeRenderTarget::readback() const noexcept {
  RenderReadback readback{width_, height_, 0, 0, 1469598103934665603ull, 1469598103934665603ull};
  if (width_ == 0 || height_ == 0 || color_.size() != depth_.size()) return readback;
  for (std::size_t i = 0; i < color_.size(); ++i) {
    const std::uint32_t color = color_[i];
    readback.color_hash ^= color;
    readback.color_hash *= 1099511628211ull;
    if (color != 0) ++readback.color_coverage;

    const auto depth_quantized = static_cast<std::uint32_t>(
        std::max(0.0f, std::min(depth_[i], 1.0f)) * 16777215.0f);
    readback.depth_hash ^= depth_quantized;
    readback.depth_hash *= 1099511628211ull;
    if (depth_[i] < 1.0f) ++readback.depth_coverage;
  }
  return readback;
}

bool NativeRenderTarget::copy_rgba8(std::vector<std::uint8_t>& pixels) const {
  if (width_ == 0 || height_ == 0 || color_.size() != static_cast<std::size_t>(width_) * height_) {
    return false;
  }
  std::vector<std::uint8_t> converted;
  converted.resize(color_.size() * 4u);
  for (std::size_t i = 0; i < color_.size(); ++i) {
    const std::uint32_t pixel = color_[i];
    converted[i * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xFFu);
    converted[i * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xFFu);
    converted[i * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xFFu);
    converted[i * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xFFu);
  }
  pixels = std::move(converted);
  return true;
}

bool NativeRenderTarget::copy_depth(std::vector<float>& depth) const {
  if (width_ == 0 || height_ == 0 ||
      depth_.size() != static_cast<std::size_t>(width_) * height_) return false;
  depth = depth_;
  return true;
}

bool NativeRenderTarget::write_ppm(const std::filesystem::path& path) const noexcept {
  if (path.empty() || width_ == 0 || height_ == 0 ||
      color_.size() != static_cast<std::size_t>(width_) * height_) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << "P6\n" << width_ << ' ' << height_ << "\n255\n";
  if (!output) return false;
  for (const std::uint32_t pixel : color_) {
    const unsigned char rgb[3] = {
        static_cast<unsigned char>((pixel >> 16u) & 0xFFu),
        static_cast<unsigned char>((pixel >> 8u) & 0xFFu),
        static_cast<unsigned char>(pixel & 0xFFu)};
    output.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
    if (!output) return false;
  }
  return true;
}

bool NativeRenderTarget::write_depth_f32(const std::filesystem::path& path) const noexcept {
  if (path.empty() || width_ == 0 || height_ == 0 ||
      depth_.size() != static_cast<std::size_t>(width_) * height_) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output.write(reinterpret_cast<const char*>(depth_.data()),
               static_cast<std::streamsize>(depth_.size() * sizeof(float)));
  return output.good();
}

MissionRuntime::MissionRuntime(std::uint32_t mission_id, const MissionAssetDatabase* assets)
    : mission_id_(mission_id), assets_(assets) {}

MissionRuntime::MissionRuntime(const MissionDefinition& definition,
                               const MissionAssetDatabase* assets)
    : mission_id_(definition.id), assets_(assets), definition_(&definition) {}

RuntimeSnapshot MissionRuntime::snapshot() const noexcept {
  return {tick_, position_x_, position_y_, position_z_, pitch_, roll_, yaw_, fixed_accumulator_};
}

bool MissionRuntime::restore(RuntimeSnapshot snapshot) noexcept {
  if (snapshot.tick == 0 || !std::isfinite(snapshot.position_x) ||
      !std::isfinite(snapshot.position_y) || !std::isfinite(snapshot.position_z) ||
      !std::isfinite(snapshot.pitch) || !std::isfinite(snapshot.roll) ||
      !std::isfinite(snapshot.yaw) || !std::isfinite(snapshot.fixed_accumulator) ||
      snapshot.fixed_accumulator < 0.0f || snapshot.fixed_accumulator >= 1.0f / 60.0f) return false;
  tick_ = snapshot.tick;
  position_x_ = snapshot.position_x;
  position_y_ = snapshot.position_y;
  position_z_ = snapshot.position_z;
  pitch_ = snapshot.pitch;
  roll_ = snapshot.roll;
  yaw_ = snapshot.yaw;
  fixed_accumulator_ = snapshot.fixed_accumulator;
  return true;
}

bool MissionRuntime::set_definition(const MissionDefinition* definition) noexcept {
  if (definition == nullptr || definition->id != mission_id_ ||
      definition->family == MissionFamily::Unknown || definition->asset_ids.empty()) {
    definition_ = nullptr;
    return false;
  }
  definition_ = definition;
  return true;
}

WorldFrame MissionRuntime::tick(float fixed_dt, InputFrame input) {
  const bool scheduler_stopped = scenario_ != nullptr &&
      (scenario_->state() == ScenarioState::Paused ||
       scenario_->state() == ScenarioState::Complete ||
       scenario_->state() == ScenarioState::Aborted);
  if (!scheduler_stopped) {
    if (!(fixed_dt > 0.0f) || fixed_dt > 0.25f) fixed_dt = 1.0f / 60.0f;
    constexpr float simulation_dt = 1.0f / 60.0f;
    constexpr std::uint32_t max_steps_per_call = 8;
    fixed_accumulator_ = std::min(fixed_accumulator_ + fixed_dt, 0.25f);
    const auto axis = [](std::int16_t value) { return static_cast<float>(value) / 32767.0f; };
    std::uint32_t steps = 0;
    while (fixed_accumulator_ + 1.0e-7f >= simulation_dt && steps < max_steps_per_call) {
      fixed_accumulator_ -= simulation_dt;
      ++tick_;
      pitch_ += axis(input.pitch) * simulation_dt;
      roll_ += axis(input.roll) * simulation_dt;
      yaw_ += axis(input.yaw) * simulation_dt;
      position_x_ += yaw_ * simulation_dt;
      position_y_ += pitch_ * simulation_dt;
      position_z_ += (static_cast<float>(input.throttle) / 255.0f) * simulation_dt;
      ++steps;
    }
  }
  bool ready = assets_ != nullptr && definition_ != nullptr && scenario_ != nullptr &&
               scenario_->state() == ScenarioState::Gameplay;
  if (ready) {
    for (const AssetId id : definition_->asset_ids) {
      ready = assets_->resolve(id) != nullptr;
      if (!ready) break;
    }
  }
  const auto active_units = units_ ? static_cast<std::uint32_t>(units_->active_count()) : 0u;
  const auto player = scenario_ ? scenario_->player() : EntityId{};
  constexpr float follow_distance = 12.0f;
  constexpr float follow_height = 3.0f;
  return WorldFrame{tick_, mission_id_, ready, position_x_, position_y_, position_z_, pitch_, roll_, yaw_,
                    active_units, player, position_x_ - follow_distance, position_y_ + follow_height,
                    position_z_ + follow_distance, position_x_, position_y_, position_z_, input};
}

MissionExecution::MissionExecution(const MissionDefinition& definition,
                                   const MissionAssetDatabase* assets,
                                   const MissionObjectiveDatabase* objectives,
                                   const RadioMessageDatabase* radios,
                                   CampaignProgression* campaign,
                                   MissionWaveDirector* waves,
                                   MissionSequenceDirector* sequence,
                                   const InputMappingDatabase* input)
    : definition_(&definition), objectives_(objectives), radios_(radios), campaign_(campaign),
      waves_(waves), sequence_(sequence), input_(input),
      runtime_(definition, assets),
      scenario_(definition) {}

bool MissionExecution::launch(const MissionLaunchDefinition& launch) noexcept {
  if (definition_ == nullptr || launch.mission_id != definition_->id) return false;
  if (campaign_ == nullptr) {
    // The standalone runtime path remains available for developer fixtures.
  } else {
    const CampaignMissionStatus* status = campaign_->status(definition_->id);
    if (status == nullptr || status->state != CampaignMissionState::Active) return false;
  }
  units_ = UnitRegistry{};
  combat_.clear();
  radio_.reset();
  if (waves_ != nullptr) waves_->reset();
  if (sequence_ != nullptr) sequence_->reset();
  scenario_ = MissionScenario(*definition_);
  if (objectives_ != nullptr) {
    for (const ObjectiveRecord* objective : objectives_->find_by_mission(definition_->id)) {
      if (objective == nullptr || !scenario_.add_objective(*objective)) {
        units_ = UnitRegistry{};
        scenario_ = MissionScenario(*definition_);
        launched_ = false;
        return false;
      }
    }
  }
  if (!configure_mission_launch(launch, units_, scenario_) ||
      !scenario_.dispatch({EventType::StartMission, launch.player_entity})) {
    units_ = UnitRegistry{};
    combat_.clear();
    scenario_ = MissionScenario(*definition_);
    launched_ = false;
    return false;
  }
  std::size_t spawn_index = 0;
  for (const UnitRecord& unit : launch.units) {
    const float spawn_x = unit.id == launch.player_entity
        ? 0.0f
        : 20.0f + static_cast<float>(spawn_index++) * 5.0f;
    if (!combat_.add_unit({unit.id, unit.owner, {spawn_x, 0.0f, 0.0f},
                           100.0f, 100.0f, 1.0f, true})) {
      units_ = UnitRegistry{};
      combat_.clear();
      scenario_ = MissionScenario(*definition_);
      launched_ = false;
      return false;
    }
  }
  runtime_.set_definition(definition_);
  runtime_.set_units(&units_);
  runtime_.set_scenario(&scenario_);
  launched_ = true;
  return true;
}

bool MissionExecution::dispatch(Event event) noexcept {
  if (!launched_) return false;
  if (event.type == EventType::Complete && campaign_ != nullptr &&
      !campaign_->can_complete(definition_->id)) return false;
  if (event.type == EventType::Abort && campaign_ != nullptr) {
    const CampaignMissionStatus* status = campaign_->status(definition_->id);
    if (status == nullptr || status->state != CampaignMissionState::Active) return false;
  }
  if (!scenario_.dispatch(event)) return false;
  if (event.type == EventType::Complete && campaign_ != nullptr) {
    return campaign_->complete(definition_->id);
  }
  if (event.type == EventType::Abort && campaign_ != nullptr) {
    return campaign_->fail(definition_->id);
  }
  return true;
}

bool MissionExecution::activate_objective(std::uint32_t id) noexcept {
  return launched_ && scenario_.activate_objective(id);
}

bool MissionExecution::complete_objective(std::uint32_t id) noexcept {
  if (!launched_) return false;
  const auto index = scenario_.objective_index(id);
  if (!index || (campaign_ != nullptr &&
                 !campaign_->can_complete_objective(definition_->id, *index))) return false;
  if (!scenario_.complete_objective(id)) return false;
  return campaign_ == nullptr || campaign_->complete_objective(definition_->id, *index);
}

bool MissionExecution::fail_objective(std::uint32_t id) noexcept {
  if (!launched_ || !scenario_.fail_objective(id)) return false;
  return campaign_ == nullptr || campaign_->fail(definition_->id);
}

bool MissionExecution::dispatch_radio(std::uint32_t id) noexcept {
  return launched_ && radios_ != nullptr && scenario_.dispatch_radio(*radios_, id);
}

bool MissionExecution::play_radio(std::uint32_t id, float duration_seconds) noexcept {
  if (!launched_ || radios_ == nullptr || !radio_.start(*radios_, definition_->id, id,
                                                          duration_seconds)) return false;
  if (scenario_.dispatch_radio(*radios_, id)) return true;
  radio_.reset();
  return false;
}

bool MissionSequenceDirector::dispatch_due(std::uint32_t mission_id, std::uint64_t tick,
                                           MissionExecution& execution) noexcept {
  for (Entry& entry : entries_) {
    if (entry.published || entry.event.mission_id != mission_id || entry.event.tick > tick) {
      continue;
    }
    bool dispatched = false;
    switch (entry.event.type) {
      case MissionSequenceEventType::ActivateObjective:
        dispatched = execution.activate_objective(entry.event.id);
        break;
      case MissionSequenceEventType::CompleteObjective:
        dispatched = execution.complete_objective(entry.event.id);
        break;
      case MissionSequenceEventType::FailObjective:
        dispatched = execution.fail_objective(entry.event.id);
        break;
      case MissionSequenceEventType::PlayRadio:
        dispatched = execution.play_radio(entry.event.id, entry.event.duration_seconds);
        break;
    }
    if (!dispatched) return false;
    entry.published = true;
  }
  return true;
}

std::size_t MissionSequenceDirector::pending(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.event.mission_id == mission_id && !entry.published;
      }));
}

std::size_t MissionSequenceDirector::dispatched(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.event.mission_id == mission_id && entry.published;
      }));
}

MissionSequenceSnapshot MissionSequenceDirector::snapshot() const {
  MissionSequenceSnapshot result;
  result.entries.reserve(entries_.size());
  for (const Entry& entry : entries_) result.entries.push_back({entry.event, entry.published});
  return result;
}

bool MissionSequenceDirector::restore(const MissionSequenceSnapshot& snapshot) noexcept {
  if (snapshot.entries.size() > 4096) return false;
  std::vector<Entry> loaded;
  loaded.reserve(snapshot.entries.size());
  std::uint32_t previous_mission = 0;
  std::uint64_t previous_tick = 0;
  std::uint32_t previous_order = 0;
  for (const MissionSequenceEntrySnapshot& candidate : snapshot.entries) {
    const MissionSequenceEvent& event = candidate.event;
    if (!event.valid() ||
        (event.mission_id < previous_mission) ||
        (event.mission_id == previous_mission && event.tick < previous_tick) ||
        (event.mission_id == previous_mission && event.tick == previous_tick &&
         event.order <= previous_order)) return false;
    loaded.push_back({event, candidate.published});
    previous_mission = event.mission_id;
    previous_tick = event.tick;
    previous_order = event.order;
  }
  entries_ = std::move(loaded);
  return true;
}

void MissionSequenceDirector::reset() noexcept {
  for (Entry& entry : entries_) entry.published = false;
}

bool MissionExecution::lock_target(EntityId target) noexcept {
  return launched_ && combat_.lock_target(scenario_.player(), target);
}

bool MissionExecution::fire_weapon(std::uint32_t weapon_id) noexcept {
  return launched_ && combat_.fire(scenario_.player(), weapon_id);
}

WorldFrame MissionExecution::tick(float fixed_dt, InputFrame input) noexcept {
  if (!launched_) return {};
  if (input_ != nullptr && input.buttons != 0) {
    const InputBinding* binding = input_->resolve(input.buttons);
    if (binding != nullptr && !dispatch({binding->event, scenario_.player()})) return {};
  }
  if (scenario_.state() == ScenarioState::Gameplay) combat_.tick(fixed_dt);
  WorldFrame frame = runtime_.tick(fixed_dt, input);
  if (scenario_.state() == ScenarioState::Gameplay) (void)radio_.tick(fixed_dt);
  if (scenario_.state() == ScenarioState::Gameplay && waves_ != nullptr &&
      !waves_->spawn_due(definition_->id, frame.tick, units_, combat_)) {
    frame.mission_ready = false;
    return frame;
  }
  if (scenario_.state() == ScenarioState::Gameplay && sequence_ != nullptr &&
      !sequence_->dispatch_due(definition_->id, frame.tick, *this)) {
    frame.mission_ready = false;
    return frame;
  }
  frame.active_units = static_cast<std::uint32_t>(units_.active_count());
  if (scenario_.state() == ScenarioState::Gameplay) {
    const CombatUnitState* player = combat_.unit(scenario_.player());
    const bool player_destroyed = player == nullptr || !player->active;
    const bool expired = failure_tick_ != 0 && frame.tick >= failure_tick_;
    if (player_destroyed || expired) {
      if (dispatch({EventType::Abort, scenario_.player()})) {
        frame.mission_ready = false;
        frame.active_units = static_cast<std::uint32_t>(combat_.active_units());
      }
    }
  }
  return frame;
}

WorldFrame MissionExecution::run_replay(float fixed_dt, const ReplayLog& replay) noexcept {
  WorldFrame frame{};
  for (const InputFrame input : replay.frames()) frame = tick(fixed_dt, input);
  return frame;
}

RuntimeSnapshot MissionExecution::snapshot() const noexcept {
  return runtime_.snapshot();
}

MissionDebrief MissionExecution::debrief() const {
  return scenario_.debrief();
}

bool MissionExecution::restore(RuntimeSnapshot snapshot) noexcept {
  return launched_ && runtime_.restore(snapshot);
}

bool MissionExecution::save_checkpoint(Checkpoint& checkpoint) const noexcept {
  if (!launched_ || runtime_.snapshot().tick == 0 || combat_.active_projectiles() != 0) {
    return false;
  }
  checkpoint.mission_id = definition_ == nullptr ? 0 : definition_->id;
  checkpoint.flight = runtime_.snapshot();
  checkpoint.scenario = scenario_.snapshot();
  checkpoint.combat_units = combat_.snapshot_units();
  checkpoint.failure_tick = failure_tick_;
  checkpoint.sequence = sequence_ == nullptr ? MissionSequenceSnapshot{} : sequence_->snapshot();
  checkpoint.radio_playback = radio_.snapshot();
  return checkpoint.mission_id != 0;
}

bool MissionExecution::restore_checkpoint(const Checkpoint& checkpoint) noexcept {
  if (!launched_ || definition_ == nullptr || checkpoint.mission_id != definition_->id ||
      checkpoint.scenario.mission_id != definition_->id ||
      checkpoint.combat_units.empty() ||
      (sequence_ == nullptr && !checkpoint.sequence.entries.empty())) return false;
  for (const MissionSequenceEntrySnapshot& entry : checkpoint.sequence.entries) {
    if (!entry.event.valid() || entry.event.mission_id != definition_->id) return false;
  }
  const RuntimeSnapshot old_flight = runtime_.snapshot();
  const MissionScenarioSnapshot old_scenario = scenario_.snapshot();
  const std::vector<CombatUnitState> old_units = combat_.snapshot_units();
  const std::uint64_t old_failure_tick = failure_tick_;
  const MissionSequenceSnapshot old_sequence =
      sequence_ == nullptr ? MissionSequenceSnapshot{} : sequence_->snapshot();
  const RadioPlaybackSnapshot old_radio = radio_.snapshot();
  if (!runtime_.restore(checkpoint.flight) || !scenario_.restore(checkpoint.scenario) ||
      !combat_.restore_units(checkpoint.combat_units) ||
      (sequence_ != nullptr && !sequence_->restore(checkpoint.sequence)) ||
      !radio_.restore(checkpoint.radio_playback)) {
    (void)runtime_.restore(old_flight);
    (void)scenario_.restore(old_scenario);
    (void)combat_.restore_units(old_units);
    failure_tick_ = old_failure_tick;
    if (sequence_ != nullptr) (void)sequence_->restore(old_sequence);
    (void)radio_.restore(old_radio);
    return false;
  }
  failure_tick_ = checkpoint.failure_tick;
  return true;
}

WorldFrame MissionRuntime::run_replay(float fixed_dt, const ReplayLog& replay) {
  WorldFrame frame{};
  for (const InputFrame input : replay.frames()) frame = tick(fixed_dt, input);
  return frame;
}

}  // namespace ac6
