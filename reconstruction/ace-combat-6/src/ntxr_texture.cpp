#include "ac6/ntxr_texture.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace ac6::retail {
namespace {

constexpr std::uint32_t kSignature = 0x4E545852u;  // 'NTXR'
// The descriptor begins at file 0x10 - derived, not assumed (cycle 1167):
// 0x8234B0C0 is the one-line accessor `addi r3,r3,0x10; blr`, and every offset
// 0x8234B360 uses is relative to what it returns. The neighbouring accessor
// 0x8234B0B8 (`lhz r3,0x6(r3)`) returns the texture count at file +0x06.
constexpr std::size_t kDescriptorBase = 0x10;
constexpr std::size_t kMipCountOffset = kDescriptorBase + 0x11;
constexpr std::size_t kFormatCodeOffset = kDescriptorBase + 0x13;
constexpr std::size_t kWidthOffset = kDescriptorBase + 0x14;
constexpr std::size_t kHeightOffset = kDescriptorBase + 0x16;
constexpr std::size_t kFlagsOffset = kDescriptorBase + 0x1C;
// File 0x30, which is descriptor +0x20 - not +0x30. Unlike the fields above
// this one is a container convention rather than a field retail reads here,
// so it is named by its file offset, which is how the control measured it.
constexpr std::size_t kDataOffsetOffset = kDescriptorBase + 0x20;
// Descriptor +0x30, which 0x8234B268 walks as a dword-per-level array. Only the
// first two entries are used here: the base surface size and the mip-chain
// size. Present only when the level count exceeds 1.
constexpr std::size_t kBaseSurfaceOffset = kDescriptorBase + 0x30;
constexpr std::size_t kMipChainOffset = kDescriptorBase + 0x34;

// 0x8234B374 compares the format code against 0x2F and fails the load when it
// is not below it. The table has exactly that many entries.
constexpr std::uint8_t kFormatCodeBound = 47;

// 0x8234B118 keeps bit 9 of the flags word: rlwinm r3,r11,0x17,0x1f,0x1f
// rotates left by 23 and masks bit 31, which is bit 9 of the original.
constexpr std::uint32_t kCubeMapBit = 1u << 9;

// The Xenos tile is 32 blocks on a side.
constexpr std::uint32_t kTileBlocks = 32;

std::uint16_t read_u16(const std::uint8_t* p) noexcept {
  return static_cast<std::uint16_t>(p[0] << 8 | p[1]);
}

std::uint32_t read_u32(const std::uint8_t* p) noexcept {
  return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
         static_cast<std::uint32_t>(p[2]) << 8 | static_cast<std::uint32_t>(p[3]);
}

std::uint32_t pad_to_tile(std::uint32_t blocks) noexcept {
  return (blocks + kTileBlocks - 1) / kTileBlocks * kTileBlocks;
}

std::uint32_t bytes_per_block(std::uint8_t xenos_format) noexcept {
  switch (xenos_format) {
    case kXenosDxt1: return 8;
    case kXenosDxt2_3:
    case kXenosDxt4_5: return 16;
    default: return 0;
  }
}

std::uint32_t pack_rgba(std::uint32_t r, std::uint32_t g, std::uint32_t b,
                        std::uint32_t a) noexcept {
  return (a << 24) | (b << 16) | (g << 8) | r;
}

struct Rgb {
  std::uint32_t r, g, b;
};

Rgb rgb565(std::uint16_t value) noexcept {
  return {static_cast<std::uint32_t>((value >> 11) * 255u / 31u),
          static_cast<std::uint32_t>(((value >> 5) & 63u) * 255u / 63u),
          static_cast<std::uint32_t>((value & 31u) * 255u / 31u)};
}

// The BC1 colour half, which BC1, BC2 and BC3 share byte for byte. `block`
// points at the eight bytes that hold it.
void decode_color_half(const std::uint8_t* block, bool punchthrough,
                       Rgb out_rgb[16], bool out_transparent[16]) noexcept {
  const std::uint16_t c0 = static_cast<std::uint16_t>(block[0] | block[1] << 8);
  const std::uint16_t c1 = static_cast<std::uint16_t>(block[2] | block[3] << 8);
  const Rgb a = rgb565(c0);
  const Rgb b = rgb565(c1);
  Rgb table[4] = {a, b, {}, {}};
  // Only BC1 has the one-bit-alpha branch; in BC2 and BC3 the four-colour
  // interpolation is used regardless of the endpoint order.
  const bool four_color = !punchthrough || c0 > c1;
  if (four_color) {
    table[2] = {(2 * a.r + b.r) / 3, (2 * a.g + b.g) / 3, (2 * a.b + b.b) / 3};
    table[3] = {(a.r + 2 * b.r) / 3, (a.g + 2 * b.g) / 3, (a.b + 2 * b.b) / 3};
  } else {
    table[2] = {(a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2};
    table[3] = {0, 0, 0};
  }
  const std::uint32_t bits = static_cast<std::uint32_t>(block[4]) |
                             static_cast<std::uint32_t>(block[5]) << 8 |
                             static_cast<std::uint32_t>(block[6]) << 16 |
                             static_cast<std::uint32_t>(block[7]) << 24;
  for (int i = 0; i < 16; ++i) {
    const std::uint32_t index = (bits >> (2 * i)) & 3u;
    out_rgb[i] = table[index];
    out_transparent[i] = (!four_color && index == 3);
  }
}

// BC2: sixteen explicit 4-bit alphas in the first eight bytes. This is the
// format the qualified wrapper 0x10002215 actually declares, and the one
// scripts/probe_ntxr_bc.py has been decoding as BC3.
void decode_bc2_alpha(const std::uint8_t* block, std::uint32_t out[16]) noexcept {
  for (int i = 0; i < 16; ++i) {
    const std::uint8_t nibble =
        (i & 1) ? static_cast<std::uint8_t>(block[i / 2] >> 4)
                : static_cast<std::uint8_t>(block[i / 2] & 0x0F);
    out[i] = static_cast<std::uint32_t>(nibble * 255u / 15u);
  }
}

// BC3: two endpoints and 3-bit indices into an interpolated ramp.
void decode_bc3_alpha(const std::uint8_t* block, std::uint32_t out[16]) noexcept {
  const std::uint32_t a0 = block[0];
  const std::uint32_t a1 = block[1];
  std::uint32_t ramp[8] = {a0, a1};
  if (a0 > a1) {
    for (std::uint32_t i = 1; i < 7; ++i) ramp[i + 1] = ((7 - i) * a0 + i * a1) / 7;
  } else {
    for (std::uint32_t i = 1; i < 5; ++i) ramp[i + 1] = ((5 - i) * a0 + i * a1) / 5;
    ramp[6] = 0;
    ramp[7] = 255;
  }
  std::uint64_t bits = 0;
  for (int i = 0; i < 6; ++i) bits |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);
  for (int i = 0; i < 16; ++i) out[i] = ramp[(bits >> (3 * i)) & 7u];
}

}  // namespace

