#include "ac6/retail_media.h"

#include "ac6/sha256.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>

namespace ac6 {
namespace {

constexpr std::array<std::uint8_t, 8> kManifestMagic{'A','C','6','M','E','D','I','A'};
constexpr std::uint32_t kManifestVersion = 2;
constexpr std::array<std::uint8_t, 8> kCurrentMagic{'A','C','6','M','C','U','R',0};
constexpr std::uint32_t kCurrentSize = 48;
constexpr std::uint32_t kManifestSize = 20 + kRetailMediaAssetCount * 40;

struct MediaDescriptor final {
  const char* filename;
  const char* container;
  std::uint64_t size;
  const char* digest;
};

constexpr std::array<MediaDescriptor, kRetailMediaAssetCount> kPalDescriptors{{
    {"bgmpack.bin", "RIFF/XMA", 724762624ull,
     "78db61397696a5c98decc83052a1c36db8f816405b49c9910b689f8ad52c86fa"},
    {"demopack_eng.bin", "RIFF/XMA", 234217472ull,
     "31aae3a752b01553f42e63d6654ba0e45867e6962360a2fa5dc7cf3c4392c589"},
    {"demopack_jpn.bin", "RIFF/XMA", 234455040ull,
     "70e8159859662cedc98dcc44740b2e21fd93004a861abda77dcb52f36d410501"},
    {"moviepack.bin", "ASF", 348307456ull,
     "40c28c384beba5cf37eb47d70bcfe99703160278df5670a2f6f56d52b00e6a5a"},
    {"voicepack_eng.bin", "RIFF/XMA", 279078912ull,
     "3e1c358714617337e30aef9ebae0b5bf43a7b84342f9d31093b8ec18296e8726"},
    {"voicepack_jpn.bin", "RIFF/XMA", 327245824ull,
     "82af039f582d741c574f62060adb1170ee087a2a0fa00fa241d18b846b03adcd"},
}};

bool read_exact(const std::filesystem::path& path, std::uint64_t offset,
                std::uint64_t size, std::vector<std::uint8_t>& output) {
  if (size > std::numeric_limits<std::size_t>::max() ||
      offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  input.seekg(static_cast<std::streamoff>(offset));
  if (!input) return false;
  output.resize(static_cast<std::size_t>(size));
  input.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(size));
  return input && static_cast<std::size_t>(input.gcount()) == output.size();
}

std::filesystem::path media_blob(const std::filesystem::path& root,
                                 const Sha256Digest& digest) {
  const std::string hex = sha256_hex(digest);
  return root / "media" / "blobs" / "sha256" / hex.substr(0, 2) / hex;
}

bool parse_manifest(const std::vector<std::uint8_t>& bytes,
                    std::array<RetailMediaRecord, kRetailMediaAssetCount>& records) {
  if (bytes.size() != kManifestSize ||
      !std::equal(kManifestMagic.begin(), kManifestMagic.end(), bytes.begin())) return false;
  auto u32 = [&bytes](std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24u) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8u) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
  };
  auto u64 = [&u32](std::size_t offset) {
    return (static_cast<std::uint64_t>(u32(offset)) << 32u) | u32(offset + 4);
  };
  if (u32(8) != kManifestVersion || u32(12) != kManifestSize ||
      u32(16) != kRetailMediaAssetCount) return false;
  std::size_t offset = 20;
  for (auto& record : records) {
    record.size = u64(offset);
    std::copy_n(bytes.begin() + offset + 8, record.sha256.size(), record.sha256.begin());
    offset += 40;
  }
  return true;
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value >> 24u));
  bytes.push_back(static_cast<std::uint8_t>(value >> 16u));
  bytes.push_back(static_cast<std::uint8_t>(value >> 8u));
  bytes.push_back(static_cast<std::uint8_t>(value));
}

void append_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  append_u32(bytes, static_cast<std::uint32_t>(value >> 32u));
  append_u32(bytes, static_cast<std::uint32_t>(value));
}

