#include "ac6demo/xaudio_callback_cpu_contract.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace {

bool rejected(const ac6demo::XAudioDescriptorTable &table,
              std::optional<std::uint8_t> processor) {
  try {
    (void)ac6demo::select_xaudio_callback_cpu(table, processor);
  } catch (const std::invalid_argument &) {
    return true;
  }
  return false;
}

} // namespace

int main() {
  using ac6demo::XAudioDescriptorLane;
  using ac6demo::XAudioDescriptorPair;
  using ac6demo::XAudioDescriptorTable;

  assert(ac6demo::xaudio_descriptor_offset(0U, XAudioDescriptorLane::A) ==
         0x0CU);
  assert(ac6demo::xaudio_descriptor_offset(0U, XAudioDescriptorLane::B) ==
         0x10U);
  assert(ac6demo::xaudio_descriptor_offset(4U, XAudioDescriptorLane::A) ==
         0x2CU);
  assert(ac6demo::xaudio_descriptor_offset(4U, XAudioDescriptorLane::B) ==
         0x30U);
  assert(ac6demo::xaudio_descriptor_offset(5U, XAudioDescriptorLane::A) ==
         0x34U);
  assert(ac6demo::xaudio_descriptor_offset(5U, XAudioDescriptorLane::B) ==
         0x38U);

  bool bad_offset = false;
  try {
    (void)ac6demo::xaudio_descriptor_offset(6U, XAudioDescriptorLane::A);
  } catch (const std::out_of_range &) {
    bad_offset = true;
  }
  assert(bad_offset);

  XAudioDescriptorTable reached{};
  reached[4] = XAudioDescriptorPair{0x10050008U, 0x10051008U};
  reached[5] = XAudioDescriptorPair{0x10052008U, 0x10053008U};

  // The reached PAL shape has two complete candidates. An implicit choice is
  // not evidence and must be refused.
  assert(rejected(reached, std::nullopt));
  assert(rejected(reached, std::uint8_t{0U}));
  assert(rejected(reached, std::uint8_t{6U}));

  const auto cpu4 =
      ac6demo::select_xaudio_callback_cpu(reached, std::uint8_t{4U});
  assert(cpu4.processor == 4U);
  assert(cpu4.explicit_request);
  assert(cpu4.descriptors.a == 0x10050008U);
  assert(cpu4.descriptors.b == 0x10051008U);
  assert(ac6demo::xaudio_descriptor_for_lane(
             reached, 4U, XAudioDescriptorLane::A) == 0x10050008U);
  assert(ac6demo::xaudio_descriptor_for_lane(
             reached, 4U, XAudioDescriptorLane::B) == 0x10051008U);

  const auto cpu5 =
      ac6demo::select_xaudio_callback_cpu(reached, std::uint8_t{5U});
  assert(cpu5.processor == 5U);
  assert(cpu5.descriptors.a == 0x10052008U);
  assert(cpu5.descriptors.b == 0x10053008U);

  XAudioDescriptorTable unique{};
  unique[5] = reached[5];
  const auto inferred =
      ac6demo::select_xaudio_callback_cpu(unique, std::nullopt);
  assert(inferred.processor == 5U);
  assert(!inferred.explicit_request);

  XAudioDescriptorTable incomplete{};
  incomplete[4].a = 0x10050008U;
  assert(rejected(incomplete, std::nullopt));
  assert(rejected(incomplete, std::uint8_t{4U}));

  bool null_lane = false;
  try {
    (void)ac6demo::xaudio_descriptor_for_lane(
        incomplete, 4U, XAudioDescriptorLane::B);
  } catch (const std::invalid_argument &) {
    null_lane = true;
  }
  assert(null_lane);
  return 0;
}