std::uint32_t xenos_tiled_2d_offset(std::uint32_t x, std::uint32_t y,
                                    std::uint32_t pitch_blocks,
                                    std::uint32_t log2_bytes_per_block) noexcept {
  // The documented Xenos Tiled2D layout. Public hardware behaviour, not read
  // out of the retail image - see the header.
  const std::uint32_t outer = (((y >> 5) * (pitch_blocks >> 5) + (x >> 5)) << 6);
  const std::uint32_t inner = (((y >> 1) & 7u) << 3) | (x & 7u);
  const std::uint32_t outer_inner = (outer | inner) << log2_bytes_per_block;
  const std::uint32_t bank = (y >> 4) & 1u;
  const std::uint32_t pipe = ((x >> 3) & 3u) ^ (((y >> 3) & 1u) << 1);
  const std::uint32_t y_lsb = y & 1u;
  return ((y_lsb << 4) | (pipe << 6) | (bank << 11) | (outer_inner & 0xFu) |
          (((outer_inner >> 4) & 1u) << 5) | (((outer_inner >> 5) & 7u) << 8) |
          ((outer_inner >> 8) << 12));
}

namespace {

std::optional<NtxrDescriptor> parse_descriptor_at(
    const std::uint8_t* bytes, const std::size_t size,
    const std::size_t descriptor_base) noexcept {
  constexpr std::size_t kMinimumDescriptorBytes = 0x24U;
  if (bytes == nullptr || descriptor_base > size ||
      kMinimumDescriptorBytes > size - descriptor_base) {
    return std::nullopt;
  }
  NtxrDescriptor descriptor;
  descriptor.mip_count =
      bytes[descriptor_base + kMipCountOffset - kDescriptorBase];
  descriptor.format_code =
      bytes[descriptor_base + kFormatCodeOffset - kDescriptorBase];
  if (descriptor.format_code >= kFormatCodeBound) return std::nullopt;
  descriptor.xenos_format = kXenosFormatTable[descriptor.format_code];
  descriptor.width =
      read_u16(bytes + descriptor_base + kWidthOffset - kDescriptorBase);
  descriptor.height =
      read_u16(bytes + descriptor_base + kHeightOffset - kDescriptorBase);
  descriptor.cube_map =
      (read_u32(bytes + descriptor_base + kFlagsOffset - kDescriptorBase) &
       kCubeMapBit) != 0;
  descriptor.data_offset = read_u32(
      bytes + descriptor_base + kDataOffsetOffset - kDescriptorBase);
  if (descriptor.mip_count > 1U && descriptor_base + 0x38U <= size) {
    descriptor.base_surface_bytes = read_u32(
        bytes + descriptor_base + kBaseSurfaceOffset - kDescriptorBase);
    descriptor.mip_chain_bytes = read_u32(
        bytes + descriptor_base + kMipChainOffset - kDescriptorBase);
  }
  return descriptor;
}

}  // namespace

