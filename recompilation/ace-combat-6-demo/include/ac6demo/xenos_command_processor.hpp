#pragma once

#include "ac6demo/xenos_commands.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ac6demo {

class XenosCommandProcessor final {
public:
  using GuestBytes = std::array<std::byte, 4>;
  using MemoryReadCallback =
      std::function<std::optional<GuestBytes>(std::uint32_t)>;

  // The input contains host-valued dwords from an already bounded and
  // flattened PM4 stream. The register file and bin state commit only after
  // the complete span has been accepted.
  [[nodiscard]] XenosBatchResult
  process_batch(std::span<const std::uint32_t> dwords,
                const MemoryReadCallback &read_memory = {});

  [[nodiscard]] std::uint32_t
  register_value(std::uint16_t index) const noexcept {
    return registers_[index];
  }
  [[nodiscard]] std::uint64_t bin_mask() const noexcept { return bin_mask_; }
  [[nodiscard]] std::uint64_t bin_select() const noexcept {
    return bin_select_;
  }
  [[nodiscard]] std::uint32_t
  gamma_lut_value(std::uint8_t index) const noexcept {
    return gamma_lut_[index];
  }
  [[nodiscard]] std::uint8_t gamma_lut_component() const noexcept {
    return gamma_lut_component_;
  }

private:
  std::array<std::uint32_t, kXenosRegisterCount> registers_{};
  std::uint64_t bin_mask_{0xFFFFFFFFULL};
  std::uint64_t bin_select_{0xFFFFFFFFULL};
  std::string vertex_shader_sha256_;
  std::string pixel_shader_sha256_;
  std::array<std::uint32_t, 256> gamma_lut_{};
  std::uint8_t gamma_lut_component_{};
};

} // namespace ac6demo
