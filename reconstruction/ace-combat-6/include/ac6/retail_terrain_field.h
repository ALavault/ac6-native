#pragma once

// The map's terrain heightfield, and the segment test retail runs against it.
//
// WHERE EVERY NUMBER COMES FROM. `0x82102568` is `CMapManager`'s
// segment-versus-terrain test, and it was read instruction by instruction out
// of the flat image at cycle 1445 -- NOT out of `exports/82102568.json`, whose
// decompilation carries "Control flow encountered bad instruction data" and
// thirty-three removed blocks and is a paraphrase of a read that never
// finished.
//
//   0x82102590  lwz    r6,0xC(r24)     the 16 x 16 grid of patch ids
//   0x821025A0  srawi  r5,r9,4         coarse x = cell >> 4, checked 0..15
//   0x821025A4  srawi  r11,r10,4       coarse z, likewise
//   0x821025C8  rlwinm r23,r10,0,28,31 fine z = cell & 15
//   0x821025D0  rlwinm r22,r9,0,28,31  fine x
//   0x821025F8  add    r21,r11,r5      index = coarse_z * 16 + coarse_x
//   0x8210261C  lwz    r8,0x10(r24)    the patch array
//   0x8210263C  lbzx   r10,r6,r21      patch id, one byte
//   0x82102648  mulli  r10,r10,0x4204  * 16900 bytes per patch
//   0x82102700  rlwinm r8,r11,6,0,25   \ r11 = fine_z * 260
//   0x8210270C  add    r11,r8,r11      /
//   0x82102710  add    r11,r11,r9      + fine_x * 4
//   0x821027C0  addi   r10,r10,0x104   the sampler's row step
//
// `0x104` is 260 bytes = 65 floats, and 65 * 260 = 16900 = `0x4204`. So a patch
// is 65 x 65 floats: sixteen cells of four samples, plus the shared edge. One
// cell is 512 world units -- `0x82069BB4` is `0.001953125` and `0x82069BB8` is
// `65536.0`, both read at cycle 1442 -- so ONE SAMPLE IS 128 WORLD UNITS and
// 16 * 16 * 64 + 1 samples span exactly 131,072, which is the +/-65,536 the
// query's bias implies. Nothing here was fitted; the span fell out.
//
// THE CONTROL, and it is why this is a reading rather than a proposal. If the
// 65th row and column are the shared edge, a patch's column 64 must equal its
// right neighbour's column 0. Over Mission 01's grid that holds for all 31,200
// edge samples with a worst difference of 0.0000, while random block pairs
// compared the same way mismatch 9,264 of 13,000. `SharedEdgeReport` re-runs it
// so the test can assert it rather than cite it.
//
// THE SENTINEL. `0x82102724` loads `0x82069BC0` = `9990.0` and `0x8210272C`
// bails when the sample is not less than it, so an unordered compare bails too:
// Mission 01's `007_ff_ff_ff_ff.bin` is 16,900 bytes of `0xFF`, every sample a
// NaN, and it reads as "this patch has no terrain". `kAbsentSample` is that
// constant and `sample_is_present` is that comparison, NaN included.
//
// THE SEGMENT TEST. `0x82102748`..`0x821027CC` walks six rows of five floats,
// stepping `0x104` per row, and keeps the larger of each pair -- `fcmpu` then
// `fmr` under `bc 4,25`, so `f0` is a MAXIMUM. `0x821027D0`..`0x821027DC` then
// compares that maximum against `[r7+4]` and `[r8+4]`, the `y` of the two
// endpoints, and abandons the query when the terrain is below both. That is an
// early-out, not an intersection: a false from `segment_may_reach_terrain` means
// the segment provably misses, a true means retail goes on to look properly, and
// THIS PORT STOPS THERE because the rest of `0x82102568` is VMX128 that cycle
// 1445 did not read.
//
// WHAT THIS IS NOT. The heightfield is not the MCA/MCI/MCD bit grid. That is a
// different pair of fields on the same object -- `[this+52/56/60]` against
// `[this+12/16]` -- read by a different function, `0x82101EE8`, at eight world
// units per bit rather than 128. Cycle 1445 compared the two over 1,048,576
// samples and the bit is water; nothing about that is needed here.
//
// The vertical axis is skipped by the query, which reads `x` at `+0` and `z` at
// `+8`, so `y` is the height this returns and `+4` is what the endpoints carry.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

// 0x82102648: mulli r10,r10,0x4204
inline constexpr std::size_t kTerrainPatchBytes = 0x4204;
// 0x821027C0: addi r10,r10,0x104 -- 65 floats
inline constexpr std::size_t kTerrainPatchSide = 0x104 / 4;
// 0x821025B8/0x821025C0: the coarse index is bounds-checked against 15
inline constexpr std::size_t kTerrainCoarseSide = 16;
// 0x82069BB4 = 0.001953125, so a cell is 512 units; four samples to a cell
inline constexpr float kTerrainCellUnits = 512.0F;
inline constexpr float kTerrainSampleUnits = kTerrainCellUnits / 4.0F;
// 0x82069BB8
inline constexpr float kTerrainWorldBias = 65536.0F;
// 0x82069BC0, loaded at 0x82102724
inline constexpr float kAbsentSample = 9990.0F;

// 0x8210272C: bc 4,24 -- bail unless the sample is strictly less. An unordered
// compare leaves LT clear, so a NaN bails, which is what 007's 0xFF means.
bool sample_is_present(float sample);

struct SharedEdgeReport {
  std::size_t matched = 0;
  std::size_t mismatched = 0;
  float worst = 0.0F;
};

class TerrainField {
 public:
  // `grid` is 256 bytes, the [this+0x0C] of 0x82102590. `patches` is the
  // [this+0x10] of 0x8210261C, a whole number of kTerrainPatchBytes records;
  // trailing bytes short of one record are ignored, as Mission 01 has four.
  static std::optional<TerrainField> open(const std::uint8_t* grid,
                                          std::size_t grid_size,
                                          const std::uint8_t* patches,
                                          std::size_t patches_size);

  std::size_t patch_count() const { return patches_.size() / kTerrainPatchSquared; }

  // 0x8210263C: the byte at coarse_z * 16 + coarse_x.
  std::uint8_t patch_id(std::size_t coarse_x, std::size_t coarse_z) const;

  // One sample of one patch, row-major, 65 to a row.
  float patch_sample(std::size_t patch, std::size_t row, std::size_t column) const;

  // A sample of the assembled field, 0..1024 on each axis, 128 units apart.
  float sample(std::size_t sample_x, std::size_t sample_z) const;

  static constexpr std::size_t field_side() {
    return kTerrainCoarseSide * (kTerrainPatchSide - 1) + 1;
  }

  // (world + 65536) * 1/512 truncated, per 0x82101F14/0x82101F24/0x82101F2C,
  // then four samples to the cell. Out-of-range positions clamp rather than
  // wrap, because retail's own bounds check abandons the query instead.
  bool height_at(float world_x, float world_z, float* height) const;

  // 0x82102748..0x821027DC: the maximum over the six-by-five neighbourhood,
  // against the y of both endpoints. False means the segment provably misses.
  bool segment_may_reach_terrain(const float from[3], const float to[3]) const;

  SharedEdgeReport check_shared_edges() const;

 private:
  static constexpr std::size_t kTerrainPatchSquared =
      kTerrainPatchSide * kTerrainPatchSide;
  std::vector<std::uint8_t> grid_;
  std::vector<float> patches_;
};

}  // namespace ac6::retail
