#include "ac6/retail_map_placement.h"

#include <cstring>

namespace ac6::retail {
namespace {

std::uint32_t be32(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

float be_float(const std::uint8_t* p) {
  const std::uint32_t bits = be32(p);
  float out = 0.0F;
  std::memcpy(&out, &bits, sizeof(out));
  return out;
}

const PlacementCell kAbsentCell{};

}  // namespace

std::optional<MapPlacement> MapPlacement::open(const std::uint8_t* pdl,
                                               std::size_t size) {
  // 0x820FBF5C: four bytes means empty, and retail frees the block outright.
  if (pdl == nullptr || size <= kPlacementEmptySize) return std::nullopt;
  if (size < kPlacementHeaderBytes) return std::nullopt;

  MapPlacement out;
  out.body_records_ = (size - kPlacementHeaderBytes) / kPlacementRecordBytes;
  out.header_.reserve(kPlacementCoarseSide * kPlacementCoarseSide);
  for (std::size_t i = 0; i < kPlacementCoarseSide * kPlacementCoarseSide; ++i) {
    const std::uint8_t* r = pdl + i * kPlacementRecordBytes;
    PlacementCell cell{be32(r), be32(r + 4), be32(r + 8)};
    out.header_total_ += cell.count;
    out.header_.push_back(cell);
  }
  // The partition is the check that makes the count/offset reading a reading.
  if (out.header_total_ != out.body_records_) return std::nullopt;

  out.instances_.reserve(out.header_total_);
  for (std::size_t i = 0; i < out.header_.size(); ++i) {
    const PlacementCell& cell = out.header_[i];
    if (cell.count == 0) continue;
    const std::size_t cx = i % kPlacementCoarseSide;
    const std::size_t cz = i / kPlacementCoarseSide;
    const std::size_t end =
        static_cast<std::size_t>(cell.offset) + cell.count * kPlacementRecordBytes;
    if (cell.offset < kPlacementHeaderBytes || end > size) return std::nullopt;
    for (std::uint32_t k = 0; k < cell.count; ++k) {
      const std::uint8_t* r = pdl + cell.offset + k * kPlacementRecordBytes;
      const std::uint32_t tag = be32(r + 12);
      MapInstance instance;
      instance.world_x = world_from_local(cx, be_float(r));
      instance.world_y = be_float(r + 4);
      instance.world_z = world_from_local(cz, be_float(r + 8));
      instance.part_id = static_cast<std::uint16_t>(tag & 0xFFFFu);
      instance.tag_high = static_cast<std::uint16_t>(tag >> 16);
      instance.coarse_x = static_cast<std::uint8_t>(cx);
      instance.coarse_z = static_cast<std::uint8_t>(cz);
      out.instances_.push_back(instance);
    }
  }
  return out;
}

const PlacementCell& MapPlacement::cell(std::size_t coarse_x,
                                        std::size_t coarse_z) const {
  if (coarse_x >= kPlacementCoarseSide || coarse_z >= kPlacementCoarseSide) {
    return kAbsentCell;
  }
  return header_[coarse_z * kPlacementCoarseSide + coarse_x];
}

}  // namespace ac6::retail
