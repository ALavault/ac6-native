// The input record: unit rules at their boundaries, then a differential against
// every retail vector.
//
// The unit half tests the boundaries A7 names -- 0x07FF, 0x0800, 0x0801 around
// the deadzone that turns out NOT to gate this path, and 0x4000, 0x7FFF, 0x8000,
// 0xFFFF around the normalisation. They are written as expectations here so a
// change to the port fails on a named boundary rather than on row 214 of a table.
//
// The differential half reads analysis/input-path/input-record-vectors.tsv, 321
// runs of 0x821CAA50 reduced to one line each, and compares the encoded record
// byte for byte. It needs no Ghidra.

#include "ac6/retail_input_record.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::uint32_t bits_of(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

void check_bits(float got, std::uint32_t want, const std::string& what) {
  if (bits_of(got) != want) {
    std::cerr << "FAIL " << what << ": got 0x" << std::hex << bits_of(got)
              << " want 0x" << want << std::dec << "\n";
    ++failures;
  }
}

// float32(v) * float32(1/32767), the operation retail performs. Written out
// rather than reusing the port's helper, so the test is not the code.
std::uint32_t scaled_bits(std::int32_t value) {
  const float scale = 1.0F / 32767.0F;
  return bits_of(static_cast<float>(value) * scale);
}

void test_axis_boundaries() {
  // The deadzone that is not one. Cycle 1315 read "at or below 0x800 the lane is
  // not written"; these three say otherwise, and 1 already stores a value.
  check_bits(ac6::retail::axis_slot(0x07FF, 0), scaled_bits(0x07FF), "axis +0x07FF");
  check_bits(ac6::retail::axis_slot(0x0800, 0), scaled_bits(0x0800), "axis +0x0800");
  check_bits(ac6::retail::axis_slot(0x0801, 0), scaled_bits(0x0801), "axis +0x0801");
  check_bits(ac6::retail::axis_slot(0x0001, 0), scaled_bits(1), "axis +0x0001");

  // The normalisation endpoints.
  check_bits(ac6::retail::axis_slot(0x4000, 0), scaled_bits(0x4000), "axis +0x4000");
  check_bits(ac6::retail::axis_slot(0x7FFF, 0), bits_of(1.0F), "axis +0x7FFF is exactly 1");

  // Above 0x7FFF the half is negative, the positive gate fails, and the slot
  // keeps what the negative half left -- which with a zero negative half is
  // NEGATIVE zero, not positive zero.
  check_bits(ac6::retail::axis_slot(0x8000, 0), 0x80000000U, "axis +0x8000 stays -0");
  check_bits(ac6::retail::axis_slot(0xFFFF, 0), 0x80000000U, "axis +0xFFFF stays -0");
  check_bits(ac6::retail::axis_slot(0, 0), 0x80000000U, "idle axis is -0, not +0");

  // The negative half: ungated, negated, and it wins when the positive half is
  // not strictly positive.
  check_bits(ac6::retail::axis_slot(0, 0x7FFF), bits_of(-1.0F), "axis -0x7FFF");
  check_bits(ac6::retail::axis_slot(0, 0x0001), scaled_bits(-1), "axis -0x0001");
  // Out of the reachable domain -- 0x8234D110 cannot produce a negative half --
  // but measured and reproduced: negating int16 -1 gives +1/32767.
  check_bits(ac6::retail::axis_slot(0, 0xFFFF), scaled_bits(1), "axis -0xFFFF negates");

  // A strictly positive half overrides whatever the negative half stored.
  check_bits(ac6::retail::axis_slot(0x7FFF, 0x7FFF), bits_of(1.0F),
             "positive half wins over negative");
}

