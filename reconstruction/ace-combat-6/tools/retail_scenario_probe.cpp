// Run the native scenario reader over a payload that is not committed.
//
// The tests pin Mission 01, whose data shaped every rule in the reader. This
// probe exists for the opposite case: pointing the same code at a mission it
// has never seen, and reporting what happens - including the two descents the
// port refuses to guess, which fail loudly here instead of passing silently.
//
// usage: ac6-retail-scenario-probe PAYLOAD

#include "ac6/retail_bin_readers.h"
#include "ac6/retail_mission_state.h"
#include "ac6/retail_scenario.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string read_file(const char* path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

template <typename Key>
std::string histogram(const std::map<Key, std::size_t>& counts) {
  std::string text = "{";
  bool first = true;
  for (const auto& [key, value] : counts) {
    if (!first) text += ", ";
    first = false;
    text += "\"" + std::to_string(static_cast<long long>(key)) + "\": " +
            std::to_string(value);
  }
  return text + "}";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s PAYLOAD\n", argv[0]);
    return 2;
  }
  const std::string raw = read_file(argv[1]);
  if (raw.empty()) {
    std::fprintf(stderr, "empty or unreadable payload\n");
    return 2;
  }
  const std::optional<ac6::retail::ScenarioPayload> payload =
      ac6::retail::ScenarioPayload::open(
          std::vector<std::uint8_t>(raw.begin(), raw.end()));
  if (!payload.has_value()) {
    std::fprintf(stderr, "payload too small to hold a node\n");
    return 1;
  }
  const std::optional<ac6::retail::MissionScenario> scenario =
      ac6::retail::MissionScenario::parse(*payload);
  if (!scenario.has_value()) {
    std::fprintf(stderr, "the scenario views do not hold on this payload\n");
    return 1;
  }

  std::map<int, std::size_t> classes;
  std::map<int, std::size_t> factions;
  for (const ac6::retail::ScenarioUnitRecord& record : scenario->units()) {
    classes[record.class_byte] += 1;
    factions[record.faction_byte] += 1;
  }
  std::map<int, std::size_t> operations;
  std::map<int, std::size_t> counter_ids;
  for (const ac6::retail::ScenarioFlagOrder& order : scenario->flag_orders()) {
    operations[order.operation] += 1;
    counter_ids[order.counter_id] += 1;
  }

  const std::optional<ac6::retail::RetailUnitBuild> build =
      ac6::retail::build_units(*scenario, ac6::retail::LocalPlayerSlot{0, 0});

  // The readers, over every reachable Set and Obj node. This is where an
  // unmodelled descent would surface.
  std::size_t reader_runs = 0;
  std::string reader_failure;
  const std::vector<std::size_t> slots = payload->children(0);
  if (!slots.empty()) {
    for (const std::size_t wrapper : payload->children(slots[0])) {
      const std::vector<std::size_t> inner = payload->children(wrapper);
      if (inner.size() != 1) continue;
      const std::vector<std::size_t> children = payload->children(inner.front());
      for (std::size_t index = 0; index < children.size() && index < 2; ++index) {
        if (!payload->present(children[index])) continue;
        const char* klass = index == 0 ? "SetBin" : nullptr;
        if (klass != nullptr) {
          ac6::retail::BinImage image;
          ac6::retail::BinReaders readers(*payload, image);
          reader_runs += 1;
          if (!readers.run(klass, children[index])) {
            reader_failure = std::string(readers.failure());
          }
        } else {
          for (const std::size_t object : payload->children(children[index])) {
            ac6::retail::BinImage image;
            ac6::retail::BinReaders readers(*payload, image);
            reader_runs += 1;
            if (!readers.run("ObjBin", object)) {
              reader_failure = std::string(readers.failure());
            }
          }
        }
      }
    }
  }

  // The remaining root slots the microexec covered, so the probe exercises the
  // whole family rather than the unit subtree only.
  const std::pair<std::size_t, const char*> root_readers[] = {
      {2, "SubMisTblBin"}, {3, "RadioTblBin"}};
  for (const auto& [slot, klass] : root_readers) {
    if (slot >= slots.size() || !payload->present(slots[slot])) continue;
    ac6::retail::BinImage image;
    ac6::retail::BinReaders readers(*payload, image);
    reader_runs += 1;
    if (!readers.run(klass, slots[slot])) {
      reader_failure = std::string(klass) + ": " + std::string(readers.failure());
    }
  }

  std::printf("{\n");
  std::printf("  \"payload_bytes\": %zu,\n", raw.size());
  std::printf("  \"root_slots\": %zu,\n", slots.size());
  std::printf("  \"present_slots\": \"");
  for (std::size_t slot = 0; slot < slots.size(); ++slot) {
    std::printf("%c", payload->present(slots[slot]) ? '1' : '0');
  }
  std::printf("\",\n");
  std::printf("  \"unit_records\": %zu,\n", scenario->units().size());
  std::printf("  \"object_records\": %zu,\n", scenario->object_records());
  std::printf("  \"factions\": %zu,\n", scenario->factions().size());
  std::printf("  \"sub_missions\": %zu,\n", scenario->sub_missions().size());
  std::printf("  \"class_bytes\": %s,\n", histogram(classes).c_str());
  std::printf("  \"faction_bytes\": %s,\n", histogram(factions).c_str());
  std::printf("  \"flag_orders\": %zu,\n", scenario->flag_orders().size());
  std::printf("  \"flag_operations\": %s,\n", histogram(operations).c_str());
  std::printf("  \"distinct_counters\": %zu,\n", counter_ids.size());
  std::printf("  \"units_built\": %s,\n",
              build.has_value() ? std::to_string(build->objects.size()).c_str() : "null");
  std::printf("  \"reader_runs\": %zu,\n", reader_runs);
  std::printf("  \"reader_failure\": %s\n",
              reader_failure.empty() ? "null" : ("\"" + reader_failure + "\"").c_str());
  std::printf("}\n");
  return reader_failure.empty() && build.has_value() ? 0 : 1;
}
