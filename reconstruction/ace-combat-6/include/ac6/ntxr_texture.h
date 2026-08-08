#pragma once

// The NTXR texture decoder, restricted to single-level block textures.
//
// The restriction is the point. This decoder refuses everything it cannot
// address correctly, and the boundary is drawn where the evidence stops, not
// where the code gets inconvenient.
//
// ---------------------------------------------------------------------------
// The descriptor, derived (cycle 1150)
// ---------------------------------------------------------------------------
//
// 0x8233EA78 is the consumer. It hands the descriptor to 0x8234B360, which is
// the whole field extraction:
//
//     8234b360  lhz    r11,0x14(r3)     ; -> out slot at r1+0x54
//     8234b368  lhz    r11,0x16(r3)     ; -> out slot at r1+0x50
//     8234b370  lbz    r11,0x13(r3)     ; the format code
//     8234b374  cmplwi cr6,r11,0x2f     ; >= 47 -> return 0, the load fails
//     8234b380  rlwinm r11,r11,0x3,...  ; * 8, an 8-byte table
//     8234b384  addi   r10,r10,0x67c0   ; the table at 0x826767C0
//     8234b38c  lwzx   r11,r11,r10      ; -> out slot at r1+0x58
//
// and to two one-line accessors:
//
//     8234b128  lbz  r3,0x11(r3)                        ; the mip count
//     8234b118  lwz  r11,0x1c(r3)                       ; the cube-map flag,
//               rlwinm r3,r11,0x17,0x1f,0x1f            ; bit 9 of that word
//
// 0x8234AA68, the base constructor, fixes which is which. It stores the
// r1+0x54 value at object+0x0C and the r1+0x50 value at object+0x10, and
// 0x8234EC38 passes object+0x0C as the first argument of 0x821FBE30 and
// object+0x10 as the second. That ordering - and nothing about the corpus -
// is why +0x14 is the width and +0x16 the height. Cycle 1149 tried to settle
// it by size arithmetic and could not: pad32(a)*pad32(b) is symmetric.
//
// The table at 0x826767C0 holds exactly 47 entries of 8 bytes, which is the
// bound the code checks. Word 0's low six bits are the Xenos TextureFormat,
// and 0x8234EC38 masks exactly those six bits (rlwinm r30,r30,0,0x1a,0x1f)
// before switching on them. kXenosFormat below is that column, read from the
// image and not inferred.
//
// ---------------------------------------------------------------------------
// The data offset, measured with a control rather than derived
// ---------------------------------------------------------------------------
//
// The pixel pointer does NOT come from the descriptor in retail: it reaches
// 0x8233EA78 as a separate argument and is stored at object+0x08, so the file
// offset is a container convention this port has not read out of the image.
//
// It is the word at file +0x30, and the evidence is a control. Using it, the
// single-level surface rule below holds for 308 of 308 wrappers; using the
// word at +0x28, +0x2C, +0x34, +0x38 or +0x3C it holds for 0 of 308. A wrong
// offset cannot produce 308 exact size matches across 26 distinct
// non-power-of-two shapes.
//
// ---------------------------------------------------------------------------
// The surface rule, measured (cycle 1151)
// ---------------------------------------------------------------------------
//
//     payload = pad32(ceil(W/4)) * pad32(ceil(H/4)) * bytes_per_block
//
// pad32 rounding up to the 32-block Xenos tile. 308 of 308 single-level block
// wrappers match exactly, over 38 shapes of which 26 are non-power-of-two -
// and the odd shapes are what make it a test, because on a power of two pad32
// is a no-op and the rule cannot fail.
//
// It is also derived (cycle 1156). 0x821FBE30 is XGSetTextureHeader: it
// allocates nothing and returns the size the caller must allocate. Two frames
// down, 0x821DF838 computes a level's surface -
//
//     821df868  li r25,0x20                    ; X align = 32 blocks
//     821df870  addi r24,r11,0x1               ; Y align = 32 block-rows
//     821df8b8  divwu r9,r8,r9                 ; untiled: 256 / bytesPerBlock
//     821df8c0  blt cr6,0x821df8c8             ;   floored at 32
//     821df8e0  andc r11,r9,r11                ; *W = roundUp(W, blockW * xAlign)
//     821df900  andc r11,r11,r9                ; *H = roundUp(H, blockH * 32)
//     821df928  rlwinm r10,r10,0x1d,0x3,0x1f   ; pitch = alignedW * bpt / 8
//     821df92c  mullw r10,r10,r9               ;   * alignedH
//     821df938  rlwinm r10,r10,0x0,0x0,0x13    ; round up to 4096
//
// - and for BC1 the untiled floor is 256/8 = 32 while for BC2/BC3 it is 16
// floored to 32, so X is 32 blocks either way. 0x821DF958 then chooses among
// four base-size formulas; this corpus takes the tiled one,
// roundUp(pitch * alignedH, 4096), confirmed on four shapes where the tiled and
// untiled answers differ by 6 to 20 percent.
//
// This decoder asserts that identity before it decodes anything, so a wrapper
// whose payload disagrees is refused rather than mis-addressed.
//
// ---------------------------------------------------------------------------
// What is NOT derived, stated plainly
// ---------------------------------------------------------------------------
//
// * The Tiled2D address swizzle is the documented Xenos layout, taken from
//   public hardware documentation by way of Xenia's texture_address::Tiled2D.
//   It is not read out of this image and is not claimed to be.
// * BC1/BC2/BC3 block decoding is the standard S3TC/DXT definition.
// * The 8-in-16 byte swap is the endianness of the stored blocks. The control
//   on record is negative and visual - omitting it corrupts colours - which is
//   weaker than the rest of this file and is why decode() exposes it as an
//   explicit argument instead of hiding it.
//
// ---------------------------------------------------------------------------
// What this decoder refuses
// ---------------------------------------------------------------------------
//
// Mip chains. Cycle 1151 measured the multi-level population and no per-level
// padding rule reproduces it: 512x512 with 10 levels holds 393,216 bytes where
// the naive tile model predicts 458,752, and 256x256 with 9 levels holds
// 131,072 against 196,608. The tails are exact powers of two, so there is a
// fixed granularity below some threshold, but it is not established. Mission
// 01's atlases are the single-level population - 308 of 670 block wrappers,
// including every non-power-of-two UI and cockpit shape - so this restriction
// costs nothing there and would cost the 4096x4096 terrain mip tails, which is
// exactly where JG will look.
//
// Cube maps, non-block formats, and any wrapper whose payload does not match
// the surface rule are refused for the same reason.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

