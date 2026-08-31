#include "ac6/native_guest_memory.h"

#include <algorithm>
#include <sys/mman.h>

namespace ac6::native {

GuestAddressSpace::GuestAddressSpace() noexcept {
  void* mapping = mmap(nullptr, kAddressSpaceSize, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (mapping != MAP_FAILED) {
    base_ = static_cast<std::uint8_t*>(mapping);
  }
}

GuestAddressSpace::~GuestAddressSpace() {
  if (base_ != nullptr) munmap(base_, kAddressSpaceSize);
}

bool GuestAddressSpace::contains(std::uint32_t address,
                                 std::size_t length) const noexcept {
  if (length > kAddressSpaceSize) return false;
  return static_cast<std::uint64_t>(address) <=
         kAddressSpaceSize - static_cast<std::uint64_t>(length);
}

std::span<std::uint8_t> GuestAddressSpace::view(std::uint32_t address,
                                                std::size_t length) noexcept {
  if (base_ == nullptr || !contains(address, length)) return {};
  return {base_ + address, length};
}

bool GuestAddressSpace::write(std::uint32_t address,
                              std::span<const std::uint8_t> bytes) noexcept {
  if (base_ == nullptr || !contains(address, bytes.size())) return false;
  std::copy(bytes.begin(), bytes.end(), base_ + address);
  return true;
}

}  // namespace ac6::native
