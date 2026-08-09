#include "ac6/retail_map_water.h"

#include <cstring>

namespace ac6::retail {
namespace {

std::uint16_t be16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

// 0x82101FC8 onward compares four bytes one at a time. So does this.
bool magic_is(const std::uint8_t* blob, const char* want) {
  for (int i = 0; i < 3; ++i) {
    if (blob[i] != static_cast<std::uint8_t>(want[i])) return false;
  }
  return blob[3] == 0;
}

// fctiwz truncates toward zero. `static_cast<long>` on a float does the same,
// and the difference from a floor matters on the negative half of the map.
long truncate(float v) { return static_cast<long>(v); }

}  // namespace

std::optional<MapWaterGrid> MapWaterGrid::open(
    const std::uint8_t* mca, std::size_t mca_size, const std::uint8_t* mci,
    std::size_t mci_size, const std::uint8_t* mcd, std::size_t mcd_size) {
  if (mca == nullptr || mci == nullptr || mcd == nullptr) return std::nullopt;
  if (mca_size < kWaterHeaderBytes + kWaterCoarseSide * kWaterCoarseSide) return std::nullopt;
  if (mci_size < kWaterHeaderBytes || mcd_size < kWaterHeaderBytes) return std::nullopt;
  if (!magic_is(mca, "MCA") || !magic_is(mci, "MCI") || !magic_is(mcd, "MCD")) {
    return std::nullopt;
  }
  MapWaterGrid out;
  out.mca_.assign(mca, mca + mca_size);
  out.mci_.assign(mci, mci + mci_size);
  out.mcd_.assign(mcd, mcd + mcd_size);
  out.group_count_ = be16(mci + 8);
  out.block_count_ = be16(mcd + 8);
  return out;
}

bool MapWaterGrid::query(float world_x, float world_z, bool* bit) const {
  // 0x82101F14/F24/F2C: (world + 65536) * 1/512, truncated.
  const long cell_x = truncate((world_x + kWaterWorldBias) / kWaterCellUnits);
  const long cell_z = truncate((world_z + kWaterWorldBias) / kWaterCellUnits);
  const long coarse_x = cell_x >> 4, coarse_z = cell_z >> 4;
  if (coarse_x < 0 || coarse_z < 0) return false;
  if (coarse_x >= static_cast<long>(kWaterCoarseSide) ||
      coarse_z >= static_cast<long>(kWaterCoarseSide)) {
    return false;
  }
  // 0x82102080..0x82102090: the +1 * 16 skips MCA's header.
  const std::size_t mca_index =
      static_cast<std::size_t>((coarse_z + 1) * 16 + coarse_x);
  if (mca_index >= mca_.size()) return false;
  const std::size_t group = mca_[mca_index];
  if (group >= group_count_) return false;                    // 0x82102094

  // 0x821020A4..0x821020C0, with the same header skip folded in as +8 u16s.
  const std::size_t mci_index =
      ((group * 16 + static_cast<std::size_t>(cell_z & 15)) * 16 +
       static_cast<std::size_t>(cell_x & 15) + 8) * 2;
  if (mci_index + 2 > mci_.size()) return false;
  const std::size_t block = be16(mci_.data() + mci_index);
  if (block >= block_count_) return false;                    // 0x821020C4

  // 0x821020CC..0x821020D4
  const std::size_t base = block * kWaterBlockBytes + kWaterHeaderBytes;

  // 0x821020E4..0x82102118: the RAW coordinates, truncated toward zero.
  const long row = truncate(world_z / kWaterBitUnits) &
                   static_cast<long>(kWaterBlockSide - 1);
  const long column = truncate(world_x / kWaterBitUnits) &
                      static_cast<long>(kWaterBlockSide - 1);
  const long linear = (row << 6) | column;

  const std::size_t byte = base + static_cast<std::size_t>(linear >> 3);
  if (byte >= mcd_.size()) return false;
  const int shift = 7 - static_cast<int>(linear & 7);         // 0x82102120
  if (bit != nullptr) *bit = ((mcd_[byte] >> shift) & 1) != 0;
  return true;
}

}  // namespace ac6::retail
