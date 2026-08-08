// The native scenario reader, against a synthetic container and, when it is
// available locally, against the retail Mission 01 payload itself.
//
// The synthetic half always runs: it fixes the container primitive - the
// presence rule, the count sign, the bounds - without any retail bytes. The
// retail half runs only when the decoded payload is present on this machine,
// because that file is never committed. It asserts the census the static
// analysis established and requires the rows this reader emits to be identical
// to the committed manifests, which a separate Python generator produced. Two
// independent implementations agreeing on 230 rows is the point.
//
// usage: retail-scenario-parser-tests MANIFEST_DIR [PAYLOAD]
// exit 77 means the retail payload was absent and only the synthetic half ran.

#include "ac6/retail_scenario.h"
#include "test_fixtures.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#include <string>
#include <vector>

namespace {

using ac6::retail::MissionScenario;
using ac6::retail::ScenarioPayload;

// A big-endian blob with patchable words, so a node graph can be laid out in
// the order that reads well rather than the order the offsets require.
class Blob {
 public:
  std::size_t size() const { return bytes_.size(); }

  std::size_t append_u32(std::uint32_t value) {
    const std::size_t at = bytes_.size();
    bytes_.push_back(static_cast<std::uint8_t>(value >> 24));
    bytes_.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes_.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes_.push_back(static_cast<std::uint8_t>(value));
    return at;
  }

  void patch_u32(std::size_t at, std::uint32_t value) {
    bytes_[at] = static_cast<std::uint8_t>(value >> 24);
    bytes_[at + 1] = static_cast<std::uint8_t>(value >> 16);
    bytes_[at + 2] = static_cast<std::uint8_t>(value >> 8);
    bytes_[at + 3] = static_cast<std::uint8_t>(value);
  }

  void append_float(float value) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    append_u32(raw);
  }

  void append_zeros(std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) bytes_.push_back(0);
  }

  void set_u8(std::size_t at, std::uint8_t value) { bytes_[at] = value; }

  std::vector<std::uint8_t> take() const { return bytes_; }

 private:
  std::vector<std::uint8_t> bytes_;
};

// A node is two words. Both are patched once their targets exist.
struct Node {
  std::size_t at{};
  std::size_t data_word{};
  std::size_t table_word{};
};

Node append_node(Blob& blob) {
  Node node;
  node.at = blob.size();
  node.data_word = blob.append_u32(0);
  node.table_word = blob.append_u32(0);
  return node;
}

void point_data(Blob& blob, const Node& node, std::size_t target) {
  blob.patch_u32(node.data_word, static_cast<std::uint32_t>(target - node.at));
}

void point_table(Blob& blob, const Node& node, std::size_t target) {
  blob.patch_u32(node.table_word, static_cast<std::uint32_t>(target - node.at));
}

// A table of `count` children whose offsets are patched as the children are
// laid down. Returns the table offset and the offsets of its slots.
struct Table {
  std::size_t at{};
  std::vector<std::size_t> slots;
};

Table append_table(Blob& blob, std::int32_t count) {
  Table table;
  table.at = blob.append_u32(static_cast<std::uint32_t>(count));
  for (std::int32_t index = 0; index < count; ++index) {
    table.slots.push_back(blob.append_u32(0));
  }
  return table;
}

void point_child(Blob& blob, const Table& table, std::size_t slot,
                 std::size_t target) {
  blob.patch_u32(table.slots[slot], static_cast<std::uint32_t>(target - table.at));
}

