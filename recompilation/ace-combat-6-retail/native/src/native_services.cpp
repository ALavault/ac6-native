#include "ac6/native_services.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <system_error>

namespace ac6::native {
namespace {

bool safe_component(std::string_view value) noexcept {
  if (value.empty() || value == "." || value == "..") return false;
  for (const char character : value) {
    if (character == '/' || character == '\\' ||
        static_cast<unsigned char>(character) < 0x20u) {
      return false;
    }
  }
  return true;
}

bool same_name(std::string_view left, std::string_view right) noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0u; index < left.size(); ++index) {
    const auto a = static_cast<unsigned char>(left[index]);
    const auto b = static_cast<unsigned char>(right[index]);
    if (std::tolower(a) != std::tolower(b)) return false;
  }
  return true;
}

ServiceError read_file(const std::filesystem::path& path,
                       std::vector<std::uint8_t>& bytes) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return ServiceError::kNotFound;
  stream.seekg(0, std::ios::end);
  const std::streamoff size = stream.tellg();
  if (size < 0 || static_cast<std::uintmax_t>(size) > (1u << 30u)) {
    return ServiceError::kCorrupt;
  }
  stream.seekg(0, std::ios::beg);
  bytes.resize(static_cast<std::size_t>(size));
  if (!bytes.empty()) {
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!stream) return ServiceError::kCorrupt;
  }
  return ServiceError::kNone;
}

std::filesystem::path find_case_insensitive(const std::filesystem::path& root,
                                            std::string_view name,
                                            bool& ambiguous) {
  ambiguous = false;
  std::error_code error;
  std::filesystem::path match;
  for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
    if (error) break;
    if (same_name(entry.path().filename().string(), name)) {
      if (!match.empty()) {
        ambiguous = true;
        return {};
      }
      match = entry.path();
    }
  }
  return match;
}

}  // namespace

std::uint32_t HandleTable::allocate() noexcept {
  try {
    for (std::uint32_t attempts = 0u; attempts != UINT32_MAX; ++attempts) {
      const std::uint32_t candidate = next_++;
      if (candidate != 0u && live_.insert(candidate).second) return candidate;
    }
  } catch (...) {
    return 0u;
  }
  return 0u;
}

bool HandleTable::close(std::uint32_t handle) noexcept {
  return live_.erase(handle) == 1u;
}

bool HandleTable::contains(std::uint32_t handle) const noexcept {
  return live_.find(handle) != live_.end();
}

ConfinedVfs::ConfinedVfs(std::filesystem::path root)
    : root_(std::filesystem::absolute(std::move(root)).lexically_normal()) {
  std::error_code error;
  valid_root_ = std::filesystem::is_directory(root_, error) && !error;
}

ServiceError ConfinedVfs::resolve(std::string_view relative,
                                  std::filesystem::path& result) const {
  if (!valid_root_ || relative.empty() || relative.front() == '/' ||
      relative.find('\\') != std::string_view::npos) {
    return ServiceError::kInvalidPath;
  }
  std::filesystem::path current = root_;
  std::size_t begin = 0u;
  while (begin < relative.size()) {
    const std::size_t end = relative.find('/', begin);
    const std::size_t length = end == std::string_view::npos
                                   ? relative.size() - begin
                                   : end - begin;
    const std::string_view component = relative.substr(begin, length);
    if (!safe_component(component)) return ServiceError::kInvalidPath;
    std::filesystem::path match;
    std::error_code iterator_error;
    for (const auto& entry : std::filesystem::directory_iterator(current,
                                                                   iterator_error)) {
      if (iterator_error) break;
      if (same_name(entry.path().filename().string(), component)) {
        if (!match.empty()) return ServiceError::kInvalidPath;
        match = entry.path();
      }
    }
    if (iterator_error) return ServiceError::kNotFound;
    if (match.empty()) return ServiceError::kNotFound;
    current = match;
    if (end == std::string_view::npos) break;
    begin = end + 1u;
  }
  result = current.lexically_normal();
  std::error_code error;
  if (!std::filesystem::is_regular_file(result, error) || error) {
    return ServiceError::kNotFound;
  }
  const std::filesystem::path canonical_root =
      std::filesystem::weakly_canonical(root_, error);
  if (error) return ServiceError::kInvalidPath;
  const std::filesystem::path canonical_result =
      std::filesystem::weakly_canonical(result, error);
  if (error) return ServiceError::kInvalidPath;
  const std::filesystem::path relative_path =
      canonical_result.lexically_relative(canonical_root);
  if (relative_path.empty() || relative_path == ".." ||
      relative_path.begin()->string() == "..") {
    return ServiceError::kInvalidPath;
  }
  return ServiceError::kNone;
}

