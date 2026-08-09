#include "ac6/retail_terrain_field.h"

#include <cmath>
#include <cstring>

namespace ac6::retail {
namespace {

float read_be_float(const std::uint8_t* p) {
  const std::uint32_t bits = (static_cast<std::uint32_t>(p[0]) << 24) |
                             (static_cast<std::uint32_t>(p[1]) << 16) |
                             (static_cast<std::uint32_t>(p[2]) << 8) |
                             static_cast<std::uint32_t>(p[3]);
  float out = 0.0F;
  std::memcpy(&out, &bits, sizeof(out));
  return out;
}

// 0x821025A0/0x821025A4 are `srawi`, an ARITHMETIC shift, and the bounds test
// at 0x821025A8..0x821025C4 rejects negatives before the index is formed.
bool coarse_in_range(long value) {
  return value >= 0 && value < static_cast<long>(kTerrainCoarseSide);
}

}  // namespace

bool sample_is_present(float sample) {
  // The retail compare is `fcmpu` then `bc 4,24`: continue only when LT is set.
  // NaN sets neither LT nor GT, so it falls to the bail with everything >= the
  // sentinel. `<` reproduces both, because a NaN comparison is false.
  return sample < kAbsentSample;
}

std::optional<TerrainField> TerrainField::open(const std::uint8_t* grid,
                                               std::size_t grid_size,
                                               const std::uint8_t* patches,
                                               std::size_t patches_size) {
  if (grid == nullptr || patches == nullptr) return std::nullopt;
  if (grid_size < kTerrainCoarseSide * kTerrainCoarseSide) return std::nullopt;
  const std::size_t count = patches_size / kTerrainPatchBytes;
  if (count == 0) return std::nullopt;

  TerrainField field;
  field.grid_.assign(grid, grid + kTerrainCoarseSide * kTerrainCoarseSide);
  // Every id the grid can name must exist, or 0x82102648 indexes past the array.
  for (const std::uint8_t id : field.grid_) {
    if (id >= count) return std::nullopt;
  }
  field.patches_.resize(count * kTerrainPatchSquared);
  for (std::size_t i = 0; i < count * kTerrainPatchSquared; ++i) {
    field.patches_[i] = read_be_float(patches + i * 4);
  }
  return field;
}

std::uint8_t TerrainField::patch_id(std::size_t coarse_x,
                                    std::size_t coarse_z) const {
  if (coarse_x >= kTerrainCoarseSide || coarse_z >= kTerrainCoarseSide) return 0;
  return grid_[coarse_z * kTerrainCoarseSide + coarse_x];
}

float TerrainField::patch_sample(std::size_t patch, std::size_t row,
                                 std::size_t column) const {
  if (patch >= patch_count() || row >= kTerrainPatchSide ||
      column >= kTerrainPatchSide) {
    return kAbsentSample;
  }
  return patches_[patch * kTerrainPatchSquared + row * kTerrainPatchSide + column];
}

float TerrainField::sample(std::size_t sample_x, std::size_t sample_z) const {
  const std::size_t span = kTerrainPatchSide - 1;  // 64 intervals per patch
  std::size_t coarse_x = sample_x / span, row_x = sample_x % span;
  std::size_t coarse_z = sample_z / span, row_z = sample_z % span;
  // The last lattice line belongs to the last patch's shared edge.
  if (coarse_x == kTerrainCoarseSide) { coarse_x = kTerrainCoarseSide - 1; row_x = span; }
  if (coarse_z == kTerrainCoarseSide) { coarse_z = kTerrainCoarseSide - 1; row_z = span; }
  if (coarse_x >= kTerrainCoarseSide || coarse_z >= kTerrainCoarseSide) {
    return kAbsentSample;
  }
  return patch_sample(patch_id(coarse_x, coarse_z), row_z, row_x);
}

bool TerrainField::height_at(float world_x, float world_z, float* height) const {
  // 0x82101F14 fadds, 0x82101F24 fmuls, 0x82101F2C fctiwz -- truncate.
  const long cell_x = static_cast<long>((world_x + kTerrainWorldBias) / kTerrainCellUnits);
  const long cell_z = static_cast<long>((world_z + kTerrainWorldBias) / kTerrainCellUnits);
  if (!coarse_in_range(cell_x >> 4) || !coarse_in_range(cell_z >> 4)) return false;
  const long sx = static_cast<long>((world_x + kTerrainWorldBias) / kTerrainSampleUnits);
  const long sz = static_cast<long>((world_z + kTerrainWorldBias) / kTerrainSampleUnits);
  const float h = sample(static_cast<std::size_t>(sx), static_cast<std::size_t>(sz));
  if (!sample_is_present(h)) return false;
  if (height != nullptr) *height = h;
  return true;
}

bool TerrainField::segment_may_reach_terrain(const float from[3],
                                             const float to[3]) const {
  // The query reads x at +0 and z at +8; +4 is the vertical it compares against.
  const long cell_x = static_cast<long>((from[0] + kTerrainWorldBias) / kTerrainCellUnits);
  const long cell_z = static_cast<long>((from[2] + kTerrainWorldBias) / kTerrainCellUnits);
  if (!coarse_in_range(cell_x >> 4) || !coarse_in_range(cell_z >> 4)) return false;

  const std::uint8_t patch = patch_id(static_cast<std::size_t>(cell_x >> 4),
                                      static_cast<std::size_t>(cell_z >> 4));
  const long fine_x = cell_x & 15, fine_z = cell_z & 15;
  // 0x8210271C probes the neighbourhood's centre first and bails on a sentinel.
  if (!sample_is_present(patch_sample(patch, static_cast<std::size_t>(fine_z * 4 + 2),
                                      static_cast<std::size_t>(fine_x * 4 + 2)))) {
    return false;
  }
  // 0x82102748..0x821027CC: six rows of five, keeping the larger.
  float highest = -kAbsentSample;
  for (long row = 0; row < 6; ++row) {
    for (long column = 0; column < 5; ++column) {
      const long r = fine_z * 4 + row, c = fine_x * 4 + column;
      if (r >= static_cast<long>(kTerrainPatchSide) ||
          c >= static_cast<long>(kTerrainPatchSide)) {
        continue;
      }
      const float v = patch_sample(patch, static_cast<std::size_t>(r),
                                   static_cast<std::size_t>(c));
      if (v > highest) highest = v;
    }
  }
  // 0x821027D0..0x821027DC: below BOTH endpoints means the segment misses.
  return !(highest < from[1] && highest < to[1]);
}

SharedEdgeReport TerrainField::check_shared_edges() const {
  SharedEdgeReport report;
  const std::size_t last = kTerrainPatchSide - 1;
  auto tally = [&report](float a, float b) {
    if (!(sample_is_present(a) && sample_is_present(b))) return;
    const float d = std::fabs(a - b);
    if (d > report.worst) report.worst = d;
    if (d <= 1e-3F) ++report.matched; else ++report.mismatched;
  };
  for (std::size_t cz = 0; cz < kTerrainCoarseSide; ++cz) {
    for (std::size_t cx = 0; cx < kTerrainCoarseSide; ++cx) {
      const std::uint8_t a = patch_id(cx, cz);
      if (cx + 1 < kTerrainCoarseSide) {
        const std::uint8_t b = patch_id(cx + 1, cz);
        for (std::size_t r = 0; r < kTerrainPatchSide; ++r) {
          tally(patch_sample(a, r, last), patch_sample(b, r, 0));
        }
      }
      if (cz + 1 < kTerrainCoarseSide) {
        const std::uint8_t b = patch_id(cx, cz + 1);
        for (std::size_t c = 0; c < kTerrainPatchSide; ++c) {
          tally(patch_sample(a, last, c), patch_sample(b, 0, c));
        }
      }
    }
  }
  return report;
}

}  // namespace ac6::retail