// A ten-slot scenario root holding one unit record, one faction and one
// sub-mission of two steps. Every value is chosen, none is retail.
std::vector<std::uint8_t> synthetic_scenario(std::uint8_t class_byte) {
  Blob blob;
  const Node root = append_node(blob);
  const Table root_table = append_table(blob, 10);

  // Slot 0: the 'Obj & Unit' list, one wrapper around one record.
  const Node unit_list = append_node(blob);
  point_child(blob, root_table, 0, unit_list.at);
  const std::size_t unit_count = blob.size();
  blob.append_u32(0x01000000);  // the count byte the consumer bounds its loop by
  point_data(blob, unit_list, unit_count);
  const Table unit_table = append_table(blob, 1);
  point_table(blob, unit_list, unit_table.at);

  const Node wrapper = append_node(blob);
  point_child(blob, unit_table, 0, wrapper.at);
  const Table wrapper_table = append_table(blob, 1);
  point_table(blob, wrapper, wrapper_table.at);

  const Node record = append_node(blob);
  point_child(blob, wrapper_table, 0, record.at);
  const std::size_t record_data = blob.size();
  blob.append_zeros(0x10);
  blob.set_u8(record_data + 0x08, class_byte);
  blob.set_u8(record_data + 0x0D, 1);  // faction index 1
  point_data(blob, record, record_data);
  const Table record_table = append_table(blob, 2);
  point_table(blob, record, record_table.at);

  const Node behaviour = append_node(blob);  // child 0, the Set program
  point_child(blob, record_table, 0, behaviour.at);
  const Table behaviour_table = append_table(blob, 0);
  point_table(blob, behaviour, behaviour_table.at);

  const Node object_list = append_node(blob);  // child 1, the Obj list
  point_child(blob, record_table, 1, object_list.at);
  const Table object_table = append_table(blob, 1);
  point_table(blob, object_list, object_table.at);

  const Node object = append_node(blob);
  point_child(blob, object_table, 0, object.at);
  const std::size_t object_data = blob.size();
  blob.append_float(1.5f);
  blob.append_float(-2.0f);
  blob.append_float(3.25f);
  blob.append_u32(0);
  point_data(blob, object, object_data);

  // Slot 2: one sub-mission whose script has two steps, tags 0 and 1.
  const Node sub_mission_list = append_node(blob);
  point_child(blob, root_table, 2, sub_mission_list.at);
  const Table sub_mission_table = append_table(blob, 1);
  point_table(blob, sub_mission_list, sub_mission_table.at);
  const Node sub_mission = append_node(blob);
  point_child(blob, sub_mission_table, 0, sub_mission.at);
  const Table script_holder = append_table(blob, 1);
  point_table(blob, sub_mission, script_holder.at);
  const Node script = append_node(blob);
  point_child(blob, script_holder, 0, script.at);
  const Table script_table = append_table(blob, 2);
  point_table(blob, script, script_table.at);
  for (std::uint8_t tag : {std::uint8_t{0}, std::uint8_t{1}}) {
    const Node step = append_node(blob);
    point_child(blob, script_table, tag, step.at);
    const std::size_t step_data = blob.size();
    blob.append_zeros(0x28);
    blob.set_u8(step_data, tag);
    point_data(blob, step, step_data);
  }

  // Slot 5: one faction entry with side code 0.
  const Node faction_list = append_node(blob);
  point_child(blob, root_table, 5, faction_list.at);
  const Table faction_table = append_table(blob, 1);
  point_table(blob, faction_list, faction_table.at);
  const Node faction = append_node(blob);
  point_child(blob, faction_table, 0, faction.at);
  const std::size_t faction_data = blob.size();
  blob.append_zeros(0x30);
  blob.patch_u32(faction_data + 0x28, 0x28C);
  point_data(blob, faction, faction_data);

  // The remaining slots exist and are empty, as slots 4 and 6 are in retail.
  for (std::size_t slot : {std::size_t{1}, std::size_t{3}, std::size_t{4},
                           std::size_t{6}, std::size_t{7}, std::size_t{8},
                           std::size_t{9}}) {
    const Node empty = append_node(blob);
    point_child(blob, root_table, slot, empty.at);
  }
  point_table(blob, root, root_table.at);
  return blob.take();
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

// The committed manifests carry a '#' provenance header this reader does not
// duplicate. Compare the rows.
std::string data_rows(const std::string& text) {
  std::string result;
  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    result += line + "\n";
  }
  return result;
}

