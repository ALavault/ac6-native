#pragma once

// The model directory a unit's model bytes index into.
//
// 0x820A7070 gets a three-word container from the unit manager's vtable slot
// +0x0C (0x820A85E0) and indexes it with the bytes at +0x61/+0x62 of each Obj
// record's ObjBin block. This is that container and those two accessors, ported.
//
// The writer, 0x8228E988, builds the container from one blob:
//
//     8228e988  stw r4,0x0(r3)      ; word0 = the blob
//     8228e98c  lwz r11,0xc(r4)
//     8228e990  add r11,r11,r4
//     8228e994  stw r11,0x4(r3)     ; word1 = blob + [blob+0x0C]
//     8228e998  lwz r11,0x10(r4)
//     8228e99c  add r11,r11,r4
//     8228e9a0  stw r11,0x8(r3)     ; word2 = blob + [blob+0x10]
//
// The count, 0x8228E9A8, is [word0 + 0x04]. The getter, 0x8228E9B8:
//
//     8228e9b8  lwz    r11,0x0(r3)
//     8228e9bc  lwz    r11,0x4(r11)   ; the count
//     8228e9c0  cmplw  cr6,r4,r11
//     8228e9c8  li     r3,0x0         ; index >= count -> null
//     8228e9d0  lwz    r11,0x4(r3)    ; the offset table
//     8228e9d4  rlwinm r9,r4,0x2,...  ; index * 4
//     8228e9d8  lwz    r10,0x8(r3)    ; the entry base
//     8228e9dc  lwzx   r11,r9,r11
//     8228e9e0  add    r3,r11,r10     ; entry = base + table[index]
//
// So a blob is a directory when its +0x04 is a count, its +0x0C locates a table
// of that many dwords, and its +0x10 locates the base those dwords are relative
// to. Mission 01's `001_MDLP.mdlp` is one: 94 entries, table at 0x1000, base at
// 0x2000, and every entry lands on an `FHM ` signature.
//
// This port adds one thing retail does not do, and it is deliberate: it
// validates the blob before serving from it. Retail trusts its own archive;
// this reads an untrusted file, so a directory whose table or base runs past the
// end is refused outright rather than served and bounds-checked per call.
//
// What it does NOT do is open a file, resolve a name, or know what an entry
// contains. 0x820A85E0 finds the blob by descending the payload tree under a
// hashed name; that lookup is not ported and this class takes the bytes it is
// given.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

// A resolved directory entry: where it starts in the blob, and how far it runs.
// The extent is to the next entry, or to the declared total for the last one -
// retail does not compute this at all, and it is here so a caller can slice
// without walking the table again.
struct ModelDirectoryEntry {
  std::uint32_t index{};
  std::size_t offset{};  // from the start of the blob
  std::size_t size{};
};

class ModelDirectory final {
 public:
  // Validates and binds. Fails when the blob is too small, when the count is
  // zero or absurd, or when the table or the entry base runs past the end.
  static std::optional<ModelDirectory> open(const std::uint8_t* bytes,
                                            std::size_t size) noexcept;

  // 0x8228E9A8.
  std::uint32_t count() const noexcept { return count_; }

  // 0x8228E9B8: null when the index is at or above the count, which is the
  // sentinel path a 0xFF model byte takes.
  std::optional<ModelDirectoryEntry> entry(std::uint32_t index) const noexcept;

  // True when every entry begins with this four-byte signature. Mission 01's
  // model directory is 94 of 94 on `FHM `, and a directory that is not is not
  // being read the way retail reads it.
  bool every_entry_starts_with(const char signature[4]) const noexcept;

 private:
  ModelDirectory() = default;
  const std::uint8_t* bytes_{};
  std::size_t size_{};
  std::uint32_t count_{};
  std::size_t table_{};  // absolute, already added to the blob base
  std::size_t base_{};
};

}  // namespace ac6::retail
