#include "ac6/native_xdvdfs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_set>

namespace ac6::native {
namespace {

constexpr std::array<std::uint8_t, 20> kMagic = {
    'M', 'I', 'C', 'R', 'O', 'S', 'O', 'F', 'T', '*', 'X', 'B', 'O', 'X',
    '*', 'M', 'E', 'D', 'I', 'A'};
constexpr std::uint64_t kSectorSize = 2048u;
constexpr std::size_t kEntryHeaderSize = 14u;
constexpr std::uint16_t kOrdinalTerminator = 0xffffu;
constexpr std::uint8_t kDirectoryAttribute = 0x10u;
constexpr std::size_t kMaxDirectorySize = 32u * 1024u * 1024u;
constexpr std::size_t kMaxDepth = 64u;
constexpr std::array<std::uint64_t, 6> kLikelyOffsets = {
    0u, 0xfb20u, 0x20600u, 0x02080000u, 0x0fd90000u, 0x18300000u};

struct Volume final {
  std::uint64_t game_offset{};
  std::uint32_t root_sector{};
  std::uint32_t root_size{};
};

struct Entry final {
  std::string name;
  std::uint32_t sector{};
  std::uint32_t size{};
  std::uint8_t attributes{};
};

void fail(std::string* error, const char* message) {
  if (error != nullptr) *error = message;
}

std::uint16_t le16(const std::vector<std::uint8_t>& bytes,
                  std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(bytes[offset + 1u] << 8u);
}

std::uint32_t le32(const std::vector<std::uint8_t>& bytes,
                  std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
         (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
         (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

bool range(std::uint64_t offset, std::uint64_t length,
           std::uint64_t file_size) {
  return offset <= file_size && length <= file_size - offset;
}

bool read_at(std::ifstream& stream, std::uint64_t offset, std::size_t length,
             std::uint64_t file_size, std::vector<std::uint8_t>& bytes,
             std::string* error) {
  if (!range(offset, length, file_size)) {
    fail(error, "XDVDFS range extends beyond the ISO");
    return false;
  }
  stream.clear();
  stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!stream) {
    fail(error, "unable to seek in ISO");
    return false;
  }
  bytes.resize(length);
  stream.read(reinterpret_cast<char*>(bytes.data()),
              static_cast<std::streamsize>(length));
  if (!stream) {
    fail(error, "short read from ISO");
    return false;
  }
  return true;
}

bool find_volume(std::ifstream& stream, std::uint64_t file_size,
                 Volume& volume, std::string* error) {
  for (const std::uint64_t game_offset : kLikelyOffsets) {
    const std::uint64_t descriptor = game_offset + 32u * kSectorSize;
    std::vector<std::uint8_t> bytes;
    if (!range(descriptor, kSectorSize, file_size) ||
        !read_at(stream, descriptor, static_cast<std::size_t>(kSectorSize),
                 file_size, bytes, error)) {
      continue;
    }
    if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) continue;
    if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin() + 0x7ecu)) {
      fail(error, "XDVDFS volume descriptor has no trailing magic");
      return false;
    }
    const std::uint32_t root_sector = le32(bytes, 20u);
    const std::uint32_t root_size = le32(bytes, 24u);
    if (root_size < kEntryHeaderSize - 1u || root_size > kMaxDirectorySize) {
      fail(error, "invalid XDVDFS root directory size");
      return false;
    }
    const std::uint64_t root_offset =
        game_offset + static_cast<std::uint64_t>(root_sector) * kSectorSize;
    if (!range(root_offset, root_size, file_size)) {
      fail(error, "XDVDFS root directory extends beyond the ISO");
      return false;
    }
    volume = Volume{game_offset, root_sector, root_size};
    return true;
  }
  fail(error, "XDVDFS volume descriptor not found");
  return false;
}

bool ascii_equal(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0u; index != left.size(); ++index) {
    const auto a = static_cast<unsigned char>(left[index]);
    const auto b = static_cast<unsigned char>(right[index]);
    if (std::tolower(a) != std::tolower(b)) return false;
  }
  return true;
}

bool read_directory(std::ifstream& stream, std::uint64_t file_size,
                    const Volume& volume, std::uint32_t sector,
                    std::uint32_t length,
                    std::unordered_set<std::uint64_t>& visited,
                    std::vector<Entry>& entries, std::string* error) {
  if (length < kEntryHeaderSize - 1u || length > kMaxDirectorySize) {
    fail(error, "invalid XDVDFS directory size");
    return false;
  }
  const std::uint64_t table_offset =
      volume.game_offset + static_cast<std::uint64_t>(sector) * kSectorSize;
  if (!visited.insert(table_offset).second) {
    fail(error, "XDVDFS directory cycle detected");
    return false;
  }
  std::vector<std::uint8_t> table;
  if (!read_at(stream, table_offset, length, file_size, table, error)) return false;
  std::vector<std::uint16_t> pending{0u};
  std::unordered_set<std::uint16_t> seen;
  while (!pending.empty()) {
    const std::uint16_t ordinal = pending.back();
    pending.pop_back();
    if (!seen.insert(ordinal).second) continue;
    const std::size_t offset = static_cast<std::size_t>(ordinal) * 4u;
    if (offset > table.size() || kEntryHeaderSize > table.size() - offset) {
      continue;
    }
    const std::uint16_t left = le16(table, offset);
    const std::uint16_t right = le16(table, offset + 2u);
    for (const std::uint16_t child : {left, right}) {
      if (child != 0u && child != kOrdinalTerminator) pending.push_back(child);
    }
    const std::uint32_t entry_sector = le32(table, offset + 4u);
    const std::uint32_t entry_size = le32(table, offset + 8u);
    const std::uint8_t attributes = table[offset + 12u];
    const std::uint8_t name_length = table[offset + 13u];
    if (name_length == 0u || name_length > table.size() - offset - kEntryHeaderSize) {
      continue;
    }
    const std::size_t name_offset = offset + kEntryHeaderSize;
    for (std::size_t index = 0u; index != name_length; ++index) {
      if (table[name_offset + index] > 0x7fu) {
        fail(error, "non-ASCII XDVDFS file name");
        return false;
      }
    }
    const std::uint64_t data_offset =
        volume.game_offset + static_cast<std::uint64_t>(entry_sector) * kSectorSize;
    if (!range(data_offset, entry_size, file_size)) {
      fail(error, "XDVDFS entry extends beyond the ISO");
      return false;
    }
    entries.push_back(Entry{
        std::string(reinterpret_cast<const char*>(table.data() + name_offset),
                    name_length),
        entry_sector, entry_size, attributes});
  }
  return true;
}