void check_synthetic() {
  const std::vector<std::uint8_t> bytes = synthetic_scenario(2);
  const std::optional<ScenarioPayload> payload = ScenarioPayload::open(bytes);
  REQUIRE(payload.has_value());
  REQUIRE(payload->children(0).size() == 10);

  const std::optional<MissionScenario> scenario = MissionScenario::parse(*payload);
  REQUIRE(scenario.has_value());
  REQUIRE(scenario->units().size() == 1);
  const ac6::retail::ScenarioUnitRecord& record = scenario->units().front();
  REQUIRE(record.index == 0);
  REQUIRE(record.class_byte == 2);
  REQUIRE(record.faction_byte == 1);
  REQUIRE(record.object_category == 4);
  REQUIRE(record.has_behaviour_set);
  REQUIRE(record.obj_scalars.size() == 1);
  REQUIRE(record.obj_scalars.front() ==
          (ac6::retail::ScenarioObjScalars{1.5f, -2.0f, 3.25f}));

  REQUIRE(scenario->factions().size() == 1);
  REQUIRE(scenario->factions().front().side_code == 0);
  REQUIRE(scenario->factions().front().word_0x28 == 0x28C);
  REQUIRE(scenario->sub_missions().size() == 1);
  REQUIRE(scenario->sub_missions().front().step_tags == std::vector<std::uint8_t>({0, 1}));

  // The row projection, on values that are not retail.
  REQUIRE(waves_manifest_rows(*scenario, 1) ==
          "1\t1\t4097\t2\t4\t2\t1.500000\t-2.000000\t3.250000"
          "\t1.000000\t1.000000\t1.000000\n");
  REQUIRE(objectives_manifest_rows(*scenario, 1) ==
          "1\t1\tmission01-submission-0\t1\n");

  // A class byte the retail switch does not implement must fail closed rather
  // than default to a category.
  const std::vector<std::uint8_t> unknown = synthetic_scenario(7);
  const std::optional<ScenarioPayload> unknown_payload = ScenarioPayload::open(unknown);
  REQUIRE(unknown_payload.has_value());
  REQUIRE(!MissionScenario::parse(*unknown_payload).has_value());
  REQUIRE(!ac6::retail::object_category(7).has_value());

  // A truncated buffer yields no payload, and a truncated node yields no child.
  REQUIRE(!ScenarioPayload::open({0, 0, 0, 0}).has_value());
  const std::optional<ScenarioPayload> truncated =
      ScenarioPayload::open(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 64));
  REQUIRE(truncated.has_value());
  REQUIRE(truncated->children(0).empty());
}

// The count word is signed in the retail table, and a negative count is not a
// huge unsigned one.
void check_negative_count_is_empty() {
  Blob blob;
  const Node node = append_node(blob);
  const Table table = append_table(blob, 0);
  blob.patch_u32(table.at, 0xFFFFFFFFu);
  point_table(blob, node, table.at);
  const std::optional<ScenarioPayload> payload = ScenarioPayload::open(blob.take());
  REQUIRE(payload.has_value());
  REQUIRE(payload->children(0).empty());
}

void check_presence_rule() {
  Blob blob;
  append_node(blob);              // node 0: both words zero, absent
  const Node second = append_node(blob);
  blob.patch_u32(second.table_word, 8);
  const std::optional<ScenarioPayload> payload = ScenarioPayload::open(blob.take());
  REQUIRE(payload.has_value());
  REQUIRE(!payload->present(0));
  REQUIRE(payload->present(8));
}

