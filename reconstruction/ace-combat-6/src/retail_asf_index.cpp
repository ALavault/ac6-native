#include "ac6/retail_asf_index.h"

#include <algorithm>
#include <array>
#include <limits>

namespace ac6 {
namespace {

constexpr std::array<std::uint8_t, 16> kAsfHeaderGuid{
    0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
    0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
constexpr std::array<std::uint8_t, 16> kAsfFilePropertiesGuid{
    0xa1, 0xdc, 0xab, 0x8c, 0x47, 0xa9, 0xcf, 0x11,
    0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
constexpr std::array<std::uint8_t, 16> kAsfHeaderExtensionGuid{
    0xb5, 0x03, 0xbf, 0x5f, 0x2e, 0xa9, 0xcf, 0x11,
    0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
constexpr std::size_t kCustomPrefixSize = 14;
constexpr std::size_t kFirstObjectOffset = 16 + kCustomPrefixSize;
constexpr std::size_t kObjectHeaderSize = 24;
constexpr std::size_t kProbeSize = 64u * 1024u;
constexpr std::size_t kScanChunkSize = 1024u * 1024u;
constexpr std::uint32_t kMaxIndexEntries = 200000;

std::uint32_t read_u32(const std::uint8_t* bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8u) |
         (static_cast<std::uint32_t>(bytes[2]) << 16u) |
         (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

std::uint64_t read_u64(const std::uint8_t* bytes) {
  return static_cast<std::uint64_t>(read_u32(bytes)) |
         (static_cast<std::uint64_t>(read_u32(bytes + 4)) << 32u);
}

bool has_guid(std::span<const std::uint8_t> bytes, std::size_t offset,
              const std::array<std::uint8_t, 16>& guid) {
  return offset <= bytes.size() && guid.size() <= bytes.size() - offset &&
         std::equal(guid.begin(), guid.end(), bytes.begin() + offset);
}

std::optional<RetailAsfBank> parse_bank(std::span<const std::uint8_t> bytes,
                                        std::uint64_t offset,
                                        std::uint64_t size,
                                        std::string& detail) {
  if (size < kFirstObjectOffset + 2 * kObjectHeaderSize ||
      bytes.size() < kFirstObjectOffset + 2 * kObjectHeaderSize ||
      !has_guid(bytes, 0, kAsfHeaderGuid)) {
    detail = "ASF bank header or probe is truncated";
    return std::nullopt;
  }
  if (bytes[20] != 'B' || bytes[21] != 'N' || bytes[22] != 'K') {
    detail = "ASF bank prefix is not BNK";
    return std::nullopt;
  }
  const std::uint32_t prefix_word = read_u32(bytes.data() + 16);
  const std::uint8_t bank_tag = bytes[23];
  const std::uint32_t stream_hint = read_u32(bytes.data() + 24);
  constexpr std::size_t file_properties_offset = kFirstObjectOffset;
  if (!has_guid(bytes, file_properties_offset, kAsfFilePropertiesGuid)) {
    detail = "ASF File Properties object is missing";
    return std::nullopt;
  }
  const std::uint64_t file_properties_size =
      read_u64(bytes.data() + file_properties_offset + 16);
  if (file_properties_size < kObjectHeaderSize ||
      file_properties_size > size ||
      file_properties_size > bytes.size() - file_properties_offset) {
    detail = "ASF File Properties object exceeds the bank";
    return std::nullopt;
  }
  const std::size_t extension_offset =
      file_properties_offset + static_cast<std::size_t>(file_properties_size);
  if (!has_guid(bytes, extension_offset, kAsfHeaderExtensionGuid)) {
    detail = "ASF Header Extension object is missing";
    return std::nullopt;
  }
  if (extension_offset + 24 > bytes.size()) {
    detail = "ASF Header Extension header is truncated";
    return std::nullopt;
  }
  const std::uint64_t extension_size = read_u64(bytes.data() + extension_offset + 16);
  if (extension_size < kObjectHeaderSize || extension_size > size - extension_offset ||
      extension_size > bytes.size() - extension_offset) {
    detail = "ASF Header Extension object exceeds the bank";
    return std::nullopt;
  }
  const std::size_t index_base =
      extension_offset + static_cast<std::size_t>(extension_size);
  if (index_base + 8 > bytes.size() || index_base >= size) {
    detail = "ASF offset table is missing";
    return std::nullopt;
  }

  std::size_t index_offset = 0;
  std::uint32_t index_count = 0;
  std::uint32_t first_index = 0;
  std::uint32_t last_index = 0;
  for (std::size_t alignment = 0; alignment != 4 && index_count == 0; ++alignment) {
    const std::size_t candidate = index_base + alignment;
    if (candidate + 8 > bytes.size() || candidate >= size) continue;
    std::uint32_t candidate_count = 0;
    std::uint32_t candidate_first = 0;
    std::uint32_t candidate_last = 0;
    std::uint32_t previous = 0;
    bool valid = true;
    for (std::size_t cursor = candidate;
         cursor + 4 <= bytes.size() && cursor < size &&
         candidate_count < kMaxIndexEntries; cursor += 4) {
      const std::uint32_t value = read_u32(bytes.data() + cursor);
      if (candidate_count != 0 && value <= previous) break;
      if (static_cast<std::uint64_t>(value) >= size) {
        valid = false;
        break;
      }
      if (candidate_count == 0) candidate_first = value;
      previous = value;
      candidate_last = value;
      ++candidate_count;
    }
    const std::size_t candidate_trailer = candidate + candidate_count * 4u;
    if (!valid || candidate_count < 2 || candidate_count == kMaxIndexEntries ||
        candidate_trailer + 8 > bytes.size() || candidate_trailer + 8 > size) {
      continue;
    }
    index_offset = candidate;
    index_count = candidate_count;
    first_index = candidate_first;
    last_index = candidate_last;
  }
  if (index_count == 0) {
    detail = "ASF offset table has no bounded monotone termination";
    return std::nullopt;
  }
  const std::size_t trailer_offset = index_offset + index_count * 4u;
  if (trailer_offset + 8 > bytes.size() || trailer_offset + 8 > size) {
    detail = "ASF offset table trailer is truncated";
    return std::nullopt;
  }
  RetailAsfBank bank;
  bank.offset = offset;
  bank.size = size;
  bank.prefix_word = prefix_word;
  bank.bank_tag = bank_tag;
  bank.stream_hint = stream_hint;
  bank.file_properties_size = file_properties_size;
  bank.header_extension_size = extension_size;
  bank.index_offset = index_offset;
  bank.index_count = index_count;
  bank.first_index = first_index;
  bank.last_index = last_index;
  bank.trailer_word0 = read_u32(bytes.data() + trailer_offset);
  bank.trailer_word1 = read_u32(bytes.data() + trailer_offset + 4);
  return bank;
}

std::vector<std::uint64_t> find_headers(std::span<const std::uint8_t> bytes) {
  std::vector<std::uint64_t> offsets;
  for (std::size_t cursor = 0; cursor + kAsfHeaderGuid.size() <= bytes.size(); ++cursor) {
    if (has_guid(bytes, cursor, kAsfHeaderGuid)) offsets.push_back(cursor);
  }
  return offsets;
}

std::optional<std::vector<RetailAsfBank>> parse_banks(std::span<const std::uint8_t> bytes,
                                                      std::string& detail) {
  const auto starts = find_headers(bytes);
  if (starts.empty()) {
    detail = "ASF header GUID is missing";
    return std::nullopt;
  }
  std::vector<RetailAsfBank> banks;
  for (std::size_t i = 0; i < starts.size(); ++i) {
    const std::uint64_t end = i + 1 < starts.size() ? starts[i + 1] : bytes.size();
    const auto bank = parse_bank(bytes.subspan(static_cast<std::size_t>(starts[i]),
                                                static_cast<std::size_t>(end - starts[i])),
                                 starts[i], end - starts[i], detail);
    if (!bank.has_value()) return std::nullopt;
    banks.push_back(*bank);
  }
  return banks;
}

}  // namespace

std::optional<RetailAsfIndex> RetailAsfIndex::open(
    std::span<const std::uint8_t> bytes, std::string& detail) {
  detail.clear();
  const auto banks = parse_banks(bytes, detail);
  if (!banks.has_value()) return std::nullopt;
  RetailAsfIndex result;
  result.banks_ = *banks;
  return result;
}

std::optional<RetailAsfIndex> RetailAsfIndex::open(const RetailMediaStore& store,
                                                   RetailMediaAsset asset,
                                                   std::string& detail) {
  detail.clear();
  if (!store.valid() || store.size(asset) == 0) {
    detail = "media store is not open";
    return std::nullopt;
  }
  const std::uint64_t total = store.size(asset);
  std::vector<std::uint64_t> starts;
  std::vector<std::uint8_t> chunk;
  for (std::uint64_t offset = 0; offset < total;) {
    const std::uint64_t length =
        std::min<std::uint64_t>(total - offset, kScanChunkSize + kAsfHeaderGuid.size() - 1);
    if (!store.read_range(asset, offset, length, chunk)) {
      detail = "ASF header scan range cannot be read";
      return std::nullopt;
    }
    for (std::size_t cursor = 0;
         cursor + kAsfHeaderGuid.size() <= chunk.size(); ++cursor) {
      if (has_guid(chunk, cursor, kAsfHeaderGuid)) {
        const std::uint64_t found = offset + cursor;
        if (starts.empty() || starts.back() != found) starts.push_back(found);
      }
    }
    if (offset + length >= total) break;
    offset += kScanChunkSize;
  }
  if (starts.empty()) {
    detail = "ASF header GUID is missing";
    return std::nullopt;
  }
  RetailAsfIndex result;
  constexpr std::uint64_t kProbe = kProbeSize;
  for (std::size_t i = 0; i < starts.size(); ++i) {
    const std::uint64_t end = i + 1 < starts.size() ? starts[i + 1] : total;
    if (end <= starts[i]) {
      detail = "ASF bank ranges are not ordered";
      return std::nullopt;
    }
    const std::uint64_t length = std::min<std::uint64_t>(end - starts[i], kProbe);
    if (!store.read_range(asset, starts[i], length, chunk)) {
      detail = "ASF bank probe cannot be read";
      return std::nullopt;
    }
    const auto bank = parse_bank(chunk, starts[i], end - starts[i], detail);
    if (!bank.has_value()) return std::nullopt;
    result.banks_.push_back(*bank);
  }
  return result;
}

}  // namespace ac6
