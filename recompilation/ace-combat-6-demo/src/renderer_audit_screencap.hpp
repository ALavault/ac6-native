#pragma once

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/audit_screencap.hpp"
#include "ac6demo/cli.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6demo/session.hpp"
#include "ac6demo/vulkan_neutral_resolve.hpp"
#include "ac6demo/xenos_tiling.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace ac6demo {
namespace renderer_audit_detail {

[[nodiscard]] inline std::string json_escape(std::string_view value) {
  std::ostringstream output;
  for (const unsigned char character : value) {
    switch (character) {
    case '\\': output << "\\\\"; break;
    case '"': output << "\\\""; break;
    case '\b': output << "\\b"; break;
    case '\f': output << "\\f"; break;
    case '\n': output << "\\n"; break;
    case '\r': output << "\\r"; break;
    case '\t': output << "\\t"; break;
    default:
      if (character < 0x20U) {
        output << "\\u00" << std::hex << std::uppercase << std::setfill('0')
               << std::setw(2) << static_cast<unsigned int>(character)
               << std::dec;
      } else {
        output << static_cast<char>(character);
      }
    }
  }
  return output.str();
}

[[nodiscard]] inline std::string address_hex(std::uint32_t value) {
  std::ostringstream output;
  output << "0x" << std::uppercase << std::hex << std::setfill('0')
         << std::setw(8) << value;
  return output.str();
}

} // namespace renderer_audit_detail

