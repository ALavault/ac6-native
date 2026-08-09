#include "ac6/retail_input_record.h"

#include <cstring>

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

void write_be32(std::uint8_t* bytes, std::uint32_t value) noexcept {
  bytes[0] = static_cast<std::uint8_t>(value >> 24);
  bytes[1] = static_cast<std::uint8_t>(value >> 16);
  bytes[2] = static_cast<std::uint8_t>(value >> 8);
  bytes[3] = static_cast<std::uint8_t>(value);
}

void write_be_float(std::uint8_t* bytes, float value) noexcept {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  write_be32(bytes, bits);
}

// device offset -> snapshot offset. 0x8234D0A0 copies from this + 0x04.
constexpr std::size_t at(std::size_t device_offset) noexcept {
  return device_offset - 0x04;
}

std::int32_t as_signed(std::uint16_t half) noexcept {
  return static_cast<std::int32_t>(static_cast<std::int16_t>(half));
}

float scale(std::int32_t value) noexcept {
  return static_cast<float>(value) * kAxisScale;
}

}  // namespace

float axis_slot(std::uint16_t positive_half, std::uint16_t negative_half) noexcept {
  // The negative half first, negated, ungated: this is what leaves -0.0 in an
  // idle slot. `-scale(0)` is -0.0F and `scale(0)` is +0.0F, and the sweep says
  // the record holds 0x80000000.
  float slot = -scale(as_signed(negative_half));
  if (as_signed(positive_half) > 0) {
    slot = scale(as_signed(positive_half));
  }
  return slot;
}

float scalar_slot(std::uint16_t field) noexcept {
  return scale(as_signed(field));
}

bool scalar_flag_set(std::uint16_t field) noexcept {
  return as_signed(field) >= kScalarFlagThreshold;
}

InputRecord build_input_record(const std::uint8_t* snapshot) noexcept {
  InputRecord record;

  // The held word at device+0x1C, remapped bit by bit. Fourteen device bits are
  // assigned and the rest are dropped, so a held word with bit 10 set produces
  // nothing -- measured, not assumed from XInput's documentation.
  const std::uint32_t held = read_be32(snapshot + at(0x1C));
  for (unsigned bit = 0; bit < kHeldBitToRecordBit.size(); ++bit) {
    if (((held >> bit) & 1U) == 0U) {
      continue;
    }
    const std::uint8_t target = kHeldBitToRecordBit[bit];
    if (target != kUnmappedBit) {
      record.flags |= 1U << target;
    }
  }

  // The two unnamed scalars, which own flag bits 14 and 15. The slot is filled
  // unconditionally and the bit is gated -- they are separate rules and the port
  // keeps them separate.
  const std::uint16_t scalar14 = read_be16(snapshot + at(0x38));
  const std::uint16_t scalar15 = read_be16(snapshot + at(0x3A));
  record.scalar14 = scalar_slot(scalar14);
  record.scalar15 = scalar_slot(scalar15);
  if (scalar_flag_set(scalar14)) {
    record.flags |= 1U << static_cast<unsigned>(RecordFlag::kScalar14);
  }
  if (scalar_flag_set(scalar15)) {
    record.flags |= 1U << static_cast<unsigned>(RecordFlag::kScalar15);
  }

  // The four axes, each from its own pair of halves.
  record.axis_lx = axis_slot(read_be16(snapshot + at(0x2E)),   // LX positive
                             read_be16(snapshot + at(0x2C)));  // LX negative
  record.axis_ly = axis_slot(read_be16(snapshot + at(0x28)),   // LY positive
                             read_be16(snapshot + at(0x2A)));  // LY negative
  record.axis_rx = axis_slot(read_be16(snapshot + at(0x36)),   // RX positive
                             read_be16(snapshot + at(0x34)));  // RX negative
  record.axis_ry = axis_slot(read_be16(snapshot + at(0x30)),   // RY positive
                             read_be16(snapshot + at(0x32)));  // RY negative

  // NO AXIS SETS A FLAG BIT. A stick at 30000 fills +0x50 and leaves the flag
  // word at zero, over 255 sampled points. The absence is deliberate.
  return record;
}

std::array<std::uint8_t, kInputRecordBytes> encode_input_record(
    const InputRecord& record) noexcept {
  std::array<std::uint8_t, kInputRecordBytes> bytes{};
  write_be32(bytes.data() + 0x08, record.flags);
  write_be_float(bytes.data() + float_slot_for_bit(14), record.scalar14);
  write_be_float(bytes.data() + float_slot_for_bit(15), record.scalar15);
  write_be_float(bytes.data() + float_slot_for_bit(16), record.axis_lx);
  write_be_float(bytes.data() + float_slot_for_bit(17), record.axis_ly);
  write_be_float(bytes.data() + float_slot_for_bit(18), record.axis_rx);
  write_be_float(bytes.data() + float_slot_for_bit(19), record.axis_ry);
  // Not interpreted, and reproduced rather than omitted: every run wrote 3 here.
  write_be32(bytes.data() + 0x98, kUninterpretedWordAt0x98);
  return bytes;
}

}  // namespace ac6::retail
