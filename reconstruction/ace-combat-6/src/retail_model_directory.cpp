#include "ac6/retail_model_directory.h"

#include <cstring>

namespace ac6::retail {
namespace {

// The three header words 0x8228E988 and 0x8228E9A8 read.
constexpr std::size_t kCountOffset = 0x04;
constexpr std::size_t kTableOffset = 0x0C;
constexpr std::size_t kBaseOffset = 0x10;
constexpr std::size_t kHeaderWords = 0x14;

// A directory with more entries than this is not one; the bound exists so a
// malformed count cannot make the table computation overflow.
constexpr std::uint32_t kMaxEntries = 1u << 20;

std::uint32_t read_u32(const std::uint8_t* p) noexcept {
  return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
         static_cast<std::uint32_t>(p[2]) << 8 | static_cast<std::uint32_t>(p[3]);
}

}  // namespace

std::optional<ModelDirectory> ModelDirectory::open(const std::uint8_t* bytes,
                                                   std::size_t size) noexcept {
  if (bytes == nullptr || size < kHeaderWords) return std::nullopt;

  ModelDirectory directory;
  directory.bytes_ = bytes;
  directory.size_ = size;
  directory.count_ = read_u32(bytes + kCountOffset);
  if (directory.count_ == 0 || directory.count_ > kMaxEntries) return std::nullopt;

  directory.table_ = read_u32(bytes + kTableOffset);
  directory.base_ = read_u32(bytes + kBaseOffset);
  // Retail adds these to the blob and indexes; this refuses first, because the
  // blob is untrusted here and is not there.
  if (directory.table_ > size || directory.base_ > size) return std::nullopt;
  const std::size_t table_bytes = static_cast<std::size_t>(directory.count_) * 4;
  if (size - directory.table_ < table_bytes) return std::nullopt;
  return directory;
}

std::optional<ModelDirectoryEntry> ModelDirectory::entry(std::uint32_t index) const noexcept {
  // 0x8228E9C0: the bound check, and the path a 0xFF model byte takes.
  if (index >= count_) return std::nullopt;
  const std::size_t offset =
      base_ + read_u32(bytes_ + table_ + static_cast<std::size_t>(index) * 4);
  if (offset >= size_) return std::nullopt;

  // The extent, which retail does not compute: to the next entry's start, or to
  // the end of the blob for the last. Entries are monotonic in every directory
  // read so far; a directory where they are not yields a zero size rather than
  // a negative one.
  std::size_t next = size_;
  if (index + 1 < count_) {
    const std::size_t candidate =
        base_ + read_u32(bytes_ + table_ + (static_cast<std::size_t>(index) + 1) * 4);
    if (candidate <= size_) next = candidate;
  }
  ModelDirectoryEntry result;
  result.index = index;
  result.offset = offset;
  result.size = next > offset ? next - offset : 0;
  return result;
}

bool ModelDirectory::every_entry_starts_with(const char signature[4]) const noexcept {
  for (std::uint32_t index = 0; index < count_; ++index) {
    const std::optional<ModelDirectoryEntry> row = entry(index);
    if (!row.has_value() || row->offset > size_ || size_ - row->offset < 4) return false;
    if (std::memcmp(bytes_ + row->offset, signature, 4) != 0) return false;
  }
  return true;
}

}  // namespace ac6::retail
