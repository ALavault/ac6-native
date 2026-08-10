#include "ac6/retail_frontend_resources.h"

#include "ac6/retail_fhm_view.h"

#include <algorithm>
#include <array>
#include <span>
#include <vector>

namespace ac6::retail {
namespace {

constexpr std::array<std::uint8_t, 4> kFhmMagic{'F', 'H', 'M', ' '};
constexpr std::array<std::uint8_t, 4> kNfhMagic{'N', 'F', 'H', '\0'};
constexpr std::uint32_t kMaximumWalkNodes = 1'000'000;
constexpr std::uint32_t kMinimumFontLeaves = 20;

std::uint32_t be32(const std::uint8_t* bytes) noexcept {
  return (static_cast<std::uint32_t>(bytes[0]) << 24u) |
         (static_cast<std::uint32_t>(bytes[1]) << 16u) |
         (static_cast<std::uint32_t>(bytes[2]) << 8u) |
         static_cast<std::uint32_t>(bytes[3]);
}

struct Walk final {
  std::uint32_t fhm_nodes{};
  std::uint32_t nfh_nodes{};
  std::uint32_t visited{};
  bool valid{true};
};

void walk(std::span<const std::uint8_t> bytes, std::uint32_t depth,
          Walk& state) noexcept {
  if (!state.valid || depth > 32u || bytes.size() < 4u ||
      state.visited++ >= kMaximumWalkNodes) {
    state.valid = false;
    return;
  }
  if (std::equal(kNfhMagic.begin(), kNfhMagic.end(), bytes.begin())) {
    // NFH starts with a big-endian table count and a non-zero glyph count.
    // The full glyph atlas remains an opaque retail blob at this boundary;
    // these fields are enough to reject truncation and random bytes while
    // avoiding a guessed text codec.
    if (bytes.size() < 16u || be32(bytes.data() + 4u) == 0u ||
        be32(bytes.data() + 4u) > 4096u || be32(bytes.data() + 8u) == 0u) {
      state.valid = false;
      return;
    }
    ++state.nfh_nodes;
    return;
  }
  if (!std::equal(kFhmMagic.begin(), kFhmMagic.end(), bytes.begin())) return;
  const std::optional<RetailFhmView> view = RetailFhmView::open(bytes);
  if (!view.has_value()) {
    state.valid = false;
    return;
  }
  ++state.fhm_nodes;
  for (std::uint32_t index = 0; index < view->child_count(); ++index) {
    const std::optional<std::span<const std::uint8_t>> child = view->child(index);
    if (!child.has_value()) {
      // Empty FHM slots are legal capacity, but a non-zero child that cannot
      // be read is a malformed resource and must fail closed.
      if (view->child_length(index).value_or(0) != 0) state.valid = false;
      continue;
    }
    walk(*child, depth + 1u, state);
    if (!state.valid) return;
  }
}

}  // namespace

std::optional<RetailFrontendResources> RetailFrontendResources::open(
    const RetailContentStore& store) noexcept {
  if (!store.valid()) return std::nullopt;
  RetailFrontendResources resources;
  resources.content_index_sha256_ = store.index_sha256();
  for (std::size_t ordinal = 0;
       ordinal < kPalFrontendFontDataTableEntries.size(); ++ordinal) {
    const std::uint32_t entry = kPalFrontendFontDataTableEntries[ordinal];
    const RetailContentRecord* record = store.find(entry);
    if (record == nullptr || record->payload_size == 0u) return std::nullopt;
    std::vector<std::uint8_t> payload;
    if (!store.read_payload(entry, payload) || payload.size() != record->payload_size) {
      return std::nullopt;
    }
    Walk state;
    walk(payload, 0u, state);
    if (!state.valid || state.nfh_nodes < kMinimumFontLeaves || state.fhm_nodes == 0u) {
      return std::nullopt;
    }
    resources.fonts_[ordinal] = {entry, payload.size(), state.fhm_nodes, state.nfh_nodes};
  }
  resources.complete_ = true;
  return resources;
}

}  // namespace ac6::retail
