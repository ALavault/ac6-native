#pragma once

#include "ac6/retail_session_replay.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace ac6::retail {

// The v3 and v4 receipts are integrity sidecars for controller projections.
// Both prove that their native-output claims describe the exact replay file
// and cache opened by the caller.  V3 accepts only a PAL source/target.  V4 is
// a separate, strict contract for the one qualified NTSC-U/J source identity
// projected into the PAL native target.  Both require an
// integrity-only runtime cadence census and are usable for provisional replay,
// but are not parity evidence or runner attestation.
// Source replay and cadence-census lineage are parsed structurally, but
// cannot be proved without their raw/parent/sidecar artefacts.
enum class RetailProjectionReceiptError : std::uint8_t {
  None,
  InvalidArgument,
  ReceiptUnreadable,
  ReceiptByteBound,
  JsonInvalid,
  JsonBound,
  JsonNonCanonical,
  SchemaMismatch,
  ReplayUnreadable,
  ReplayByteBound,
  ReplayIdentityMismatch,
  CacheIdentityMismatch,
  ReplayMetadataMismatch,
};

const char *retail_projection_receipt_error_name(
    RetailProjectionReceiptError error) noexcept;

struct RetailProjectionReceiptPreflight final {
  RetailProjectionReceiptError error{
      RetailProjectionReceiptError::InvalidArgument};
  std::string detail;
  Sha256Digest receipt_sha256{};
  Sha256Digest replay_sha256{};
  bool native_output_verified{};
  bool source_lineage_verified{};

  bool passed() const noexcept {
    return error == RetailProjectionReceiptError::None;
  }
};

// Both paths are bounded and read in binary mode.  replay must be the object
// already accepted by RetailSessionReplay::read_file; cache_index_sha256 must
// come from the currently opened RetailContentStore.
RetailProjectionReceiptPreflight
preflight_retail_projection_receipt(const std::filesystem::path &receipt_path,
                                    const std::filesystem::path &replay_path,
                                    const RetailSessionReplay &replay,
                                    const Sha256Digest &cache_index_sha256);

} // namespace ac6::retail