bool split_path(std::string_view path, std::vector<std::string>& parts,
                std::string* error) {
  std::string component;
  for (std::size_t index = 0u; index <= path.size(); ++index) {
    const char character = index == path.size() ? '/' : path[index];
    if (character == '/' || character == '\\') {
      if (!component.empty()) {
        if (component == "." || component == "..") {
          fail(error, "internal path escapes XDVDFS root");
          return false;
        }
        parts.push_back(std::move(component));
        component.clear();
      }
    } else {
      component.push_back(character);
    }
  }
  if (parts.empty() || parts.size() > kMaxDepth) {
    fail(error, "invalid XDVDFS internal path");
    return false;
  }
  return true;
}

bool locate(std::ifstream& stream, std::uint64_t file_size,
           std::string_view internal_path, Volume& volume, Entry& target,
           std::string* error) {
  std::vector<std::string> parts;
  if (!split_path(internal_path, parts, error)) return false;
  if (!find_volume(stream, file_size, volume, error)) return false;
  std::uint32_t sector = volume.root_sector;
  std::uint32_t length = volume.root_size;
  std::unordered_set<std::uint64_t> visited;
  for (std::size_t depth = 0u; depth != parts.size(); ++depth) {
    std::vector<Entry> entries;
    if (!read_directory(stream, file_size, volume, sector, length, visited,
                        entries, error)) {
      return false;
    }
    const auto found = std::find_if(
        entries.begin(), entries.end(), [&](const Entry& entry) {
          return ascii_equal(entry.name, parts[depth]);
        });
    if (found == entries.end()) {
      fail(error, "file not found in XDVDFS image");
      return false;
    }
    const bool directory = (found->attributes & kDirectoryAttribute) != 0u;
    if (depth + 1u == parts.size()) {
      if (directory) {
        fail(error, "internal path names a directory");
        return false;
      }
      target = *found;
      return true;
    }
    if (!directory) {
      fail(error, "internal path crosses a file");
      return false;
    }
    sector = found->sector;
    length = found->size;
  }
  return true;
}

bool open_iso(const std::filesystem::path& iso, std::ifstream& stream,
             std::uint64_t& file_size, std::string* error) {
  std::error_code ec;
  const std::uintmax_t size = std::filesystem::file_size(iso, ec);
  if (ec || size > std::numeric_limits<std::uint64_t>::max()) {
    fail(error, "ISO file size is invalid");
    return false;
  }
  stream.open(iso, std::ios::binary);
  if (!stream) {
    fail(error, "unable to open ISO");
    return false;
  }
  file_size = static_cast<std::uint64_t>(size);
  return true;
}

}  // namespace

bool read_xdvdfs_file(const std::filesystem::path& iso,
                      std::string_view internal_path,
                      std::vector<std::uint8_t>& bytes,
                      std::size_t maximum_size, XdvdfsFile* file,
                      std::string* error) {
  bytes.clear();
  if (maximum_size == 0u || maximum_size > 16u * 1024u * 1024u) {
    fail(error, "XDVDFS maximum extraction size is invalid");
    return false;
  }
  std::ifstream stream;
  std::uint64_t file_size = 0u;
  if (!open_iso(iso, stream, file_size, error)) return false;
  Volume volume;
  Entry target;
  if (!locate(stream, file_size, internal_path, volume, target, error)) {
    return false;
  }
  if (target.size > maximum_size) {
    fail(error, "bounded XDVDFS extraction rejected file");
    return false;
  }
  const std::uint64_t data_offset =
      volume.game_offset + static_cast<std::uint64_t>(target.sector) * kSectorSize;
  if (!read_at(stream, data_offset, target.size, file_size, bytes, error)) {
    return false;
  }
  if (file != nullptr) {
    *file = XdvdfsFile{data_offset, target.sector, target.size};
  }
  return true;
}

bool locate_xdvdfs_file(const std::filesystem::path& iso,
                        std::string_view internal_path, XdvdfsFile& file,
                        std::string* error) {
  std::ifstream stream;
  std::uint64_t file_size = 0u;
  if (!open_iso(iso, stream, file_size, error)) return false;
  Volume volume;
  Entry target;
  if (!locate(stream, file_size, internal_path, volume, target, error)) {
    return false;
  }
  file = XdvdfsFile{
      volume.game_offset + static_cast<std::uint64_t>(target.sector) * kSectorSize,
      target.sector, target.size};
  return true;
}

}  // namespace ac6::native
