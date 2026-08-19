#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace ac6demo {

inline constexpr std::size_t kXAudioProcessorCount = 6U;
inline constexpr std::uint32_t kXAudioDescriptorTableBaseOffset = 0x0CU;
inline constexpr std::uint32_t kXAudioDescriptorPairStride = 8U;

enum class XAudioDescriptorLane : std::uint8_t { A = 0U, B = 1U };

struct XAudioDescriptorPair final {
  std::uint32_t a{};
  std::uint32_t b{};

  [[nodiscard]] bool complete() const noexcept { return a != 0U && b != 0U; }
};

struct XAudioCallbackCpuSelection final {
  std::uint8_t processor{};
  XAudioDescriptorPair descriptors{};
  bool explicit_request{};
};

using XAudioDescriptorTable =
    std::array<XAudioDescriptorPair, kXAudioProcessorCount>;

[[nodiscard]] constexpr std::uint32_t xaudio_descriptor_offset(
    std::uint8_t processor, XAudioDescriptorLane lane) {
  if (processor >= kXAudioProcessorCount) {
    throw std::out_of_range{"XAudio processor is outside Xenon range"};
  }
  return kXAudioDescriptorTableBaseOffset +
         static_cast<std::uint32_t>(processor) * kXAudioDescriptorPairStride +
         (lane == XAudioDescriptorLane::B ? 4U : 0U);
}

[[nodiscard]] inline std::uint32_t xaudio_descriptor_for_lane(
    const XAudioDescriptorTable &table, std::uint8_t processor,
    XAudioDescriptorLane lane) {
  if (processor >= table.size()) {
    throw std::out_of_range{"XAudio processor is outside descriptor table"};
  }
  const auto &pair = table[processor];
  const auto descriptor = lane == XAudioDescriptorLane::A ? pair.a : pair.b;
  if (descriptor == 0U) {
    throw std::invalid_argument{"selected XAudio descriptor is null"};
  }
  return descriptor;
}

[[nodiscard]] inline std::array<std::uint8_t, kXAudioProcessorCount>
xaudio_complete_descriptor_processors(const XAudioDescriptorTable &table,
                                      std::size_t *count) noexcept {
  std::array<std::uint8_t, kXAudioProcessorCount> result{};
  std::size_t found = 0U;
  for (std::uint8_t processor = 0U; processor < table.size(); ++processor) {
    if (table[processor].complete()) {
      result[found++] = processor;
    }
  }
  if (count != nullptr) {
    *count = found;
  }
  return result;
}

// Fail-closed selection for the synthetic render-driver callback.
//
// An explicit processor is accepted only when both descriptor lanes are
// populated. Without an explicit request, selection is permitted only if the
// guest table identifies exactly one complete processor pair. Two live pairs
// (the reached PAL state has CPUs 4 and 5) are deliberately ambiguous: the
// caller must run the bounded A/B instead of silently preferring the first.
[[nodiscard]] inline XAudioCallbackCpuSelection select_xaudio_callback_cpu(
    const XAudioDescriptorTable &table,
    std::optional<std::uint8_t> requested_processor) {
  if (requested_processor.has_value()) {
    const auto processor = *requested_processor;
    if (processor >= table.size() || !table[processor].complete()) {
      throw std::invalid_argument{
          "explicit XAudio processor has an incomplete descriptor pair"};
    }
    return XAudioCallbackCpuSelection{processor, table[processor], true};
  }

  std::size_t count = 0U;
  const auto processors = xaudio_complete_descriptor_processors(table, &count);
  if (count != 1U) {
    throw std::invalid_argument{
        "XAudio callback processor is absent or ambiguous"};
  }
  const auto processor = processors[0];
  return XAudioCallbackCpuSelection{processor, table[processor], false};
}

} // namespace ac6demo
