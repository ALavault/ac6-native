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
// THE TAG IS FOUR FIELDS, read out of `0x82102340`..`0x82102378` at cycle 1452:
//
//   bits  0..15   a 16-bit value, 0..172 over Mission 01
//   bits 16..24   NINE bits, 8..169, 160 distinct
//   bits 25..26   zero on every accepted record
//   bits 27..29   `(tag >> 27) & 7`; retail continues ONLY on 0 or 7
//   bits 30..31   two bits, counts 345 / 584 / 3277 / 20
//
// `0x82102340` masks three bits and `0x82102364` masks NINE -- `rlwinm
// r29,r30,16,23,31` -- so the high half is not one number. 4,226 of 4,318
// records carry 7 in the three-bit field and 92 carry 1, 2, 5 or 6, and
// `0x82102350` SKIPS those 92.
//
// CORRECTING CYCLE 1451, WHICH GOT THE MECHANISM WRONG. That cycle measured the
// circular resultant of `tag >> 16` read as a fraction of a turn and found
// R(4t) = 0.9757 against 0.68-0.70 elsewhere, with zero of 2000 trials reaching
// it under either null model. The statistic is real and reproducible. Its
// explanation is not an angle: **the four-fold structure is the two-bit field in
// bits 30..31**, which moves `tag >> 16` by exactly 0x4000 -- a quarter turn in
// that reading -- per step. Four values of two bits, spaced 90 degrees by
// construction. The "street grid rotated 79.33 degrees" was the average of the
// remaining fourteen bits and means nothing.
//
// It is the shape cycle 1440 warned about: a statistic with a null model can
// still be explained by the wrong mechanism, and the fix was not a better null
// but reading the instructions that consume the field. The water-overhang test
// cycle 1451 proposed as the next step was built and REFUTED as an instrument in
// the same cycle -- a random-angle null scored better than either sign -- which
// is what sent the search back to the code.
//
// `four_fold_resultant` is kept because the number is true and the test asserts
// it; what it measures is a two-bit field, and this comment is the difference.
//
// WHAT THE NINE-BIT FIELD IS FOR. `0x82102378` passes it as `r4` to vtable slot
// `+0x5C` (`0x82100600`), which is `this->table[0x1B63 + index]` bounds-checked
// against the count at `this+0x74` -- a resource table, and `parts/%d` is in the
// same string table as the `.pdl` name. So the NINE-BIT FIELD is retail's part
// selector. `tag & 0xFFFF` is extracted separately at `0x821023B4` and used for
// something else.
//
// THE NINE-BIT FIELD IS THE MODEL INDEX, settled at cycle 1454 by the loader
// rather than by looking. `0x820FC340`..`0x820FC42C` is a 256-iteration loop:
//
//   0x820FC334  r19 = 0x8205BE24 = "parts/%d"
//   0x820FC34C  sprintf(buf, "parts/%d", i)
//   0x820FC36C  load(buf under the map path)  -> this[0x40B0 + i*4]
//   0x820FC3A4  r20 = 0x8205BFD0 = "%s.nud"
//   0x820FC3C8  load("parts/%d.nud")          -> this[0x6D8C + i*4]
//   0x820FC404  ++this[0x74] when that load succeeded
//   0x820FC428  while i < 0x100
//
// `this+0x6D8C` is exactly the table vtable slot `+0x5C` (`0x82100600`) indexes,
// so the nine-bit field is the `parts/%d` number. Three independent facts agree:
//
//   - the loader keys both tables by that index;
//   - the container holds **170** `NDXR` entries, ordinals 0..169, and 86
//     `unknown.bin` after them -- and the nine-bit field runs **8..169**,
//     entirely inside the models;
//   - `tag & 0xFFFF` reaches **170, 171 and 172**, which are not models.
//
// So `tag & 0xFFFF` is NOT a model id, and cycle 1453's picture -- which leaned
// the other way because the nine-bit render "looked wrong" -- was decided by
// eye against three things that can be read. The pair of the two fields is still
// unique per instance (4,226 instances, 27,680 possible pairs, zero collisions
// where chance predicts about 323), so the low sixteen bits carry something; what
// they carry is unread.
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
  std::uint16_t part_id = 0;      // tag & 0xFFFF -- NOT the model; see above
  std::uint16_t tag_high = 0;     // the whole high half, kept for the statistic
  std::uint16_t selector = 0;     // (tag >> 16) & 0x1FF -- the parts/%d index
  std::uint8_t quadrant = 0;      // (tag >> 30) & 3
  std::uint8_t kind = 0;          // (tag >> 27) & 7; retail accepts 0 and 7
  bool accepted = false;          // 0x82102344..0x82102350
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
