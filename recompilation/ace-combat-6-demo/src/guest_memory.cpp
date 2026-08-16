#include "ac6demo/guest_memory.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

#include <sys/mman.h>
#include <dlfcn.h>

namespace ac6demo {
namespace {

void watch_ib_host_write(std::uint32_t address, std::size_t size,
                         void *caller) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_IB_WRITERS") != nullptr;
  const std::uint64_t end = static_cast<std::uint64_t>(address) + size;
  if (!enabled || address >= 0x1274CF54U || end <= 0x1274A000U) {
    return;
  }
  Dl_info info{};
  const bool resolved = dladdr(caller, &info) != 0 && info.dli_fbase != nullptr;
  const auto module_offset = resolved
                                 ? static_cast<std::uintptr_t>(
                                       static_cast<const char *>(caller) -
                                       static_cast<const char *>(info.dli_fbase))
                                 : 0U;
  std::fprintf(stderr,
               "AC6_IB_HOST_WRITE address=0x%08X size=%zu "
               "caller_module_offset=0x%zX symbol=%s\n",
               address, size, module_offset,
               resolved && info.dli_sname != nullptr ? info.dli_sname : "");
}

void watch_frontbuffer_host_write(std::uint32_t address, std::size_t size,
                                  void *caller) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_FRONTBUFFER_WRITERS") != nullptr;
  constexpr std::uint32_t kFrontbufferBegin = 0x1374A000U;
  constexpr std::uint32_t kFrontbufferEnd = 0x13AE2000U;
  const auto end = static_cast<std::uint64_t>(address) + size;
  if (!enabled || address >= kFrontbufferEnd || end <= kFrontbufferBegin) {
    return;
  }
  Dl_info info{};
  const bool resolved = dladdr(caller, &info) != 0 && info.dli_fbase != nullptr;
  const auto module_offset = resolved
                                 ? static_cast<std::uintptr_t>(
                                       static_cast<const char *>(caller) -
                                       static_cast<const char *>(info.dli_fbase))
                                 : 0U;
  std::fprintf(stderr,
               "AC6_FRONTBUFFER_HOST_WRITE address=0x%08X size=%zu "
               "caller_module_offset=0x%zX symbol=%s\n",
               address, size, module_offset,
               resolved && info.dli_sname != nullptr ? info.dli_sname : "");
}

void watch_frontbuffer_host_read(std::uint32_t address, std::size_t size,
                                 void *caller) noexcept {
  static const bool enabled =
      std::getenv("AC6_DEMO_WATCH_FRONTBUFFER_HOST_READS") != nullptr;
  constexpr std::uint32_t kFrontbufferBegin = 0x1374A000U;
  constexpr std::uint32_t kFrontbufferEnd = 0x13AE2000U;
  const auto end = static_cast<std::uint64_t>(address) + size;
  if (!enabled || address >= kFrontbufferEnd || end <= kFrontbufferBegin) {
    return;
  }
  static std::uint32_t record_count = 0U;
  if (record_count >= 128U) {
    return;
  }
  ++record_count;
  Dl_info info{};
  const bool resolved = dladdr(caller, &info) != 0 && info.dli_fbase != nullptr;
  const auto module_offset =
      resolved ? static_cast<std::uintptr_t>(
                     static_cast<const char *>(caller) -
                     static_cast<const char *>(info.dli_fbase))
               : 0U;
  std::fprintf(stderr,
               "AC6_FRONTBUFFER_HOST_READ address=0x%08X size=%zu "
               "caller_module_offset=0x%zX symbol=%s\n",
               address, size, module_offset,
               resolved && info.dli_sname != nullptr ? info.dli_sname : "");
}

} // namespace

