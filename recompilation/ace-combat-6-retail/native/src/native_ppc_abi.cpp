#include "ac6/native_ppc_abi.h"

#include <algorithm>

namespace ac6::native {
namespace {

std::uint16_t be16(const std::uint8_t* bytes) noexcept {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(bytes[0]) << 8u) | bytes[1]);
}

std::uint32_t be32(const std::uint8_t* bytes) noexcept {
  return (static_cast<std::uint32_t>(bytes[0]) << 24u) |
         (static_cast<std::uint32_t>(bytes[1]) << 16u) |
         (static_cast<std::uint32_t>(bytes[2]) << 8u) |
         static_cast<std::uint32_t>(bytes[3]);
}

std::uint64_t be64(const std::uint8_t* bytes) noexcept {
  std::uint64_t result = 0u;
  for (std::size_t index = 0u; index != 8u; ++index) {
    result = (result << 8u) | bytes[index];
  }
  return result;
}

void put16(std::uint8_t* bytes, std::uint16_t value) noexcept {
  bytes[0] = static_cast<std::uint8_t>(value >> 8u);
  bytes[1] = static_cast<std::uint8_t>(value);
}

void put32(std::uint8_t* bytes, std::uint32_t value) noexcept {
  bytes[0] = static_cast<std::uint8_t>(value >> 24u);
  bytes[1] = static_cast<std::uint8_t>(value >> 16u);
  bytes[2] = static_cast<std::uint8_t>(value >> 8u);
  bytes[3] = static_cast<std::uint8_t>(value);
}

void put64(std::uint8_t* bytes, std::uint64_t value) noexcept {
  for (std::size_t index = 0u; index != 8u; ++index) {
    bytes[7u - index] = static_cast<std::uint8_t>(value >> (index * 8u));
  }
}

}  // namespace

GuestMemory::GuestMemory(std::size_t bytes)
    : bytes_(std::min<std::size_t>(bytes, 1u << 30u), 0u) {}

bool GuestMemory::contains(std::uint32_t address,
                           std::size_t length) const noexcept {
  return static_cast<std::size_t>(address) <= bytes_.size() &&
         length <= bytes_.size() - static_cast<std::size_t>(address);
}

bool GuestMemory::load_u8(std::uint32_t address, std::uint8_t& value) const noexcept {
  if (!contains(address, 1u)) return false;
  value = bytes_[address];
  return true;
}

bool GuestMemory::load_u16(std::uint32_t address,
                           std::uint16_t& value) const noexcept {
  if (!contains(address, 2u)) return false;
  value = be16(bytes_.data() + address);
  return true;
}

bool GuestMemory::load_u32(std::uint32_t address,
                           std::uint32_t& value) const noexcept {
  if (!contains(address, 4u)) return false;
  value = be32(bytes_.data() + address);
  return true;
}

bool GuestMemory::load_u64(std::uint32_t address,
                           std::uint64_t& value) const noexcept {
  if (!contains(address, 8u)) return false;
  value = be64(bytes_.data() + address);
  return true;
}

bool GuestMemory::store_u8(std::uint32_t address, std::uint8_t value) noexcept {
  if (!contains(address, 1u)) return false;
  invalidate_reservation(address, 1u);
  bytes_[address] = value;
  return true;
}

bool GuestMemory::store_u16(std::uint32_t address, std::uint16_t value) noexcept {
  if (!contains(address, 2u)) return false;
  invalidate_reservation(address, 2u);
  put16(bytes_.data() + address, value);
  return true;
}

bool GuestMemory::store_u32(std::uint32_t address, std::uint32_t value) noexcept {
  if (!contains(address, 4u)) return false;
  invalidate_reservation(address, 4u);
  put32(bytes_.data() + address, value);
  return true;
}

bool GuestMemory::store_u64(std::uint32_t address, std::uint64_t value) noexcept {
  if (!contains(address, 8u)) return false;
  invalidate_reservation(address, 8u);
  put64(bytes_.data() + address, value);
  return true;
}

bool GuestMemory::lwarx(std::uint32_t address, std::uint32_t& value) noexcept {
  if ((address & 3u) != 0u) return false;
  if (!load_u32(address, value)) return false;
  reservation_address_ = address;
  reservation_valid_ = true;
  return true;
}

bool GuestMemory::stwcx(std::uint32_t address, std::uint32_t value) noexcept {
  const bool success = (address & 3u) == 0u && reservation_valid_ &&
                       reservation_address_ == address;
  reservation_valid_ = false;
  const bool stored = success && store_u32(address, value);
  publish_cr0(stored);
  return stored;
}

bool GuestMemory::ldarx(std::uint32_t address, std::uint64_t& value) noexcept {
  if ((address & 7u) != 0u) return false;
  if (!load_u64(address, value)) return false;
  reservation_address_ = address;
  reservation_valid_ = true;
  return true;
}

bool GuestMemory::stdcx(std::uint32_t address, std::uint64_t value) noexcept {
  const bool success = (address & 7u) == 0u && reservation_valid_ &&
                       reservation_address_ == address;
  reservation_valid_ = false;
  const bool stored = success && store_u64(address, value);
  publish_cr0(stored);
  return stored;
}

void GuestMemory::invalidate_reservation(std::uint32_t address,
                                          std::size_t length) noexcept {
  if (!reservation_valid_) return;
  const std::uint64_t begin = address;
  const std::uint64_t end = begin + length;
  const std::uint64_t reserved_begin = reservation_address_;
  const std::uint64_t reserved_end = reserved_begin + 8u;
  if (begin < reserved_end && reserved_begin < end) reservation_valid_ = false;
}

void GuestMemory::publish_cr0(bool success) noexcept {
  // CR0[EQ] is bit 29 in the full 32-bit condition register.
  cr_ = (cr_ & 0x0FFFFFFFu) | (success ? 0x20000000u : 0u);
}

bool PpcDispatcher::bind(std::uint32_t address, Function function) {
  if (address == 0u || !function) return false;
  return functions_.emplace(address, std::move(function)).second;
}

bool PpcDispatcher::call(std::uint32_t address, PpcContext& context,
                         GuestMemory& memory) const {
  const auto found = functions_.find(address);
  if (found == functions_.end()) return false;
  found->second(context, memory);
  return true;
}

}  // namespace ac6::native