bool write_sync(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes) {
  const int descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (descriptor < 0) return false;
  std::size_t offset = 0;
  while (offset < bytes.size()) {
    const ssize_t count = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
    if (count <= 0) {
      ::close(descriptor);
      return false;
    }
    offset += static_cast<std::size_t>(count);
  }
  const bool synced = ::fsync(descriptor) == 0;
  ::close(descriptor);
  return synced;
}

bool replace_file(const std::filesystem::path& temporary,
                  const std::filesystem::path& destination) {
  std::error_code error;
  std::filesystem::rename(temporary, destination, error);
  if (!error) return true;
  if (error != std::errc::file_exists) return false;
  std::filesystem::remove(temporary, error);
  return !error;
}

bool copy_media(const std::filesystem::path& source,
                const std::filesystem::path& temporary,
                std::uint64_t expected_size) {
  std::ifstream input(source, std::ios::binary);
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!input || !output) return false;
  std::array<char, 1024 * 1024> buffer{};
  std::uint64_t total = 0;
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    if (count <= 0) continue;
    output.write(buffer.data(), count);
    total += static_cast<std::uint64_t>(count);
    if (!output || total > expected_size) return false;
  }
  output.flush();
  return input.eof() && output && total == expected_size;
}

}  // namespace

RetailMediaPolicy RetailMediaPolicy::pal() {
  RetailMediaPolicy policy;
  policy.required = true;
  for (std::size_t i = 0; i < policy.assets.size(); ++i) {
    const auto& source = kPalDescriptors[i];
    policy.assets[i].filename = source.filename;
    policy.assets[i].container = source.container;
    policy.assets[i].size = source.size;
    // Every descriptor is parsed from a complete, measured SHA-256 identity.
    (void)parse_sha256(source.digest, policy.assets[i].sha256);
  }
  return policy;
}

bool RetailMediaStore::open(const std::filesystem::path& cache_root,
                            const RetailMediaPolicy& policy) {
  valid_ = false;
  cache_root_.clear();
  detail_ = "media manifest is missing or incompatible";
  if (!policy.required) {
    valid_ = true;
    detail_ = "media policy is optional";
    return true;
  }
  std::vector<std::uint8_t> current;
  if (!read_exact(cache_root / "media/current", 0, kCurrentSize, current) ||
      !std::equal(kCurrentMagic.begin(), kCurrentMagic.end(), current.begin())) return false;
  auto u32 = [&current](std::size_t offset) {
    return (static_cast<std::uint32_t>(current[offset]) << 24u) |
           (static_cast<std::uint32_t>(current[offset + 1]) << 16u) |
           (static_cast<std::uint32_t>(current[offset + 2]) << 8u) |
           static_cast<std::uint32_t>(current[offset + 3]);
  };
  if (u32(8) != kManifestVersion || u32(12) != kCurrentSize) return false;
  std::copy_n(current.begin() + 16, manifest_sha256_.size(), manifest_sha256_.begin());
  const auto path = cache_root / "media/manifests" / (sha256_hex(manifest_sha256_) + ".ac6media");
  if (!read_exact(path, 0, kManifestSize, current) ||
      sha256_bytes(current) != manifest_sha256_ || !parse_manifest(current, records_)) return false;
  for (std::size_t i = 0; i < records_.size(); ++i) {
    if (records_[i].size != policy.assets[i].size ||
        records_[i].sha256 != policy.assets[i].sha256) {
      detail_ = "media manifest identity mismatch";
      return false;
    }
    records_[i].container = policy.assets[i].container;
    std::error_code error;
    const auto blob = media_blob(cache_root, records_[i].sha256);
    if (!std::filesystem::is_regular_file(blob, error) || error ||
        std::filesystem::file_size(blob, error) != records_[i].size || error) {
      detail_ = "media blob is missing or truncated";
      return false;
    }
    Sha256Digest blob_digest{};
    if (!sha256_file(blob, blob_digest, records_[i].size) ||
        blob_digest != records_[i].sha256) {
      detail_ = "media blob digest mismatch";
      return false;
    }
  }
  cache_root_ = cache_root;
  valid_ = true;
  detail_ = "media store is valid";
  return true;
}

