#include "ac6/retail_scenario.h"

#include <bit>
#include <cstdio>
#include <cstring>

namespace ac6::retail {
namespace {

// The scenario root's slots, numbered as the dispatch table numbers them.
constexpr std::size_t kRootNode = 0;
constexpr std::size_t kSlotObjAndUnit = 0;
constexpr std::size_t kSlotSubMissions = 2;
constexpr std::size_t kSlotFactions = 5;
constexpr std::size_t kRootSlots = 10;

// Field offsets inside a unit record's data block, read at 0x820A70FC and
// 0x820A7118.
constexpr std::size_t kUnitClassByte = 0x08;
constexpr std::size_t kUnitFactionByte = 0x0D;

// The nine-way flag selector inside a faction entry, read at 0x820A7420.
constexpr std::size_t kFactionSideByte = 0x2C;
constexpr std::size_t kFactionWord = 0x28;

std::string format_float(float value) {
  // Must match Python's f"{value:.6f}" so the two generators are comparable.
  char text[64];
  std::snprintf(text, sizeof(text), "%.6f", static_cast<double>(value));
  return text;
}

}  // namespace

std::optional<ScenarioPayload> ScenarioPayload::open(std::vector<std::uint8_t> bytes) {
  if (bytes.size() < 8) return std::nullopt;
  return ScenarioPayload(std::move(bytes));
}

std::optional<std::uint8_t> ScenarioPayload::u8(std::size_t offset) const noexcept {
  if (offset >= bytes_.size()) return std::nullopt;
  return bytes_[offset];
}

std::optional<std::uint32_t> ScenarioPayload::u32(std::size_t offset) const noexcept {
  if (offset > bytes_.size() || bytes_.size() - offset < 4) return std::nullopt;
  return static_cast<std::uint32_t>(bytes_[offset]) << 24 |
         static_cast<std::uint32_t>(bytes_[offset + 1]) << 16 |
         static_cast<std::uint32_t>(bytes_[offset + 2]) << 8 |
         static_cast<std::uint32_t>(bytes_[offset + 3]);
}

std::optional<float> ScenarioPayload::f32(std::size_t offset) const noexcept {
  const std::optional<std::uint32_t> raw = u32(offset);
  if (!raw.has_value()) return std::nullopt;
  return std::bit_cast<float>(*raw);
}

std::optional<std::size_t> ScenarioPayload::resolve(std::size_t node,
                                                    unsigned word) const noexcept {
  const std::optional<std::uint32_t> offset = u32(node + 4 * word);
  if (!offset.has_value() || *offset == 0) return std::nullopt;
  const std::size_t target = node + *offset;
  if (target >= bytes_.size()) return std::nullopt;
  return target;
}

std::vector<std::size_t> ScenarioPayload::children(std::size_t node) const {
  std::vector<std::size_t> result;
  const std::optional<std::size_t> table = resolve(node, 1);
  if (!table.has_value()) return result;
  const std::optional<std::uint32_t> raw_count = u32(*table);
  if (!raw_count.has_value()) return result;
  const std::int32_t count = static_cast<std::int32_t>(*raw_count);
  if (count <= 0) return result;
  result.reserve(static_cast<std::size_t>(count));
  for (std::int32_t index = 0; index < count; ++index) {
    const std::optional<std::uint32_t> offset = u32(*table + 4 + 4 * index);
    if (!offset.has_value()) return {};
    const std::size_t child = *table + *offset;
    if (child + 8 > bytes_.size()) return {};
    result.push_back(child);
  }
  return result;
}

bool ScenarioPayload::present(std::size_t node) const noexcept {
  const std::optional<std::uint32_t> first = u32(node);
  const std::optional<std::uint32_t> second = u32(node + 4);
  if (!first.has_value() || !second.has_value()) return false;
  return *first != 0 || *second != 0;
}

std::optional<std::uint32_t> object_category(std::uint8_t class_byte) noexcept {
  // The switch at 0x820A72E0: 0 -> 1, 1 -> 4, 2 -> 4, 3 -> 4, 4 -> 3. Any other
  // byte leaves the category at zero, which the factory does not implement.
  switch (class_byte) {
    case 0: return 1;
    case 1:
    case 2:
    case 3: return 4;
    case 4: return 3;
    default: return std::nullopt;
  }
}

std::optional<MissionScenario> MissionScenario::parse(const ScenarioPayload& payload) {
  const std::vector<std::size_t> slots = payload.children(kRootNode);
  if (slots.size() != kRootSlots) return std::nullopt;

  MissionScenario scenario;

  // Slot 0. Each element of the list is a wrapper with no data and exactly one
  // child; the record the consumer iterates is that child.
  std::uint32_t index = 0;
  for (const std::size_t wrapper : payload.children(slots[kSlotObjAndUnit])) {
    const std::vector<std::size_t> inner = payload.children(wrapper);
    if (inner.size() != 1) return std::nullopt;
    const std::size_t node = inner.front();
    const std::optional<std::size_t> data = payload.resolve(node, 0);
    if (!data.has_value()) return std::nullopt;
    const std::optional<std::uint8_t> class_byte = payload.u8(*data + kUnitClassByte);
    const std::optional<std::uint8_t> faction_byte =
        payload.u8(*data + kUnitFactionByte);
    if (!class_byte.has_value() || !faction_byte.has_value()) return std::nullopt;
    const std::optional<std::uint32_t> category = object_category(*class_byte);
    if (!category.has_value()) return std::nullopt;

    ScenarioUnitRecord record;
    record.index = index++;
    record.class_byte = *class_byte;
    record.faction_byte = *faction_byte;
    record.object_category = *category;

    const std::vector<std::size_t> record_children = payload.children(node);
    record.has_behaviour_set =
        !record_children.empty() && payload.present(record_children[0]);
    if (record_children.size() > 1 && payload.present(record_children[1])) {
      for (const std::size_t object : payload.children(record_children[1])) {
        const std::optional<std::size_t> object_data = payload.resolve(object, 0);
        if (!object_data.has_value()) return std::nullopt;
        const std::optional<float> x = payload.f32(*object_data);
        const std::optional<float> y = payload.f32(*object_data + 4);
        const std::optional<float> z = payload.f32(*object_data + 8);
        if (!x.has_value() || !y.has_value() || !z.has_value()) return std::nullopt;
        record.objects.push_back({*x, *y, *z});
      }
    }
    scenario.units_.push_back(std::move(record));
  }

  // Slot 5, the faction table. Its entries carry data without a table, so the
  // block is read directly rather than through resolve's presence rule.
  std::uint32_t faction_index = 0;
  for (const std::size_t node : payload.children(slots[kSlotFactions])) {
    const std::optional<std::size_t> data = payload.resolve(node, 0);
    if (!data.has_value()) return std::nullopt;
    const std::optional<std::uint8_t> side = payload.u8(*data + kFactionSideByte);
    const std::optional<std::uint32_t> word = payload.u32(*data + kFactionWord);
    if (!side.has_value() || !word.has_value()) return std::nullopt;
    scenario.factions_.push_back({faction_index++, *side, *word});
  }

  // Slot 2, the sub-missions, each with the 0x28-stride script 0x8226E158 runs.
  std::uint32_t sub_mission_index = 0;
  for (const std::size_t node : payload.children(slots[kSlotSubMissions])) {
    ScenarioSubMission sub_mission;
    sub_mission.index = sub_mission_index++;
    for (const std::size_t list : payload.children(node)) {
      for (const std::size_t step : payload.children(list)) {
        const std::optional<std::size_t> data = payload.resolve(step, 0);
        if (!data.has_value()) continue;
        const std::optional<std::uint8_t> tag = payload.u8(*data);
        if (!tag.has_value()) return std::nullopt;
        sub_mission.step_tags.push_back(*tag);
      }
    }
    scenario.sub_missions_.push_back(std::move(sub_mission));
  }

  return scenario;
}

std::size_t MissionScenario::object_records() const noexcept {
  std::size_t total = 0;
  for (const ScenarioUnitRecord& record : units_) total += record.objects.size();
  return total;
}

std::string waves_manifest_rows(const MissionScenario& scenario,
                                std::uint32_t mission_id) {
  std::string text;
  for (const ScenarioUnitRecord& record : scenario.units()) {
    const ScenarioVector position =
        record.objects.empty() ? ScenarioVector{} : record.objects.front();
    const std::uint32_t faction = record.faction_byte + 1u;
    text += std::to_string(mission_id);
    text += "\t1\t";                                    // one load-time pass
    text += std::to_string(kEntityBase + record.index);
    text += "\t" + std::to_string(faction);             // owner
    text += "\t" + std::to_string(record.object_category);
    text += "\t" + std::to_string(faction);             // faction == owner
    text += "\t" + format_float(position.x);
    text += "\t" + format_float(position.y);
    text += "\t" + format_float(position.z);
    text += "\t1.000000\t1.000000\t1.000000\n";         // declared placeholders
  }
  return text;
}

std::string objectives_manifest_rows(const MissionScenario& scenario,
                                     std::uint32_t mission_id) {
  std::string text;
  for (const ScenarioSubMission& sub_mission : scenario.sub_missions()) {
    text += std::to_string(mission_id);
    text += "\t" + std::to_string(sub_mission.index + 1);
    text += "\tmission01-submission-" + std::to_string(sub_mission.index);
    text += "\t1\n";
  }
  return text;
}

}  // namespace ac6::retail
