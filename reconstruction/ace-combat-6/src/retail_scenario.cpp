#include "ac6/retail_scenario.h"

#include "ac6/retail_world_position.h"

#include <bit>
#include <cstdio>
#include <cstring>

namespace ac6::retail {
namespace {

// The scenario root's slots, numbered as the dispatch table numbers them.
constexpr std::size_t kRootNode = 0;
constexpr std::size_t kSlotObjAndUnit = 0;
constexpr std::size_t kSlotCounters = 1;
constexpr std::size_t kSlotSubMissions = 2;
constexpr std::size_t kSlotFactions = 5;
constexpr std::size_t kSlotAreas = 6;
constexpr std::size_t kRootSlots = 10;

// Field offsets inside a unit record's data block, read at 0x820A70FC and
// 0x820A7118.
constexpr std::size_t kUnitClassByte = 0x08;
constexpr std::size_t kUnitFactionByte = 0x0D;

// The order tag whose payload writes a mission counter, and the payload's
// three fields, read at 0x82296BC0 and 0x82267474.
constexpr std::uint8_t kFlagOrderTag = 6;
// The order 0x822969F8 sends to 0x82295A88, whose default sub-kind resolves a
// world position through 0x822953F0.
constexpr std::uint8_t kPositionOrderTag = 2;
constexpr std::size_t kFlagOrderCounterId = 0x00;
constexpr std::size_t kFlagOrderLiteral = 0x02;
constexpr std::size_t kFlagOrderOperation = 0x04;

// The nine-way flag selector inside a faction entry, read at 0x820A7420.
constexpr std::size_t kFactionSideByte = 0x2C;
// 0x820A7944 and 0x820A7968, on the ObjBin data block.
constexpr std::size_t kObjModelPrimary = 0x61;
constexpr std::size_t kObjModelSecondary = 0x62;
constexpr std::size_t kFactionWord = 0x28;

std::string format_float(float value) {
  // Must match Python's f"{value:.6f}" so the two generators are comparable.
  char text[64];
  std::snprintf(text, sizeof(text), "%.6f", static_cast<double>(value));
  return text;
}

}  // namespace

// The entity's initial position, as retail reads it: the first three floats of
// the Obj record's data block, which reach the live transform through
// entity+0x184, 0x8229AF80 and the commit at 0x8229BE98 (cycle 1142). The name
// stays `placeholder` for one honest reason - retail applies this triple
// relative to a parent this port does not model, so the value is right and its
// frame is not.
ScenarioVector position_placeholder(const ScenarioUnitRecord& record) {
  if (record.obj_scalars.empty()) return {};
  const ScenarioObjScalars& scalars = record.obj_scalars.front();
  return {scalars.first, scalars.second, scalars.third};
}

std::optional<ScenarioVector> initial_world_position(const MissionScenario& scenario,
                                                     const ScenarioUnitRecord& record) {
  // The order the unit's program runs first. 0x82295A88 reaches 0x822953F0
  // through its default arm, which is every +0x45 except 5, 8 and 9.
  for (const ScenarioPositionRecord& position : scenario.positions()) {
    if (position.unit_index != record.index) continue;
    if (position.kind == 5 || position.kind == 8 || position.kind == 9) continue;
    // No anchor and no queried height are supplied, so the resolver refuses
    // mode 1 and the height flag. That refusal is the coverage boundary, and
    // it is deliberate: this port has no unit manager to answer 0x82270380
    // with at load time, and no object at global+0x36084 to answer the height
    // query with.
    return resolve_world_position(position);
  }
  return std::nullopt;
}

std::optional<ScenarioPayload> ScenarioPayload::open(std::vector<std::uint8_t> bytes) {
  if (bytes.size() < 8) return std::nullopt;
  return ScenarioPayload(std::move(bytes));
}

std::optional<std::uint8_t> ScenarioPayload::u8(std::size_t offset) const noexcept {
  if (offset >= bytes_.size()) return std::nullopt;
  return bytes_[offset];
}

std::optional<std::uint16_t> ScenarioPayload::u16(std::size_t offset) const noexcept {
  if (offset > bytes_.size() || bytes_.size() - offset < 2) return std::nullopt;
  return static_cast<std::uint16_t>(bytes_[offset] << 8 | bytes_[offset + 1]);
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
    if (child > bytes_.size() || bytes_.size() - child < 8) return {};
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
        record.obj_scalars.push_back({*x, *y, *z});

        // The model indices are not on this node. 0x82330158 stores the Obj
        // entry's child[0] data block into the 0x20-byte record, and 0x820A7944
        // reads +0x61/+0x62 from that. Cycle 1148 measured this node instead and
        // found only zeros, which is why three cycles hunted an external table.
        ScenarioModelBinding binding;
        const std::vector<std::size_t> entry_children = payload.children(object);
        if (!entry_children.empty()) {
          const std::optional<std::size_t> bin = payload.resolve(entry_children[0], 0);
          if (bin.has_value()) {
            const std::optional<std::uint8_t> primary = payload.u8(*bin + kObjModelPrimary);
            const std::optional<std::uint8_t> secondary = payload.u8(*bin + kObjModelSecondary);
            if (primary.has_value()) binding.primary = *primary;
            if (secondary.has_value()) binding.secondary = *secondary;
          }
        }
        record.model_bindings.push_back(binding);
      }
    }
    // The unit's Set -> Act -> Order program, kept only for the orders that
    // write a mission counter.
    if (record.has_behaviour_set) {
      for (const std::size_t act : payload.children(record_children[0])) {
        for (const std::size_t order : payload.children(act)) {
          const std::optional<std::size_t> order_data = payload.resolve(order, 0);
          if (!order_data.has_value()) continue;
          const std::optional<std::uint8_t> tag = payload.u8(*order_data);
          if (!tag.has_value()) continue;
          if (*tag == kPositionOrderTag) {
            // 0x82295BF0 hands the order's own payload to 0x822953F0 as its
            // third argument; these are the fields that function reads.
            const std::vector<std::size_t> position_children = payload.children(order);
            if (position_children.empty()) continue;
            const std::optional<std::size_t> block =
                payload.resolve(position_children[0], 0);
            if (!block.has_value()) continue;
            const std::optional<float> x = payload.f32(*block + 0x08);
            const std::optional<float> y = payload.f32(*block + 0x0C);
            const std::optional<float> z = payload.f32(*block + 0x10);
            const std::optional<std::uint16_t> position_flags = payload.u16(*block + 0x40);
            const std::optional<std::uint8_t> mode = payload.u8(*block + 0x42);
            const std::optional<std::uint8_t> anchor_a = payload.u8(*block + 0x43);
            const std::optional<std::uint8_t> anchor_b = payload.u8(*block + 0x44);
            const std::optional<std::uint8_t> kind = payload.u8(*block + 0x45);
            const std::optional<std::uint8_t> slot = payload.u8(*block + 0x46);
            if (!x || !y || !z || !position_flags || !mode || !anchor_a || !anchor_b ||
                !kind || !slot) {
              return std::nullopt;
            }
            scenario.positions_.push_back({record.index, *x, *y, *z, *position_flags,
                                           *mode, *anchor_a, *anchor_b, *kind, *slot});
            continue;
          }
          if (*tag != kFlagOrderTag) continue;
          const std::vector<std::size_t> order_children = payload.children(order);
          if (order_children.empty() || !payload.present(order_children[0])) continue;
          const std::optional<std::size_t> flag = payload.resolve(order_children[0], 0);
          if (!flag.has_value()) continue;
          const std::optional<std::uint16_t> counter_id =
              payload.u16(*flag + kFlagOrderCounterId);
          const std::optional<std::uint16_t> literal =
              payload.u16(*flag + kFlagOrderLiteral);
          const std::optional<std::uint8_t> operation =
              payload.u8(*flag + kFlagOrderOperation);
          if (!counter_id.has_value() || !literal.has_value() ||
              !operation.has_value()) {
            return std::nullopt;
          }
          scenario.flag_orders_.push_back(
              {record.index, *counter_id, *literal, *operation});
        }
      }
    }

    scenario.units_.push_back(std::move(record));
  }

  // Slot 1, whose u16 count sizes the mission counter table.
  {
    const std::optional<std::size_t> data =
        payload.resolve(slots[kSlotCounters], 0);
    if (data.has_value()) {
      const std::optional<std::uint16_t> count = payload.u16(*data);
      if (!count.has_value()) return std::nullopt;
      scenario.counter_capacity_ = *count;
    }
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

  // Slot 6, the area records. FUN_82266EF0 keys them by the byte at +0xA6 and
  // 0x82268B28 turns +0x28/+0x30/+0x34/+0x3C into a rectangle.
  if (slots.size() > kSlotAreas && payload.present(slots[kSlotAreas])) {
    for (const std::size_t node : payload.children(slots[kSlotAreas])) {
      const std::optional<std::size_t> data = payload.resolve(node, 0);
      if (!data.has_value()) continue;
      const std::optional<std::uint8_t> kind = payload.u8(*data + 0xA6);
      const std::optional<float> x0 = payload.f32(*data + 0x28);
      const std::optional<float> z0 = payload.f32(*data + 0x30);
      const std::optional<float> x1 = payload.f32(*data + 0x34);
      const std::optional<float> z1 = payload.f32(*data + 0x3C);
      if (!kind.has_value() || !x0.has_value() || !z0.has_value() ||
          !x1.has_value() || !z1.has_value()) {
        return std::nullopt;
      }
      scenario.areas_.push_back({*kind, *x0, *z0, *x1, *z1});
    }
  }

  // Slot 2, the sub-missions, each with the 0x28-stride script 0x8226E158 runs.
  std::uint32_t sub_mission_index = 0;
  for (const std::size_t node : payload.children(slots[kSlotSubMissions])) {
    ScenarioSubMission sub_mission;
    sub_mission.index = sub_mission_index++;
    for (const std::size_t list : payload.children(node)) {
      // 0x822673E4-0x822673EC: lwz r10,0x8(r10); lwz r10,0x0(r10); lbz r10,0x0(r10)
      // - the step count the advance compares the cursor against.
      const std::optional<std::size_t> list_data = payload.resolve(list, 0);
      if (list_data.has_value()) {
        const std::optional<std::uint8_t> count = payload.u8(*list_data);
        if (!count.has_value()) return std::nullopt;
        sub_mission.step_count_byte = *count;
      }
      for (const std::size_t step : payload.children(list)) {
        const std::optional<std::size_t> data = payload.resolve(step, 0);
        if (!data.has_value()) continue;
        const std::optional<std::uint8_t> tag = payload.u8(*data);
        if (!tag.has_value()) return std::nullopt;
        sub_mission.step_tags.push_back(*tag);
        if (*tag != 0 || sub_mission.setup.present) continue;
        // 0x8226E2A0: pfVar3 = **(float ***)(step + 4) - the step's first
        // child's data block, which is where the sub-mission's rectangle, its
        // time limit and its completion code live.
        const std::vector<std::size_t> step_children = payload.children(step);
        if (step_children.empty()) continue;
        const std::optional<std::size_t> setup = payload.resolve(step_children[0], 0);
        if (!setup.has_value()) continue;
        ScenarioSubMissionSetup& record = sub_mission.setup;
        const std::optional<float> f0 = payload.f32(*setup);
        const std::optional<float> f1 = payload.f32(*setup + 0x04);
        const std::optional<float> f2 = payload.f32(*setup + 0x08);
        const std::optional<float> f3 = payload.f32(*setup + 0x0C);
        const std::optional<float> limit = payload.f32(*setup + 0x24);
        const std::optional<std::uint32_t> flags = payload.u32(*setup + 0x30);
        const std::optional<std::uint8_t> code = payload.u8(*setup + 0x40);
        if (!f0 || !f1 || !f2 || !f3 || !limit || !flags || !code) return std::nullopt;
        record = {true, *f0, *f2, *f1, *f3, *limit, *flags, *code};
      }
    }
    scenario.sub_missions_.push_back(std::move(sub_mission));
  }

  return scenario;
}

std::size_t MissionScenario::object_records() const noexcept {
  std::size_t total = 0;
  for (const ScenarioUnitRecord& record : units_) total += record.obj_scalars.size();
  return total;
}

std::string waves_manifest_rows(const MissionScenario& scenario,
                                std::uint32_t mission_id) {
  std::string text;
  for (const ScenarioUnitRecord& record : scenario.units()) {
    const ScenarioVector position =
        position_placeholder(record);
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
