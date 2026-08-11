#include "ac6/retail_mission_world_bundle.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ac6::retail {
namespace {

constexpr std::uint32_t kFirstWorldEntry = 119;
constexpr std::uint32_t kLastWorldEntry = 133;
constexpr std::array<std::uint8_t, 4> kFhmMagic{'F', 'H', 'M', ' '};
constexpr std::array<std::uint8_t, 4> kNtxrMagic{'N', 'T', 'X', 'R'};
constexpr std::array<std::uint8_t, 4> kMcaMagic{'M', 'C', 'A', 0};
constexpr std::array<std::uint8_t, 4> kMcdMagic{'M', 'C', 'D', 0};
constexpr std::array<std::uint8_t, 4> kMciMagic{'M', 'C', 'I', 0};

bool has_magic(std::optional<std::span<const std::uint8_t>> bytes,
               const std::array<std::uint8_t, 4>& magic) noexcept {
  return bytes.has_value() && bytes->size() >= magic.size() &&
         std::equal(magic.begin(), magic.end(), bytes->begin());
}

bool qualified_layout(const RetailCampaignBundle& bundle) noexcept {
  if (bundle.child_count() != 23) return false;
  const std::optional<std::span<const std::uint8_t>> map_bytes = bundle.child(21);
  const std::optional<std::span<const std::uint8_t>> mapset_bytes = bundle.child(22);
  if (!map_bytes.has_value() || !mapset_bytes.has_value()) return false;
  const std::optional<RetailFhmView> map = RetailFhmView::open(*map_bytes);
  const std::optional<RetailFhmView> mapset = RetailFhmView::open(*mapset_bytes);
  if (!map.has_value() || !mapset.has_value() || map->child_count() != 17 ||
      mapset->child_count() != 12) {
    return false;
  }
  if (!has_magic(map->child(1), kMcaMagic) ||
      !has_magic(map->child(2), kMcdMagic) ||
      !has_magic(map->child(3), kMciMagic)) {
    return false;
  }
  for (const std::uint32_t index : {14u, 15u, 16u}) {
    const std::optional<std::span<const std::uint8_t>> bytes = map->child(index);
    if (!bytes.has_value() || !RetailFhmView::open(*bytes).has_value()) return false;
  }
  for (const std::uint32_t index : {5u, 6u}) {
    const std::optional<std::span<const std::uint8_t>> bytes = mapset->child(index);
    if (!bytes.has_value() || !RetailFhmView::open(*bytes).has_value()) return false;
  }
  for (std::uint32_t index = 7; index <= 11; ++index) {
    if (!has_magic(mapset->child(index), kNtxrMagic)) return false;
  }
  return true;
}

}  // namespace

std::optional<std::uint32_t> mission_world_data_table_entry(
    std::uint32_t mission_id) noexcept {
  if (mission_id == 0 || mission_id > kLastWorldEntry - kFirstWorldEntry + 1u) {
    return std::nullopt;
  }
  return kFirstWorldEntry + mission_id - 1u;
}

std::optional<RetailMissionWorldBundle> RetailMissionWorldBundle::open(
    const RetailContentStore& store, std::uint32_t mission_id) {
  const std::optional<std::uint32_t> entry =
      mission_world_data_table_entry(mission_id);
  if (!entry.has_value()) return std::nullopt;
  std::optional<RetailMissionWorldBundle> result = open_entry(store, *entry);
  if (result.has_value()) result->mission_id_ = mission_id;
  return result;
}

std::optional<RetailMissionWorldBundle> RetailMissionWorldBundle::open_entry(
    const RetailContentStore& store, std::uint32_t data_table_entry) {
  if (!store.valid() || data_table_entry < kFirstWorldEntry ||
      data_table_entry > kLastWorldEntry) {
    return std::nullopt;
  }
  std::optional<RetailCampaignBundle> bundle =
      RetailCampaignBundle::open_entry(store, data_table_entry);
  if (!bundle.has_value() || !qualified_layout(*bundle)) return std::nullopt;
  RetailMissionWorldBundle result;
  result.mission_id_ = data_table_entry - kFirstWorldEntry + 1u;
  result.bundle_ = std::move(*bundle);
  return result;
}

std::optional<RetailFhmView> RetailMissionWorldBundle::map() const noexcept {
  if (!bundle_.has_value()) return std::nullopt;
  const std::optional<std::span<const std::uint8_t>> bytes = bundle_->child(21);
  return bytes.has_value() ? RetailFhmView::open(*bytes) : std::nullopt;
}

std::optional<RetailFhmView> RetailMissionWorldBundle::mapset() const noexcept {
  if (!bundle_.has_value()) return std::nullopt;
  const std::optional<std::span<const std::uint8_t>> bytes = bundle_->child(22);
  return bytes.has_value() ? RetailFhmView::open(*bytes) : std::nullopt;
}

std::optional<std::span<const std::uint8_t>>
RetailMissionWorldBundle::map_resource(std::uint32_t index) const noexcept {
  const std::optional<RetailFhmView> view = map();
  return view.has_value() ? view->child(index) : std::nullopt;
}

std::optional<std::span<const std::uint8_t>>
RetailMissionWorldBundle::mapset_resource(std::uint32_t index) const noexcept {
  const std::optional<RetailFhmView> view = mapset();
  return view.has_value() ? view->child(index) : std::nullopt;
}

}  // namespace ac6::retail
