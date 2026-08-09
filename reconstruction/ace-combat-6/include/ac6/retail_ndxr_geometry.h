#pragma once

// One NDXR polygon descriptor, decoded into positions and a strip index list.
//
// WHERE EVERY NUMBER COMES FROM. The addressing below was arbitrated by
// cross-match over all 1,227 descriptors of Mission 01's model package at cycle
// 1426, not assumed:
//
//   vertices  sections.second + vertex_offset      1227/1227 plausible
//             (sections.first 974, the file base 907)
//   indices   sections.first  + index_offset       1227/1227 in range,
//             u16, RELATIVE to this descriptor's own vertices
//             (the absolute reading scored 292 and is refused)
//   stride    NdxrContainer::VertexStride, T8[hi] + T18[lo], ported at 1217
//             from 0x82345100; zero for a code outside either table, and no
//             descriptor in the package produces zero
//
// The descriptor itself is `NdxrDescriptor`, ported from the draw at 0x82364518
// -- the loader never touches geometry, which is why cycle 1212 had to read the
// field mapping out of the draw rather than out of the loader.
//
// `index_offset` is a BYTE offset; `>> 1` is the StartIndex 0x823648C4 computes.
// Both arrays accumulate across a container's descriptors -- descriptor 1's
// vertex_offset is exactly descriptor 0's vertex bytes.
//
// 0xFFFF IS A STRIP RESTART, not an index. Cycle 1426's range tests failed three
// times over rejecting it; the sixteen u16s that settled it read
// `23 24 22 25 65535 56 54 55`. It is preserved in the index list rather than
// resolved here, because a caller drawing strips needs to see the break.
//
// WHAT CORROBORATES THE SECTION ASSIGNMENT, and cycle 1432 closed it.
// 0x82362190 binds exactly two buffers, from `[r31+116]` and `[r31+120]` where
// `r31` is the container plus 0x10 -- so those are sections.first and
// sections.second, the same two the arbitration chose between. And the two
// creators are not interchangeable:
//
//   sections.first  -> 0x821FBB10, which writes 2 at descriptor+0 and packs a
//                      3-bit field from its r5 (the call site passes 1) --
//                      the shape of an INDEX FORMAT selector
//   sections.second -> 0x821FBA78, which writes 1 at descriptor+0 and packs a
//                      masked address plus format bits into +28
//
// So retail creates two DIFFERENT resource types, one per section, in the
// order the arbitration assigns them. That the type words 2 and 1 are
// D3DRTYPE_INDEXBUFFER and D3DRTYPE_VERTEXBUFFER is a reading of an external
// convention and is NOT derived here; what is derived is that the two sections
// take two different creators, and which section takes which.
//
// The pair is what carries the claim: 1227/1227 by cross-match, and a binder
// that makes two distinct typed resources from exactly those two fields.
//
// THE REST OF THE VERTEX, read at cycle 1433. T8's and T18's records are
// `{u16 stride; u16 count; const elems*}` and the elements are the Xenon
// `D3DVERTEXELEMENT9`, twelve bytes each with a DWORD type:
//
//   T8[6]  @ 0x8201140C   offset  0  usage 0  POSITION
//                         offset 12  usage 3  NORMAL
//   T18[1] @ 0x820111D8   offset  0  usage 5  TEXCOORD0        -> stride 28
//   T18[3] @ 0x820111FC   offset  0  usage 10 COLOR
//                         offset  4  usage 5  TEXCOORD0        -> stride 32
//
// The T18 elements are appended after T8's stride, so for stride 28 the layout
// is POSITION at 0, NORMAL at 12, TEXCOORD0 at 20.
//
// THE COMPONENT TYPES ARE MEASURED, NOT DECODED FROM THE TYPE WORD. The Xenos
// format codes (0x002A23B9, 0x001A2360, 0x002C23A5) are not decoded here; what
// each field is was settled by reading real vertices:
//
//   NORMAL    four float16 -- the first three are a UNIT vector to three
//             decimals over the whole package, and the fourth is 1.0
//   TEXCOORD  two big-endian float32, and they land in [0, 1]
//
// A wrong reading fails both tests loudly: bytes read as the wrong type do not
// come out unit-length. The unit-length property is asserted as a control in
// the tests rather than trusted.
//
// COLOR at stride 32 is four bytes and is NOT decoded -- eight descriptors of
// 1227 carry it and nothing here needs it yet.
//
// The three floats being positions is itself evidence rather than assumption:
// they are finite and inside a model-sized box for 1,227 of 1,227 descriptors,
// which is what chose sections.second over the two alternatives.

#include "ac6/retail_ndxr_container.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

struct NdxrPosition {
  float x{};
  float y{};
  float z{};
  bool operator==(const NdxrPosition&) const = default;
};

struct NdxrBounds {
  float min_x{};
  float min_y{};
  float min_z{};
  float max_x{};
  float max_y{};
  float max_z{};
  bool valid{};
};

struct NdxrTexcoord {
  float u{};
  float v{};
  bool operator==(const NdxrTexcoord&) const = default;
};

struct NdxrMesh {
  std::vector<NdxrPosition> positions;
  // Parallel to `positions`. Empty when the stride has no room for them.
  std::vector<NdxrPosition> normals;
  std::vector<NdxrTexcoord> texcoords;
  std::vector<std::uint16_t> indices;  // 0xFFFF preserved
  NdxrBounds bounds{};
};

// Where each field sits, for the two strides Mission 01's package uses.
inline constexpr std::size_t kNormalOffset = 12;
inline constexpr std::size_t kTexcoordOffset28 = 20;
inline constexpr std::size_t kTexcoordOffset32 = 24;

// The value that breaks a strip rather than addressing a vertex.
inline constexpr std::uint16_t kStripRestart = 0xFFFF;

// `bytes`/`size` are the container's own span -- the same ones passed to
// `NdxrContainer::Open`, because the container keeps them private.
// Returns nullopt when the stride is zero, when either span leaves the
// container, or when an index addresses past the descriptor's vertices.
std::optional<NdxrMesh> decode_ndxr_descriptor(const NdxrContainer& container,
                                               const std::uint8_t* bytes,
                                               std::size_t size,
                                               const NdxrDescriptor& descriptor) noexcept;

}  // namespace ac6::retail