void test_scalar_rules() {
  // A single signed field, no second half, no negation -- so an idle scalar is
  // POSITIVE zero where an idle axis is negative zero.
  check_bits(ac6::retail::scalar_slot(0), 0x00000000U, "idle scalar is +0");
  check_bits(ac6::retail::scalar_slot(0xFFFF), scaled_bits(-1), "scalar -1");
  check_bits(ac6::retail::scalar_slot(0x8000), scaled_bits(-32768), "scalar -32768");
  check_bits(ac6::retail::scalar_slot(20000), scaled_bits(20000), "scalar 20000");

  // The flag bit is not the slot, and its boundary is exactly 31.
  check(!ac6::retail::scalar_flag_set(30), "scalar flag clear at 30");
  check(ac6::retail::scalar_flag_set(31), "scalar flag set at 31");
  check(!ac6::retail::scalar_flag_set(1), "scalar flag clear at 1 (not a sign test)");
  check(ac6::retail::scalar_flag_set(256), "scalar flag set at 256 (not the 0x800 deadzone)");
  check(!ac6::retail::scalar_flag_set(0x8000), "scalar flag clear at -32768");
}

void test_button_remap() {
  std::array<std::uint8_t, 0x40> snapshot{};
  auto set_held = [&snapshot](std::uint32_t held) {
    const std::size_t at = 0x1C - 0x04;
    snapshot[at + 0] = static_cast<std::uint8_t>(held >> 24);
    snapshot[at + 1] = static_cast<std::uint8_t>(held >> 16);
    snapshot[at + 2] = static_cast<std::uint8_t>(held >> 8);
    snapshot[at + 3] = static_cast<std::uint8_t>(held);
  };

  set_held(1U << 12);  // XInput A
  check(ac6::retail::build_input_record(snapshot.data()).flags == (1U << 5),
        "device bit 12 becomes record bit 5");
  set_held(1U << 15);  // XInput Y
  check(ac6::retail::build_input_record(snapshot.data()).flags == (1U << 4),
        "device bit 15 becomes record bit 4");
  set_held(1U << 10);
  check(ac6::retail::build_input_record(snapshot.data()).flags == 0,
        "device bit 10 is unmapped");
  set_held(0xFFFF0000U);
  check(ac6::retail::build_input_record(snapshot.data()).flags == 0,
        "the top half of the held word is unmapped");
}

// ---------------------------------------------------------------- differential

struct Vector {
  std::string name;
  std::uint32_t held{};
  std::uint16_t scalar14{};
  std::uint16_t scalar15{};
  std::map<std::string, std::uint16_t> halves;
  std::uint32_t flags{};
  std::map<std::size_t, std::uint32_t> slots;
};

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::stringstream stream(text);
  std::string item;
  while (std::getline(stream, item, separator)) {
    parts.push_back(item);
  }
  return parts;
}

// device offsets of the eight halves, keyed the way the vector table names them.
const std::map<std::string, std::size_t>& half_offsets() {
  static const std::map<std::string, std::size_t> offsets{
      {"LX+", 0x2E}, {"LX-", 0x2C}, {"LY+", 0x28}, {"LY-", 0x2A},
      {"RX+", 0x36}, {"RX-", 0x34}, {"RY+", 0x30}, {"RY-", 0x32}};
  return offsets;
}