std::optional<NtxrDescriptor> parse_ntxr_descriptor(const std::uint8_t* bytes,
                                                    std::size_t size) noexcept {
  if (bytes == nullptr || size < kDataOffsetOffset + 4U ||
      read_u32(bytes) != kSignature) {
    return std::nullopt;
  }
  return parse_descriptor_at(bytes, size, kDescriptorBase);
}

std::optional<std::uint32_t> ntxr_gidx_identifier(
    const std::uint8_t* bytes, std::size_t size) noexcept {
  const std::optional<NtxrDescriptor> descriptor =
      parse_ntxr_descriptor(bytes, size);
  if (!descriptor.has_value()) return std::nullopt;
  const std::size_t payload = kDescriptorBase + descriptor->data_offset;
  if (payload > size) return std::nullopt;

  std::optional<std::uint32_t> identifier;
  for (std::size_t offset = kDescriptorBase; offset + 0x20 <= payload;
       offset += 0x10) {
    if (std::memcmp(bytes + offset, "eXt\0", 4) != 0 ||
        read_u32(bytes + offset + 4) != 0x20 ||
        std::memcmp(bytes + offset + 0x10, "GIDX", 4) != 0 ||
        read_u32(bytes + offset + 0x14) != 0x10) {
      continue;
    }
    const std::uint32_t candidate = read_u32(bytes + offset + 0x18);
    if (candidate == 0 || identifier.has_value()) return std::nullopt;
    identifier = candidate;
  }
  return identifier;
}