bool RetailMediaStore::read_range(RetailMediaAsset asset, std::uint64_t offset,
                                  std::uint64_t size,
                                  std::vector<std::uint8_t>& output) const {
  output.clear();
  if (!valid_ || static_cast<std::size_t>(asset) >= records_.size()) return false;
  const auto& record = records_[static_cast<std::size_t>(asset)];
  if (offset > record.size || size > record.size - offset) return false;
  return read_exact(media_blob(cache_root_, record.sha256), offset, size, output);
}

bool import_retail_media(const std::filesystem::path& source_root,
                         const std::filesystem::path& cache_root,
                         const std::filesystem::path& staging_root,
                         const RetailMediaPolicy& policy,
                         Sha256Digest& manifest_sha256,
                         std::string& detail) {
  manifest_sha256 = {};
  if (!policy.required) return true;
  std::error_code error;
  std::filesystem::create_directories(cache_root / "media/blobs/sha256", error);
  if (error) { detail = "cannot create media blob directory"; return false; }
  std::array<RetailMediaRecord, kRetailMediaAssetCount> records{};
  for (std::size_t i = 0; i < policy.assets.size(); ++i) {
    const auto& asset = policy.assets[i];
    if (asset.filename == nullptr || asset.container == nullptr || asset.size == 0 ||
        asset.sha256 == Sha256Digest{}) {
      detail = "media policy contains an unresolved identity";
      return false;
    }
    const auto source = source_root / asset.filename;
    Sha256Digest actual{};
    if (!sha256_file(source, actual, asset.size) || actual != asset.sha256) {
      detail = "media source identity mismatch: " + source.filename().string();
      return false;
    }
    records[i] = {asset.size, asset.sha256, asset.container};
    const auto destination = media_blob(cache_root, asset.sha256);
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) { detail = "cannot create media blob parent"; return false; }
    if (std::filesystem::is_regular_file(destination, error) && !error &&
        std::filesystem::file_size(destination, error) == asset.size &&
        sha256_file(destination, actual, asset.size) && actual == asset.sha256) continue;
    const auto temporary = staging_root / (std::string("media-") + std::to_string(i));
    if (!copy_media(source, temporary, asset.size) ||
        !sha256_file(temporary, actual, asset.size) || actual != asset.sha256 ||
        !replace_file(temporary, destination)) {
      detail = "cannot stage media blob: " + source.filename().string();
      return false;
    }
  }
  std::vector<std::uint8_t> manifest;
  manifest.insert(manifest.end(), kManifestMagic.begin(), kManifestMagic.end());
  append_u32(manifest, kManifestVersion);
  append_u32(manifest, kManifestSize);
  append_u32(manifest, kRetailMediaAssetCount);
  for (const auto& record : records) {
    append_u64(manifest, record.size);
    manifest.insert(manifest.end(), record.sha256.begin(), record.sha256.end());
  }
  manifest_sha256 = sha256_bytes(manifest);
  std::filesystem::create_directories(cache_root / "media/manifests", error);
  if (error) { detail = "cannot create media manifest directory"; return false; }
  const auto manifest_path = cache_root / "media/manifests" /
      (sha256_hex(manifest_sha256) + ".ac6media");
  if (!std::filesystem::exists(manifest_path) &&
      (!write_sync(staging_root / "media-manifest", manifest) ||
       !replace_file(staging_root / "media-manifest", manifest_path))) {
    detail = "cannot publish media manifest";
    return false;
  }
  std::vector<std::uint8_t> current;
  current.insert(current.end(), kCurrentMagic.begin(), kCurrentMagic.end());
  append_u32(current, kManifestVersion);
  append_u32(current, kCurrentSize);
  current.insert(current.end(), manifest_sha256.begin(), manifest_sha256.end());
  if (!write_sync(staging_root / "media-current", current) ||
      !replace_file(staging_root / "media-current", cache_root / "media/current")) {
    detail = "cannot publish media current pointer";
    return false;
  }
  detail.clear();
  return true;
}

}  // namespace ac6
