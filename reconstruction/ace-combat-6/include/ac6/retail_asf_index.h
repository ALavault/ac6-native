#pragma once

#include "ac6/retail_media.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ac6 {

// The PAL movie pack is an ASF-header-compatible bank format.  The retail
// file prepends a small BNK prefix, keeps the standard File Properties and
// Header Extension objects, then stores a bounded little-endian offset table.
// This reader qualifies that resource/index boundary only; it does not infer
// a video codec or claim that an index entry is a decoded frame.
struct RetailAsfBank final {
  std::uint64_t offset{};
  std::uint64_t size{};
  std::uint32_t prefix_word{};
  std::uint8_t bank_tag{};
  std::uint32_t stream_hint{};
  std::uint64_t file_properties_size{};
  std::uint64_t header_extension_size{};
  std::uint64_t index_offset{};
  std::uint32_t index_count{};
  std::uint32_t first_index{};
  std::uint32_t last_index{};
  std::uint32_t trailer_word0{};
  std::uint32_t trailer_word1{};
  // Relative offsets read verbatim from the bounded monotone table. Adjacent
  // values delimit indexed compressed ranges; no packet, frame or codec
  // meaning is inferred.
  std::vector<std::uint32_t> entry_offsets;
};

struct RetailAsfEntryRange final {
  std::uint64_t offset{};  // absolute offset in the content-addressed blob
  std::uint64_t size{};
  bool operator==(const RetailAsfEntryRange&) const = default;
};

class RetailAsfIndex final {
 public:
  // Parses an already bounded byte span.  The span may contain one or more
  // concatenated banks and is used by deterministic parser tests.
  static std::optional<RetailAsfIndex> open(std::span<const std::uint8_t> bytes,
                                             std::string& detail);

  // Reads only overlapping scan/probe windows from the content-addressed
  // media store.  The compressed movie remains on disk and is never loaded as
  // one contiguous buffer.
  static std::optional<RetailAsfIndex> open(const RetailMediaStore& store,
                                             RetailMediaAsset asset,
                                             std::string& detail);

  std::size_t bank_count() const noexcept { return banks_.size(); }
  const RetailAsfBank& bank(std::size_t index) const noexcept { return banks_[index]; }
  const std::vector<RetailAsfBank>& banks() const noexcept { return banks_; }
  std::optional<RetailAsfEntryRange> entry_range(
      std::size_t bank_index, std::size_t entry_index) const noexcept;

 private:
  std::vector<RetailAsfBank> banks_;
};

}  // namespace ac6