std::size_t single_level_surface_bytes(const NtxrDescriptor& descriptor) noexcept {
  const std::uint32_t block_bytes = bytes_per_block(descriptor.xenos_format);
  if (block_bytes == 0 || descriptor.width == 0 || descriptor.height == 0) return 0;
  const std::uint32_t blocks_x = pad_to_tile((descriptor.width + 3u) / 4u);
  const std::uint32_t blocks_y = pad_to_tile((descriptor.height + 3u) / 4u);
  return static_cast<std::size_t>(blocks_x) * blocks_y * block_bytes;
}

namespace {

std::optional<DecodedTexture> decode_base_level_at(
    const std::uint8_t* bytes, const std::size_t size,
    const std::size_t descriptor_base, const std::size_t payload_end,
    const bool swap_16, NtxrRefusal* const refusal) noexcept {
  const auto refuse = [&](NtxrRefusal cause) {
    if (refusal != nullptr) *refusal = cause;
    return std::optional<DecodedTexture>{};
  };
  if (refusal != nullptr) *refusal = NtxrRefusal::None;

  const std::optional<NtxrDescriptor> descriptor =
      parse_descriptor_at(bytes, size, descriptor_base);
  if (!descriptor.has_value()) return refuse(NtxrRefusal::BadHeader);

  const std::uint32_t block_bytes = bytes_per_block(descriptor->xenos_format);
  if (block_bytes == 0) return refuse(NtxrRefusal::NotBlockFormat);
  // Cube maps are the only structural refusal left: six faces are not
  // addressed. A mip chain is fine, because only the base level is decoded and
  // the file states where it ends.
  if (descriptor->cube_map) return refuse(NtxrRefusal::CubeMap);

  if (descriptor->data_offset >
      std::numeric_limits<std::size_t>::max() - descriptor_base) {
    return refuse(NtxrRefusal::BadHeader);
  }
  const std::size_t start = descriptor_base + descriptor->data_offset;
  if (start > payload_end || payload_end > size) {
    return refuse(NtxrRefusal::BadHeader);
  }
  const std::size_t payload = payload_end - start;
  const std::size_t expected = single_level_surface_bytes(*descriptor);
  if (expected == 0) return refuse(NtxrRefusal::PayloadSizeMismatch);
  // Two independent statements of the same number, and both are checked.
  //
  //   single level : the payload IS the base surface, by the measured rule.
  //   mip chain    : the file declares the base surface at +0x40 and the chain
  //                  at +0x44, and their sum is the payload. The declared base
  //                  must also equal the measured rule - it does for 360 of 360
  //                  multi-level wrappers, which is the rule and the file
  //                  agreeing from derivations that share nothing.
  if (descriptor->mip_count > 1) {
    if (descriptor->base_surface_bytes != expected) {
      return refuse(NtxrRefusal::PayloadSizeMismatch);
    }
    if (static_cast<std::size_t>(descriptor->base_surface_bytes) +
            descriptor->mip_chain_bytes != payload) {
      return refuse(NtxrRefusal::PayloadSizeMismatch);
    }
  } else if (payload != expected) {
    return refuse(NtxrRefusal::PayloadSizeMismatch);
  }

  const std::uint32_t blocks_x = (descriptor->width + 3u) / 4u;
  const std::uint32_t blocks_y = (descriptor->height + 3u) / 4u;
  const std::uint32_t pitch = pad_to_tile(blocks_x);
  const std::uint32_t log2_block = block_bytes == 8 ? 3u : 4u;

  DecodedTexture out;
  out.width = descriptor->width;
  out.height = descriptor->height;
  out.pixels.assign(static_cast<std::size_t>(out.width) * out.height, 0u);

  std::uint8_t block[16];
  for (std::uint32_t by = 0; by < blocks_y; ++by) {
    for (std::uint32_t bx = 0; bx < blocks_x; ++bx) {
      const std::size_t offset =
          start + xenos_tiled_2d_offset(bx, by, pitch, log2_block);
      if (offset > payload_end || block_bytes > payload_end - offset) {
        return refuse(NtxrRefusal::PayloadSizeMismatch);
      }
      std::memcpy(block, bytes + offset, block_bytes);
      if (swap_16) {
        for (std::uint32_t i = 0; i + 1 < block_bytes; i += 2) {
          const std::uint8_t t = block[i];
          block[i] = block[i + 1];
          block[i + 1] = t;
        }
      }

      Rgb rgb[16];
      bool transparent[16] = {};
      std::uint32_t alpha[16];
      switch (descriptor->xenos_format) {
        case kXenosDxt1:
          decode_color_half(block, /*punchthrough=*/true, rgb, transparent);
          for (int i = 0; i < 16; ++i) alpha[i] = transparent[i] ? 0u : 255u;
          break;
        case kXenosDxt2_3:
          decode_color_half(block + 8, /*punchthrough=*/false, rgb, transparent);
          decode_bc2_alpha(block, alpha);
          break;
        case kXenosDxt4_5:
          decode_color_half(block + 8, /*punchthrough=*/false, rgb, transparent);
          decode_bc3_alpha(block, alpha);
          break;
        default:
          return refuse(NtxrRefusal::NotBlockFormat);
      }

      for (std::uint32_t py = 0; py < 4; ++py) {
        const std::uint32_t y = by * 4 + py;
        if (y >= out.height) break;
        for (std::uint32_t px = 0; px < 4; ++px) {
          const std::uint32_t x = bx * 4 + px;
          if (x >= out.width) break;
          const int texel = static_cast<int>(py * 4 + px);
          out.pixels[static_cast<std::size_t>(y) * out.width + x] =
              pack_rgba(rgb[texel].r, rgb[texel].g, rgb[texel].b, alpha[texel]);
        }
      }
    }
  }
  return out;
}

}  // namespace

