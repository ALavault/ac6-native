#include "ac6/retail_container_index.h"

namespace ac6::retail {
namespace {

std::uint32_t be32(const std::uint8_t* at) noexcept {
  return (static_cast<std::uint32_t>(at[0]) << 24) |
         (static_cast<std::uint32_t>(at[1]) << 16) |
         (static_cast<std::uint32_t>(at[2]) << 8) | at[3];
}

std::uint16_t be16(const std::uint8_t* at) noexcept {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(at[0]) << 8) | at[1]);
}

std::uint16_t swap16(std::uint16_t value) noexcept {
  return static_cast<std::uint16_t>(((value & 0x00FFu) << 8) | (value >> 8));
}

std::uint32_t swap32(std::uint32_t value) noexcept {
  return ((value & 0x000000FFu) << 24) | ((value & 0x0000FF00u) << 8) |
         ((value & 0x00FF0000u) >> 8) | (value >> 24);
}

}  // namespace

bool parse_container_index(ContainerIndex& out, const std::uint8_t* file,
                           std::size_t size, std::uint32_t base) noexcept {
  if (file == nullptr || size < 8) return false;

  // 0x82234C18..0x82234C24: the two bytes, straight across.
  out.version = file[4];
  out.endian = file[5];

  // 0x82234C28..0x82234C58: the header size, swapped when the flag is not 1.
  std::uint16_t header = be16(file + 6);
  if (out.endian != kNativeEndianFlag) header = swap16(header);
  out.header_size = header;
  if (static_cast<std::size_t>(header) + 4 > size) return false;

  // 0x82234C64..0x82234C98. The count is read RAW and the four pointers are
  // derived from it BEFORE any swap -- see the header for why that matters.
  const std::uint32_t raw = be32(file + header);
  const std::uint32_t table = base + header + 4u;
  out.base = base;
  out.array0 = table;
  out.array1 = table + raw * 4u;
  out.array2 = table + raw * 8u;
  out.array3 = table + raw * 12u;

  // 0x82234C9C: the native path stops here with the raw count in place.
  out.count = raw;
  if (out.endian == kNativeEndianFlag) return true;

  // 0x82234CA0..0x82234DC0: the swapped count replaces it, then the four
  // arrays are rewritten in place. The rewrite is not modelled here -- this
  // port takes an immutable buffer and no shipped file reaches the path; the
  // descriptor it produces is what the differential compares.
  out.count = swap32(raw);
  return true;
}

std::uint32_t container_entry(const ContainerIndex& index,
                              const std::uint8_t* file, std::size_t size,
                              std::uint32_t which) noexcept {
  // 0x82234DD0..0x82234DE0.
  if (which >= index.count) return 0;
  const std::uint32_t at = index.array0 - index.base + which * 4u;
  if (file == nullptr || static_cast<std::size_t>(at) + 4 > size) return 0;
  // 0x82234DE4..0x82234DF4: a zero table entry is a null, not base + 0.
  const std::uint32_t offset = be32(file + at);
  if (offset == 0) return 0;
  return index.base + offset;
}

std::uint32_t container_entry_length(const ContainerIndex& index,
                                     const std::uint8_t* file, std::size_t size,
                                     std::uint32_t which) noexcept {
  if (which >= index.count) return 0;
  const std::uint32_t at = index.array1 - index.base + which * 4u;
  if (file == nullptr || static_cast<std::size_t>(at) + 4 > size) return 0;
  return be32(file + at);
}

}  // namespace ac6::retail
