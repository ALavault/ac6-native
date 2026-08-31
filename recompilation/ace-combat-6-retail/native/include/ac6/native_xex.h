#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
#include <string>

namespace ac6::native {

// Header identity and layout needed before a guest image can be mapped.
struct XexMetadata final {
  std::uint32_t header_size{};
  std::uint32_t security_offset{};
  std::uint32_t image_size{};
  std::uint32_t load_address{};
  std::uint32_t entry_point{};
  std::uint32_t image_base{};
  std::uint32_t tls_address{};
  std::uint32_t tls_data_size{};
  std::uint32_t tls_raw_size{};
  std::uint32_t stack_size{};
  std::uint32_t system_flags{};
  std::uint16_t encryption{};
  std::uint16_t compression{};
};

struct XexImage final {
  XexMetadata metadata{};
  std::vector<std::uint8_t> image;
};

// Parse and validate the qualified XEX2 header. Throws no exceptions to the
// caller: false means file is absent, malformed, or outside the Xenon image
// contract. The payload is never copied into the product tree.
[[nodiscard]] bool inspect_xex_metadata(const std::filesystem::path& path,
                                         XexMetadata& metadata,
                                         std::string* error = nullptr);

[[nodiscard]] bool inspect_xex_metadata_bytes(
    std::span<const std::uint8_t> bytes, XexMetadata& metadata,
    std::string* error = nullptr);

// Decode a XEX2 image into its Xenon load-address image. Normal AES encryption
// and basic compression are supported; normal/LZX compression and unsupported
// formats fail closed. The returned bytes remain caller-owned memory.
[[nodiscard]] bool load_xex_image_bytes(std::span<const std::uint8_t> bytes,
                                        XexImage& image,
                                        std::string* error = nullptr);

[[nodiscard]] bool load_xex_image_file(const std::filesystem::path& path,
                                       XexImage& image,
                                       std::string* error = nullptr);

}  // namespace ac6::native
