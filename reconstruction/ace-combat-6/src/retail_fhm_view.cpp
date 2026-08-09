#include "ac6/retail_fhm_view.h"

#include <algorithm>
#include <array>
#include <limits>

namespace ac6::retail {
namespace {

constexpr std::array<std::uint8_t, 4> kFhmMagic{'F', 'H', 'M', ' '};
constexpr std::uint32_t kMaximumChildren = 4096;

bool add_within(std::uint64_t left, std::uint64_t right,
                std::uint64_t limit) noexcept {
  return left <= limit && right <= limit - left;
}

}  // namespace

std::optional<RetailFhmView> RetailFhmView::open(
    std::span<const std::uint8_t> bytes) noexcept {
  if (bytes.size() < 8 ||
      bytes.size() > std::numeric_limits<std::uint32_t>::max() ||
      !std::equal(kFhmMagic.begin(), kFhmMagic.end(), bytes.begin())) {
    return std::nullopt;
  }

  RetailFhmView view;
  view.bytes_ = bytes;
  if (!parse_container_index(view.index_, bytes.data(), bytes.size(), 0) ||
      view.index_.version != 1 || view.index_.endian != kNativeEndianFlag ||
      view.index_.count == 0 || view.index_.count > kMaximumChildren) {
    return std::nullopt;
  }

  const std::uint64_t table_begin =
      static_cast<std::uint64_t>(view.index_.header_size) + 4u;
  const std::uint64_t table_size =
      static_cast<std::uint64_t>(view.index_.count) * 4u * sizeof(std::uint32_t);
  if (!add_within(table_begin, table_size, bytes.size())) return std::nullopt;

  for (std::uint32_t index = 0; index < view.index_.count; ++index) {
    const std::uint32_t length = container_entry_length(
        view.index_, bytes.data(), bytes.size(), index);
    if (length == 0) continue;
    const std::uint32_t offset =
        container_entry(view.index_, bytes.data(), bytes.size(), index);
    if (offset == 0 || !add_within(offset, length, bytes.size())) {
      return std::nullopt;
    }
  }
  return view;
}

std::optional<std::uint32_t> RetailFhmView::child_length(
    std::uint32_t index) const noexcept {
  if (index >= index_.count) return std::nullopt;
  return container_entry_length(index_, bytes_.data(), bytes_.size(), index);
}

std::optional<std::span<const std::uint8_t>> RetailFhmView::child(
    std::uint32_t index) const noexcept {
  if (index >= index_.count) return std::nullopt;
  const std::uint32_t length =
      container_entry_length(index_, bytes_.data(), bytes_.size(), index);
  const std::uint32_t offset =
      container_entry(index_, bytes_.data(), bytes_.size(), index);
  if (offset == 0 || length == 0 || offset > bytes_.size() ||
      length > bytes_.size() - offset) {
    return std::nullopt;
  }
  return bytes_.subspan(offset, length);
}

std::optional<RetailFhmView> RetailFhmView::nested(
    std::uint32_t index) const noexcept {
  const std::optional<std::span<const std::uint8_t>> bytes = child(index);
  return bytes.has_value() ? open(*bytes) : std::nullopt;
}

std::optional<std::span<const std::uint8_t>> RetailFhmView::descendant(
    std::span<const std::uint32_t> path) const noexcept {
  if (path.empty()) return bytes_;
  RetailFhmView current = *this;
  for (std::size_t depth = 0; depth < path.size(); ++depth) {
    const std::optional<std::span<const std::uint8_t>> bytes =
        current.child(path[depth]);
    if (!bytes.has_value() || depth + 1 == path.size()) return bytes;
    const std::optional<RetailFhmView> nested_view = open(*bytes);
    if (!nested_view.has_value()) return std::nullopt;
    current = *nested_view;
  }
  return std::nullopt;
}

}  // namespace ac6::retail