ServiceError ConfinedVfs::read(std::string_view relative,
                               std::vector<std::uint8_t>& bytes) const {
  std::filesystem::path path;
  const ServiceError status = resolve(relative, path);
  if (status != ServiceError::kNone) return status;
  return read_file(path, bytes);
}

AtomicSaveStore::AtomicSaveStore(std::filesystem::path root)
    : root_(std::filesystem::absolute(std::move(root)).lexically_normal()) {
  std::error_code error;
  std::filesystem::create_directories(root_, error);
  if (error) root_.clear();
}

ServiceError AtomicSaveStore::write(std::string_view name,
                                    std::span<const std::uint8_t> bytes) {
  if (root_.empty() || !safe_component(name)) return ServiceError::kInvalidPath;
  bool ambiguous = false;
  const std::filesystem::path existing =
      find_case_insensitive(root_, name, ambiguous);
  if (ambiguous) return ServiceError::kInvalidPath;
  const std::filesystem::path destination =
      existing.empty() ? root_ / std::string(name) : existing;
  const std::filesystem::path temporary =
      root_ / ("." + std::string(name) + ".tmp." +
               std::to_string(++temporary_counter_));
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) return ServiceError::kBusy;
    if (!bytes.empty()) {
      stream.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }
    stream.flush();
    if (!stream) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return ServiceError::kBusy;
    }
  }
  std::error_code error;
  std::filesystem::rename(temporary, destination, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    return ServiceError::kBusy;
  }
  return ServiceError::kNone;
}

ServiceError AtomicSaveStore::read(std::string_view name,
                                   std::vector<std::uint8_t>& bytes) const {
  if (root_.empty() || !safe_component(name)) return ServiceError::kInvalidPath;
  bool ambiguous = false;
  const std::filesystem::path existing =
      find_case_insensitive(root_, name, ambiguous);
  if (ambiguous) return ServiceError::kInvalidPath;
  if (existing.empty()) return ServiceError::kNotFound;
  return read_file(existing, bytes);
}

StrictInputReplay::StrictInputReplay(std::vector<ReplaySample> samples)
    : samples_(std::move(samples)) {}

ServiceError StrictInputReplay::poll(std::uint64_t tick, std::uint32_t user,
                                     ReplaySample& sample) noexcept {
  if (cursor_ >= samples_.size()) return ServiceError::kPollMismatch;
  const ReplaySample& expected = samples_[cursor_];
  if (expected.tick != tick || expected.user != user) {
    return ServiceError::kPollMismatch;
  }
  sample = expected;
  ++cursor_;
  return ServiceError::kNone;
}

ServiceError StrictInputReplay::finalize() const noexcept {
  return cursor_ == samples_.size() ? ServiceError::kNone
                                    : ServiceError::kReplayIncomplete;
}

XmaDoubleBuffer::XmaDoubleBuffer(std::size_t slot_capacity)
    : slot_capacity_(slot_capacity) {}

ServiceError XmaDoubleBuffer::push(std::span<const std::uint8_t> packet) {
  if (packet.size() > slot_capacity_) return ServiceError::kCorrupt;
  if (queued_ == 2u) return ServiceError::kRingFull;
  slots_[write_slot_].assign(packet.begin(), packet.end());
  write_slot_ = (write_slot_ + 1u) % 2u;
  ++queued_;
  return ServiceError::kNone;
}

ServiceError XmaDoubleBuffer::pop(std::vector<std::uint8_t>& packet) {
  if (queued_ == 0u) return ServiceError::kRingEmpty;
  packet = std::move(slots_[read_slot_]);
  slots_[read_slot_].clear();
  read_slot_ = (read_slot_ + 1u) % 2u;
  --queued_;
  return ServiceError::kNone;
}

}  // namespace ac6::native
