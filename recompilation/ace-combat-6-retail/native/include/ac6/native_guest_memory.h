#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ac6::native {

// XenonRecomp emits `base + uint32_guest_address`.  Reserve the complete
// 32-bit guest address space so those pointers remain valid on a 64-bit host;
// MAP_NORESERVE keeps the reservation virtual until pages are touched.
class GuestAddressSpace final {
 public:
  static constexpr std::size_t kAddressSpaceSize = std::size_t{1} << 32u;

  GuestAddressSpace() noexcept;
  ~GuestAddressSpace();

  GuestAddressSpace(const GuestAddressSpace&) = delete;
  GuestAddressSpace& operator=(const GuestAddressSpace&) = delete;

  [[nodiscard]] bool valid() const noexcept { return base_ != nullptr; }
  [[nodiscard]] std::uint8_t* base() noexcept { return base_; }
  [[nodiscard]] const std::uint8_t* base() const noexcept { return base_; }
  [[nodiscard]] bool contains(std::uint32_t address,
                              std::size_t length = 1u) const noexcept;
  [[nodiscard]] std::span<std::uint8_t> view(std::uint32_t address,
                                             std::size_t length) noexcept;
  [[nodiscard]] bool write(std::uint32_t address,
                           std::span<const std::uint8_t> bytes) noexcept;

 private:
  std::uint8_t* base_{};
};

}  // namespace ac6::native
