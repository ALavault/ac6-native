#pragma once

// The per-controller input record 0x821CAA50 builds from the snapshot, ported
// from execution rather than from a reading.
//
// 0x821CAA50 is 744 instructions and this file does NOT interpret all of them.
// It ports the part that has been executed and measured, and names the rest as
// unproven, because a port that waits for every field to be understood is a port
// that does not exist.
//
// WHERE THE RECORDS ARE. Four of them, stride 0xA0, base 0x826EDB98 -- the array
// base itself. Cycle 1316 put record 0 at 0x826EDBA0 and every cycle to 1319
// repeated it; cycle 1320 corrected it by running the function. The stride is
// confirmed by four write runs 0xA0 apart.
//
// WHICH RECORD, AND FROM WHAT. The consumer takes the controller index in r20.
// `li r20,0x0` at 0x821CAA88 is the only write to r20 in the whole function and
// it dominates both reads; none of the three callers -- 0x821CA908, 0x821CB5F0,
// 0x821D7A90 -- writes or even reads r20. Executed under three controller plans
// (one axis per controller, one value per controller, one controller per
// driver-pointer slot): all four records are filled from driver-pointer slot 0,
// at an identical 4740 steps. Slots 1..4 are not read on this path.
//
// THE FRAME PRECONDITION. 0x821CA908 clears record +0x00..+0x83 every frame, so
// the consumer runs on a zeroed record and this port models exactly that. It is
// also why the evidence is read from the harness's 0x00 poison pass and not its
// 0xCD one -- see cycles 1321 and 1322.
//
// WHAT IS NOT PORTED, AND IS NOT GUESSED:
//
//   * the constant 3 at record+0x98 -- observed in every run, meaning unknown;
//   * record +0x8C..+0x94, written zero in every run, outside the cleared range,
//     so a value that is not zero in some other state cannot be ruled out;
//   * the second normalisation at 0x821CB244 -- subtract 0x4000, threshold
//     0x800, multiply by float32(1/16383) -- which no input yet found reaches.
//     The four raw thumbs were the obvious candidate and cycle 1322's control
//     refuted them.
//
// WHERE THE HALVES COME FROM. The eight axis halves this record consumes are
// written by the axis stage 0x8234D110, which walks the table at 0x8201250C and
// can only produce values in 0..32767 -- so a negative half is outside the
// reachable domain, and `axis_slot` reproduces the measured behaviour there
// anyway rather than leaving it to differ. The snapshot itself is the 0x40 bytes
// 0x8234D0A0 copies from device+0x04, and the scale is the float32(1/32767) at
// 0x82069BFC, which retail MULTIPLIES by.
//
// WHAT DOES *NOT* HAPPEN, each with a positive control in the same batch:
//
//   * the `pressed` and `released` words at device+0x14/+0x18 contribute no
//     record flag bit;
//   * no axis contributes a flag bit either -- a stick at 30000 fills its float
//     slot and leaves the flag word untouched, so the masks 0x10000..0x80000 of
//     cycle 1318 select a slot and are not stored;
//   * float slots for flag bits 0..13 are never written.

#include <array>
#include <cstddef>
#include <cstdint>

