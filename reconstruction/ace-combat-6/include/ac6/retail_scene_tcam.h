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

struct RetailTcamCameraSample final {
  std::uint16_t frame{};
  std::array<float, 3> position{};
  std::array<float, 4> orientation{};
  float vertical_fov_radians{};
};

// The first three GYZ records of the qualified M01 Tcam resource are a
// bounded camera track: position keys, angular-orientation keys, and one
// vertical-FOV scalar. The orientation values remain serialized angular
// values; this type deliberately does not assign a retail Euler order.
class RetailTcamCameraTrack final {
 public:
  static std::optional<RetailTcamCameraTrack> open(
      const RetailTcamMopView& view) noexcept;

  std::size_t sample_count() const noexcept { return times_.size(); }
  std::uint16_t first_frame() const noexcept { return times_.front(); }
  std::uint16_t last_frame() const noexcept { return times_.back(); }
  std::optional<RetailTcamCameraSample> sample(
      std::uint16_t frame) const noexcept;

 private:
  std::vector<std::uint16_t> times_;
  std::vector<std::array<float, 4>> positions_;
  std::vector<std::array<float, 4>> orientations_;
  float vertical_fov_radians_{};
};

struct RetailNficEventView final {
  std::uint16_t tag{};
  std::span<const std::uint8_t> payload{};
};

struct RetailNficCameraCommand final {
  std::uint16_t scene_object_id{};
  std::uint16_t tcam_frame{};
};

// Bounded reader for the qualified NFIC CUT chunk/event representation. It
// exposes serialized control events and the first camera join only; it does
// not claim that archive order activates a mission scene at runtime.
class RetailNficCutView final {
 public:
  static std::optional<RetailNficCutView> open(
      std::span<const std::uint8_t> bytes) noexcept;

  std::size_t chunk_count() const noexcept { return chunk_count_; }
  std::size_t event_count() const noexcept { return event_offsets_.size(); }
  std::optional<std::span<const std::uint8_t>> chunk_payload(
      std::uint16_t tag) const noexcept;
  std::optional<RetailNficEventView> event(
      std::size_t index) const noexcept;
  bool has_dictionary_symbol(std::uint16_t tag,
                             std::string_view name) const noexcept;
  bool has_terminal_event() const noexcept { return has_terminal_event_; }
  std::optional<RetailNficCameraCommand> initial_camera_command() const noexcept;

 private:
  struct Chunk final {
    std::uint16_t tag{};
    std::uint32_t payload_offset{};
    std::uint32_t payload_size{};
  };

  std::span<const std::uint8_t> bytes_;
  std::array<Chunk, 16> chunks_{};
  std::size_t chunk_count_{};
  std::vector<std::uint32_t> event_offsets_;
  bool has_terminal_event_{};
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
  std::size_t nfic_payload_offset{};
  std::size_t nfic_size{};
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
