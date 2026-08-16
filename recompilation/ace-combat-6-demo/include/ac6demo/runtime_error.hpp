#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace ac6demo {

class RuntimeTrap final : public std::runtime_error {
 public:
  RuntimeTrap(std::string reason, std::uint64_t tick = 0,
              std::uint32_t lr = 0, std::uint32_t address = 0)
      : std::runtime_error(std::move(reason)), tick_(tick), lr_(lr), address_(address) {}

  [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
  [[nodiscard]] std::uint32_t lr() const noexcept { return lr_; }
  [[nodiscard]] std::uint32_t address() const noexcept { return address_; }

 private:
  std::uint64_t tick_{};
  std::uint32_t lr_{};
  std::uint32_t address_{};
};

}  // namespace ac6demo
