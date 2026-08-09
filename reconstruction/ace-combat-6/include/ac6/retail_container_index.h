#pragma once

// Retail's container header parser and getter, ported from 0x82234C18 and
// 0x82234DD0.
//
// WHERE THEY SIT. These two are the last hop between a mission's model byte and
// an NDXR span, and every step on either side of them was already ported:
//
//   binding.primary  ->  ModelDirectory.entry(id)      0x8228E9B8   ported
//      -> an FHM span
//         -> 0x82234C18 parses its header                           THIS
//            -> 0x82234DD0 indexes sub-entry j                      THIS
//               -> array1[j] gives the exact length
//                  -> NdxrContainer::Open                           ported
//
// HOW THEY WERE FOUND, because it took three cycles and two wrong turns. The
// consumer is `CX360ActorModelSetup`, a STATIC SINGLETON at 0x826A0708 reached
// through the global 0x826A0728 -- a pointer the linker resolved, which is why
// cycle 1421 found no store to it anywhere. Its vtable is 0x820674D8 and slots
// 6 and 7, 0x821C1748 and 0x821C1960, take the FHM pointer; 0x821C1748 builds a
// descriptor with 0x82234C18 and indexes it with 0x82234DD0.
//
// THE HEADER IS GENERAL, not FHM-specific:
//
//     +0x04  version byte
//     +0x05  ENDIAN FLAG -- when it is not 1, the fields below are byte-swapped
//     +0x06  header size, u16
//     at file + header_size:  a count, then FOUR parallel arrays of that many
//                             dwords: offsets, lengths, and two more
//
// Every FHM in Mission 01's package carries version 1, flag 1, size 0x10 -- so
// "count at +0x10, offsets at +0x14", which cycle 1419 read off the file, is the
// +0x06 field and not a constant.
//
// ARRAY 1 IS THE LENGTH and it is exact. Cycle 1419 computed a span by
// subtracting neighbouring offsets, which includes the padding between
// sub-entries, and reported 292 disagreements with each NDXR's own declared
// size. Array 1 agrees with that size at 292 of 292. Read the array; do not
// subtract offsets.
//
// THE ONE THING A TIDY PORT GETS WRONG, and only a synthetic case can catch it:
// on the byte-swapped path the three array pointers are computed from the RAW,
// UNSWAPPED count. 0x82234C7C multiplies the word as loaded, 0x82234C84..
// 0x82234C98 store the four pointers, and only afterwards does 0x82234CD0
// replace the count at +0x00 with the swapped one. So a byte-swapped container
// gets array pointers derived from a count with its bytes reversed, which run
// far out of range. That is retail's behaviour and this reproduces it.
//
// No shipped file reaches that path -- all 94 carry flag 1 -- so a port checked
// only against real data would never execute it. The differential's synthetic
// case rejected the corrected version at exactly those three fields:
// array1 retail 0xF8000014 against a "fixed" 0xB000005C.
//
// A ZERO TABLE ENTRY IS A NULL, not base+0. 0x82234DF0 tests the offset after
// loading it and returns 0, which is a different answer from "the entry at the
// start of the file" and is why the getter returns an address rather than an
// offset.
//
// Verified by `tools/audit_container_index_microexec.py`: 8 cases, 32 values,
// no tolerance.

#include <cstddef>
#include <cstdint>

namespace ac6::retail {

// The descriptor 0x82234C18 fills, field for field. `+0x14` is deliberately
// absent: the parser never writes it, which is why its caller zeroes the whole
// struct at 0x821C176C..0x821C1790 before calling.
struct ContainerIndex {
  std::uint32_t count{};        // +0x00, the swapped count on the swap path
  std::uint32_t base{};         // +0x04, the file pointer verbatim
  std::uint16_t header_size{};  // +0x08
  std::uint8_t version{};       // +0x0A, from file+4
  std::uint8_t endian{};        // +0x0B, from file+5
  std::uint32_t array0{};       // +0x0C, offsets
  std::uint32_t array1{};       // +0x10, lengths
  std::uint32_t array2{};       // +0x18
  std::uint32_t array3{};       // +0x1C
  bool operator==(const ContainerIndex&) const = default;
};

// The flag value that means "already in this machine's order".
inline constexpr std::uint8_t kNativeEndianFlag = 1;

// 0x82234C18. `base` is the address the file is mapped at, carried so the
// pointer arithmetic reproduces retail's exactly; `file` is the bytes.
// Returns true, as retail does -- it has no failure path.
bool parse_container_index(ContainerIndex& out, const std::uint8_t* file,
                           std::size_t size, std::uint32_t base) noexcept;

// 0x82234DD0. Returns the ADDRESS of sub-entry `index`, or 0 when the index is
// at or above the count or the table entry is itself zero.
std::uint32_t container_entry(const ContainerIndex& index,
                              const std::uint8_t* file, std::size_t size,
                              std::uint32_t which) noexcept;

// Array 1 for the same index: the sub-entry's exact length. Not a retail
// function -- retail's callers read the array directly -- but the offset and
// the length are always wanted together and separating them is how cycle 1419
// came to subtract offsets instead.
std::uint32_t container_entry_length(const ContainerIndex& index,
                                     const std::uint8_t* file, std::size_t size,
                                     std::uint32_t which) noexcept;

}  // namespace ac6::retail
