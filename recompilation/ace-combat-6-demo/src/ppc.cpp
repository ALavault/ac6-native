#include "ac6demo/ppc.hpp"

#include <bit>
#include <limits>

namespace ac6demo {

float xenon_reciprocal_estimate(float value) noexcept {
  if (std::isnan(value)) {
    return std::numeric_limits<float>::quiet_NaN();
  }
  if (value == 0.0F) {
    return std::copysign(std::numeric_limits<float>::infinity(), value);
  }
  if (!std::isfinite(value)) {
    return std::copysign(0.0F, value);
  }
  const float magnitude = std::fabs(value);
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(magnitude);
  float estimate = std::bit_cast<float>(0x7EF311C3U - bits);
  estimate = estimate * (2.0F - magnitude * estimate);
  return std::copysign(estimate, value);
}

float xenon_rsqrt_estimate(float value) noexcept {
  if (std::isnan(value)) {
    return std::numeric_limits<float>::quiet_NaN();
  }
  if (value == 0.0F) {
    return std::numeric_limits<float>::infinity();
  }
  if (value < 0.0F || !std::isfinite(value)) {
    return value == std::numeric_limits<float>::infinity()
               ? 0.0F
               : std::numeric_limits<float>::quiet_NaN();
  }
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  float estimate = std::bit_cast<float>(0x5F375A86U - (bits >> 1U));
  estimate = estimate * (1.5F - 0.5F * value * estimate * estimate);
  return estimate;
}

std::uint64_t PpcRuntimeHooks::read_timebase(const PpcContext& /*context*/) const noexcept {
  // The qualified 60 Hz profile exposes a 50 MHz guest timebase. Integer
  // arithmetic keeps the same result in headless and Vulkan replay.
  return (tick_ * 50'000'000ULL) / 60ULL;
}

std::uint32_t PpcRuntimeHooks::lwarx(PpcContext& context, std::uint32_t address) {
  if ((address & 3U) != 0U) {
    throw RuntimeTrap("unaligned lwarx", tick_, static_cast<std::uint32_t>(context.lr),
                      address);
  }
  const std::uint32_t value = memory_.load_u32(address);
  context.reservation_address = address;
  context.reservation_generation = memory_.write_generation(address);
  context.reservation_valid = true;
  return value;
}

bool PpcRuntimeHooks::stwcx(PpcContext& context, std::uint32_t address,
                            std::uint32_t value) {
  if ((address & 3U) != 0U) {
    throw RuntimeTrap("unaligned stwcx", tick_, static_cast<std::uint32_t>(context.lr),
                      address);
  }
  const bool success = context.reservation_valid && context.reservation_address == address &&
                       memory_.write_generation(address) == context.reservation_generation;
  context.reservation_valid = false;
  if (success) {
    memory_.store_u32(address, value);
  }
  return success;
}

}  // namespace ac6demo