// Publish a canonical, lossless audit capture only after the renderer's
// qualified tiled writeback has been read back and matched byte-for-byte to
// the linear RGBA source. This is an audit artifact, not a gameplay claim.
inline void publish_renderer_audit_screencap(
    const DemoSession &session, std::span<const std::byte> guest_linear,
    const VulkanNeutralResolveResult &resolve) {
  const char *directory_text =
      std::getenv("AC6_DEMO_AUDIT_SCREENCAP_DIR");
  if (directory_text == nullptr) {
    return;
  }
  if (*directory_text == '\0') {
    throw RuntimeTrap("audit screencap directory is empty", session.tick());
  }
  const std::filesystem::path directory{directory_text};
  std::error_code filesystem_error;
  if (!std::filesystem::is_directory(directory, filesystem_error) ||
      filesystem_error) {
    throw RuntimeTrap("audit screencap directory is unavailable",
                      session.tick());
  }
  constexpr std::uint32_t kGuestAddress = 0x1374A000U;
  if (!resolve.guest_writeback || resolve.width != kReachedResolveWidth ||
      resolve.height != kReachedResolveHeight ||
      resolve.destination_address != kGuestAddress ||
      guest_linear.size() != kReachedResolveLinearBytes ||
      resolve.guest_linear_rgba8_sha256.empty() ||
      Sha256::bytes(guest_linear) != resolve.guest_linear_rgba8_sha256) {
    throw RuntimeTrap("audit screencap source is not the qualified writeback",
                      session.tick(), 0, resolve.destination_address);
  }

  const auto tick_text = std::to_string(session.tick());
  const auto present_text = std::to_string(session.graphics_present_count());
  const auto address_text = renderer_audit_detail::address_hex(kGuestAddress);
  const std::array metadata{
      AuditPngText{"Software", "ac6-demo-recomp renderer audit"},
      AuditPngText{"ac6.target", "Xbox 360 PAL demo"},
      AuditPngText{"ac6.stage", "guest-linear-after-qualified-writeback"},
      AuditPngText{"ac6.tick", tick_text},
      AuditPngText{"ac6.present_count", present_text},
      AuditPngText{"ac6.guest_address", address_text},
      AuditPngText{"ac6.rgba8_sha256", resolve.guest_linear_rgba8_sha256},
      AuditPngText{"ac6.gameplay_claim", "false"}};
  const auto encoded = encode_rgba8_audit_png(
      guest_linear, resolve.width, resolve.height, metadata);
  const auto png_span = std::as_bytes(std::span<const char>{
      encoded.bytes.data(), encoded.bytes.size()});
  const auto png_sha256 = Sha256::bytes(png_span);

  std::ostringstream stem;
  stem << "ac6-demo-pal-present-t" << std::setfill('0') << std::setw(12)
       << session.tick() << "-p" << std::setw(12)
       << session.graphics_present_count() << '-'
       << resolve.guest_linear_rgba8_sha256.substr(0U, 16U);
  const auto png_path = directory / (stem.str() + ".png");
  const auto json_path = directory / (stem.str() + ".json");

  std::ostringstream json;
  json << "{\n"
       << "  \"schema\": \"ac6-demo-renderer-audit-screencap/v1\",\n"
       << "  \"target\": {\n"
       << "    \"id\": \"ac6-demo-xbox360-pal\",\n"
       << "    \"module\": \"Default.xex\",\n"
       << "    \"xex_sha256\": "
          "\"de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8\"\n"
       << "  },\n"
       << "  \"capture\": {\n"
       << "    \"tick\": " << session.tick() << ",\n"
       << "    \"present_count\": " << session.graphics_present_count()
       << ",\n"
       << "    \"stage\": \"guest-linear-after-qualified-writeback\",\n"
       << "    \"guest_address\": \"" << address_text << "\",\n"
       << "    \"width\": " << resolve.width << ",\n"
       << "    \"height\": " << resolve.height << ",\n"
       << "    \"format\": \"RGBA8\",\n"
       << "    \"png\": \""
       << renderer_audit_detail::json_escape(png_path.filename().string())
       << "\",\n"
       << "    \"png_sha256\": \"" << png_sha256 << "\",\n"
       << "    \"rgba8_sha256\": \""
       << resolve.guest_linear_rgba8_sha256 << "\"\n"
       << "  },\n"
       << "  \"pixels\": {\n"
       << "    \"count\": " << encoded.stats.pixels << ",\n"
       << "    \"rgb_nonzero\": " << encoded.stats.rgb_nonzero_pixels
       << ",\n"
       << "    \"rgba_zero\": " << encoded.stats.rgba_zero_pixels << ",\n"
       << "    \"alpha_zero\": " << encoded.stats.alpha_zero_pixels
       << ",\n"
       << "    \"alpha_full\": " << encoded.stats.alpha_full_pixels
       << ",\n"
       << "    \"alpha_partial\": "
       << encoded.stats.alpha_partial_pixels << ",\n"
       << "    \"rgb_all_black\": "
       << (encoded.stats.rgb_all_black() ? "true" : "false") << "\n"
       << "  },\n"
       << "  \"classification\": {\n"
       << "    \"audit_capture\": true,\n"
       << "    \"gameplay_screenshot_claim\": false,\n"
       << "    \"source_verified_after_guest_writeback\": true\n"
       << "  }\n"
       << "}\n";

  publish_new_file(json_path, json.str());
  try {
    publish_new_file(png_path, encoded.bytes);
  } catch (...) {
    std::error_code cleanup_error;
    static_cast<void>(std::filesystem::remove(json_path, cleanup_error));
    throw;
  }
  std::fprintf(stderr,
               "AC6_AUDIT_SCREENCAP tick=%llu present=%llu path=%s "
               "png_sha256=%s rgba8_sha256=%s rgb_nonzero=%llu "
               "rgb_all_black=%u\n",
               static_cast<unsigned long long>(session.tick()),
               static_cast<unsigned long long>(session.graphics_present_count()),
               png_path.c_str(), png_sha256.c_str(),
               resolve.guest_linear_rgba8_sha256.c_str(),
               static_cast<unsigned long long>(
                   encoded.stats.rgb_nonzero_pixels),
               encoded.stats.rgb_all_black() ? 1U : 0U);
}

} // namespace ac6demo

#endif