int run_differential(const char* path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "cannot open " << path << "\n";
    return -1;
  }
  std::string line;
  int compared = 0;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const std::vector<std::string> columns = split(line, '\t');
    if (columns.size() != 7) {
      std::cerr << "FAIL malformed row: " << line << "\n";
      ++failures;
      continue;
    }
    Vector vector;
    vector.name = columns[0];
    vector.held = static_cast<std::uint32_t>(std::stoul(columns[1], nullptr, 16));
    vector.scalar14 = static_cast<std::uint16_t>(std::stoul(columns[2], nullptr, 16));
    vector.scalar15 = static_cast<std::uint16_t>(std::stoul(columns[3], nullptr, 16));
    if (columns[4] != "-") {
      for (const std::string& item : split(columns[4], ',')) {
        const std::size_t equals = item.find('=');
        vector.halves[item.substr(0, equals)] = static_cast<std::uint16_t>(
            std::stoul(item.substr(equals + 1), nullptr, 16));
      }
    }
    vector.flags = static_cast<std::uint32_t>(std::stoul(columns[5], nullptr, 16));
    for (const std::string& item : split(columns[6], ',')) {
      const std::size_t equals = item.find('=');
      vector.slots[std::stoul(item.substr(0, equals), nullptr, 16)] =
          static_cast<std::uint32_t>(std::stoul(item.substr(equals + 1), nullptr, 16));
    }

    // Rebuild the snapshot the run produced, then the record from it.
    std::array<std::uint8_t, 0x40> snapshot{};
    auto put32 = [&snapshot](std::size_t device, std::uint32_t value) {
      const std::size_t at = device - 0x04;
      snapshot[at + 0] = static_cast<std::uint8_t>(value >> 24);
      snapshot[at + 1] = static_cast<std::uint8_t>(value >> 16);
      snapshot[at + 2] = static_cast<std::uint8_t>(value >> 8);
      snapshot[at + 3] = static_cast<std::uint8_t>(value);
    };
    auto put16 = [&snapshot](std::size_t device, std::uint16_t value) {
      const std::size_t at = device - 0x04;
      snapshot[at + 0] = static_cast<std::uint8_t>(value >> 8);
      snapshot[at + 1] = static_cast<std::uint8_t>(value);
    };
    put32(0x1C, vector.held);
    put16(0x38, vector.scalar14);
    put16(0x3A, vector.scalar15);
    for (const auto& [name, value] : vector.halves) {
      const auto found = half_offsets().find(name);
      if (found == half_offsets().end()) {
        std::cerr << "FAIL unknown half " << name << " in " << vector.name << "\n";
        ++failures;
        continue;
      }
      put16(found->second, value);
    }

    const ac6::retail::InputRecord record =
        ac6::retail::build_input_record(snapshot.data());
    const auto encoded = ac6::retail::encode_input_record(record);

    if (record.flags != vector.flags) {
      std::cerr << "FAIL " << vector.name << ": flags 0x" << std::hex << record.flags
                << " want 0x" << vector.flags << std::dec << "\n";
      ++failures;
    }
    for (const auto& [offset, want] : vector.slots) {
      std::uint32_t got = 0;
      for (int index = 0; index < 4; ++index) {
        got = (got << 8) | encoded[offset + static_cast<std::size_t>(index)];
      }
      if (got != want) {
        std::cerr << "FAIL " << vector.name << ": slot 0x" << std::hex << offset
                  << " got 0x" << got << " want 0x" << want << std::dec << "\n";
        ++failures;
      }
    }
    ++compared;
  }
  std::cout << "differential vectors=" << compared << "\n";
  if (compared == 0) {
    std::cerr << "FAIL the vector table produced no comparisons\n";
    ++failures;
  }
  return compared;
}

}  // namespace

int main(int argc, char** argv) {
  test_axis_boundaries();
  test_scalar_rules();
  test_button_remap();
  int compared = 0;
  if (argc > 1) {
    compared = run_differential(argv[1]);
    if (compared < 0) {
      return 1;
    }
  } else {
    std::cerr << "FAIL no vector table given; the differential did not run\n";
    ++failures;
  }
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  // The contract's native-test artefact, written by the run that earns it rather
  // than by hand, so the number in it cannot drift from the number compared.
  if (argc > 2) {
    std::ofstream report(argv[2]);
    report << "{\n"
           << "  \"schema\": \"ac6.retail-input-record-test.v1\",\n"
           << "  \"vectors\": " << compared << ",\n"
           << "  \"divergences\": 0,\n"
           << "  \"statement\": \"every retail vector of 0x821CAA50 reproduced "
              "bit for bit by build_input_record, plus the named boundaries "
              "0x07FF/0x0800/0x0801 and 0x4000/0x7FFF/0x8000/0xFFFF\"\n"
           << "}\n";
  }
  std::cout << "retail_input_record=pass\n";
  return 0;
}
