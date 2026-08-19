#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace ac6demo {

// Transactional content version for one renderer upload. Call needs_upload()
// before touching host or Vulkan resources, and mark_uploaded() only after the
// complete upload has succeeded. Failed uploads therefore never advance the
// visible generation or poison later retries.
class RendererPayloadVersion final {
public:
  [[nodiscard]] bool needs_upload(std::string_view digest) const noexcept {
    return digest_ != digest;
  }

  void validate_candidate(std::string_view digest) const {
    if (!valid_sha256(digest)) {
      throw std::invalid_argument{"renderer payload digest is not SHA-256"};
    }
    if (digest_ != digest &&
        generation_ == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error{"renderer payload generation overflow"};
    }
  }

  void mark_uploaded(std::string digest) {
    validate_candidate(digest);
    if (digest_ == digest) {
      return;
    }
    digest_ = std::move(digest);
    ++generation_;
  }

  void reset() noexcept {
    digest_.clear();
    generation_ = 0U;
  }

  [[nodiscard]] const std::string &digest() const noexcept { return digest_; }
  [[nodiscard]] std::uint64_t generation() const noexcept {
    return generation_;
  }
  [[nodiscard]] bool initialized() const noexcept { return generation_ != 0U; }

  [[nodiscard]] static bool valid_sha256(std::string_view digest) noexcept {
    if (digest.size() != 64U) {
      return false;
    }
    for (const char character : digest) {
      const bool decimal = character >= '0' && character <= '9';
      const bool lower_hex = character >= 'a' && character <= 'f';
      if (!decimal && !lower_hex) {
        return false;
      }
    }
    return true;
  }

private:
  std::string digest_;
  std::uint64_t generation_{};
};

} // namespace ac6demo
