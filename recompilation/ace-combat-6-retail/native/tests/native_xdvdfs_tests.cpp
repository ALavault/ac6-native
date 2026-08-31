#include "ac6/native_xdvdfs.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void le16(std::vector<std::uint8_t>& bytes, std::size_t offset,
          std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
}

void le32(std::vector<std::uint8_t>& bytes, std::size_t offset,
          std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value);
  bytes[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 2u] = static_cast<std::uint8_t>(value >> 16u);
  bytes[offset + 3u] = static_cast<std::uint8_t>(value >> 24u);
}

std::filesystem::path make_image() {
  const auto path = std::filesystem::temp_directory_path() /
                    "ac6-native-xdvdfs-test.iso";
  std::vector<std::uint8_t> descriptor(2048u, 0u);
  const std::string magic = "MICROSOFT*XBOX*MEDIA";
  std::copy(magic.begin(), magic.end(), descriptor.begin());
  std::copy(magic.begin(), magic.end(), descriptor.begin() + 0x7ecu);
  le32(descriptor, 20u, 0x100u);
  le32(descriptor, 24u, 32u);
  std::vector<std::uint8_t> table(32u, 0u);
  le16(table, 0u, 0xffffu);
  le16(table, 2u, 0xffffu);
  le32(table, 4u, 0x101u);
  le32(table, 8u, 4u);
  table[12u] = 0u;
  table[13u] = 11u;
  const std::string name = "default.xex";
  std::copy(name.begin(), name.end(), table.begin() + 14u);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.seekp(0x10000u);
  stream.write(reinterpret_cast<const char*>(descriptor.data()),
               static_cast<std::streamsize>(descriptor.size()));
  stream.seekp(0x80000u);
  stream.write(reinterpret_cast<const char*>(table.data()),
               static_cast<std::streamsize>(table.size()));
  stream.seekp(0x80800u);
  stream.write("XEX2", 4);
  return path;
}

}  // namespace

int main() {
  const auto path = make_image();
  std::vector<std::uint8_t> bytes;
  ac6::native::XdvdfsFile file;
  std::string error;
  assert(ac6::native::read_xdvdfs_file(path, "DEFAULT.XEX", bytes, 16u,
                                       &file, &error));
  assert(bytes == std::vector<std::uint8_t>({'X', 'E', 'X', '2'}));
  assert(file.iso_offset == 0x80800u);
  error.clear();
  assert(!ac6::native::read_xdvdfs_file(path, "../default.xex", bytes, 16u,
                                        nullptr, &error));
  assert(error == "internal path escapes XDVDFS root");
  error.clear();
  assert(!ac6::native::read_xdvdfs_file(path, "default.xex", bytes, 3u,
                                        nullptr, &error));
  assert(error == "bounded XDVDFS extraction rejected file");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  return 0;
}
