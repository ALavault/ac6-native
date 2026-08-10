#pragma once

#include "ac6/campaign_progression.h"
#include "ac6/product_runtime.h"
#include "ac6/sha256.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ac6::retail {

// A replay recorded against the sealed retail cache.  The cache identity is
// part of the file, rather than a sidecar selected by the caller, so replay
// cannot silently run against another import or another loadout.
struct RetailSessionReplay final {
  static constexpr std::uint32_t kCurrentVersion = 1;
  static constexpr std::size_t kMaximumFrames = 1'000'000;

  std::uint32_t version{kCurrentVersion};
  std::uint32_t mission_id{};
  CampaignLoadout loadout{};
  Sha256Digest content_index_sha256{};
  std::vector<InputFrame> frames;

  bool valid() const noexcept;
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);
};

}  // namespace ac6::retail
