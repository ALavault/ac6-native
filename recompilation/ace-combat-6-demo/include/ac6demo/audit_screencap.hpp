#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ac6demo {

struct AuditPngText final {
  std::string keyword;
  std::string value;
};

struct AuditPixelStats final {
  std::uint64_t pixels{};
  std::uint64_t rgb_nonzero_pixels{};
  std::uint64_t rgba_zero_pixels{};
  std::uint64_t alpha_zero_pixels{};
  std::uint64_t alpha_full_pixels{};
  std::uint64_t alpha_partial_pixels{};
  std::array<std::uint8_t, 4> channel_min{255U, 255U, 255U, 255U};
  std::array<std::uint8_t, 4> channel_max{};

  [[nodiscard]] bool rgb_all_black() const noexcept {
    return rgb_nonzero_pixels == 0U;
  }
};

struct AuditPng final {
  std::string bytes;
  AuditPixelStats stats;
};

namespace audit_png_detail {

inline void append_u32_be(std::string &output, std::uint32_t value) {
  output.push_back(static_cast<char>((value >> 24U) & 0xFFU));
  output.push_back(static_cast<char>((value >> 16U) & 0xFFU));
  output.push_back(static_cast<char>((value >> 8U) & 0xFFU));
  output.push_back(static_cast<char>(value & 0xFFU));
}

inline void append_u16_le(std::string &output, std::uint16_t value) {
  output.push_back(static_cast<char>(value & 0xFFU));
  output.push_back(static_cast<char>((value >> 8U) & 0xFFU));
}

[[nodiscard]] inline std::uint32_t
crc32(std::span<const std::byte> bytes) noexcept {
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const auto byte : bytes) {
    crc ^= std::to_integer<std::uint8_t>(byte);
    for (unsigned bit = 0U; bit < 8U; ++bit) {
      const auto mask = static_cast<std::uint32_t>(
          -static_cast<std::int32_t>(crc & 1U));
      crc = (crc >> 1U) ^ (0xEDB88320U & mask);
    }
  }
  return ~crc;
}

[[nodiscard]] inline std::uint32_t
adler32(std::span<const std::byte> bytes) noexcept {
  constexpr std::uint32_t kModulus = 65521U;
  std::uint32_t a = 1U;
  std::uint32_t b = 0U;
  for (const auto byte : bytes) {
    a = (a + std::to_integer<std::uint8_t>(byte)) % kModulus;
    b = (b + a) % kModulus;
  }
  return (b << 16U) | a;
}

inline void append_chunk(std::string &png, std::string_view type,
                         std::span<const std::byte> data) {
  if (type.size() != 4U ||
      data.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument{"invalid PNG chunk"};
  }
  append_u32_be(png, static_cast<std::uint32_t>(data.size()));
  const auto crc_begin = png.size();
  png.append(type.data(), type.size());
  if (!data.empty()) {
    png.append(reinterpret_cast<const char *>(data.data()), data.size());
  }
  const auto crc_bytes = std::as_bytes(std::span<const char>{
      png.data() + static_cast<std::ptrdiff_t>(crc_begin),
      png.size() - crc_begin});
  append_u32_be(png, crc32(crc_bytes));
}

[[nodiscard]] inline bool valid_text_keyword(std::string_view keyword) {
  if (keyword.empty() || keyword.size() > 79U || keyword.front() == ' ' ||
      keyword.back() == ' ') {
    return false;
  }
  bool previous_space = false;
  for (const unsigned char character : keyword) {
    if (character < 32U || character > 126U ||
        (character == ' ' && previous_space)) {
      return false;
    }
    previous_space = character == ' ';
  }
  return true;
}

[[nodiscard]] inline std::string
make_stored_zlib_stream(std::span<const std::byte> raw) {
  std::string result;
  // CMF/FLG for DEFLATE, 32 KiB window, fastest/no-compression profile.
  result.push_back(static_cast<char>(0x78));
  result.push_back(static_cast<char>(0x01));
  std::size_t offset = 0U;
  do {
    const auto remaining = raw.size() - offset;
    const auto block_size = static_cast<std::uint16_t>(
        remaining > 65535U ? 65535U : remaining);
    const bool final = offset + block_size == raw.size();
    // Stored blocks start on a byte boundary. BFINAL occupies bit 0 and
    // BTYPE=00 occupies bits 1..2, so the complete header byte is 0 or 1.
    result.push_back(static_cast<char>(final ? 0x01 : 0x00));
    append_u16_le(result, block_size);
    append_u16_le(result, static_cast<std::uint16_t>(~block_size));
    if (block_size != 0U) {
      result.append(reinterpret_cast<const char *>(raw.data() + offset),
                    block_size);
      offset += block_size;
    }
  } while (offset < raw.size());
  append_u32_be(result, adler32(raw));
  return result;
}

} // namespace audit_png_detail

