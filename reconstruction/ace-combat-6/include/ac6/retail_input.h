#pragma once

// The controller snapshot NU::Input hands to gameplay, ported from the chain
// 0x823911C0 -> 0x8234D3F0 -> 0x8234D510 -> 0x8234D110 / 0x8234D378 ->
// 0x8234D0A0, with the dispatch at 0x82343A30 above it.
//
// Cycles 1307-1310 derived it. They replace the former bridge-only claims
// "canonical device+0x3E", "raw device+0x4E", and "0x8234D110 splits to
// +0x28/+0x2A". All three were correct but had not been evidence-backed.
//
// THE KERNEL BOUNDARY. 0x823911C0 is three instructions -
//
//     823911c0  or  r5,r4,r4
//     823911c4  li  r4,0x1
//     823911c8  b   0x823d737c      ; XamInputGetState(user, 1, pState)
//
// - and it has exactly two callers, 0x8234D3F0 and 0x8234D478, found by scanning
// 827,798 instructions. Both pass `this + 0x44`:
//
//     8234d40c  addi r4,r31,0x44
//
// so XINPUT_STATE sits at device+0x44 and its fields fall where the structure
// puts them: wButtons at +0x48, the four thumbs at +0x4C, +0x4E, +0x50, +0x52.
// 0x8234D510 is the poll entry, slot +0x10 of NU::Input::DriverController
// (vtable 0x820124F8), and it dispatches on the connection state at +0x08.
//
// THE AXIS STAGE, 0x8234D110. It copies the four thumbs verbatim to +0x3C, +0x3E,
// +0x40, +0x42, then runs four iterations over a table at 0x8201250C, stride
// 0xC, whose three words per entry are halfword indices addressed as
// (index + 0x14) * 2:
//
//     8234d144  lwz   r10,-0x4(r11)   ; source
//     8234d148  addi  r10,r10,0x14
//     8234d14c  rlwinm r10,r10,0x1,0x0,0x1e
//     8234d150  lhzx  r10,r10,r3
//     8234d154  extsh. r9,r10
//     8234d158  blt   0x8234d180
//     ...       positive: pos = raw, neg = 0
//     8234d184  subfic r9,r9,-0x1     ; negative: pos = 0, neg = -1 - v
//
// The vtable ends at +0x10 and the table begins at +0x14; the five words before
// it are code pointers and the words after are small integers, so the boundary
// is read rather than assumed.
//
// THE BUTTON STAGE, 0x8234D378:
//
//     8234d38c  lhz  r11,0x48(r31)    ; wButtons, zero-extended
//     8234d390  stw  r11,0x1c(r31)    ; +0x1C = current
//     8234d3a4  nor  r9,r11,r11       ; +0x20 = ~current, over 32 bits
//     8234d3a8  xor  r8,r10,r11       ; changed = prev ^ cur
//     8234d3b0  and  r11,r8,r11       ; +0x14 = pressed
//     8234d3c8  and  r10,r8,r9        ; +0x18 = released
//     8234d3d8  stw  r11,0x74(r31)    ; +0x74 = previous, written last
//
// +0x20 is the complement of a value whose top half is zero, so its top half is
// 0xFFFF. That is what retail stores and it is reproduced rather than tidied.
//
// THE ACCESSOR, 0x8234D0A0, is five instructions:
//
//     8234d0a8  addi r4,r11,0x4
//     8234d0ac  li   r5,0x40
//     8234d0b0  b    0x82382f70       ; memcpy(out, this + 0x04, 0x40)
//
// so the snapshot is device+0x04 .. +0x43 and stops one byte before the
// XINPUT_STATE. The copy boundary falls exactly where the derived fields end,
// which corroborates the layout from a direction it was not derived from.
//
// AND THE RETURN VALUE. 0x82343838 ends with cntlzw([out+0x04]) >> 31, and
// out+0x04 is device+0x08 - the connection state 0x8234D510 sets to 0 on a
// successful poll. The API answers "connected, and this snapshot is valid".
//
// NOT PORTED, because it is not established: what 0x8234D2B0 does with the float
// 0x82343A68 passes it, what 0x82343A90 and 0x82343AD0 do, and what the
// 744-instruction consumer 0x821CAA50 does with the snapshot.

#include <array>
#include <cstdint>

namespace ac6::retail {

// The snapshot is copied verbatim out of guest memory, so it is big-endian and
// is decoded as such rather than reinterpreted.
inline constexpr std::size_t kInputSnapshotBytes = 0x40;

// One entry of the table at 0x8201250C. The words are halfword indices; the
// byte offsets below are (index + 0x14) * 2, computed here so the derivation
// and the constant cannot drift apart.
struct AxisSplitEntry {
  std::uint16_t source_offset{};    // in the device
  std::uint16_t positive_offset{};
  std::uint16_t negative_offset{};
};

// Read at 0x8201250C: (10,3,2), (11,0,1), (12,7,6), (13,4,5).
inline constexpr std::array<AxisSplitEntry, 4> kAxisSplitTable{{
    {0x3C, 0x2E, 0x2C},  // sThumbLX
    {0x3E, 0x28, 0x2A},  // sThumbLY
    {0x40, 0x36, 0x34},  // sThumbRX
    {0x42, 0x30, 0x32},  // sThumbRY
}};

// The rule of 0x8234D154..0x8234D1A0. Both halves are non-negative: a negative
// v becomes -1 - v, which maps -32768..-1 onto 32767..0.
struct AxisHalves {
  std::uint16_t positive{};
  std::uint16_t negative{};
  bool operator==(const AxisHalves&) const = default;
};
AxisHalves split_axis(std::int16_t raw) noexcept;

// The rule of 0x8234D3A4..0x8234D3CC, over 32 bits as retail computes it.
struct ButtonEdges {
  std::uint32_t pressed{};
  std::uint32_t released{};
  std::uint32_t not_held{};
  std::uint32_t current{};
  bool operator==(const ButtonEdges&) const = default;
};
ButtonEdges button_edges(std::uint32_t previous, std::uint16_t buttons) noexcept;

// The 0x40 bytes 0x8234D0A0 copies, named. Offsets are given in the device so
// they can be checked against the listing; in the snapshot each is 4 lower.
struct InputSnapshot {
  std::int32_t connection_state{};  // device +0x08; 0 means valid
  std::uint32_t pressed{};          // +0x14
  std::uint32_t released{};         // +0x18
  std::uint32_t current{};          // +0x1C
  std::uint32_t not_held{};         // +0x20
  std::array<AxisHalves, 4> axes{};   // +0x28..+0x36, in kAxisSplitTable order
  std::array<std::int16_t, 4> raw{};  // +0x3C..+0x42, LX LY RX RY
  bool operator==(const InputSnapshot&) const = default;
};

// Decodes the copied block. `bytes` is the 0x40 the accessor wrote, so
// bytes[n] is device[n + 4].
InputSnapshot decode_snapshot(const std::uint8_t* bytes) noexcept;

// 0x82343838's answer: the pad is connected and the snapshot may be read.
inline bool snapshot_valid(const InputSnapshot& snapshot) noexcept {
  return snapshot.connection_state == 0;
}

// 0x8234D0B8, applied by the reconnect path to the capability byte at
// device+0x59 and stored at device+0x10. Anything outside 1..5 is 0.
std::uint32_t capability_code(std::uint8_t capability) noexcept;

}  // namespace ac6::retail
