#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ac6::native {

struct XdvdfsFile final {
  std::uint64_t iso_offset{};
  std::uint32_t sector{};
  std::uint32_t size{};
};

// Read one case-insensitive file from an Xbox 360 XDVDFS volume. The reader
// validates descriptor, directory, path, cycle, and file bounds before any
// payload is returned; it never creates or writes a retail file.
[[nodiscard]] bool read_xdvdfs_file(const std::filesystem::path& iso,
                                    std::string_view internal_path,
                                    std::vector<std::uint8_t>& bytes,
                                    std::size_t maximum_size,
                                    XdvdfsFile* file = nullptr,
                                    std::string* error = nullptr);

}  // namespace ac6::native
