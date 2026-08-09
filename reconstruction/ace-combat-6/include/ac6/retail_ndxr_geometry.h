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
// `index_offset` is a BYTE offset; `>> 1` is the StartIndex 0x823648C4 computes.
// Both arrays accumulate across a container's descriptors -- descriptor 1's
// vertex_offset is exactly descriptor 0's vertex bytes.
//
// 0xFFFF IS A STRIP RESTART, not an index. Cycle 1426's range tests failed three
// times over rejecting it; the sixteen u16s that settled it read
// `23 24 22 25 65535 56 54 55`. It is preserved in the index list rather than
// resolved here, because a caller drawing strips needs to see the break.
//
// WHAT CORROBORATES THE SECTION ASSIGNMENT. 0x82362190 binds exactly two
// buffers, from `[r31+116]` and `[r31+120]` where `r31` is the container plus
// 0x10 -- so those are sections.first and sections.second, the same two the
// arbitration chose between. The binder confirms there are two and which
// fields hold them; the arbitration says which is vertices and which is
// indices. Neither alone would have been enough and the pair is recorded that
// way rather than as a single reading.
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
