#pragma once

// The map's per-position bit -- retail's `.mca`/`.mci`/`.mcd` triple.
//
// WHERE THE FIELDS COME FROM. `0x820FBC28`, `CMapManager`'s loader, assigns
// `+0x34`, `+0x38` and `+0x3C` from the names `%s.mca`, `%s.mci` and `%s.mcd` at
// `0x8205BDF8`, `0x8205BE00` and `0x8205BE08`. `0x82101EE8` reads exactly those
// three and validates each blob's four-byte magic with an inline byte-by-byte
// compare, starting at `0x82101FC8`, against the strings at
// `0x8205BFD8`/`0x8205BFDC`/`0x8205BFE0` -- never as a 32-bit
// constant, which is why two corpus searches for the magics as immediates
// returned zero and were misread as "retail does not validate".
//
// THE CHAIN, read at `0x82102080`..`0x82102138`:
//
//   cell   = trunc((world + 65536) * 1/512)          0x82101F14/F24/F2C
//   coarse = cell >> 4, bounds-checked 0..15         0x82101F78..0x82101F90
//   group  = MCA[(coarse_z + 1) * 16 + coarse_x]     0x82102080..0x82102090
//            the +1 * 16 is the sixteen-byte header
//            refused unless < the u16 at MCI+8       0x82102084/0x82102094
//   block  = MCI[((group*16 + (cz&15))*16 + (cx&15) + 8) * 2]
//            the group is scaled at 0x8210209C, the rest 0x821020A4..0x821020C0
//            the +8 before the doubling is that header again
//            refused unless < the u16 at MCD+8       0x821020A0/0x821020C4
//   base   = MCD + block * 512 + 16                  0x821020CC..0x821020D4
//   bit    = ((trunc(z * 1/8) & 63) << 6) | (trunc(x * 1/8) & 63)   0x821020E4..
//   return (base[bit >> 3] >> (7 - (bit & 7))) & 1   0x82102124..0x82102138
//
// **THE BIT INDEX USES THE RAW COORDINATES, NOT THE BIASED ONES.** `f11` and
// `f10` are loaded once at `0x82101EF4`/`0x82101EF8` and never written again;
// the `+65536` at `0x82101F14` goes into `f13`/`f12` for the cell, and
// `0x821020EC` multiplies the untouched `f10`/`f11` by the `0.125` at
// `0x8200322C`. Since `fctiwz` truncates toward ZERO, that is not the same as
// biasing first: for `x = -100` retail indexes bit 52 and a biased-then-floored
// reading indexes 51.
//
// Cycles 1445, 1447 and 1449 all used the biased form in TOOLS. Measured rather
// than assumed, because the consequence is much smaller than the defect: the
// INDEX differs for every negative non-integral coordinate, but the resulting
// BIT differs at only 0.02% of 200,000 off-lattice probes -- adjacent 8-unit
// cells almost always agree, so it shows only at a water boundary. On the
// 128-unit lattice those cycles actually sampled, `w / 8` is exact and the two
// forms agree at all 262,144 points, so cycle 1445's 97.43% stands unchanged.
// The port is still what caught it, and this is the form retail executes.
//
// `0x82101EE8` returns -1 when any check fails, and this returns false from
// `query` for the same cases rather than inventing a bit.
//
// WHAT THE BIT MEANS. Retail does not name it. Cycle 1445 measured it against
// the independently decoded heightfield over 1,048,576 samples: set on water and
// clear on land 97.4% of the time, with 96.7% of the residual being flat ground
// at or below 0.5 with the bit clear -- the city, the harbour and the river
// banks, exactly where an elevation proxy must fail -- and only 0.09% carrying
// the bit over ground above 0.5, along rivers narrower than the 128-unit height
// lattice. `is_water` is named from that measurement and from nothing retail
// says. What would refute it: a caller that tests the bit before something that
// has no relation to water.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ac6::retail {

inline constexpr float kWaterCellUnits = 512.0F;       // 0x82069BB4 = 1/512
inline constexpr float kWaterWorldBias = 65536.0F;     // 0x82069BB8
inline constexpr float kWaterBitUnits = 8.0F;          // 0x8200322C = 0.125
inline constexpr std::size_t kWaterCoarseSide = 16;
inline constexpr std::size_t kWaterBlockBytes = 512;   // 0x821020CC: block << 9
inline constexpr std::size_t kWaterBlockSide = 64;     // 0x82102108: six bits each
inline constexpr std::size_t kWaterHeaderBytes = 16;   // 0x821020D4 and the +1*16

class MapWaterGrid {
 public:
  static std::optional<MapWaterGrid> open(const std::uint8_t* mca, std::size_t mca_size,
                                          const std::uint8_t* mci, std::size_t mci_size,
                                          const std::uint8_t* mcd, std::size_t mcd_size);

  // False where 0x82101EE8 returns -1: outside the coarse grid, or a bounds
  // check refused. `bit` is untouched in that case.
  bool query(float world_x, float world_z, bool* bit) const;

  // The same, named from cycle 1445's measurement rather than from retail.
  bool is_water(float world_x, float world_z) const {
    bool bit = false;
    return query(world_x, world_z, &bit) && bit;
  }

  std::size_t group_count() const { return group_count_; }
  std::size_t block_count() const { return block_count_; }
  std::span<const std::uint8_t> mca_bytes() const noexcept { return mca_; }
  std::span<const std::uint8_t> mci_bytes() const noexcept { return mci_; }
  std::span<const std::uint8_t> mcd_bytes() const noexcept { return mcd_; }

 private:
  std::vector<std::uint8_t> mca_, mci_, mcd_;
  std::size_t group_count_ = 0;   // the u16 at MCI+8
  std::size_t block_count_ = 0;   // the u16 at MCD+8
};

}  // namespace ac6::retail