namespace ac6::retail {

inline constexpr std::size_t kInputRecordBytes = 0xA0;
inline constexpr std::size_t kInputRecordCount = 4;
// Documentation, not an address this code dereferences.
inline constexpr std::uint32_t kInputRecordArrayBase = 0x826EDB98;

// The parallel float array: the slot for flag bit b is (b + 3) * 4, derived at
// cycle 1318 from `addi r9,r10,0x3` / `rlwinm r9,r9,0x2` and confirmed by
// execution for bits 14..19.
constexpr std::size_t float_slot_for_bit(unsigned bit) noexcept {
  return (bit + 3) * 4;
}

// device held-button bit -> record flag bit. MEASURED, one case per bit with
// that bit alone set plus a null control, at cycle 1321. The consumer remaps
// rather than copies: device bit 12 becomes record bit 5.
//
// A value of kUnmappedBit means the run set that device bit and no record flag
// bit changed. Device bits 10, 11 and 16..31 are all unmapped.
inline constexpr std::uint8_t kUnmappedBit = 0xFF;
inline constexpr std::array<std::uint8_t, 32> kHeldBitToRecordBit{{
    0, 1, 2, 3,                       // device 0..3
    10, 11, 12, 13,                   // device 4..7
    8, 9,                             // device 8..9
    kUnmappedBit, kUnmappedBit,       // device 10..11
    5, 7, 6, 4,                       // device 12..15
    kUnmappedBit, kUnmappedBit, kUnmappedBit, kUnmappedBit,
    kUnmappedBit, kUnmappedBit, kUnmappedBit, kUnmappedBit,
    kUnmappedBit, kUnmappedBit, kUnmappedBit, kUnmappedBit,
    kUnmappedBit, kUnmappedBit, kUnmappedBit, kUnmappedBit,
}};

// The XInput assignment for each device bit position. It is a LABEL: the runs
// set a bit, not a button, and nothing here measured which physical control the
// bit belongs to.
enum class RecordFlag : unsigned {
  kDPadUp = 0,          // device 0,  XINPUT_GAMEPAD_DPAD_UP
  kDPadDown = 1,        // device 1
  kDPadLeft = 2,        // device 2
  kDPadRight = 3,       // device 3
  kFaceY = 4,           // device 15, XINPUT_GAMEPAD_Y
  kFaceA = 5,           // device 12, XINPUT_GAMEPAD_A
  kFaceX = 6,           // device 14, XINPUT_GAMEPAD_X
  kFaceB = 7,           // device 13, XINPUT_GAMEPAD_B
  kShoulderLeft = 8,    // device 8
  kShoulderRight = 9,   // device 9
  kStart = 10,          // device 4
  kBack = 11,           // device 5
  kThumbLeft = 12,      // device 6
  kThumbRight = 13,     // device 7
  // Bits 14 and 15 are fed by device+0x38 and device+0x3A, two halfwords cycle
  // 1318 found between the axis halves and the raw thumbs and could not place.
  // They are placed and measured; they are NOT named, because the stage that
  // writes them has not been read.
  kScalar14 = 14,
  kScalar15 = 15,
  kAxisLX = 16,
  kAxisLY = 17,
  kAxisRX = 18,
  kAxisRY = 19,
};

// float32(1/32767), the constant at 0x82069BFC. Retail MULTIPLIES by it. That is
// not a stylistic detail: at a half of 513 the multiply gives 0x3C804100 and a
// divide gives 0x3C804101, and the 255-point sweep is bit-exact on the multiply
// at every point.
inline constexpr float kAxisScale = 1.0F / 32767.0F;

// One axis slot, from BOTH halves of its axis, in retail's order.
//
//     slot = -(float(int16(negative)) * kAxisScale)        unconditional
//     if int16(positive) > 0: slot = float(int16(positive)) * kAxisScale
//
// The negative half is written first, negated, and is NOT gated -- which is why
// an idle stick reads NEGATIVE zero and not positive zero, and why a native port
// that writes +0.0F there differs from retail byte for byte while agreeing
// numerically. The positive half then overwrites, and only when strictly
// positive.
//
// THE 0x800 DEADZONE DOES NOT GATE THIS. Cycle 1315 read "at or below it the
// lane is not written"; a half of 1 already stores 1/32767. The deadzone belongs
// to the other normalisation, which this path does not reach.
//
// The halves are taken as SIGNED. The axis stage 0x8234D110 can only produce
// 0..32767, so a negative half is outside the reachable domain; the behaviour
// there is measured and reproduced anyway rather than left to differ.
float axis_slot(std::uint16_t positive_half, std::uint16_t negative_half) noexcept;

// The slots for flag bits 14 and 15, fed by device+0x38 and device+0x3A.
// A different rule from the axis one, in adjacent slots of the same record:
// a single signed field, scaled, with no second half and no negation.
float scalar_slot(std::uint16_t field) noexcept;

// And its flag bit, which is NOT its slot. The slot is written for every value,
// negatives included; the bit is set only when the signed field is at least 31.
// Bracketed by seventeen cases between 1 and 255 (cycle 1322). It is neither a
// sign test -- 1 does not set it -- nor the 0x800 deadzone -- 256 does.
//
// Whether retail compares the integer against 30 or the float against a constant
// in (0.000916, 0.000946] is NOT ESTABLISHED. Both fit every measured value.
inline constexpr std::int32_t kScalarFlagThreshold = 31;
bool scalar_flag_set(std::uint16_t field) noexcept;

// The record as the consumer leaves it, on a record the frame stage has cleared.
struct InputRecord {
  std::uint32_t flags{};                  // +0x08
  float scalar14{};                       // +0x44, from device+0x38
  float scalar15{};                       // +0x48, from device+0x3A
  float axis_lx{};                        // +0x4C
  float axis_ly{};                        // +0x50
  float axis_rx{};                        // +0x54
  float axis_ry{};                        // +0x58
  bool operator==(const InputRecord&) const = default;
};

// Built from the 0x40 bytes 0x8234D0A0 copies, so `snapshot[n]` is `device[n+4]`
// -- the same convention retail_input.h uses.
InputRecord build_input_record(const std::uint8_t* snapshot) noexcept;

// The record laid out as the 0xA0 guest bytes, big-endian, on a zeroed record.
// This is what the micro-execution differential compares against, so it carries
// the two fields this port does not interpret: record+0x98 holds the constant 3
// that every run wrote, and +0x8C..+0x94 stay zero.
inline constexpr std::uint32_t kUninterpretedWordAt0x98 = 3;
std::array<std::uint8_t, kInputRecordBytes> encode_input_record(
    const InputRecord& record) noexcept;

}  // namespace ac6::retail
