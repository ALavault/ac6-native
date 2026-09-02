#pragma once

#include "ac6/native_frontend.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ac6::native {

// Guest-facing file service used by the build-only Xenon import boundary
// (NtCreateFile/NtReadFile, tools/materialize_native_import_stubs.py). Bound
// once at boot to the same MediaInput the runtime already validated (ISO or
// assets directory); never writes retail bytes, only reads them.
class NativeGuestMediaService final {
 public:
  NativeGuestMediaService() = default;
  NativeGuestMediaService(const NativeGuestMediaService&) = delete;
  NativeGuestMediaService& operator=(const NativeGuestMediaService&) = delete;

  void bind(MediaInput media) noexcept;

  // Opens a guest-relative path (already stripped of any drive prefix, with
  // backslashes converted to the host separator) and caches its full
  // contents. Returns a nonzero handle on success; a real file that does not
  // exist in the bound media is a normal, expected failure (nullopt), not an
  // error -- this project never fabricates missing retail content.
  [[nodiscard]] std::optional<std::uint32_t> open_file(
      std::string_view relative_path) noexcept;

  // Copies up to `length` bytes starting at `offset` from the cached file
  // into `dest` (a raw host pointer, already resolved from the guest
  // buffer). Returns false only for an unknown handle; a read past
  // end-of-file returns true with `bytes_read` set to whatever remains
  // (possibly zero).
  [[nodiscard]] bool read_file(std::uint32_t handle, std::uint64_t offset,
                               std::uint8_t* dest, std::uint32_t length,
                               std::uint32_t& bytes_read) noexcept;

  void close_file(std::uint32_t handle) noexcept;

  // Returns the real size of an already-open file, or nullopt for an
  // unknown handle -- used by imports that report file attributes/size
  // without a full open/read/close cycle (e.g. NtQueryFullAttributesFile).
  [[nodiscard]] std::optional<std::uint64_t> file_size(
      std::uint32_t handle) noexcept;

 private:
  struct OpenFile final {
    // Assets-directory mode (small, dev-only fixtures): fully cached.
    // ISO mode: streamed directly from `iso_offset`/`size` on every read,
    // since a real title's data packages run past 2GiB and this project
    // never pulls a file that size into memory at once.
    std::vector<std::uint8_t> bytes;
    bool streamed{};
    std::uint64_t iso_offset{};
    std::uint64_t size{};
  };

  std::mutex mutex_;
  MediaInput media_;
  bool bound_{};
  std::unordered_map<std::uint32_t, OpenFile> files_;
  std::uint32_t next_handle_{1u};
};

NativeGuestMediaService& native_guest_media_service() noexcept;

}  // namespace ac6::native