std::optional<DecodedTexture> decode_ntxr_base_level(
    const std::uint8_t* bytes, const std::size_t size, const bool swap_16,
    NtxrRefusal* const refusal) noexcept {
  if (bytes == nullptr || size < kDataOffsetOffset + 4U ||
      read_u32(bytes) != kSignature) {
    if (refusal != nullptr) *refusal = NtxrRefusal::BadHeader;
    return std::nullopt;
  }
  return decode_base_level_at(bytes, size, kDescriptorBase, size, swap_16,
                              refusal);
}

std::optional<DecodedTexture> decode_ntxr_pack_texture(
    const std::uint8_t* bytes, const std::size_t size,
    const std::uint32_t identifier, const bool swap_16,
    NtxrRefusal* const refusal) noexcept {
  const auto refuse = [&](const NtxrRefusal cause) {
    if (refusal != nullptr) *refusal = cause;
    return std::optional<DecodedTexture>{};
  };
  if (refusal != nullptr) *refusal = NtxrRefusal::None;
  constexpr std::size_t kSiblingStride = 0x50U;
  constexpr std::uint16_t kMaximumTextures = 4096U;
  if (bytes == nullptr || identifier == 0U ||
      size < kDescriptorBase + 0x24U || read_u32(bytes) != kSignature) {
    return refuse(NtxrRefusal::BadHeader);
  }
  const std::uint16_t texture_count = read_u16(bytes + 0x06U);
  if (texture_count == 0U || texture_count > kMaximumTextures) {
    return refuse(NtxrRefusal::BadHeader);
  }
  const std::size_t section_offset = read_u16(bytes + kDescriptorBase + 0x0CU);
  if (section_offset > std::numeric_limits<std::size_t>::max() -
                           kDescriptorBase) {
    return refuse(NtxrRefusal::BadHeader);
  }
  const std::size_t section_base = kDescriptorBase + section_offset;
  if (section_base < kDescriptorBase + kSiblingStride ||
      static_cast<std::size_t>(texture_count - 1U) >
          (std::numeric_limits<std::size_t>::max() - section_base) /
              kSiblingStride) {
    return refuse(NtxrRefusal::BadHeader);
  }
  const std::size_t terminator_base =
      section_base + static_cast<std::size_t>(texture_count - 1U) *
                         kSiblingStride;
  if (terminator_base > size || 0x24U > size - terminator_base ||
      bytes[terminator_base + 0x11U] != 0U ||
      read_u16(bytes + terminator_base + 0x14U) != 0U ||
      read_u16(bytes + terminator_base + 0x16U) != 0U) {
    return refuse(NtxrRefusal::BadHeader);
  }

  struct PackEntry final {
    std::uint32_t identifier{};
    std::size_t descriptor_base{};
    std::size_t data_begin{};
    std::size_t data_end{};
  };
  std::vector<PackEntry> entries;
  entries.reserve(texture_count);
  std::optional<std::size_t> selected;
  for (std::uint16_t index = 0U; index < texture_count; ++index) {
    const std::size_t descriptor_base =
        index == 0U
            ? kDescriptorBase
            : section_base + static_cast<std::size_t>(index - 1U) *
                                 kSiblingStride;
    const std::size_t next_descriptor =
        section_base + static_cast<std::size_t>(index) * kSiblingStride;
    if (next_descriptor < 0x20U || next_descriptor > size ||
        std::memcmp(bytes + next_descriptor - 0x20U, "eXt\0", 4U) != 0 ||
        read_u32(bytes + next_descriptor - 0x1CU) != 0x20U ||
        std::memcmp(bytes + next_descriptor - 0x10U, "GIDX", 4U) != 0 ||
        read_u32(bytes + next_descriptor - 0x0CU) != 0x10U) {
      return refuse(NtxrRefusal::BadHeader);
    }
    const std::uint32_t candidate =
        read_u32(bytes + next_descriptor - 0x08U);
    if (candidate == 0U ||
        std::any_of(entries.begin(), entries.end(),
                    [candidate](const PackEntry& entry) {
                      return entry.identifier == candidate;
                    })) {
      return refuse(NtxrRefusal::BadHeader);
    }
    const std::optional<NtxrDescriptor> descriptor =
        parse_descriptor_at(bytes, size, descriptor_base);
    if (!descriptor.has_value() || descriptor->mip_count != 1U) {
      return refuse(NtxrRefusal::BadHeader);
    }
    const std::size_t payload = single_level_surface_bytes(*descriptor);
    if (payload == 0U || descriptor->data_offset >
                             std::numeric_limits<std::size_t>::max() -
                                 descriptor_base) {
      return refuse(NtxrRefusal::NotBlockFormat);
    }
    const std::size_t data_begin = descriptor_base + descriptor->data_offset;
    if (data_begin > size || payload > size - data_begin) {
      return refuse(NtxrRefusal::PayloadSizeMismatch);
    }
    entries.push_back(
        {candidate, descriptor_base, data_begin, data_begin + payload});
    if (candidate == identifier) {
      if (selected.has_value()) return refuse(NtxrRefusal::BadHeader);
      selected = entries.size() - 1U;
    }
  }
  for (std::size_t index = 0U; index + 1U < entries.size(); ++index) {
    if (entries[index].data_end != entries[index + 1U].data_begin) {
      return refuse(NtxrRefusal::PayloadSizeMismatch);
    }
  }
  const std::size_t logical_end = entries.back().data_end;
  if (logical_end > size ||
      !std::all_of(bytes + logical_end, bytes + size,
                   [](const std::uint8_t value) { return value == 0U; })) {
    return refuse(NtxrRefusal::PayloadSizeMismatch);
  }
  if (!selected.has_value()) return refuse(NtxrRefusal::IdentifierNotFound);
  const PackEntry& entry = entries[*selected];
  return decode_base_level_at(bytes, size, entry.descriptor_base,
                              entry.data_end, swap_16, refusal);
}

}  // namespace ac6::retail