[[nodiscard]] inline AuditPng encode_rgba8_audit_png(
    std::span<const std::byte> rgba, std::uint32_t width,
    std::uint32_t height, std::span<const AuditPngText> text = {}) {
  constexpr std::uint32_t kDimensionLimit = 16384U;
  constexpr std::size_t kPayloadLimit = 512U * 1024U * 1024U;
  if (width == 0U || height == 0U || width > kDimensionLimit ||
      height > kDimensionLimit) {
    throw std::invalid_argument{"audit screencap dimensions are invalid"};
  }
  const auto pixel_count = static_cast<std::uint64_t>(width) * height;
  const auto byte_count = pixel_count * 4U;
  if (byte_count > kPayloadLimit || byte_count != rgba.size()) {
    throw std::invalid_argument{"audit screencap RGBA extent is invalid"};
  }
  if (text.size() > 64U) {
    throw std::invalid_argument{"too many PNG audit metadata records"};
  }

  AuditPng result;
  result.stats.pixels = pixel_count;
  for (std::size_t offset = 0U; offset < rgba.size(); offset += 4U) {
    std::array<std::uint8_t, 4> channel{};
    for (std::size_t index = 0U; index < channel.size(); ++index) {
      channel[index] = std::to_integer<std::uint8_t>(rgba[offset + index]);
      result.stats.channel_min[index] =
          std::min(result.stats.channel_min[index], channel[index]);
      result.stats.channel_max[index] =
          std::max(result.stats.channel_max[index], channel[index]);
    }
    result.stats.rgb_nonzero_pixels +=
        channel[0] != 0U || channel[1] != 0U || channel[2] != 0U ? 1U : 0U;
    result.stats.rgba_zero_pixels +=
        channel[0] == 0U && channel[1] == 0U && channel[2] == 0U &&
                channel[3] == 0U
            ? 1U
            : 0U;
    result.stats.alpha_zero_pixels += channel[3] == 0U ? 1U : 0U;
    result.stats.alpha_full_pixels += channel[3] == 255U ? 1U : 0U;
    result.stats.alpha_partial_pixels +=
        channel[3] != 0U && channel[3] != 255U ? 1U : 0U;
  }

  const auto row_bytes = static_cast<std::size_t>(width) * 4U;
  std::vector<std::byte> filtered;
  filtered.reserve((row_bytes + 1U) * static_cast<std::size_t>(height));
  for (std::uint32_t y = 0U; y < height; ++y) {
    filtered.push_back(std::byte{}); // PNG filter type 0: None.
    const auto row_begin = static_cast<std::size_t>(y) * row_bytes;
    filtered.insert(filtered.end(), rgba.begin() +
                                        static_cast<std::ptrdiff_t>(row_begin),
                    rgba.begin() + static_cast<std::ptrdiff_t>(row_begin +
                                                               row_bytes));
  }

  std::string png;
  png.reserve(filtered.size() + filtered.size() / 65535U * 5U + 512U);
  static constexpr std::array<unsigned char, 8> kSignature{
      0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
  png.append(reinterpret_cast<const char *>(kSignature.data()),
             kSignature.size());

  std::array<std::byte, 13> ihdr{};
  const auto store_u32_be = [&ihdr](std::size_t offset, std::uint32_t value) {
    ihdr[offset] = static_cast<std::byte>((value >> 24U) & 0xFFU);
    ihdr[offset + 1U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    ihdr[offset + 2U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    ihdr[offset + 3U] = static_cast<std::byte>(value & 0xFFU);
  };
  store_u32_be(0U, width);
  store_u32_be(4U, height);
  ihdr[8] = std::byte{8}; // bit depth
  ihdr[9] = std::byte{6}; // RGBA
  audit_png_detail::append_chunk(png, "IHDR", ihdr);

  std::size_t metadata_bytes = 0U;
  for (const auto &entry : text) {
    if (!audit_png_detail::valid_text_keyword(entry.keyword) ||
        entry.value.find('\0') != std::string::npos) {
      throw std::invalid_argument{"invalid PNG audit metadata"};
    }
    metadata_bytes += entry.keyword.size() + 1U + entry.value.size();
    if (metadata_bytes > 65536U) {
      throw std::invalid_argument{"PNG audit metadata exceeds bound"};
    }
    std::string payload = entry.keyword;
    payload.push_back('\0');
    payload += entry.value;
    audit_png_detail::append_chunk(
        png, "tEXt", std::as_bytes(std::span<const char>{payload}));
  }

  const auto zlib = audit_png_detail::make_stored_zlib_stream(filtered);
  audit_png_detail::append_chunk(
      png, "IDAT", std::as_bytes(std::span<const char>{zlib}));
  audit_png_detail::append_chunk(png, "IEND", {});
  result.bytes = std::move(png);
  return result;
}

} // namespace ac6demo
