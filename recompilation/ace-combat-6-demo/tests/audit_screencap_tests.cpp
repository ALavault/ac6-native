#ifdef NDEBUG
#error "This test suite uses assert(); compile with -UNDEBUG."
#endif

#include "ac6demo/audit_screencap.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::uint32_t read_u32_be(std::string_view bytes, std::size_t offset) {
  assert(offset + 4U <= bytes.size());
  return (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset]))
          << 24U) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 1U]))
          << 16U) |
         (static_cast<std::uint32_t>(
              static_cast<unsigned char>(bytes[offset + 2U]))
          << 8U) |
         static_cast<std::uint32_t>(
             static_cast<unsigned char>(bytes[offset + 3U]));
}

std::vector<std::byte> decode_stored_zlib(std::string_view zlib) {
  assert(zlib.size() >= 6U);
  assert(static_cast<unsigned char>(zlib[0]) == 0x78U);
  assert(static_cast<unsigned char>(zlib[1]) == 0x01U);
  std::vector<std::byte> raw;
  std::size_t offset = 2U;
  bool final = false;
  while (!final) {
    assert(offset + 5U <= zlib.size());
    const auto header = static_cast<unsigned char>(zlib[offset++]);
    final = (header & 1U) != 0U;
    assert((header & 0x06U) == 0U);
    const auto len = static_cast<std::uint16_t>(
        static_cast<unsigned char>(zlib[offset]) |
        (static_cast<unsigned char>(zlib[offset + 1U]) << 8U));
    const auto nlen = static_cast<std::uint16_t>(
        static_cast<unsigned char>(zlib[offset + 2U]) |
        (static_cast<unsigned char>(zlib[offset + 3U]) << 8U));
    offset += 4U;
    assert(static_cast<std::uint16_t>(~len) == nlen);
    assert(offset + len <= zlib.size() - 4U);
    for (std::uint16_t index = 0U; index < len; ++index) {
      raw.push_back(static_cast<std::byte>(
          static_cast<unsigned char>(zlib[offset + index])));
    }
    offset += len;
  }
  assert(offset + 4U == zlib.size());
  const auto expected = read_u32_be(zlib, offset);
  assert(ac6demo::audit_png_detail::adler32(raw) == expected);
  return raw;
}

} // namespace

