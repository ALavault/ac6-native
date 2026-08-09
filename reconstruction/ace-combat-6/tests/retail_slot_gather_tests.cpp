#include "ac6/retail_slot_gather.h"
#include "ac6/retail_input_record.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

int failures = 0;
void check(bool condition, const std::string& what) {
  if (!condition) { std::cerr << "FAIL " << what << "\n"; ++failures; }
}

void test_empty_flags() {
  std::array<std::uint32_t, ac6::retail::kSlotMaskCount> masks{};
  masks.fill(0xFFFFFFFFU);
  check(ac6::retail::gather_active_slots(0U, masks) == 0U,
        "a zero flag word gathers nothing, however permissive the masks");
}

void test_any_overlap_sets_the_slot() {
  std::array<std::uint32_t, ac6::retail::kSlotMaskCount> masks{};
  masks[0] = 1U << 17;          // LY's record bit
  masks[5] = 1U << 3;           // an unrelated bit
  masks[31] = 1U << 17;
  const std::uint32_t got = ac6::retail::gather_active_slots(1U << 17, masks);
  check(got == ((1U << 0) | (1U << 31)),
        "every slot whose mask intersects the flag word is set, and only those");
}

void test_partial_overlap_counts() {
  std::array<std::uint32_t, ac6::retail::kSlotMaskCount> masks{};
  // A mask naming three bits, only one of which is set: ANY overlap suffices,
  // so a port testing for a subset match would report nothing here.
  masks[2] = (1U << 4) | (1U << 17) | (1U << 19);
  check(ac6::retail::gather_active_slots(1U << 19, masks) == (1U << 2),
        "one bit of a three-bit mask is enough");
}

void test_it_meets_the_contracted_record() {
  // The flag word this reads is the one retail_input_record produces, and its
  // axis slots are at (bit + 3) * 4. Assert the two agree on where LY lives, so
  // a change to either is caught here rather than in a capsule.
  check(ac6::retail::float_slot_for_bit(17) == 0x50,
        "LY's record bit 17 addresses slot 0x50");
  check(ac6::retail::kHeldBitToRecordBit[12] == 5,
        "and the button remap is the contracted one");
}

}  // namespace

int main() {
  test_empty_flags();
  test_any_overlap_sets_the_slot();
  test_partial_overlap_counts();
  test_it_meets_the_contracted_record();
  if (failures != 0) { std::cerr << failures << " failure(s)\n"; return 1; }
  std::cout << "retail_slot_gather=pass\n";
  return 0;
}