// Low six bits of word 0 of each entry of the table at 0x826767C0, read from
// the image. Index is the descriptor's format code at +0x13; the code checks
// it against 47, which is exactly the number of entries.
inline constexpr std::uint8_t kXenosFormatTable[47] = {
    0x12, 0x13, 0x14, 0x31, 0x3c, 0x02, 0x02, 0x04, 0x05, 0x03, 0x03,
    0x0f, 0x0f, 0x0f, 0x0a, 0x0a, 0x0a, 0x18, 0x1e, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x36, 0x36, 0x19, 0x19, 0x19, 0x37, 0x38, 0x37,
    0x38, 0x1f, 0x21, 0x24, 0x1a, 0x1a, 0x20, 0x22, 0x22, 0x22, 0x25,
    0x23, 0x23, 0x26,
};

// The three Xenos block formats this decoder handles, as the low six bits of
// the table word. 0x8234EC38 switches on exactly this field.
inline constexpr std::uint8_t kXenosDxt1 = 0x12;   // k_DXT1,   BC1
inline constexpr std::uint8_t kXenosDxt2_3 = 0x13; // k_DXT2_3, BC2
inline constexpr std::uint8_t kXenosDxt4_5 = 0x14; // k_DXT4_5, BC3

struct NtxrDescriptor {
  std::uint16_t width{};        // file +0x24, via lhz descriptor+0x14
  std::uint16_t height{};       // file +0x26, via lhz descriptor+0x16
  std::uint8_t format_code{};   // file +0x23, < 47
  // file +0x21. 0x8234B128 reads it and 0x8234AED8 tests it against 1 to
  // pick the plain context over the mip-mapped one. That it counts levels
  // is a NAME, not a derivation (cycle 1153): nothing in the image loops
  // over it. This decoder only ever compares it to 1, which is the part
  // that is derived.
  std::uint8_t mip_count{};
  bool cube_map{};              // file +0x2C bit 9
  std::uint32_t data_offset{};  // file +0x30, relative to file 0x10
  std::uint8_t xenos_format{};  // kXenosFormatTable[format_code]
  // File +0x40 and +0x44, present only when mip_count > 1. 0x8234B268 sums a
  // dword-per-level array at descriptor +0x30 (file +0x40) to find each level's
  // source offset, and 0x821FBC30 returns roundup4096(base) + chain as the
  // total. Measured: base + chain == the payload for 360 of 360 multi-level
  // wrappers. When mip_count == 1 there is no array - file +0x40 holds the
  // ASCII "eXt\0" chunk signature instead - and both stay zero.
  std::uint32_t base_surface_bytes{};
  std::uint32_t mip_chain_bytes{};

  bool operator==(const NtxrDescriptor&) const = default;
};

// Parses the header. Fails when the signature is wrong, the file is short, or
// the format code is out of the range the retail bound allows.
std::optional<NtxrDescriptor> parse_ntxr_descriptor(const std::uint8_t* bytes,
                                                    std::size_t size) noexcept;

// Bytes a single mip level occupies, by the measured rule. Zero when the
// format is not one this decoder addresses.
std::size_t single_level_surface_bytes(const NtxrDescriptor& descriptor) noexcept;

// One decoded surface, straight RGBA8 in row-major order, no tiling left.
struct DecodedTexture {
  std::uint32_t width{};
  std::uint32_t height{};
  std::vector<std::uint32_t> pixels;  // 0xAABBGGRR, one per texel
};

// Why a wrapper was refused, so a caller can count the refusals by cause
// instead of reporting a bare failure.
enum class NtxrRefusal {
  None,
  BadHeader,
  NotBlockFormat,
  CubeMap,
  PayloadSizeMismatch,
};

// Decodes the **base level** of a block texture, whether or not it carries a
// mip chain. Levels above 0 are not decoded: 0x8234B268 locates them by summing
// the per-level array, which this port has read but not validated, and nothing
// downstream consumes them yet.
//
// `swap_16` applies the 8-in-16 byte swap; it is an argument and not a constant
// because its evidence is visual, unlike everything else here.
std::optional<DecodedTexture> decode_ntxr_base_level(const std::uint8_t* bytes,
                                                     std::size_t size, bool swap_16,
                                                     NtxrRefusal* refusal) noexcept;

// The Xenos Tiled2D block address, in bytes, for a block at (x, y) of a
// surface whose pitch is `pitch_blocks`. Public hardware layout, not derived
// from this image.
std::uint32_t xenos_tiled_2d_offset(std::uint32_t x, std::uint32_t y,
                                    std::uint32_t pitch_blocks,
                                    std::uint32_t log2_bytes_per_block) noexcept;

}  // namespace ac6::retail
