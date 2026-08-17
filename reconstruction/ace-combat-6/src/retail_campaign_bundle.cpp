#include "ac6/retail_campaign_bundle.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace ac6::retail {
namespace {

constexpr std::array<std::uint8_t, 4> kFhmMagic{'F', 'H', 'M', ' '};
constexpr std::uint32_t kMaximumRootChildren = 4096;

bool add_within(std::uint64_t left, std::uint64_t right,
                std::uint64_t limit) noexcept {
  return left <= limit && right <= limit - left;
}

bool is_campaign_entry(std::uint32_t data_table_entry) noexcept {
  return std::find(kPalCampaignDataTableEntries.begin(),
                   kPalCampaignDataTableEntries.end(),
                   data_table_entry) != kPalCampaignDataTableEntries.end();
}

}  // namespace

std::optional<RetailCampaignBundle> RetailCampaignBundle::open(
    const RetailContentStore& store, std::uint32_t mission_id) {
  if (!store.valid() || mission_id == 0 ||
      mission_id > kPalCampaignDataTableEntries.size()) {
    return std::nullopt;
  }
  std::optional<RetailCampaignBundle> bundle =
      open_entry(store, kPalCampaignDataTableEntries[mission_id - 1]);
  if (bundle.has_value()) bundle->mission_id_ = mission_id;
  return bundle;
}

std::optional<RetailCampaignBundle> RetailCampaignBundle::open_entry(
    const RetailContentStore& store, std::uint32_t data_table_entry) {
  if (!store.valid() || store.find(data_table_entry) == nullptr) {
    return std::nullopt;
  }
  RetailCampaignBundle bundle;
  bundle.data_table_entry_ = data_table_entry;
  bundle.content_index_sha256_ = store.index_sha256();
  if (!store.read_payload(bundle.data_table_entry_, bundle.bytes_) ||
      bundle.bytes_.size() < 8 ||
      !std::equal(kFhmMagic.begin(), kFhmMagic.end(), bundle.bytes_.begin()) ||
      !parse_container_index(bundle.root_, bundle.bytes_.data(),
                             bundle.bytes_.size(), 0) ||
      bundle.root_.version != 1 ||
      bundle.root_.endian != kNativeEndianFlag || bundle.root_.count == 0 ||
      bundle.root_.count > kMaximumRootChildren) {
    return std::nullopt;
  }

  // 0x82234C18 lays out four parallel u32 arrays after the count. Validate the
  // complete table with 64-bit arithmetic before asking 0x82234DD0's port for
  // any child. The retail parser trusts these extents; the native cache reader
  // treats them as untrusted input and fails closed.
  const std::uint64_t table_begin =
      static_cast<std::uint64_t>(bundle.root_.header_size) + 4u;
  const std::uint64_t table_size =
      static_cast<std::uint64_t>(bundle.root_.count) * 4u * sizeof(std::uint32_t);
  if (!add_within(table_begin, table_size, bundle.bytes_.size())) {
    return std::nullopt;
  }
  for (std::uint32_t index = 0; index < bundle.root_.count; ++index) {
    const std::uint32_t length = container_entry_length(
        bundle.root_, bundle.bytes_.data(), bundle.bytes_.size(), index);
    if (length == 0) continue;
    const std::uint32_t offset = container_entry(
        bundle.root_, bundle.bytes_.data(), bundle.bytes_.size(), index);
    if (offset == 0 || !add_within(offset, length, bundle.bytes_.size())) {
      return std::nullopt;
    }
  }
  if (!bundle.child(0).has_value()) return std::nullopt;
  if (is_campaign_entry(data_table_entry)) {
    std::optional<RetailSceneTcamCatalog> catalog =
        RetailSceneTcamCatalog::scan(bundle.bytes_);
    if (!catalog.has_value()) return std::nullopt;
    bundle.scene_tcams_ = std::move(*catalog);
  }
  return bundle;
}

std::optional<std::span<const std::uint8_t>> RetailCampaignBundle::child(
    std::uint32_t index) const noexcept {
  if (index >= root_.count) return std::nullopt;
  const std::uint32_t length =
      container_entry_length(root_, bytes_.data(), bytes_.size(), index);
  const std::uint32_t offset =
      container_entry(root_, bytes_.data(), bytes_.size(), index);
  if (offset == 0 || length == 0 || offset > bytes_.size() ||
      length > bytes_.size() - offset) {
    return std::nullopt;
  }
  return std::span<const std::uint8_t>(bytes_).subspan(offset, length);
}

std::optional<std::span<const std::uint8_t>>
RetailCampaignBundle::tcam_bytes(std::size_t index) const noexcept {
  const RetailSceneTcamResource* resource = scene_tcams_.resource(index);
  if (resource == nullptr || resource->payload_offset > bytes_.size() ||
      resource->size > bytes_.size() - resource->payload_offset) {
    return std::nullopt;
  }
  return std::span<const std::uint8_t>(bytes_).subspan(
      resource->payload_offset, resource->size);
}

std::optional<std::span<const std::uint8_t>>
RetailCampaignBundle::nfic_cut_bytes(std::size_t index) const noexcept {
  const RetailSceneTcamResource* resource = scene_tcams_.resource(index);
  if (resource == nullptr || resource->nfic_payload_offset > bytes_.size() ||
      resource->nfic_size > bytes_.size() - resource->nfic_payload_offset) {
    return std::nullopt;
  }
  return std::span<const std::uint8_t>(bytes_).subspan(
      resource->nfic_payload_offset, resource->nfic_size);
}

}  // namespace ac6::retail
