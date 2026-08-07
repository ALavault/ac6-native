#pragma once

// The ten retail *Bin readers, natively.
//
// Cycle 1098 gave the product the container and the views its consumers use.
// This is the other half: the readers themselves, which lay a parsed image into
// a record and a buffer exactly as the retail parsers do, including the four
// sizer quirks that only multi-node comparison exposed.
//
// The reference is not a rewrite of the same idea: it is the p-code
// micro-execution of the retail instructions, reduced to one digest per case by
// tools/emit_ac6_reader_digests.py. A native reader that agrees on all 138
// cases reproduces what the retail machine code writes, byte for byte.
//
// The synthetic address space is the one MicroExecuteScenarioParser.java uses,
// so the digests are directly comparable rather than merely similar.

#include "ac6/retail_scenario.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ac6::retail {

inline constexpr std::uint32_t kPayloadBase = 0xB0000000u;
inline constexpr std::uint32_t kRecordBase = 0xB4000000u;
inline constexpr std::uint32_t kBufferBase = 0xB5000000u;
inline constexpr std::size_t kRecordBytes = 0x100;
inline constexpr std::size_t kBufferBytes = 0x8000;

// The record and buffer, with an explicit written mask. A parser can write a
// byte equal to any fill value, so writes are tracked rather than inferred by
// diffing - the same reason the emulator side takes the union of two poison
// passes.
class BinImage final {
 public:
  BinImage();

  void write32(std::uint32_t address, std::uint32_t value) noexcept;
  void zero(std::uint32_t address, unsigned words) noexcept;

  // True once a write landed outside the two regions. The image is then
  // unusable and its digest meaningless.
  bool overflowed() const noexcept { return overflowed_; }

  struct Run {
    std::uint32_t address{};
    std::vector<std::uint8_t> bytes;
  };
  std::vector<Run> runs() const;

  std::size_t run_count() const;
  std::size_t written_bytes() const;

  // FNV-1a 64 over "{address:08x}:{size}:{hex}\n" per run, in address order.
  // The same definition tools/emit_ac6_reader_digests.py applies to the
  // committed p-code snapshots.
  std::uint64_t digest() const;

 private:
  std::vector<std::uint8_t> record_;
  std::vector<std::uint8_t> record_written_;
  std::vector<std::uint8_t> buffer_;
  std::vector<std::uint8_t> buffer_written_;
  bool overflowed_{};
};

// One reproduced call into the retail error printer, with the string address
// the branch materializes. The only call kind a native reader can reproduce.
struct ReaderDiagnostic {
  std::uint32_t string_address{};
};

class BinReaders final {
 public:
  BinReaders(const ScenarioPayload& payload, BinImage& image) noexcept
      : payload_(payload), image_(image) {}

  // Runs the reader named by `klass` at the given payload-relative node, with
  // the record and buffer at their canonical bases. False when the class is
  // unknown, when the payload reads out of range, or when the node exercises a
  // descent this port does not model - never a silent guess.
  bool run(std::string_view klass, std::size_t node);

  const std::vector<ReaderDiagnostic>& diagnostics() const noexcept {
    return diagnostics_;
  }
  std::string_view failure() const noexcept { return failure_; }

 private:
  // Each reader mirrors one retail function; the sizers keep their own names.
  void com_read(std::uint32_t record, std::size_t node);
  std::uint32_t comtbl_size(std::size_t node);
  void comtbl_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  void maneuver_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  std::uint32_t maneuver_size(std::size_t node);
  void maneuver_block_read(std::uint32_t record, std::size_t node,
                           std::uint32_t buffer);
  std::uint32_t maneuver_block_size(std::size_t node);
  void param_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  std::uint32_t param_size(std::size_t node);
  void order_tag2_read(std::uint32_t record, std::optional<std::size_t> node);
  std::uint32_t order_tag2_size(std::optional<std::size_t> node);
  void order_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  std::uint32_t order_size(std::optional<std::size_t> node);
  void act_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  std::uint32_t act_size(std::optional<std::size_t> node);
  void set_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  void unnamed28_read(std::uint32_t record, std::optional<std::size_t> node,
                      std::uint32_t buffer);
  std::uint32_t unnamed28_size(std::optional<std::size_t> node);
  void unnamed28_list_read(std::uint32_t record, std::size_t node,
                           std::uint32_t buffer);
  std::uint32_t unnamed28_list_size(std::size_t node);
  void submis_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  std::uint32_t submis_size(std::size_t node);
  void submistbl_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  void radiotbl_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);
  void obj_read(std::uint32_t record, std::size_t node, std::uint32_t buffer);

  // Helpers with the same fail-closed discipline as the readers.
  std::optional<std::size_t> resolve(std::size_t node, unsigned word);
  std::optional<std::size_t> child_at(const std::vector<std::size_t>& children,
                                      std::size_t index);
  std::uint8_t u8(std::size_t offset);
  std::int32_t s32(std::size_t offset);
  std::uint16_t u16(std::size_t offset);
  static std::uint32_t guest(std::optional<std::size_t> offset) noexcept;
  void fail(std::uint32_t string_address);
  void unmodelled(std::string_view what);
  void out_of_range();

  const ScenarioPayload& payload_;
  BinImage& image_;
  std::vector<ReaderDiagnostic> diagnostics_;
  std::string failure_;
  bool ok_{true};
};

}  // namespace ac6::retail
