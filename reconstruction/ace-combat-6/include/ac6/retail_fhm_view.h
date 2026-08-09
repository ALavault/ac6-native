#pragma once

#include "ac6/retail_container_index.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace ac6::retail {

// A bounded non-owning view of one native-endian retail FHM. The source bytes
// must outlive the view and every span returned from it.
class RetailFhmView final {
 public:
  static std::optional<RetailFhmView> open(
      std::span<const std::uint8_t> bytes) noexcept;

  std::uint32_t child_count() const noexcept { return index_.count; }
  std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }
  std::optional<std::uint32_t> child_length(
      std::uint32_t index) const noexcept;
  std::optional<std::span<const std::uint8_t>> child(
      std::uint32_t index) const noexcept;
  std::optional<RetailFhmView> nested(std::uint32_t index) const noexcept;
  std::optional<std::span<const std::uint8_t>> descendant(
      std::span<const std::uint32_t> path) const noexcept;

 private:
  std::span<const std::uint8_t> bytes_;
  ContainerIndex index_{};
};

}  // namespace ac6::retail
