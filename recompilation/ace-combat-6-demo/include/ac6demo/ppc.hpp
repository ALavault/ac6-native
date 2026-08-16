#pragma once

#include "ac6demo/guest_memory.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace ac6demo {

struct PpcVector final {
  std::array<std::byte, 16> bytes{};

  [[nodiscard]] std::uint32_t u32(std::size_t lane) const {
    return read_be32(bytes, lane * 4U);
  }
  [[nodiscard]] std::int32_t s32(std::size_t lane) const {
    return static_cast<std::int32_t>(u32(lane));
  }
  [[nodiscard]] float f32(std::size_t lane) const {
    return std::bit_cast<float>(u32(lane));
  }
  void set_u32(std::size_t lane, std::uint32_t value) {
    write_be32(bytes, lane * 4U, value);
  }
  void set_s32(std::size_t lane, std::int32_t value) {
    set_u32(lane, static_cast<std::uint32_t>(value));
  }
  void set_f32(std::size_t lane, float value) {
    set_u32(lane, std::bit_cast<std::uint32_t>(value));
  }
};

struct PpcCrField final {
  bool lt{};
  bool gt{};
  bool eq{};
  bool so{};

  [[nodiscard]] std::uint8_t packed() const noexcept {
    return static_cast<std::uint8_t>((lt ? 8U : 0U) | (gt ? 4U : 0U) |
                                     (eq ? 2U : 0U) | (so ? 1U : 0U));
  }
};

struct PpcContext final {
  std::array<std::uint64_t, 32> gpr{};
  std::array<std::uint64_t, 32> fpr{};
  std::array<PpcVector, 128> vmx{};
  std::array<PpcCrField, 8> cr{};
  std::uint64_t xer{};
  std::uint64_t lr{};
  std::uint64_t ctr{};
  std::uint64_t msr{};
  std::uint32_t fpscr{};
  std::uint32_t reservation_address{};
  std::uint64_t reservation_generation{};
  bool reservation_valid{};
};

[[nodiscard]] inline std::int16_t saturate_s16(std::int32_t value) noexcept {
  if (value < std::numeric_limits<std::int16_t>::min()) {
    return std::numeric_limits<std::int16_t>::min();
  }
  if (value > std::numeric_limits<std::int16_t>::max()) {
    return std::numeric_limits<std::int16_t>::max();
  }
  return static_cast<std::int16_t>(value);
}

// Xenon VMX stores a vector as four big-endian words. The signed pack
// interleaves the saturated lanes from the two source vectors.
[[nodiscard]] inline PpcVector vpkswss(const PpcVector& a, const PpcVector& b) noexcept {
  PpcVector result;
  for (std::size_t lane = 0; lane < 4U; ++lane) {
    const auto high = static_cast<std::uint32_t>(
        static_cast<std::uint16_t>(saturate_s16(a.s32(lane))));
    const auto low = static_cast<std::uint32_t>(
        static_cast<std::uint16_t>(saturate_s16(b.s32(lane))));
    result.set_u32(lane, (high << 16U) | low);
  }
  return result;
}

struct VcmpbfpResult final {
  PpcVector value;
  PpcCrField cr6;
};

// Bounds compare: bit 31 marks A < -B and bit 30 marks A > B. A NaN in either
// operand sets both bound bits, as required by the AltiVec compare-bounds
// operation. CR6.EQ is set only when every lane is inside its bounds.
[[nodiscard]] inline VcmpbfpResult vcmpbfp(const PpcVector& a,
                                           const PpcVector& b) noexcept {
  VcmpbfpResult result;
  bool all_in_bounds = true;
  for (std::size_t lane = 0; lane < 4U; ++lane) {
    const float left = a.f32(lane);
    const float bound = b.f32(lane);
    const bool low = !(left >= -bound);
    const bool high = !(left <= bound);
    all_in_bounds = all_in_bounds && !low && !high;
    result.value.set_u32(lane, (low ? 0x80000000U : 0U) | (high ? 0x40000000U : 0U));
  }
  result.cr6.lt = false;
  result.cr6.gt = false;
  result.cr6.eq = all_in_bounds;
  result.cr6.so = false;
  return result;
}

// These are Xenon-style estimate stages. They intentionally use a bounded
// integer seed plus one Newton refinement, never host division or sqrt.
[[nodiscard]] float xenon_reciprocal_estimate(float value) noexcept;
[[nodiscard]] float xenon_rsqrt_estimate(float value) noexcept;

class PpcRuntimeHooks final {
 public:
  explicit PpcRuntimeHooks(GuestMemory& memory) : memory_(memory) {}

  [[nodiscard]] std::uint64_t read_timebase(const PpcContext& context) const noexcept;
  [[nodiscard]] std::uint32_t lwarx(PpcContext& context, std::uint32_t address);
  [[nodiscard]] bool stwcx(PpcContext& context, std::uint32_t address,
                           std::uint32_t value);
  void eieio() noexcept { ++barrier_count_; }
  void sync() noexcept { ++barrier_count_; }
  void lwsync() noexcept { ++barrier_count_; }
  [[nodiscard]] std::uint64_t barrier_count() const noexcept { return barrier_count_; }

  [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
  void set_tick(std::uint64_t tick) noexcept { tick_ = tick; }

 private:
  GuestMemory& memory_;
  std::uint64_t tick_{};
  std::uint64_t barrier_count_{};
};

}  // namespace ac6demo
