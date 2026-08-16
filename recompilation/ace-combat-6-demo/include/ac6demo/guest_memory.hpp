#pragma once

#include "ac6demo/endian.hpp"
#include "ac6demo/runtime_error.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace ac6demo {

constexpr std::uint64_t kGuestMemoryBytes = 0x1'0000'0000ULL;
constexpr std::uint32_t kGuestPageBytes = 0x1000U;
constexpr std::uint32_t kGuestPageCount =
    static_cast<std::uint32_t>(kGuestMemoryBytes / kGuestPageBytes);

class GuestMemory final {
 public:
  using MmioRead = std::function<std::uint64_t(std::uint32_t, std::size_t)>;
  using MmioWrite = std::function<void(std::uint32_t, std::uint64_t, std::size_t)>;

  GuestMemory();
  ~GuestMemory();

  GuestMemory(const GuestMemory&) = delete;
  GuestMemory& operator=(const GuestMemory&) = delete;
  GuestMemory(GuestMemory&&) = delete;
  GuestMemory& operator=(GuestMemory&&) = delete;

  // Pointer passed to generated XenonRecomp functions. The mapping is a
  // reserved 4 GiB guest address space; only committed RAM pages are valid.
  // MMIO intentionally remains outside this raw path.
  [[nodiscard]] std::uint8_t* raw_base() noexcept {
    return static_cast<std::uint8_t*>(raw_base_);
  }
  [[nodiscard]] const std::uint8_t* raw_base() const noexcept {
    return static_cast<const std::uint8_t*>(raw_base_);
  }

  void map_zero(std::uint32_t address, std::size_t length);
  void map_bytes(std::uint32_t address, std::span<const std::byte> bytes);
  void map_mmio(std::uint32_t address, std::size_t length, MmioRead read,
                MmioWrite write);

  [[nodiscard]] std::uint8_t load_u8(std::uint32_t address) const;
  [[nodiscard]] std::uint16_t load_u16(std::uint32_t address) const;
  [[nodiscard]] std::uint32_t load_u32(std::uint32_t address) const;
  [[nodiscard]] std::uint64_t load_u64(std::uint32_t address) const;
  [[nodiscard]] std::vector<std::byte> load_bytes(std::uint32_t address,
                                                   std::size_t length) const;

  void store_u8(std::uint32_t address, std::uint8_t value);
  void store_u16(std::uint32_t address, std::uint16_t value);
  void store_u32(std::uint32_t address, std::uint32_t value);
  void store_u64(std::uint32_t address, std::uint64_t value);
  void store_bytes(std::uint32_t address, std::span<const std::byte> bytes);

  // A monotonically increasing version for the aligned guest granule touched
  // by a store. It is the reservation witness used by lwarx/stwcx.
  [[nodiscard]] std::uint64_t write_generation(std::uint32_t address,
                                                std::size_t length = 4U) const;

  [[nodiscard]] bool mapped(std::uint32_t address, std::size_t length = 1U) const noexcept;
  [[nodiscard]] std::uint32_t committed_page_count() const noexcept {
    return committed_page_count_;
  }
  void set_protection(std::uint32_t address, std::size_t length,
                      std::uint32_t protection);
  [[nodiscard]] std::uint32_t protection(std::uint32_t address) const;

 private:
  // Metadata only; bytes live in the contiguous mmap reservation so checked
  // accessors and generated code observe the same guest memory.
  struct Page final {
    // Allocated only after the first reservation on this page. Normal guest
    // stores therefore do not pay for a sparse generation hash lookup.
    mutable std::unique_ptr<std::uint64_t[]> generations;
    std::uint32_t protection{4U};
  };
  struct Mmio final {
    std::uint32_t address{};
    std::uint32_t length{};
    MmioRead read;
    MmioWrite write;
  };

  [[nodiscard]] const Mmio* find_mmio(std::uint32_t address, std::size_t length) const;
  [[nodiscard]] Mmio* find_mmio(std::uint32_t address, std::size_t length);
  [[nodiscard]] const std::byte* pointer(std::uint32_t address, std::size_t length) const;
  [[nodiscard]] std::byte* pointer(std::uint32_t address, std::size_t length);
  static void check_range(std::uint32_t address, std::size_t length);
  [[nodiscard]] Page& ensure_page(std::uint32_t page);
  [[nodiscard]] std::uint64_t* ensure_generation_page(
      std::uint32_t page) const;
  void bump_generation(std::uint32_t address, std::size_t length);

  mutable std::mutex mutex_;
  // Guest execution is serialized by DeterministicScheduler. The hot access
  // path intentionally reads this immutable-between-boundaries table without
  // taking mutex_; map_zero/map_mmio only mutate it at those boundaries or
  // from the current guest token.
  mutable std::vector<std::unique_ptr<Page>> pages_;
  std::vector<Mmio> mmio_;
  void* raw_base_{};
  std::uint32_t committed_page_count_{};
};

}  // namespace ac6demo