int main(int argc, char **argv) {
  const std::array<std::byte, 16> rgba{
      std::byte{255}, std::byte{0},   std::byte{0},   std::byte{255},
      std::byte{0},   std::byte{255}, std::byte{0},   std::byte{128},
      std::byte{0},   std::byte{0},   std::byte{255}, std::byte{0},
      std::byte{0},   std::byte{0},   std::byte{0},   std::byte{255}};
  const std::array metadata{
      ac6demo::AuditPngText{"Software", "ac6-demo audit test"},
      ac6demo::AuditPngText{"ac6.tick", "42"}};
  const auto encoded = ac6demo::encode_rgba8_audit_png(rgba, 2U, 2U, metadata);

  assert(encoded.stats.pixels == 4U);
  assert(encoded.stats.rgb_nonzero_pixels == 3U);
  assert(encoded.stats.rgba_zero_pixels == 0U);
  assert(encoded.stats.alpha_zero_pixels == 1U);
  assert(encoded.stats.alpha_full_pixels == 2U);
  assert(encoded.stats.alpha_partial_pixels == 1U);
  assert(!encoded.stats.rgb_all_black());
  const std::array<std::uint8_t, 4> expected_min{0U, 0U, 0U, 0U};
  const std::array<std::uint8_t, 4> expected_max{255U, 255U, 255U, 255U};
  assert(encoded.stats.channel_min == expected_min);
  assert(encoded.stats.channel_max == expected_max);

  const std::string_view png{encoded.bytes};
  static constexpr std::array<unsigned char, 8> signature{
      0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
  assert(png.size() > signature.size());
  for (std::size_t index = 0U; index < signature.size(); ++index) {
    assert(static_cast<unsigned char>(png[index]) == signature[index]);
  }

  std::size_t offset = 8U;
  bool saw_ihdr = false;
  bool saw_software = false;
  bool saw_tick = false;
  bool saw_iend = false;
  std::string idat;
  while (offset < png.size()) {
    const auto length = read_u32_be(png, offset);
    offset += 4U;
    assert(offset + 4U + length + 4U <= png.size());
    const std::string_view type = png.substr(offset, 4U);
    const auto crc_begin = offset;
    offset += 4U;
    const std::string_view payload = png.substr(offset, length);
    offset += length;
    const auto expected_crc = read_u32_be(png, offset);
    const auto crc_span = std::as_bytes(std::span<const char>{
        png.data() + static_cast<std::ptrdiff_t>(crc_begin), 4U + length});
    assert(ac6demo::audit_png_detail::crc32(crc_span) == expected_crc);
    offset += 4U;

    if (type == "IHDR") {
      saw_ihdr = true;
      assert(length == 13U);
      assert(read_u32_be(payload, 0U) == 2U);
      assert(read_u32_be(payload, 4U) == 2U);
      assert(static_cast<unsigned char>(payload[8]) == 8U);
      assert(static_cast<unsigned char>(payload[9]) == 6U);
    } else if (type == "tEXt") {
      static constexpr std::string_view software_text{
          "Software\0ac6-demo audit test", 28U};
      static constexpr std::string_view tick_text{"ac6.tick\0" "42", 11U};
      saw_software = saw_software || payload == software_text;
      saw_tick = saw_tick || payload == tick_text;
    } else if (type == "IDAT") {
      idat.append(payload.data(), payload.size());
    } else if (type == "IEND") {
      saw_iend = true;
      assert(payload.empty());
    }
  }
  assert(saw_ihdr && saw_software && saw_tick && saw_iend && !idat.empty());

  const auto raw = decode_stored_zlib(idat);
  const std::array<std::byte, 18> expected_raw{
      std::byte{0},
      std::byte{255}, std::byte{0},   std::byte{0},   std::byte{255},
      std::byte{0},   std::byte{255}, std::byte{0},   std::byte{128},
      std::byte{0},
      std::byte{0},   std::byte{0},   std::byte{255}, std::byte{0},
      std::byte{0},   std::byte{0},   std::byte{0},   std::byte{255}};
  assert(raw == std::vector<std::byte>(expected_raw.begin(), expected_raw.end()));

  bool rejected = false;
  try {
    (void)ac6demo::encode_rgba8_audit_png(rgba, 3U, 2U);
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  assert(rejected);

  if (argc >= 2) {
    std::ofstream output(argv[1], std::ios::binary);
    assert(output);
    output.write(encoded.bytes.data(),
                 static_cast<std::streamsize>(encoded.bytes.size()));
    assert(output.good());
  }
  if (argc >= 3) {
    constexpr std::uint32_t width = 1280U;
    constexpr std::uint32_t height = 720U;
    std::vector<std::byte> black(
        static_cast<std::size_t>(width) * height * 4U, std::byte{});
    const std::array black_metadata{
        ac6demo::AuditPngText{
            "ac6.rgba8_sha256",
            "0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f"},
        ac6demo::AuditPngText{"ac6.gameplay_claim", "false"}};
    const auto black_png = ac6demo::encode_rgba8_audit_png(
        black, width, height, black_metadata);
    assert(black_png.stats.pixels == 921600U);
    assert(black_png.stats.rgb_all_black());
    assert(black_png.stats.rgba_zero_pixels == 921600U);
    std::ofstream output(argv[2], std::ios::binary);
    assert(output);
    output.write(black_png.bytes.data(),
                 static_cast<std::streamsize>(black_png.bytes.size()));
    assert(output.good());
  }
  return 0;
}
