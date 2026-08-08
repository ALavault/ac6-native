#pragma once

// The NDXR container walk, derived from the retail image.
//
// This is the container ONLY: the file identity, the four section extents, the
// record array, and the string table. It deliberately stops at the polygon
// descriptor, because cycle 1203 established that the product's existing reader
// and this derivation disagree about that record and that NEITHER field mapping
// is controlled. Porting a descriptor here would replace a measured guess with
// an uncontrolled one.
//
// The chain, stage by stage, with the address that establishes each.
//
// RECOGNISE — 0x8234CA28
//     8234ca3c  lis  r10,0x4e55
//     8234ca40  ori  r10,r10,0x5033   ; 0x4E555033 = "NUP3"
//     8234ca44  lwz  r11,0x0(r31)     ; the file's first dword
//     8234ca58  bl   0x8233ef48       ; 0x4E445852 = "NDXR"
//     8234ca68  lbz  r11,0x8(r31)     ; the type code is the u16 at +0x08
//     8234ca7c  bl   0x8233ef68       ; 0x47494458 = "GIDX"
//     8234ca8c  addi r31,r31,0x10     ;   a GIDX file is a 0x10-byte header
//     8234ca90  bl   0x8233ef48       ;   in front of an NDXR
//   Both predicates are the branchless subf/cntlzw/rlwinm(27,31,31) equality.
//
// DISPATCH — 0x8234CB58 handles exactly three codes: 1 and 2 to 0x82350AF8,
//   and 0x200 to 0x82350CA0 / 0x82350C50. Every one of the 537 retail NDXR
//   files carries 0x200, so 0x82350AF8 is not on any shipped path.
//
// CONSTRUCT — 0x82350C50 and 0x82350CA0 are the same function but for a vtable
//   constant, 0x8201283C and 0x820128B4:
//     82350cc0  stw r29,0x8(r31)   ; this+0x08 = the byte size
//     82350cd4  stw r10,0x98(r31)  ; this+0x98 = 0, the sub-record counter
//     82350ce0  bl  0x82352b88     ; the load sequencer
//   0x82352B88 runs vtable slots +0x18, +0x10, +0x20. In vtable 0x820128B4,
//   +0x18 is 0x82350F08 and both +0x10 and +0x20 are 0x8234D098, a single blr:
//   two of the three stages are stubs and the parse is 0x82350F08 alone.
//
// HEADER — 0x82350F08
//     82350f20  stw r3,0xc(r31)     ; this+0x0C = the buffer
//     82350f24  lwz r11,0x10(r3)
//     82350f2c  addi r11,r11,0x30
//     82350f30  stw r11,0x84(r31)   ; this+0x84 = buf + [buf+0x10] + 0x30
//     82350f34  lwz r10,0x14(r3)
//     82350f3c  stw r11,0x88(r31)   ; this+0x88 = this+0x84 + [buf+0x14]
//     82350f40  lwz r10,0x18(r3)
//     82350f54  stw r11,0x8c(r31)   ; this+0x8C = this+0x88 + [buf+0x18]
//     82350f44  lwz r9,0x1c(r3)     ;   forced to 0 when [buf+0x1C] == 0
//     82350f70  stw r11,0x90(r31)   ; this+0x90 = the end of the body
//   The 0x30 is load-bearing and is the one constant here with a discriminating
//   control: see the test.
//
// RECORDS — 0x823556E0 driving 0x823555D0
//     823556f8  lhz  r30,0xa(r31)   ; the record count is the u16 at +0x0A
//     823556fc  addi r3,r31,0x30    ; the record array begins at file + 0x30
//     82355710  bl   0x823555d0     ;   the next pointer is its return value
//   and per record:
//     823555e8  lhz  r9,0x26(r31)             ; rec+0x26 flags
//     823555ec  rlwinm. r11,r9,0x0,0x0,0x10   ;   bit 0x8000: already relocated
//     82355614  stw  r11,0x20(r31)            ; rec+0x20 += [obj+0x90]
//     82355628  stw  r11,0x2c(r31)            ; rec+0x2C += fileBase
//     8235562c  lhz  r30,0x2a(r31)            ; rec+0x2A descriptor count
//     82355650  addi r3,r31,0x30              ; the stride is a fixed 0x30
//
// STRINGS — the relocation base for rec+0x20 is [obj+0x90], the end of the four
//   sections, and 0x82355318 relocates a second chain against the same base:
//     82355380  lwz r9,0x80(r28)    ; [obj+0x90]
//     82355388  stw r10,0x4(r11)    ; node+0x04 += it
//   Resolving rec+0x20 there yields a printable C string in 13,014 of 13,014
//   records. The rival base without the +0x30 yields a name of eight or more
//   characters in 0 of 537 files, and the derived base in 537 of 537 — the
//   test carries that comparison, because in-bounds alone cannot separate
//   the two.
//
// WHAT THIS DOES NOT DO, and why.
//
// It does not read the polygon descriptor. Retail reads +0x0C, +0x0E and four
// stream pointers at +0x10..+0x1C, then resolves an integer id through a
// resource map (0x8233EE40 / 0x8233EF88 into containers 0x828C8100 and
// 0x828CCB80, whose manager sits at +0x80 and whose map is at manager+0x80).
// Nothing inserts into those maps on any path yet read, so the vertices are
// behind a resolution this port cannot perform. See cycles 1200-1203.
//
// It does not mutate the buffer. Retail relocates in place and guards with two
// bits — 0x8000 for "pointers rebased", 0x4000 for "ids resolved". This reader
// takes a const buffer and resolves on use, which is a deliberate deviation:
// the guard bits are therefore expected CLEAR here, and the test asserts it.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ac6::retail {

