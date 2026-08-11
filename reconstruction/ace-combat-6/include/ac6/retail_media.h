#pragma once

#include "ac6/sha256.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ac6 {

enum class RetailMediaAsset : std::uint8_t {
  Bgm = 0,
  DemoEnglish = 1,
  DemoJapanese = 2,
  Movie = 3,
  VoiceEnglish = 4,
  VoiceJapanese = 5,
};

inline constexpr std::size_t kRetailMediaAssetCount = 6;

struct RetailMediaAssetIdentity final {
  const char* filename{};
  const char* container{};
  std::uint64_t size{};
  Sha256Digest sha256{};
};

struct RetailMediaPolicy final {
  std::array<RetailMediaAssetIdentity, kRetailMediaAssetCount> assets{};
  bool required{};

  static RetailMediaPolicy pal();
};

struct RetailMediaRecord final {
  std::uint64_t size{};
  Sha256Digest sha256{};
  const char* container{};
  bool operator==(const RetailMediaRecord&) const = default;
};

class RetailMediaStore final {
 public:
  RetailMediaStore() = default;
  bool open(const std::filesystem::path& cache_root,
            const RetailMediaPolicy& policy = RetailMediaPolicy::pal());
  bool valid() const noexcept { return valid_; }
  const char* detail() const noexcept { return detail_; }
  const std::array<RetailMediaRecord, kRetailMediaAssetCount>& records() const noexcept {
    return records_;
  }
  bool read_range(RetailMediaAsset asset, std::uint64_t offset,
                  std::uint64_t size, std::vector<std::uint8_t>& output) const;
  std::filesystem::path compressed_path(RetailMediaAsset asset) const;
  std::uint64_t size(RetailMediaAsset asset) const noexcept;
  const Sha256Digest& manifest_sha256() const noexcept { return manifest_sha256_; }

 private:
  std::filesystem::path cache_root_;
  std::array<RetailMediaRecord, kRetailMediaAssetCount> records_{};
  Sha256Digest manifest_sha256_{};
  const char* detail_{"media store is not open"};
  bool valid_{};
};

struct RetailDecodedAudio final {
  std::uint32_t sample_rate{};
  std::uint32_t channels{};
  std::vector<float> pcm;
  Sha256Digest pcm_sha256{};
  std::string decoder_version;
};

class RetailMediaDecoder final {
 public:
  static bool decode_audio(const RetailMediaStore& store, RetailMediaAsset asset,
                           RetailDecodedAudio& output, std::string& detail);
};

// Copies the compressed media assets into a staged, content-addressed cache
// and atomically publishes its manifest/current pair. The caller publishes
// the content index only after this function succeeds, so an interrupted
// import cannot make an incomplete media generation current.
bool import_retail_media(const std::filesystem::path& source_root,
                         const std::filesystem::path& cache_root,
                         const std::filesystem::path& staging_root,
                         const RetailMediaPolicy& policy,
                         Sha256Digest& manifest_sha256,
                         std::string& detail);

}  // namespace ac6
