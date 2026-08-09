#pragma once

// The map's part placement list -- retail's `.pdl` -- decoded to world space.
//
// WHERE THE FIELD COMES FROM. `0x820FBC28` is `CMapManager`'s loader. It formats
// a name, calls `0x82101A18(this, name, &size)`, and stores the pointer; the
// names sit at `0x8205BDA8` onward and give every field its extension:
//
//   +0x0C .mha   +0x10 .mhd   +0x14 .mia   +0x18 .mii   +0x1C .mid
//   +0x20 .mta   +0x24 .mti   +0x28 .pdl   +0x2C .edl
//   +0x34 .mca   +0x38 .mci   +0x3C .mcd
//
// So this is `+0x28`, and its size is `+0x5C`. `0x820FBF5C` compares that size
// against 4 and frees the block when it matches: **a four-byte file is how
// retail spells "empty"**, which is why `open` refuses one rather than
// reporting zero instances.
//
// WHERE THE LAYOUT COMES FROM. `0x82102148` consumes the field:
//
//   0x8210217C  lwz    r8,0x28(r31)     the list
//   0x82102198  lwz    r11,0x5C(r31)    its size, bail if <= 4
//   0x821021A4  srawi  r8,r9,4          coarse x, bounds-checked 0..15
//   0x821021D0  add    r6,r11,r8        index = coarse_z * 16 + coarse_x
//   0x82102234  rlwinm r9,r6,4,0,27     index * 16
//   0x82102240  add    r9,r9,r8         -> the list plus index * 16
//
// A SIXTEEN-BYTE HEADER RECORD PER COARSE CELL, 256 of them, and the body
// follows. The record's first word is a count and its second an absolute byte
// offset; that reading is not assumed, it is CHECKED -- `header_partitions_body`
// requires the 256 counts to sum to exactly `(size - 4096) / 16`, and `open`
// refuses the file when they do not. For Mission 01 both sides are 4318.
//
// WHERE THE TRANSFORM COMES FROM. Instance positions are local and run to
// +/-4096, and `0x82102148` converts a coarse index to world in three
// instructions:
//
//   0x8210220C  rlwinm r9,r8,13,0,18    coarse * 8192
//   0x82102214  ori    r11,r11,0xF000   61440
//   0x82102220  subf   r9,r11,r9        coarse * 8192 - 61440
//
// `61440` is `65536 - 4096`: the world bias of `0x82069BB8` less half a coarse
// cell. The local origin is therefore the cell's CENTRE, and cycle 1447 inferred
// that from the +/-4096 range before this instruction was read.
//
// `tag >> 16` IS AN ANGLE, and cycle 1451 measured it rather than assuming it.
// Reading the u16 as a fraction of a turn, the circular resultant of four times
// the angle over all 4318 instances is
//
//     R(1t) 0.7035   R(2t) 0.7020   R(3t) 0.6913
//     R(4t) 0.9757   R(5t) 0.6822   R(6t) 0.6839
//
// The 4th harmonic and only the 4th. Against two null models -- 2000 uniform
// random fields, and 2000 reshufflings that keep the multiset structure and
// randomise only the angles -- **zero** reached 0.9757, the best uniform trial
// being 0.0423. The three populated orientations sit at 79.08, 169.16 and
// 259.22 degrees: gaps of 90.074 and 90.061. That is a right-angle street grid
// rotated 79.33 degrees, which is what a city is and what a random field is not.
// `four_fold_resultant` re-runs it so the test asserts it.
//
// WHAT IS STILL NOT CLAIMED. The **sign** is not established. Both `+theta` and
// `-theta` produce a 4-fold set, so the distribution cannot choose between them,
// and cycle 1451 rendered the city three ways -- unrotated, `+`, `-` -- and the
// pictures did not discriminate either. Nor is the scale forced: any reading
// under which the data is 4-fold works, and "65536 is one turn" is the standard
// convention rather than a derivation. So `tag_high` stays raw and no rotation
// is applied anywhere in the product.
//
// `part_id` -- `tag & 0xFFFF` -- is claimed only as
// an identifier in 0..172 against 178 parts; which part each id names is not
// established, and neither is the third word of the header record.
//
// THE CONTROLS live in the test, because a placement is only as good as what it
// lands on. `retail_map_placement_tests` puts every instance on the ported
// heightfield of `retail_terrain_field.h` -- a different file, read by a
// different retail function -- and requires the ground under them to be flat far
// more often than under a random scatter of the same size. A wrong header
// reading scatters, and a scatter scores the null model.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

// 0x821021A4/0x821021C4: the coarse index is 16 x 16, bounds-checked.
inline constexpr std::size_t kPlacementCoarseSide = 16;
// 0x82102234: index * 16
inline constexpr std::size_t kPlacementRecordBytes = 16;
inline constexpr std::size_t kPlacementHeaderBytes =
    kPlacementCoarseSide * kPlacementCoarseSide * kPlacementRecordBytes;
// 0x8210220C: coarse * 8192
inline constexpr float kPlacementCoarseUnits = 8192.0F;
// 0x82102214/0x82102220: 0xF000
inline constexpr float kPlacementOriginBias = 61440.0F;
// 0x820FBF5C: a four-byte file is empty
inline constexpr std::size_t kPlacementEmptySize = 4;

struct MapInstance {
  float world_x = 0.0F;
  float world_y = 0.0F;
  float world_z = 0.0F;
  std::uint16_t part_id = 0;      // tag & 0xFFFF
  std::uint16_t tag_high = 0;     // NOT named; see the header comment
  std::uint8_t coarse_x = 0;
  std::uint8_t coarse_z = 0;
};

struct PlacementCell {
  std::uint32_t count = 0;
  std::uint32_t offset = 0;
  std::uint32_t third = 0;        // not established
};

class MapPlacement {
 public:
  static std::optional<MapPlacement> open(const std::uint8_t* pdl, std::size_t size);

  const PlacementCell& cell(std::size_t coarse_x, std::size_t coarse_z) const;
  const std::vector<MapInstance>& instances() const { return instances_; }

  // 0x8210220C..0x82102220, with the local offset added.
  static float world_from_local(std::size_t coarse, float local) {
    return static_cast<float>(coarse) * kPlacementCoarseUnits - kPlacementOriginBias
           + local;
  }

  // The circular resultant of `harmonic * theta`, reading tag_high as a
  // fraction of a turn. 1.0 is perfect alignment, 0.0 is uniform.
  double four_fold_resultant(int harmonic = 4) const;

  std::size_t header_total() const { return header_total_; }
  std::size_t body_records() const { return body_records_; }
  bool header_partitions_body() const { return header_total_ == body_records_; }

 private:
  std::vector<PlacementCell> header_;
  std::vector<MapInstance> instances_;
  std::size_t header_total_ = 0;
  std::size_t body_records_ = 0;
};

}  // namespace ac6::retail