// A named cause, so a refusal is never silent.
enum class NdxrRefusal {
  kNone = 0,
  kTooSmall,
  kBadMagic,
  kSizeMismatch,      // [+0x04] is not the buffer length
  kUnsupportedCode,   // [+0x08] is not 0x200
  kNoRecords,
  kSectionOverflow,   // the four extents run past the end
  kRecordOverflow,    // the record array does not fit
  kStringOverflow,    // a record's name offset leaves the buffer
};

// Google style: non-accessor functions are CamelCase. The accessors below
// stay snake_case, which the guide permits when they name their variable.
const char* RefusalToString(NdxrRefusal refusal) noexcept;

// 0x82350F08's four boundaries, as absolute offsets into the buffer.
struct NdxrSections {
  std::size_t first{};   // this+0x84
  std::size_t second{};  // this+0x88
  std::size_t third{};   // this+0x8C, zero when [+0x1C] is zero
  std::size_t end{};     // this+0x90, and the string table base
};

struct NdxrRecord {
  std::uint16_t index{};
  std::string_view name{};             // rec+0x20 resolved against the table
  std::uint16_t descriptor_count{};    // rec+0x2A
  std::uint32_t descriptor_offset{};   // rec+0x2C, as stored (file-relative)
  bool relocated{};                    // rec+0x26 bit 0x8000, expected false
};

class NdxrContainer final {
 public:
  // 0x8234CA28 + 0x8234CB58 + 0x82350F08, validated before anything is served.
  // A GIDX wrapper is unwrapped first, exactly as 0x8234CA28 does.
  static std::optional<NdxrContainer> Open(
      const std::uint8_t* bytes, std::size_t size,
      NdxrRefusal* refusal = nullptr) noexcept;

  std::uint16_t record_count() const noexcept { return record_count_; }
  const NdxrSections& sections() const noexcept { return sections_; }
  std::uint16_t type_code() const noexcept { return type_code_; }
  // True when this file arrived inside a 0x10-byte GIDX header.
  bool was_gidx_wrapped() const noexcept { return gidx_wrapped_; }

  std::optional<NdxrRecord> Record(std::uint16_t index) const noexcept;

  static constexpr std::size_t kRecordStride = 0x30;
  static constexpr std::size_t kRecordArrayBase = 0x30;
  static constexpr std::size_t kBodyBaseBias = 0x30;
  static constexpr std::uint16_t kSupportedTypeCode = 0x200;
  static constexpr std::uint16_t kRelocatedBit = 0x8000;
  static constexpr std::size_t kGidxHeaderSize = 0x10;

 private:
  NdxrContainer() = default;
  const std::uint8_t* bytes_{};
  std::size_t size_{};
  std::uint16_t record_count_{};
  std::uint16_t type_code_{};
  bool gidx_wrapped_{};
  NdxrSections sections_{};
};

}  // namespace ac6::retail
