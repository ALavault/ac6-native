#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace ac6::native {

struct PpcVmx128 final {
  std::array<std::uint8_t, 16> bytes{};
};

struct PpcContext final {
  std::array<std::uint64_t, 32> gpr{};
  std::array<PpcVmx128, 128> vmx{};
  std::uint32_t cr{};
  std::uint32_t xer{};
  std::uint64_t lr{};
  std::uint64_t ctr{};
};

class GuestMemory final {
 public:
  explicit GuestMemory(std::size_t bytes);

  [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }
  [[nodiscard]] bool contains(std::uint32_t address, std::size_t length) const noexcept;
  [[nodiscard]] bool load_u8(std::uint32_t address, std::uint8_t& value) const noexcept;
  [[nodiscard]] bool load_u16(std::uint32_t address, std::uint16_t& value) const noexcept;
  [[nodiscard]] bool load_u32(std::uint32_t address, std::uint32_t& value) const noexcept;
  [[nodiscard]] bool load_u64(std::uint32_t address, std::uint64_t& value) const noexcept;
  [[nodiscard]] bool store_u8(std::uint32_t address, std::uint8_t value) noexcept;
  [[nodiscard]] bool store_u16(std::uint32_t address, std::uint16_t value) noexcept;
  [[nodiscard]] bool store_u32(std::uint32_t address, std::uint32_t value) noexcept;
  [[nodiscard]] bool store_u64(std::uint32_t address, std::uint64_t value) noexcept;

  [[nodiscard]] bool lwarx(std::uint32_t address, std::uint32_t& value) noexcept;
  [[nodiscard]] bool stwcx(std::uint32_t address, std::uint32_t value) noexcept;
  [[nodiscard]] bool ldarx(std::uint32_t address, std::uint64_t& value) noexcept;
  [[nodiscard]] bool stdcx(std::uint32_t address, std::uint64_t value) noexcept;
  [[nodiscard]] std::uint32_t condition_register() const noexcept {
    return cr_;
  }

 private:
  std::vector<std::uint8_t> bytes_;
  std::uint32_t reservation_address_{};
  bool reservation_valid_{};
  std::uint32_t cr_{};

  void invalidate_reservation(std::uint32_t address,
                              std::size_t length) noexcept;
  void publish_cr0(bool success) noexcept;
};

class PpcDispatcher final {
 public:
  using Function = std::function<void(PpcContext&, GuestMemory&)>;

  [[nodiscard]] bool bind(std::uint32_t address, Function function);
  [[nodiscard]] bool call(std::uint32_t address, PpcContext& context,
                          GuestMemory& memory) const;
  [[nodiscard]] std::size_t size() const noexcept { return functions_.size(); }

 private:
  std::unordered_map<std::uint32_t, Function> functions_;
};

}  // namespace ac6::native
