#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ac6/sha256.h"

namespace ac6::retail {

inline constexpr std::size_t kRetailScenePathRecordBytes = 0x80;
inline constexpr std::size_t kRetailTcamRecordCount = 3;
inline constexpr std::size_t kRetailTcamRecordBytes = 0x30;

// A bounded, non-owning view of the proved wrapper/GYZ shape used by retail
// Tcam*.mop resources. The three records deliberately remain opaque; only
// their fixed extents and the two proved, GYZ-relative data offsets are
// exposed. The source bytes must outlive this view and its returned spans.
class RetailTcamMopView final {
 public:
  static std::optional<RetailTcamMopView> open(
      std::span<const std::uint8_t> bytes) noexcept;

  std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }
  std::uint32_t gyz_offset() const noexcept { return gyz_offset_; }
  std::span<const std::uint8_t> gyz_bytes() const noexcept {
    return bytes_.subspan(gyz_offset_, gyz_size_);
  }
  std::optional<std::span<const std::uint8_t>> record_bytes(
      std::size_t index) const noexcept;
  std::optional<std::array<std::uint32_t, 2>> record_data_offsets(
      std::size_t index) const noexcept;

 private:
  std::span<const std::uint8_t> bytes_;
  std::uint32_t gyz_offset_{};
  std::uint32_t gyz_size_{};
  std::uint32_t record_table_{};
};

// Owned metadata for one Tcam path/resource join. fhm_path is the exact child
// index route from the campaign payload root through the resource FHM to the
// Tcam bytes. payload_offset is retained instead of a span so this record and
// its owning bundle remain valid after copies and moves.
struct RetailSceneTcamResource final {
  std::string path;
  std::vector<std::uint32_t> fhm_path;
  std::size_t payload_offset{};
  std::size_t size{};
  Sha256Digest sha256{};
};

// Structural census of the Scene triplets in one campaign payload. A scan is
// all-or-nothing. Exact Tcam paths are unique; a duplicate rejects the whole
// payload so find_exact always has one unambiguous answer.
class RetailSceneTcamCatalog final {
 public:
  RetailSceneTcamCatalog() = default;

  static std::optional<RetailSceneTcamCatalog> scan(
      std::span<const std::uint8_t> payload) noexcept;

  std::size_t scene_table_count() const noexcept { return scene_tables_; }
  std::size_t scene_path_count() const noexcept { return scene_paths_; }
  std::size_t size() const noexcept { return resources_.size(); }
  bool empty() const noexcept { return resources_.empty(); }
  std::span<const RetailSceneTcamResource> resources() const noexcept {
    return resources_;
  }
  const RetailSceneTcamResource* resource(std::size_t index) const noexcept;
  std::optional<std::size_t> find_exact(std::string_view path) const noexcept;

 private:
  friend class RetailSceneTcamScanner;

  std::size_t scene_tables_{};
  std::size_t scene_paths_{};
  std::vector<RetailSceneTcamResource> resources_;
};

}  // namespace ac6::retail
