#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ac6xbox360 {

inline constexpr std::string_view kXamInputMovieSchema =
    "ac6.xam-input-movie.v1";
inline constexpr std::size_t kXamInputStateBytes = 16U;
inline constexpr std::size_t kMaxXamInputMovieBytes = 128U * 1024U * 1024U;
inline constexpr std::size_t kMaxXamInputMovieEvents = 1'000'000U;

using XamInputState = std::array<std::byte, kXamInputStateBytes>;

// Target identity is supplied by the product adapter.  The host component
// deliberately contains no title PC, project name, XEX identity, or language.
struct XamInputMovieIdentity final {
  std::string target_id;
  std::string module;
  std::string xex_sha256;
  std::uint32_t base_address{};
  std::string ghidra_project;
  std::string ghidra_language;

  bool operator==(const XamInputMovieIdentity &) const = default;
};

struct XamInputObservation final {
  std::uint64_t ordinal{};
  std::uint64_t guest_tick{};
  std::uint32_t guest_thread{};
  std::uint32_t caller_lr{};
  std::uint32_t user{};
  std::uint32_t flags{};
  bool state_ptr_null{true};
  std::uint32_t result{};
  bool has_state{};
  XamInputState state{};
};

struct XamInputReplayGuards final {
  std::uint32_t caller_lr{};
  std::uint32_t user{};
  std::uint32_t flags{};
  bool state_ptr_null{true};
};

struct XamInputReplayValue final {
  std::uint32_t result{};
  bool has_state{};
  XamInputState state{};
};

enum class XamInputMovieMode : std::uint8_t { Disabled, Record, Replay };

class XamInputMovie final {
public:
  [[nodiscard]] bool begin_record(const XamInputMovieIdentity &identity,
                                  std::string &error);
  [[nodiscard]] bool begin_replay(std::string_view movie,
                                  const XamInputMovieIdentity &identity,
                                  std::string &error);
  [[nodiscard]] bool record(const XamInputObservation &observation,
                            std::string &error);
  [[nodiscard]] bool replay(const XamInputReplayGuards &guards,
                            XamInputReplayValue &output, std::string &error);
  [[nodiscard]] bool finalize(std::string &sealed_movie, std::string &error);

  [[nodiscard]] XamInputMovieMode mode() const noexcept { return mode_; }
  [[nodiscard]] std::uint64_t ordinal() const noexcept { return ordinal_; }
  [[nodiscard]] bool replay_consumed() const noexcept {
    return mode_ == XamInputMovieMode::Replay &&
           replay_cursor_ == replay_events_.size();
  }

private:
  [[nodiscard]] bool fail(std::string message, std::string &error);
  void reset() noexcept;

  XamInputMovieMode mode_{XamInputMovieMode::Disabled};
  bool failed_{};
  std::string failure_;
  std::string header_;
  std::string body_;
  std::string normalized_;
  std::string expected_normalized_sha256_;
  std::vector<std::string> replay_events_;
  std::size_t replay_cursor_{};
  std::uint64_t ordinal_{};
};

} // namespace ac6xbox360
