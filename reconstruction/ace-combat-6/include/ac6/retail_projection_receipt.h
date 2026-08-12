#pragma once

#include "ac6/retail_session_replay.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace ac6::retail {

// The receipt is an integrity sidecar for a controller projection.  This
// preflight proves that its native-output claims describe the exact replay
// file and the cache opened by the caller.  Source replay lineage is parsed
// structurally, but cannot be proved without the raw/parent replay artefacts.
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
