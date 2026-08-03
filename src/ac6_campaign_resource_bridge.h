#pragma once

#include <cstddef>
#include <cstdint>

namespace ac6::campaign_resource {

struct FhmParent {
  bool valid = false;
  uint32_t size = 0;
  uint32_t member_count = 0;
};

struct FhmWrapper {
  bool valid = false;
  uint32_t inner_offset = 0;
  uint32_t inner_size = 0;
  uint32_t inner_member_count = 0;
};

constexpr uint32_t kCampaignLeafSize = 0xEB000u;

// The post-cutscene result/map task indexes mission packages as 209 + level.
// Level zero resolves to the common `results` SWG bundle, but the task's
// constructor consumes the BRDB/BMAP pair present from entry 210 onward.
// Promote only the new-campaign transition state to the first mission index.
inline bool ShouldSelectFirstCampaignMission(uint32_t current_level,
                                             uint32_t level_mode,
                                             uint32_t level_selector) {
  return current_level == 0 && level_mode == 1 && level_selector == 0;
}

inline uint16_t ReadU16(const uint8_t* p, bool big_endian) {
  return big_endian ? (uint16_t(p[0]) << 8) | uint16_t(p[1])
                    : (uint16_t(p[1]) << 8) | uint16_t(p[0]);
}

inline uint32_t ReadU32(const uint8_t* p, bool big_endian) {
  if (big_endian) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8) | uint32_t(p[3]);
  }
  return (uint32_t(p[3]) << 24) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[1]) << 8) | uint32_t(p[0]);
}

// Validate that `header` is an FHM directory containing the exact leaf the
// guest registry exposed. The campaign loader needs member 1 from the parent,
// so an empty or absent member 1 is rejected as well.
inline FhmParent InspectFhmParent(const uint8_t* header,
                                  std::size_t header_size,
                                  uint32_t parent_address,
                                  uint32_t leaf_address,
                                  uint32_t leaf_size) {
  if (!header || header_size < 12 || header[0] != 'F' || header[1] != 'H' ||
      header[2] != 'M' || header[3] != ' ' || header[5] > 1) {
    return {};
  }

  const bool big_endian = header[5] == 1;
  const uint32_t directory = ReadU16(header + 6, big_endian);
  if (directory > header_size - 4) return {};
  const uint32_t count = ReadU32(header + directory, big_endian);
  if (count < 2 || count > 64) return {};

  const std::size_t arrays = std::size_t(directory) + 4;
  if (arrays + std::size_t(count) * 8 > header_size) return {};

  uint32_t parent_size = 0;
  bool leaf_found = false;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t offset = ReadU32(header + arrays + i * 4, big_endian);
    const uint32_t size =
        ReadU32(header + arrays + (count + i) * 4, big_endian);
    if (offset > UINT32_MAX - size) return {};
    const uint32_t end = offset + size;
    if (end > parent_size) parent_size = end;
    if (parent_address <= UINT32_MAX - offset &&
        parent_address + offset == leaf_address && size == leaf_size) {
      leaf_found = true;
    }
  }

  const uint32_t required_size =
      ReadU32(header + arrays + (count + 1) * 4, big_endian);
  if (!leaf_found || required_size == 0 || parent_size == 0) return {};
  return {true, parent_size, count};
}

// The registry exposes the decoded entry's outer FHM wrapper (one member at
// +0x1000), while the campaign constructor expects the inner FHM directly.
// Validate both directories and require non-empty members 1 and 3, which are
// immediately consumed by the constructor and the mission record walker.
inline FhmWrapper InspectCampaignWrapper(const uint8_t* outer,
                                         std::size_t available) {
  if (!outer || available < 128 || outer[0] != 'F' || outer[1] != 'H' ||
      outer[2] != 'M' || outer[3] != ' ' || outer[5] > 1) {
    return {};
  }
  const bool outer_be = outer[5] == 1;
  const uint32_t outer_directory = ReadU16(outer + 6, outer_be);
  if (outer_directory > available - 12 ||
      ReadU32(outer + outer_directory, outer_be) != 1) {
    return {};
  }
  const std::size_t outer_arrays = std::size_t(outer_directory) + 4;
  const uint32_t inner_offset = ReadU32(outer + outer_arrays, outer_be);
  const uint32_t inner_size = ReadU32(outer + outer_arrays + 4, outer_be);
  if (inner_offset != 0x1000 || inner_size < 128 ||
      inner_offset > UINT32_MAX - inner_size || inner_offset > available ||
      inner_size > available - inner_offset) {
    return {};
  }

  const uint8_t* inner = outer + inner_offset;
  if (inner[0] != 'F' || inner[1] != 'H' || inner[2] != 'M' ||
      inner[3] != ' ' || inner[5] > 1) {
    return {};
  }
  const bool inner_be = inner[5] == 1;
  const uint32_t inner_directory = ReadU16(inner + 6, inner_be);
  if (inner_directory > 124) return {};
  const uint32_t count = ReadU32(inner + inner_directory, inner_be);
  if (count < 4 || count > 64) return {};
  const std::size_t arrays = std::size_t(inner_directory) + 4;
  if (arrays + std::size_t(count) * 8 > 128) return {};
  const uint32_t member1_size =
      ReadU32(inner + arrays + std::size_t(count + 1) * 4, inner_be);
  const uint32_t member3_size =
      ReadU32(inner + arrays + std::size_t(count + 3) * 4, inner_be);
  const uint32_t member1_offset =
      ReadU32(inner + arrays + std::size_t(1) * 4, inner_be);
  const uint32_t member3_offset =
      ReadU32(inner + arrays + std::size_t(3) * 4, inner_be);
  if (member1_size == 0 || member3_size == 0 ||
      member1_offset > inner_size || member1_size > inner_size - member1_offset ||
      member3_offset > inner_size || member3_size > inner_size - member3_offset) {
    return {};
  }
  return {true, inner_offset, inner_size, count};
}

// The PAL resource-size walker treats the first embedded record of this NTXR
// profile as optional. Its directory accessor returns null when the count at
// the byte-swapped +0x100 directory is zero; the retail low page then supplies
// the zero record count read at null+2. Detect that exact, bounded case so the
// native override can return the same zero without touching the protected host
// null page.
inline bool HasEmptyNtxrDirectory(const uint8_t* payload,
                                  std::size_t available) {
  if (!payload || available < 0x104 || payload[0] != 'N' ||
      payload[1] != 'T' || payload[2] != 'X' || payload[3] != 'R' ||
      payload[4] != 2 || payload[5] != 0x0C || payload[6] != 0 ||
      payload[7] != 1) {
    return false;
  }
  const uint32_t directory = ReadU16(payload + 6, false);
  return directory <= available - 4 &&
         ReadU32(payload + directory, false) == 0;
}

}  // namespace ac6::campaign_resource
