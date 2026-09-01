#include "ac6/native_guest_media.h"

#include "ac6/native_xdvdfs.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace ac6::native {

void NativeGuestMediaService::bind(MediaInput media) noexcept {
  std::lock_guard lock(mutex_);
  media_ = std::move(media);
  bound_ = true;
}

std::optional<std::uint32_t> NativeGuestMediaService::open_file(
    std::string_view relative_path) noexcept {
  std::lock_guard lock(mutex_);
  if (!bound_) return std::nullopt;

  OpenFile opened;

  if (media_.kind == MediaKind::kIso) {
    // This product's own data packages (DATA00.PAC, DATA01.PAC) run past
    // 2GiB, so the file is never pulled into memory here -- only its
    // offset/size is resolved, and each read_file() call streams directly
    // from the ISO. See native_xdvdfs.h's own note on why read_xdvdfs_file
    // (an eager, size-capped copy) is the wrong tool for this.
    XdvdfsFile located;
    std::string error;
    if (!locate_xdvdfs_file(media_.path, relative_path, located, &error)) {
      return std::nullopt;
    }
    opened.streamed = true;
    opened.iso_offset = located.iso_offset;
    opened.size = located.size;
  } else {
    // A real title package can run to tens of megabytes; bound it well
    // above any single asset file this product has seen without being
    // unbounded.
    constexpr std::size_t kMaximumFileSize = 512u * 1024u * 1024u;
    std::vector<std::uint8_t>& bytes = opened.bytes;
    std::error_code error;
    const std::filesystem::path candidate = media_.path / relative_path;
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
      return std::nullopt;
    }
    const std::uintmax_t size = std::filesystem::file_size(candidate, error);
    if (error || size > kMaximumFileSize) return std::nullopt;
    std::ifstream stream(candidate, std::ios::binary);
    if (!stream) return std::nullopt;
    bytes.resize(static_cast<std::size_t>(size));
    if (!bytes.empty() &&
        !stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()))) {
      return std::nullopt;
    }
  }

  const std::uint32_t handle = next_handle_++;
  files_[handle] = std::move(opened);
  return handle;
}

bool NativeGuestMediaService::read_file(std::uint32_t handle,
                                        std::uint64_t offset,
                                        std::uint8_t* dest,
                                        std::uint32_t length,
                                        std::uint32_t& bytes_read) noexcept {
  std::lock_guard lock(mutex_);
  const auto it = files_.find(handle);
  if (it == files_.end()) {
    bytes_read = 0u;
    return false;
  }
  const OpenFile& opened = it->second;
  const std::uint64_t file_size =
      opened.streamed ? opened.size : opened.bytes.size();
  if (offset >= file_size) {
    bytes_read = 0u;
    return true;
  }
  const std::uint64_t available = file_size - offset;
  const std::uint32_t copy_length = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(available, length));
  if (copy_length != 0u && dest != nullptr) {
    if (opened.streamed) {
      std::ifstream stream(media_.path, std::ios::binary);
      if (!stream) {
        bytes_read = 0u;
        return true;
      }
      stream.seekg(static_cast<std::streamoff>(opened.iso_offset + offset),
                   std::ios::beg);
      stream.read(reinterpret_cast<char*>(dest),
                  static_cast<std::streamsize>(copy_length));
      if (!stream) {
        bytes_read = 0u;
        return true;
      }
    } else {
      std::memcpy(dest, opened.bytes.data() + offset, copy_length);
    }
  }
  bytes_read = copy_length;
  return true;
}

void NativeGuestMediaService::close_file(std::uint32_t handle) noexcept {
  std::lock_guard lock(mutex_);
  files_.erase(handle);
}

NativeGuestMediaService& native_guest_media_service() noexcept {
  static NativeGuestMediaService service;
  return service;
}

}  // namespace ac6::native
