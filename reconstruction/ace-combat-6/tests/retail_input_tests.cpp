// The NU::Input snapshot, against the rules read off the listing.
//
// Nothing here needs the retail payload: every assertion is a rule taken from an
// instruction, exercised at the boundaries where a wrong rule and the right one
// disagree. The interesting ones are the axis split at -32768, where -1 - v
// would overflow a 16-bit intermediate, and the complement at +0x20, whose top
// half is 0xFFFF because retail complements a zero-extended halfword and does
// not tidy the result.
//
// usage: retail-input-tests [REPORT_JSON]

#include "ac6/retail_input.h"
#include "test_fixtures.h"

#include <array>
#include <cstdio>
#include <vector>

namespace {

using ac6::retail::AxisHalves;
using ac6::retail::InputSnapshot;
using ac6::retail::kAxisSplitTable;
using ac6::retail::kInputSnapshotBytes;

void write_be16(std::uint8_t* bytes, std::uint16_t value) {
  bytes[0] = static_cast<std::uint8_t>(value >> 8);
  bytes[1] = static_cast<std::uint8_t>(value & 0xFF);
}

void write_be32(std::uint8_t* bytes, std::uint32_t value) {
  bytes[0] = static_cast<std::uint8_t>(value >> 24);
  bytes[1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
  bytes[2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(value & 0xFF);
}

// The table's own offsets, checked against what cycle 1309 read at 0x8201250C
// rather than against the header restating itself.
void test_axis_table() {
  REQUIRE(kAxisSplitTable.size() == 4);
  const std::array<std::array<std::uint16_t, 3>, 4> expected{{
      {0x3C, 0x2E, 0x2C},
      {0x3E, 0x28, 0x2A},
      {0x40, 0x36, 0x34},
      {0x42, 0x30, 0x32},
  }};
  for (std::size_t index = 0; index < expected.size(); ++index) {
    REQUIRE(kAxisSplitTable[index].source_offset == expected[index][0]);
    REQUIRE(kAxisSplitTable[index].positive_offset == expected[index][1]);
    REQUIRE(kAxisSplitTable[index].negative_offset == expected[index][2]);
    // Every offset is inside the 0x40 the accessor copies, and none reaches the
    // XINPUT_STATE at device+0x44.
    for (std::uint16_t offset : expected[index]) {
      REQUIRE(offset >= 0x04);
      REQUIRE(offset + 2u <= 0x44);
    }
  }
}

void test_axis_split() {
  // 8234d158 blt: zero takes the non-negative arm.
  REQUIRE(ac6::retail::split_axis(0) == (AxisHalves{0, 0}));
  REQUIRE(ac6::retail::split_axis(1) == (AxisHalves{1, 0}));
  REQUIRE(ac6::retail::split_axis(32767) == (AxisHalves{32767, 0}));
  // 8234d184 subfic r9,r9,-0x1.
  REQUIRE(ac6::retail::split_axis(-1) == (AxisHalves{0, 0}));
  REQUIRE(ac6::retail::split_axis(-2) == (AxisHalves{0, 1}));
  REQUIRE(ac6::retail::split_axis(-32768) == (AxisHalves{0, 32767}));
  // Both halves are always non-negative and never both non-zero.
  for (int value = -32768; value <= 32767; ++value) {
    const AxisHalves halves = ac6::retail::split_axis(static_cast<std::int16_t>(value));
    REQUIRE(!(halves.positive != 0 && halves.negative != 0));
    REQUIRE(halves.positive <= 32767);
    REQUIRE(halves.negative <= 32767);
  }
}

void test_button_edges() {
  // Nothing held, nothing pressed.
  auto edges = ac6::retail::button_edges(0, 0);
  REQUIRE(edges.pressed == 0);
  REQUIRE(edges.released == 0);
  REQUIRE(edges.current == 0);
  REQUIRE(edges.not_held == 0xFFFFFFFFu);

  // A rising edge on bit 4 while bit 0 stays held.
  edges = ac6::retail::button_edges(0x0001, 0x0011);
  REQUIRE(edges.pressed == 0x0010);
  REQUIRE(edges.released == 0);
  REQUIRE(edges.current == 0x0011);

  // A falling edge on bit 0.
  edges = ac6::retail::button_edges(0x0011, 0x0010);
  REQUIRE(edges.pressed == 0);
  REQUIRE(edges.released == 0x0001);

  // 8234d3a4 nor: the complement of a zero-extended halfword keeps 0xFFFF in
  // its top half. A port that masked to 16 bits would differ here.
  edges = ac6::retail::button_edges(0, 0x8000);
  REQUIRE(edges.not_held == 0xFFFF7FFFu);
  REQUIRE(edges.pressed == 0x8000);

  // Holding a button is not pressing it again.
  edges = ac6::retail::button_edges(0xFFFF, 0xFFFF);
  REQUIRE(edges.pressed == 0);
  REQUIRE(edges.released == 0);
}

void test_decode() {
  std::array<std::uint8_t, kInputSnapshotBytes> bytes{};
  const auto at = [](std::uint16_t device_offset) -> std::size_t {
    return device_offset - 0x04;
  };
  write_be32(bytes.data() + at(0x08), 0);
  write_be32(bytes.data() + at(0x14), 0x0010);
  write_be32(bytes.data() + at(0x18), 0x0001);
  write_be32(bytes.data() + at(0x1C), 0x0011);
  write_be32(bytes.data() + at(0x20), 0xFFFFFFEEu);
  // LY pushed fully negative, LX fully positive, the right stick centred.
  write_be16(bytes.data() + at(0x3C), 0x7FFF);
  write_be16(bytes.data() + at(0x2E), 0x7FFF);
  write_be16(bytes.data() + at(0x3E), 0x8000);
  write_be16(bytes.data() + at(0x2A), 0x7FFF);

  const InputSnapshot snapshot = ac6::retail::decode_snapshot(bytes.data());
  REQUIRE(ac6::retail::snapshot_valid(snapshot));
  REQUIRE(snapshot.pressed == 0x0010);
  REQUIRE(snapshot.released == 0x0001);
  REQUIRE(snapshot.current == 0x0011);
  REQUIRE(snapshot.not_held == 0xFFFFFFEEu);
  REQUIRE(snapshot.raw[0] == 32767);
  REQUIRE(snapshot.raw[1] == -32768);
  REQUIRE(snapshot.axes[0] == (AxisHalves{0x7FFF, 0}));
  REQUIRE(snapshot.axes[1] == (AxisHalves{0, 0x7FFF}));
  REQUIRE(snapshot.axes[2] == (AxisHalves{0, 0}));

  // The decoded halves are what split_axis would have produced from the raw
  // values, which is the join between the two stages the device performs.
  for (std::size_t index = 0; index < snapshot.axes.size(); ++index) {
    REQUIRE(snapshot.axes[index] == ac6::retail::split_axis(snapshot.raw[index]));
  }

  // A disconnected pad refuses.
  write_be32(bytes.data() + at(0x08), 0xFFFFFFFEu);
  REQUIRE(!ac6::retail::snapshot_valid(ac6::retail::decode_snapshot(bytes.data())));
}

void test_capability_code() {
  REQUIRE(ac6::retail::capability_code(1) == 1);
  REQUIRE(ac6::retail::capability_code(2) == 4);
  REQUIRE(ac6::retail::capability_code(3) == 2);
  REQUIRE(ac6::retail::capability_code(4) == 3);
  REQUIRE(ac6::retail::capability_code(5) == 5);
  REQUIRE(ac6::retail::capability_code(0) == 0);
  REQUIRE(ac6::retail::capability_code(6) == 0);
  REQUIRE(ac6::retail::capability_code(255) == 0);
}

}  // namespace

int main(int argc, char** argv) {
  test_axis_table();
  test_axis_split();
  test_button_edges();
  test_decode();
  test_capability_code();
  // Written before the numbers are read back, so the report cannot describe a
  // run that did not happen.
  if (argc > 1) {
    std::FILE* report = std::fopen(argv[1], "w");
    REQUIRE(report != nullptr);
    std::fprintf(report,
                 "{\n  \"schema\": \"ac6.retail-input.v1\",\n"
                 "  \"statement\": \"the axis split, the button edges and the "
                 "0x40-byte snapshot behave as 0x8234D110, 0x8234D378 and "
                 "0x8234D0A0 compute them\",\n"
                 "  \"axis_entries\": %zu,\n"
                 "  \"snapshot_bytes\": %zu,\n"
                 "  \"axis_split_inputs_exercised\": 65536,\n"
                 "  \"capability_codes\": 5\n}\n",
                 kAxisSplitTable.size(), kInputSnapshotBytes);
    std::fclose(report);
  }
  std::printf("retail_input=pass axes=%zu snapshot_bytes=%zu\n",
              kAxisSplitTable.size(), kInputSnapshotBytes);
  return 0;
}
