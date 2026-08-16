#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace ac6demo {

// XEX mission-bin pointers are guest addresses. Keeping them explicitly
// 32-bit prevents the host pointer size from changing the serialized ABI.
using GuestAddress = std::uint32_t;

struct UnitBinAbi final {
  GuestAddress serialized_data{};  // +0x00
  GuestAddress word_04{};          // +0x04; identity not yet qualified
};

static_assert(sizeof(UnitBinAbi) == 0x08);
static_assert(offsetof(UnitBinAbi, serialized_data) == 0x00);
static_assert(offsetof(UnitBinAbi, word_04) == 0x04);

struct UnitTblBinAbi final {
  GuestAddress serialized_data{};  // +0x00
  GuestAddress units{};            // +0x04; UnitBinAbi stride 0x08
  GuestAddress objects{};          // +0x08; ObjBinAbi stride 0x20
};

static_assert(sizeof(UnitTblBinAbi) == 0x0c);
static_assert(offsetof(UnitTblBinAbi, units) == 0x04);
static_assert(offsetof(UnitTblBinAbi, objects) == 0x08);

struct DurableBinAbi final {
  GuestAddress payload{};  // +0x00; non-owning pointer into decoded mission data
  std::array<std::byte, 0x0c> reserved{};
};

static_assert(sizeof(DurableBinAbi) == 0x10);
static_assert(offsetof(DurableBinAbi, payload) == 0x00);

struct ObjBinAbi final {
  GuestAddress serialized_data{};  // +0x00
  GuestAddress child_type_0{};     // +0x04
  GuestAddress child_type_1{};     // +0x08
  GuestAddress durable{};          // +0x0c -> DurableBinAbi
  GuestAddress weapon{};           // +0x10 -> WeaponBin (opaque here)
  GuestAddress child_type_4{};     // +0x14
  GuestAddress child_type_5{};     // +0x18
  GuestAddress child_type_6{};     // +0x1c
};

static_assert(sizeof(ObjBinAbi) == 0x20);
static_assert(offsetof(ObjBinAbi, durable) == 0x0c);
static_assert(offsetof(ObjBinAbi, weapon) == 0x10);

// A resolved view is deliberately length-less: ObjBin::read resolves and
// stores the payload address but does not establish a payload size or schema.
struct DurableBinView final {
  const std::byte* data{};

  [[nodiscard]] bool valid() const noexcept { return data != nullptr; }
};

// Resolve the guest address stored by ObjBin::read against the still-live
// decoded mission buffer. The returned view deliberately carries no length:
// the XEX reader proves only the address, not the payload extent or schema.
[[nodiscard]] inline DurableBinView resolve_durable_payload(
    const DurableBinAbi& wrapper,
    GuestAddress decoded_base,
    std::span<const std::byte> decoded) noexcept {
  if (wrapper.payload == 0 || wrapper.payload < decoded_base) {
    return {};
  }

  const std::uint64_t offset =
      static_cast<std::uint64_t>(wrapper.payload - decoded_base);
  if (offset >= decoded.size()) {
    return {};
  }
  return {decoded.data() + static_cast<std::size_t>(offset)};
}

}  // namespace ac6demo
