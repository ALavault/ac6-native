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
//
// `maximum_size` bounds this call's own eager copy (rejects a call asking
// for more than 16MiB in one shot, independent of the file's real size) --
// it is not a general "largest file this format supports" limit. A title's
// multi-hundred-megabyte data packages (this product's own DATA00.PAC is
// over 2GiB) must locate their offset/size via `locate_xdvdfs_file` below
// and stream reads directly, never through this eager-copy path.
[[nodiscard]] bool read_xdvdfs_file(const std::filesystem::path& iso,
                                    std::string_view internal_path,
                                    std::vector<std::uint8_t>& bytes,
                                    std::size_t maximum_size,
                                    XdvdfsFile* file = nullptr,
                                    std::string* error = nullptr);

// Resolves one case-insensitive path to its ISO offset and size, with the
// same descriptor/directory/path/cycle validation as `read_xdvdfs_file`,
// but without reading any file payload -- no size cap applies, since no
// copy happens. Callers that need to stream a large file (rather than pull
// it entirely into memory) use this to get `iso_offset`/`size` and then
// seek/read the host ISO file directly, bounded by their own request size.
[[nodiscard]] bool locate_xdvdfs_file(const std::filesystem::path& iso,
                                      std::string_view internal_path,
                                      XdvdfsFile& file,
                                      std::string* error = nullptr);

}  // namespace ac6::native
