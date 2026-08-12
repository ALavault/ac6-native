#pragma once

#include "ac6/sha256.h"
#include "ac6/retail_media.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ac6 {

class RetailResourceGraph;

inline constexpr std::array<std::uint32_t, 15> kPalCampaignDataTableEntries{
    9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};

// DATA.TBL entries 2..8 are the PAL frontend font/glyph packs.  They are
// optional for the scenario-only diagnostic lane but required by the product
// frontend; keeping the selection separate lets old bounded tests continue to
// exercise a one-payload importer without silently manufacturing UI text.
inline constexpr std::array<std::uint32_t, 7> kPalFrontendFontDataTableEntries{
    2, 3, 4, 5, 6, 7, 8};
// The runtime language order is English, French, German, Italian, Spanish.
// DATA.TBL entries 2 and 3 remain common/font resources rather than locale
// slots; the PAL selector at 0x821BAB70 -> 0x821BB118 maps the five locales
// in the non-numeric order below.
inline constexpr std::array<std::uint32_t, 5> kPalFrontendLocaleDataTableEntries{
    4, 7, 5, 6, 8};

// Entry 1 contains the common retail camera tables. Entry 119 is the qualified
// Mission 01 world/map resource: terrain, placement, map parts, textures and
// mapset. Both are selected outside the campaign payload.
inline constexpr std::array<std::uint32_t, 17>
    kPalRequiredDataTableEntries{
        1, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 119};

enum class RetailArchive : std::uint8_t { Data00 = 0, Data01 = 1 };
enum class RetailStorageCodec : std::uint8_t {
  Mode1PiXorRawDeflate = 1,
  Mode1PiXorRaw = 2,
};

struct RetailSourceIdentity final {
  Sha256Digest xex_sha256{};
  Sha256Digest data_table_sha256{};
  Sha256Digest data00_sha256{};
  Sha256Digest data01_sha256{};
  std::uint64_t xex_size{};
  std::uint64_t data_table_size{};
  std::uint64_t data00_size{};
  std::uint64_t data01_size{};
  bool operator==(const RetailSourceIdentity&) const = default;
};

struct RetailIdentityPolicy final {
  RetailSourceIdentity identity{};
  RetailMediaPolicy media{};
  std::uint32_t data_table_entries{};
  std::uint32_t pack_count{};

  static RetailIdentityPolicy pal();
};

struct RetailImportLimits final {
  std::uint32_t maximum_table_entries{4096};
  std::uint64_t maximum_table_size{16u * 1024u * 1024u};
  std::uint64_t maximum_stored_size{256u * 1024u * 1024u};
  std::uint64_t maximum_expanded_size{512u * 1024u * 1024u};
  // The complete PAL table expands to more than 5 GiB across independent
  // payloads. Each payload remains bounded separately; this cap bounds the
  // generation rather than silently truncating the offline closure.
  std::uint64_t maximum_total_expanded_size{8ull * 1024ull * 1024ull * 1024ull};
};

struct RetailContentRecord final {
  std::uint32_t data_table_index{};
  std::uint32_t group{};
  RetailArchive archive{};
  RetailStorageCodec codec{};
  std::uint64_t source_offset{};
  std::uint64_t stored_size{};
  std::uint64_t expanded_size{};
  std::uint64_t payload_size{};
  Sha256Digest stored_sha256{};
  Sha256Digest payload_sha256{};
  bool operator==(const RetailContentRecord&) const = default;
};

enum class RetailContentError : std::uint8_t {
  None,
  InvalidArgument,
  SourceMissing,
  SourceIdentityMismatch,
  DataTableInvalid,
  EntryDuplicate,
  EntryOutOfRange,
  SourceRangeInvalid,
  SizeLimitExceeded,
  DecodeFailed,
  CacheIoFailed,
  CacheIncompatible,
  CacheIncomplete,
  CacheDigestMismatch,
};

const char* retail_content_error_name(RetailContentError error) noexcept;

struct RetailImportReport final {
  RetailContentError error{RetailContentError::None};
  std::string detail;
  Sha256Digest index_sha256{};
  Sha256Digest graph_manifest_sha256{};
  std::size_t imported_records{};
  std::uint64_t imported_bytes{};
  bool passed() const noexcept { return error == RetailContentError::None; }
};

class RetailContentImporter final {
 public:
  explicit RetailContentImporter(
      RetailIdentityPolicy policy = RetailIdentityPolicy::pal(),
      RetailImportLimits limits = {});

  RetailImportReport run(const std::filesystem::path& source_root,
                         const std::filesystem::path& cache_root,
                         std::span<const std::uint32_t> data_table_entries =
                             std::span<const std::uint32_t>{}) const;

 private:
  RetailIdentityPolicy policy_;
  RetailImportLimits limits_;
};

class RetailContentStore final {
 public:
  explicit RetailContentStore(
      RetailIdentityPolicy policy = RetailIdentityPolicy::pal(),
      RetailImportLimits limits = {});
  ~RetailContentStore();

  bool open(const std::filesystem::path& cache_root);
  void close() noexcept;
  bool valid() const noexcept { return error_ == RetailContentError::None && !records_.empty(); }
  RetailContentError error() const noexcept { return error_; }
  const std::string& detail() const noexcept { return detail_; }
  const RetailSourceIdentity& identity() const noexcept { return identity_; }
  const Sha256Digest& index_sha256() const noexcept { return index_sha256_; }
  const RetailMediaStore& media() const noexcept { return media_; }
  const RetailResourceGraph* resource_graph() const noexcept {
    return graph_.get();
  }
  const std::vector<RetailContentRecord>& records() const noexcept { return records_; }
  const RetailContentRecord* find(std::uint32_t data_table_index) const noexcept;
  bool read_payload(std::uint32_t data_table_index,
                    std::vector<std::uint8_t>& payload) const;

 private:
  bool fail(RetailContentError error, std::string detail);

  RetailIdentityPolicy policy_;
  RetailImportLimits limits_;
  std::filesystem::path cache_root_;
  RetailSourceIdentity identity_{};
  Sha256Digest index_sha256_{};
  RetailMediaStore media_{};
  std::unique_ptr<RetailResourceGraph> graph_;
  std::vector<RetailContentRecord> records_;
  RetailContentError error_{RetailContentError::CacheIncomplete};
  std::string detail_;
};

std::filesystem::path default_retail_cache_root();

// The transform is symmetric. It is public so the codec can be checked with
// independent fixtures without exposing a second storage format.
void retail_mode1_xor(std::span<std::uint8_t> bytes,
                      std::uint32_t data_table_index) noexcept;

}  // namespace ac6
