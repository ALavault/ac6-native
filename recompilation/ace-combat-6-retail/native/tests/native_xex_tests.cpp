#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6/native_xex.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

void put16(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 1u] = static_cast<std::uint8_t>(value);
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t offset,
           std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 24u);
  bytes[offset + 1u] = static_cast<std::uint8_t>(value >> 16u);
  bytes[offset + 2u] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 3u] = static_cast<std::uint8_t>(value);
}

std::vector<std::uint8_t> minimal_xex() {
  std::vector<std::uint8_t> bytes(0x200u, 0u);
  put32(bytes, 0x00u, 0x58455832u);
  put32(bytes, 0x08u, 0x100u);
  put32(bytes, 0x10u, 0x90u);
  put32(bytes, 0x14u, 6u);
  constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 6> headers{{
      {0x000003ffu, 0x150u}, {0x00010100u, 0x82000010u},
      {0x00010201u, 0x82000000u}, {0x00020104u, 0x170u},
      {0x00020200u, 0x40000u}, {0x00030000u, 0u}}};
  for (std::size_t index = 0u; index != headers.size(); ++index) {
    put32(bytes, 0x18u + index * 8u, headers[index].first);
    put32(bytes, 0x1cu + index * 8u, headers[index].second);
  }
  put32(bytes, 0x94u, 0x1000u);
  put32(bytes, 0x1a0u, 0x82000000u);
  put32(bytes, 0x150u, 0x28u);
  put16(bytes, 0x154u, 1u);
  put16(bytes, 0x156u, 1u);
  put32(bytes, 0x170u, 16u);
  put32(bytes, 0x174u, 0x82000100u);
  put32(bytes, 0x178u, 0x20u);
  put32(bytes, 0x17cu, 0x20u);
  return bytes;
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() / "ac6-native-xex-test";
  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
  std::filesystem::create_directories(root, ignored);
  const auto path = root / "default.xex";
  const auto bytes = minimal_xex();
  {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }

  ac6::native::XexMetadata metadata;
  std::string error;
  assert(ac6::native::inspect_xex_metadata(path, metadata, &error));
  assert(error.empty());
  assert(metadata.load_address == 0x82000000u);
  assert(metadata.entry_point == 0x82000010u);
  assert(metadata.image_size == 0x1000u);
  assert(metadata.encryption == 1u);
  assert(metadata.compression == 1u);

  auto malformed = bytes;
  malformed[0] = 'N';
  {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(malformed.data()),
                 static_cast<std::streamsize>(malformed.size()));
  }
  error.clear();
  assert(!ac6::native::inspect_xex_metadata(path, metadata, &error));
  assert(error == "input is not XEX2");

  std::filesystem::remove_all(root, ignored);
  return 0;
}