GuestMemory::GuestMemory()
    : pages_(static_cast<std::size_t>(kGuestPageCount)) {
  raw_base_ = mmap(nullptr, static_cast<std::size_t>(kGuestMemoryBytes), PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (raw_base_ == MAP_FAILED) {
    raw_base_ = nullptr;
    throw std::system_error(errno, std::generic_category(),
                            "reserve 4 GiB guest address space");
  }
}

GuestMemory::~GuestMemory() {
  if (raw_base_ != nullptr) {
    (void)munmap(raw_base_, static_cast<std::size_t>(kGuestMemoryBytes));
  }
}

void GuestMemory::check_range(std::uint32_t address, std::size_t length) {
  const std::uint64_t end = static_cast<std::uint64_t>(address) + length;
  if (end > kGuestMemoryBytes) {
    throw RuntimeTrap("guest memory range exceeds 4 GiB", 0, 0, address);
  }
}

GuestMemory::Page& GuestMemory::ensure_page(std::uint32_t page) {
  auto& slot = pages_[page];
  if (!slot) {
    slot = std::make_unique<Page>();
    ++committed_page_count_;
  }
  return *slot;
}

std::uint64_t* GuestMemory::ensure_generation_page(std::uint32_t page) const {
  if (page >= pages_.size() || pages_[page] == nullptr) {
    return nullptr;
  }
  auto& generations = pages_[page]->generations;
  if (!generations) {
    generations = std::make_unique<std::uint64_t[]>(kGuestPageBytes /
                                                    sizeof(std::uint32_t));
  }
  return generations.get();
}

void GuestMemory::bump_generation(std::uint32_t address, std::size_t length) {
  if (length == 0U) {
    return;
  }
  const std::uint64_t last = static_cast<std::uint64_t>(address) + length - 1U;
  const auto first_page = address / kGuestPageBytes;
  const auto last_page = static_cast<std::uint32_t>(last / kGuestPageBytes);
  for (std::uint32_t page = first_page; page <= last_page; ++page) {
    const auto* page_metadata = pages_[page].get();
    if (page_metadata == nullptr || page_metadata->generations == nullptr) {
      continue;
    }
    const auto page_begin = static_cast<std::uint64_t>(page) * kGuestPageBytes;
    const auto range_begin = std::max<std::uint64_t>(address, page_begin);
    const auto range_end = std::min<std::uint64_t>(last, page_begin + kGuestPageBytes - 1U);
    const auto first_granule = static_cast<std::uint32_t>((range_begin - page_begin) / 4U);
    const auto last_granule = static_cast<std::uint32_t>((range_end - page_begin) / 4U);
    for (std::uint32_t granule = first_granule; granule <= last_granule; ++granule) {
      ++page_metadata->generations[granule];
    }
    if (page == last_page || page == kGuestPageCount - 1U) {
      break;
    }
  }
}

const GuestMemory::Mmio* GuestMemory::find_mmio(std::uint32_t address,
                                                std::size_t length) const {
  const std::uint64_t end = static_cast<std::uint64_t>(address) + length;
  for (const auto& region : mmio_) {
    const std::uint64_t region_end = static_cast<std::uint64_t>(region.address) + region.length;
    if (static_cast<std::uint64_t>(address) >= region.address && end <= region_end) {
      return &region;
    }
  }
  return nullptr;
}

GuestMemory::Mmio* GuestMemory::find_mmio(std::uint32_t address, std::size_t length) {
  const std::uint64_t end = static_cast<std::uint64_t>(address) + length;
  for (auto& region : mmio_) {
    const std::uint64_t region_end = static_cast<std::uint64_t>(region.address) + region.length;
    if (static_cast<std::uint64_t>(address) >= region.address && end <= region_end) {
      return &region;
    }
  }
  return nullptr;
}

const std::byte* GuestMemory::pointer(std::uint32_t address, std::size_t length) const {
  check_range(address, length);
  if (length == 0U) {
    return nullptr;
  }
  const std::uint32_t first_page = address / kGuestPageBytes;
  const std::uint32_t last_page =
      static_cast<std::uint32_t>((static_cast<std::uint64_t>(address) + length - 1U) /
                                 kGuestPageBytes);
  if (first_page != last_page) {
    return nullptr;
  }
  if (pages_[first_page] == nullptr) {
    return nullptr;
  }
  if (pages_[first_page]->protection == 1U) {
    throw RuntimeTrap("read from protected guest page", 0, 0, address);
  }
  return static_cast<const std::byte*>(raw_base_) + address;
}

std::byte* GuestMemory::pointer(std::uint32_t address, std::size_t length) {
  const auto* readable = std::as_const(*this).pointer(address, length);
  if (readable == nullptr) {
    return nullptr;
  }
  const auto protection_value = pages_[address / kGuestPageBytes]->protection;
  if (protection_value != 4U && protection_value != 0x40U) {
    throw RuntimeTrap("write to protected guest page", 0, 0, address);
  }
  return const_cast<std::byte*>(readable);
}

void GuestMemory::set_protection(std::uint32_t address, std::size_t length,
                                 std::uint32_t protection_value) {
  check_range(address, length);
  if (length == 0U) {
    throw std::invalid_argument("guest protection range is empty");
  }
  std::lock_guard lock(mutex_);
  const auto first = address / kGuestPageBytes;
  const auto last = static_cast<std::uint32_t>(
      (static_cast<std::uint64_t>(address) + length - 1U) / kGuestPageBytes);
  for (std::uint32_t page = first; page <= last; ++page) {
    if (pages_[page] == nullptr) {
      throw RuntimeTrap("protection of unmapped guest page", 0, 0,
                        page * kGuestPageBytes);
    }
  }
  for (std::uint32_t page = first; page <= last; ++page) {
    pages_[page]->protection = protection_value;
  }
}

std::uint32_t GuestMemory::protection(std::uint32_t address) const {
  check_range(address, 1U);
  std::lock_guard lock(mutex_);
  const auto& page = pages_[address / kGuestPageBytes];
  return page == nullptr ? 0U : page->protection;
}

void GuestMemory::map_zero(std::uint32_t address, std::size_t length) {
  check_range(address, length);
  std::lock_guard lock(mutex_);
  if (length == 0U) {
    return;
  }
  const std::uint64_t last = static_cast<std::uint64_t>(address) + length - 1U;
  for (const auto& region : mmio_) {
    const std::uint64_t left = address;
    const std::uint64_t right = left + length;
    const std::uint64_t other_left = region.address;
    const std::uint64_t other_right = other_left + region.length;
    if (left < other_right && other_left < right) {
      throw std::invalid_argument("guest RAM mapping overlaps MMIO");
    }
  }
  const std::uint32_t first_page = address / kGuestPageBytes;
  const std::uint32_t last_page = static_cast<std::uint32_t>(last / kGuestPageBytes);
  const std::size_t page_count =
      static_cast<std::size_t>(last_page - first_page) + 1U;
  auto* page_address = static_cast<std::byte*>(raw_base_) +
                       static_cast<std::size_t>(first_page) * kGuestPageBytes;
  if (mprotect(page_address, page_count * kGuestPageBytes,
               PROT_READ | PROT_WRITE) != 0) {
    throw RuntimeTrap("cannot commit guest memory pages", 0, 0, address);
  }
  for (std::uint64_t page = address / kGuestPageBytes;
       page <= last / kGuestPageBytes; ++page) {
    (void)ensure_page(static_cast<std::uint32_t>(page));
  }
}

void GuestMemory::map_bytes(std::uint32_t address, std::span<const std::byte> bytes) {
  map_zero(address, bytes.size());
  std::lock_guard lock(mutex_);
  std::memcpy(static_cast<std::byte*>(raw_base_) + address, bytes.data(), bytes.size());
}

void GuestMemory::map_mmio(std::uint32_t address, std::size_t length, MmioRead read,
                           MmioWrite write) {
  check_range(address, length);
  if (length == 0U || !read || !write) {
    throw std::invalid_argument("MMIO mappings require a non-empty range and both callbacks");
  }
  if (length > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("MMIO mapping is too large");
  }
  std::lock_guard lock(mutex_);
  for (const auto& region : mmio_) {
    const std::uint64_t left = address;
    const std::uint64_t right = left + length;
    const std::uint64_t other_left = region.address;
    const std::uint64_t other_right = other_left + region.length;
    if (left < other_right && other_left < right) {
      throw std::invalid_argument("overlapping MMIO mappings are not permitted");
    }
  }
  const std::uint64_t last = static_cast<std::uint64_t>(address) + length - 1U;
  for (std::uint64_t page = address / kGuestPageBytes;
       page <= last / kGuestPageBytes; ++page) {
    if (pages_[static_cast<std::uint32_t>(page)] != nullptr) {
      throw std::invalid_argument("MMIO mapping overlaps committed guest RAM");
    }
  }
  mmio_.push_back(Mmio{address, static_cast<std::uint32_t>(length), std::move(read),
                       std::move(write)});
}

std::uint8_t GuestMemory::load_u8(std::uint32_t address) const {
  if (const auto* mmio = find_mmio(address, 1U); mmio != nullptr) {
    return static_cast<std::uint8_t>(mmio->read(address, 1U));
  }
  const auto* data = pointer(address, 1U);
  if (data == nullptr) {
    throw RuntimeTrap("unmapped guest byte read", 0, 0, address);
  }
  return std::to_integer<std::uint8_t>(*data);
}

std::uint16_t GuestMemory::load_u16(std::uint32_t address) const {
  if (const auto* mmio = find_mmio(address, 2U); mmio != nullptr) {
    return static_cast<std::uint16_t>(mmio->read(address, 2U));
  }
  if (const auto* bytes = pointer(address, 2U); bytes != nullptr) {
    std::uint16_t value{};
    std::memcpy(&value, bytes, sizeof(value));
    return __builtin_bswap16(value);
  }
  std::array<std::byte, 2> data{};
  for (std::size_t index = 0; index < data.size(); ++index) {
    const auto* byte = pointer(address + static_cast<std::uint32_t>(index), 1U);
    if (byte == nullptr) {
      throw RuntimeTrap("unmapped guest 16-bit read", 0, 0, address);
    }
    data[index] = *byte;
  }
  return read_be16(data, 0U);
}

std::uint32_t GuestMemory::load_u32(std::uint32_t address) const {
  if (const auto* mmio = find_mmio(address, 4U); mmio != nullptr) {
    return static_cast<std::uint32_t>(mmio->read(address, 4U));
  }
  if (const auto* bytes = pointer(address, 4U); bytes != nullptr) {
    std::uint32_t value{};
    std::memcpy(&value, bytes, sizeof(value));
    return __builtin_bswap32(value);
  }
  std::array<std::byte, 4> data{};
  for (std::size_t index = 0; index < data.size(); ++index) {
    const auto* byte = pointer(address + static_cast<std::uint32_t>(index), 1U);
    if (byte == nullptr) {
      throw RuntimeTrap("unmapped guest 32-bit read", 0, 0, address);
    }
    data[index] = *byte;
  }
  return read_be32(data, 0U);
}

std::uint64_t GuestMemory::load_u64(std::uint32_t address) const {
  if (const auto* mmio = find_mmio(address, 8U); mmio != nullptr) {
    return mmio->read(address, 8U);
  }
  if (const auto* bytes = pointer(address, 8U); bytes != nullptr) {
    std::uint64_t value{};
    std::memcpy(&value, bytes, sizeof(value));
    return __builtin_bswap64(value);
  }
  std::array<std::byte, 8> data{};
  for (std::size_t index = 0; index < data.size(); ++index) {
    const auto* byte = pointer(address + static_cast<std::uint32_t>(index), 1U);
    if (byte == nullptr) {
      throw RuntimeTrap("unmapped guest 64-bit read", 0, 0, address);
    }
    data[index] = *byte;
  }
  return read_be64(data, 0U);
}

std::vector<std::byte> GuestMemory::load_bytes(std::uint32_t address,
                                               std::size_t length) const {
  watch_frontbuffer_host_read(address, length, __builtin_return_address(0));
  check_range(address, length);
  std::vector<std::byte> result(length);
  for (std::size_t index = 0; index < length; ++index) {
    result[index] = static_cast<std::byte>(load_u8(address + static_cast<std::uint32_t>(index)));
  }
  return result;
}

void GuestMemory::store_u8(std::uint32_t address, std::uint8_t value) {
  watch_ib_host_write(address, 1U, __builtin_return_address(0));
  if (auto* mmio = find_mmio(address, 1U); mmio != nullptr) {
    bump_generation(address, 1U);
    mmio->write(address, value, 1U);
    return;
  }
  auto* data = pointer(address, 1U);
  if (data == nullptr) {
    throw RuntimeTrap("unmapped guest byte write", 0, 0, address);
  }
  *data = static_cast<std::byte>(value);
  bump_generation(address, 1U);
}

void GuestMemory::store_u16(std::uint32_t address, std::uint16_t value) {
  watch_ib_host_write(address, 2U, __builtin_return_address(0));
  std::array<std::byte, 2> data{};
  write_be16(data, 0U, value);
  if (auto* mmio = find_mmio(address, data.size()); mmio != nullptr) {
    bump_generation(address, data.size());
    mmio->write(address, value, data.size());
    return;
  }
  if (auto* bytes = pointer(address, data.size()); bytes != nullptr) {
    std::memcpy(bytes, data.data(), data.size());
    bump_generation(address, data.size());
    return;
  }
  for (std::size_t index = 0; index < data.size(); ++index) {
    auto* byte = pointer(address + static_cast<std::uint32_t>(index), 1U);
    if (byte == nullptr) {
      throw RuntimeTrap("unmapped guest 16-bit write", 0, 0, address);
    }
    *byte = data[index];
  }
  bump_generation(address, data.size());
}

void GuestMemory::store_u32(std::uint32_t address, std::uint32_t value) {
  watch_ib_host_write(address, 4U, __builtin_return_address(0));
  std::array<std::byte, 4> data{};
  write_be32(data, 0U, value);
  if (auto* mmio = find_mmio(address, data.size()); mmio != nullptr) {
    bump_generation(address, data.size());
    mmio->write(address, value, data.size());
    return;
  }
  if (auto* bytes = pointer(address, data.size()); bytes != nullptr) {
    std::memcpy(bytes, data.data(), data.size());
    bump_generation(address, data.size());
    return;
  }
  for (std::size_t index = 0; index < data.size(); ++index) {
    auto* byte = pointer(address + static_cast<std::uint32_t>(index), 1U);
    if (byte == nullptr) {
      throw RuntimeTrap("unmapped guest 32-bit write", 0, 0, address);
    }
    *byte = data[index];
  }
  bump_generation(address, data.size());
}

void GuestMemory::store_u64(std::uint32_t address, std::uint64_t value) {
  watch_ib_host_write(address, 8U, __builtin_return_address(0));
  std::array<std::byte, 8> data{};
  write_be64(data, 0U, value);
  if (auto* mmio = find_mmio(address, data.size()); mmio != nullptr) {
    bump_generation(address, data.size());
    mmio->write(address, value, data.size());
    return;
  }
  if (auto* bytes = pointer(address, data.size()); bytes != nullptr) {
    std::memcpy(bytes, data.data(), data.size());
    bump_generation(address, data.size());
    return;
  }
  for (std::size_t index = 0; index < data.size(); ++index) {
    auto* byte = pointer(address + static_cast<std::uint32_t>(index), 1U);
    if (byte == nullptr) {
      throw RuntimeTrap("unmapped guest 64-bit write", 0, 0, address);
    }
    *byte = data[index];
  }
  bump_generation(address, data.size());
}

void GuestMemory::store_bytes(std::uint32_t address, std::span<const std::byte> bytes) {
  watch_ib_host_write(address, bytes.size(), __builtin_return_address(0));
  watch_frontbuffer_host_write(address, bytes.size(), __builtin_return_address(0));
  check_range(address, bytes.size());
  std::size_t copied = 0;
  while (copied < bytes.size()) {
    const std::uint32_t current = address + static_cast<std::uint32_t>(copied);
    const std::size_t count = std::min<std::size_t>(bytes.size() - copied, 1U);
    store_u8(current, std::to_integer<std::uint8_t>(bytes[copied]));
    copied += count;
  }
}

std::uint64_t GuestMemory::write_generation(std::uint32_t address,
                                            std::size_t length) const {
  check_range(address, length);
  if (length == 0U) {
    return 0U;
  }
  const auto page = address / kGuestPageBytes;
  auto* generations = ensure_generation_page(page);
  if (generations == nullptr) {
    return 0U;
  }
  const auto granule = (address % kGuestPageBytes) / sizeof(std::uint32_t);
  return generations[granule];
}

bool GuestMemory::mapped(std::uint32_t address, std::size_t length) const noexcept {
  try {
    check_range(address, length);
  } catch (...) {
    return false;
  }
  if (find_mmio(address, length) != nullptr) {
    return true;
  }
  if (length == 0U) {
    return true;
  }
  const auto first_page = address / kGuestPageBytes;
  const auto last_page = static_cast<std::uint32_t>(
      (static_cast<std::uint64_t>(address) + length - 1U) / kGuestPageBytes);
  for (auto page = first_page; page <= last_page; ++page) {
    if (pages_[page] == nullptr) {
      return false;
    }
    if (page == last_page || page == kGuestPageCount - 1U) {
      break;
    }
  }
  return true;
}

}  // namespace ac6demo
