#pragma once

#include "ac6/campaign_progression.h"
#include "ac6/product_runtime.h"
#include "ac6/retail_mission_bundle.h"
#include "ac6/sha256.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ac6::retail {

// A replay recorded against the sealed retail cache.  The cache identity is
// part of the file, rather than a sidecar selected by the caller, so replay
// cannot silently run against another import or another loadout.
struct RetailSessionReplay final {
  static constexpr std::uint32_t kCurrentVersion = 3;
  static constexpr std::size_t kMaximumFrames = 1'000'000;
  static constexpr std::size_t kMaximumCheckpoints = 4096;

  struct Checkpoint final {
    std::uint32_t frame_index{};
    Sha256Digest input_digest{};
    bool operator==(const Checkpoint&) const = default;
  };

  std::uint32_t version{kCurrentVersion};
  std::uint32_t mission_id{};
  CampaignLoadout loadout{};
  Sha256Digest content_index_sha256{};
  std::vector<InputFrame> frames;
  RetailDifficulty difficulty{RetailDifficulty::Normal};
  std::uint64_t random_seed{0xAC60000000000001ull};
  std::uint64_t final_tick{};
  Sha256Digest final_digest{};
  std::vector<Checkpoint> checkpoints;

  bool valid() const noexcept;
  Sha256Digest input_digest() const noexcept;
  Sha256Digest input_digest(std::size_t frame_count) const noexcept;
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);
};

}  // namespace ac6::retail
