#include "ac6/retail_input.h"

namespace ac6::retail {
namespace {

std::uint16_t read_be16(const std::uint8_t* bytes) noexcept {
  return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

std::uint32_t read_be32(const std::uint8_t* bytes) noexcept {
  return (static_cast<std::uint32_t>(bytes[0]) << 24) |
         (static_cast<std::uint32_t>(bytes[1]) << 16) |
         (static_cast<std::uint32_t>(bytes[2]) << 8) |
         static_cast<std::uint32_t>(bytes[3]);
}

// device offset -> snapshot offset. 0x8234D0A0 copies from this + 0x04.
constexpr std::size_t at(std::size_t device_offset) noexcept {
  return device_offset - 0x04;
}

}  // namespace

AxisHalves split_axis(std::int16_t raw) noexcept {
  // 8234d154 extsh. r9,r10 / 8234d158 blt. The stored positive half is the
  // halfword as loaded, not the sign-extended value, which matters only in that
  // it is the same bits.
  if (raw >= 0) {
    return AxisHalves{static_cast<std::uint16_t>(raw), 0};
  }
  // 8234d184 subfic r9,r9,-0x1: r9 = -1 - v, in 0..32767 for v in -32768..-1.
  return AxisHalves{0, static_cast<std::uint16_t>(-1 - static_cast<std::int32_t>(raw))};
}

ButtonEdges button_edges(std::uint32_t previous, std::uint16_t buttons) noexcept {
  // 8234d38c lhz: the current mask is the halfword zero-extended to 32 bits, so
  // ~current has 0xFFFF in its top half. Retail stores that; so does this.
  const std::uint32_t current = buttons;
  const std::uint32_t changed = previous ^ current;
  return ButtonEdges{
      changed & current,   // 8234d3b0
      changed & ~current,  // 8234d3c8
      ~current,            // 8234d3a4
      current,             // 8234d390
  };
}

InputSnapshot decode_snapshot(const std::uint8_t* bytes) noexcept {
  InputSnapshot snapshot;
  snapshot.connection_state = static_cast<std::int32_t>(read_be32(bytes + at(0x08)));
  snapshot.pressed = read_be32(bytes + at(0x14));
  snapshot.released = read_be32(bytes + at(0x18));
  snapshot.current = read_be32(bytes + at(0x1C));
  snapshot.not_held = read_be32(bytes + at(0x20));
  for (std::size_t index = 0; index < kAxisSplitTable.size(); ++index) {
    const AxisSplitEntry& entry = kAxisSplitTable[index];
    snapshot.axes[index] = AxisHalves{
        read_be16(bytes + at(entry.positive_offset)),
        read_be16(bytes + at(entry.negative_offset)),
    };
    snapshot.raw[index] =
        static_cast<std::int16_t>(read_be16(bytes + at(entry.source_offset)));
  }
  return snapshot;
}

std::uint32_t capability_code(std::uint8_t capability) noexcept {
  // 0x8234D0B8, a five-way compare with no table: 1->1, 2->4, 3->2, 4->3, 5->5.
  switch (capability) {
    case 1: return 1;  // 8234d104
    case 2: return 4;  // 8234d0fc
    case 3: return 2;  // 8234d0f4
    case 4: return 3;  // 8234d0ec
    case 5: return 5;  // 8234d0e4
    default: return 0;  // 8234d0b8 li r3,0x0, and 8234d0e0 bnelr
  }
}

}  // namespace ac6::retail
