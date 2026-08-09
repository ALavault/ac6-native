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
// WHAT THIS DOES NOT DECODE, and it is most of each vertex. The strides are 28
// and 32 bytes; this reads the first twelve as three big-endian floats and
// discards the rest. The element layouts live behind T8's and T18's
// `const elems*` pointers, which are not read -- so texture coordinates,
// normals and anything else are NOT here. Cycle 1425 declared this cost when
// the entry point's shape was settled; this is where it is paid.
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

struct NdxrMesh {
  std::vector<NdxrPosition> positions;
  std::vector<std::uint16_t> indices;  // 0xFFFF preserved
  NdxrBounds bounds{};
};

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
