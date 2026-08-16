#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace ac6demo {

[[nodiscard]] inline std::uint16_t read_be16(std::span<const std::byte> bytes,
                                             std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 2U) {
    throw std::out_of_range("big-endian 16-bit read outside guest input");
  }
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) << 8U) |
      static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])));
}

[[nodiscard]] inline std::uint32_t read_be32(std::span<const std::byte> bytes,
                                             std::size_t offset) {
  if (offset > bytes.size() || bytes.size() - offset < 4U) {
    throw std::out_of_range("big-endian 32-bit read outside guest input");
  }
  return (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) << 24U) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 16U) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 8U) |
         static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U]));
}

[[nodiscard]] inline std::uint64_t read_be64(std::span<const std::byte> bytes,
                                             std::size_t offset) {
  return (static_cast<std::uint64_t>(read_be32(bytes, offset)) << 32U) |
         static_cast<std::uint64_t>(read_be32(bytes, offset + 4U));
}

inline void write_be16(std::span<std::byte> bytes, std::size_t offset,
                       std::uint16_t value) {
  if (offset > bytes.size() || bytes.size() - offset < 2U) {
    throw std::out_of_range("big-endian 16-bit write outside guest input");
  }
  bytes[offset] = static_cast<std::byte>(value >> 8U);
  bytes[offset + 1U] = static_cast<std::byte>(value & 0xffU);
}

inline void write_be32(std::span<std::byte> bytes, std::size_t offset,
                       std::uint32_t value) {
  if (offset > bytes.size() || bytes.size() - offset < 4U) {
    throw std::out_of_range("big-endian 32-bit write outside guest input");
  }
  bytes[offset] = static_cast<std::byte>(value >> 24U);
  bytes[offset + 1U] = static_cast<std::byte>((value >> 16U) & 0xffU);
  bytes[offset + 2U] = static_cast<std::byte>((value >> 8U) & 0xffU);
  bytes[offset + 3U] = static_cast<std::byte>(value & 0xffU);
}

inline void write_be64(std::span<std::byte> bytes, std::size_t offset,
                       std::uint64_t value) {
  write_be32(bytes, offset, static_cast<std::uint32_t>(value >> 32U));
  write_be32(bytes, offset + 4U, static_cast<std::uint32_t>(value));
}

[[nodiscard]] inline float bit_cast_float(std::uint32_t value) noexcept {
  return std::bit_cast<float>(value);
}

[[nodiscard]] inline std::uint32_t bit_cast_u32(float value) noexcept {
  return std::bit_cast<std::uint32_t>(value);
}

}  // namespace ac6demo