int check_retail(const std::filesystem::path& manifests,
                 const std::filesystem::path& payload_path) {
  if (!std::filesystem::exists(payload_path)) {
    std::fprintf(stderr, "retail payload absent, synthetic half only: %s\n",
                 payload_path.c_str());
    return 77;
  }
  const std::string raw = read_file(payload_path);
  const std::optional<ScenarioPayload> payload = ScenarioPayload::open(
      std::vector<std::uint8_t>(raw.begin(), raw.end()));
  REQUIRE(payload.has_value());
  const std::optional<MissionScenario> scenario = MissionScenario::parse(*payload);
  REQUIRE(scenario.has_value());

  REQUIRE(scenario->units().size() == 230);

  // The model-directory indices, from the ObjBin data block the Obj entry's
  // child[0] resolves to (cycle 1171). Cycle 1148 read the entry node itself,
  // found one distinct value of zero, and concluded the selector was not in the
  // container at all - which sent three cycles hunting an external table. These
  // numbers are the correction, and they are asserted so the wrong node cannot
  // be read again without failing.
  std::size_t bindings = 0, without_model = 0, with_secondary = 0, consecutive = 0;
  std::set<std::uint8_t> primaries;
  std::uint8_t highest = 0;
  for (const ac6::retail::ScenarioUnitRecord& unit : scenario->units()) {
    REQUIRE(unit.model_bindings.size() == unit.obj_scalars.size());
    for (const ac6::retail::ScenarioModelBinding& binding : unit.model_bindings) {
      bindings += 1;
      if (!binding.has_model()) {
        // 0x820A7944 skips the whole model block on the sentinel, so a record
        // without a primary must not carry a secondary either.
        REQUIRE(!binding.has_secondary());
        without_model += 1;
        continue;
      }
      primaries.insert(binding.primary);
      if (binding.primary > highest) highest = binding.primary;
      if (binding.has_secondary()) {
        with_secondary += 1;
        if (binding.secondary == binding.primary + 1) consecutive += 1;
      }
    }
  }
  REQUIRE(bindings == 434);
  REQUIRE(without_model == 123);
  REQUIRE(with_secondary == 309);
  REQUIRE(consecutive == 281);
  REQUIRE(primaries.size() == 38);
  // Every index addresses Mission 01's own 94-entry model directory. An index
  // at or above the count would make 0x8228E9B8 return null.
  REQUIRE(highest == 74);
  REQUIRE(highest < 94);
  REQUIRE(scenario->object_records() == 434);
  REQUIRE(scenario->factions().size() == 4);
  REQUIRE(scenario->sub_missions().size() == 4);

  std::map<std::uint8_t, std::size_t> class_bytes;
  std::map<std::uint8_t, std::size_t> faction_bytes;
  for (const ac6::retail::ScenarioUnitRecord& record : scenario->units()) {
    class_bytes[record.class_byte] += 1;
    faction_bytes[record.faction_byte] += 1;
  }
  REQUIRE(class_bytes.size() == 4);
  REQUIRE(class_bytes.at(0) == 1 && class_bytes.at(1) == 40);
  REQUIRE(class_bytes.at(2) == 188 && class_bytes.at(4) == 1);
  REQUIRE(faction_bytes.size() == 3);
  REQUIRE(faction_bytes.at(0) == 140 && faction_bytes.at(1) == 42 &&
          faction_bytes.at(2) == 48);

  std::size_t steps = 0;
  for (const ac6::retail::ScenarioSubMission& sub : scenario->sub_missions()) {
    steps += sub.step_tags.size();
  }
  REQUIRE(steps == 6);

  // The two independent generators must agree row for row.
  REQUIRE(waves_manifest_rows(*scenario, 1) ==
          data_rows(read_file(manifests / "waves.tsv")));
  REQUIRE(objectives_manifest_rows(*scenario, 1) ==
          data_rows(read_file(manifests / "objectives.tsv")));
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s MANIFEST_DIR [PAYLOAD]\n", argv[0]);
    return 2;
  }
  check_synthetic();
  check_negative_count_is_empty();
  check_presence_rule();
  if (argc < 3) return 0;
  return check_retail(argv[1], argv[2]);
}
