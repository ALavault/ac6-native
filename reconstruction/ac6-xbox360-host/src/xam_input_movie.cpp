#include "ac6xbox360/xam_input_movie.hpp"

#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <sstream>
#include <utility>

namespace ac6xbox360 {
namespace {

constexpr std::size_t kMaxLineBytes = 1024U * 1024U;

[[nodiscard]] bool set_error(std::string message, std::string &error) {
  error = std::move(message);
  return false;
}

[[nodiscard]] bool lowercase_sha256(std::string_view value) noexcept {
  return value.size() == SHA256_DIGEST_LENGTH * 2U &&
         std::ranges::all_of(value, [](char byte) {
           return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
         });
}

[[nodiscard]] std::string sha256(std::string_view value) {
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char *>(value.data()), value.size(),
         digest.data());
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result(digest.size() * 2U, '0');
  for (std::size_t index = 0U; index < digest.size(); ++index) {
    result[index * 2U] = kHex[digest[index] >> 4U];
    result[index * 2U + 1U] = kHex[digest[index] & 0x0FU];
  }
  return result;
}

[[nodiscard]] std::string json_string(std::string_view value) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(value.size() + 2U);
  result.push_back('"');
  for (const unsigned char byte : value) {
    switch (byte) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (byte < 0x20U) {
        result += "\\u00";
        result.push_back(kHex[byte >> 4U]);
        result.push_back(kHex[byte & 0x0FU]);
      } else {
        result.push_back(static_cast<char>(byte));
      }
      break;
    }
  }
  result.push_back('"');
  return result;
}

[[nodiscard]] bool valid_identity(const XamInputMovieIdentity &identity,
                                  std::string &error) {
  if (identity.target_id.empty() || identity.module.empty() ||
      identity.ghidra_project.empty() || identity.ghidra_language.empty() ||
      identity.base_address == 0U || !lowercase_sha256(identity.xex_sha256)) {
    return set_error("invalid XAM movie identity", error);
  }
  return true;
}

[[nodiscard]] std::string header(const XamInputMovieIdentity &identity) {
  return "{\"kind\":\"header\",\"schema\":" +
         json_string(kXamInputMovieSchema) +
         ",\"target\":{\"target_id\":" + json_string(identity.target_id) +
         ",\"module\":" + json_string(identity.module) +
         ",\"xex_sha256\":" + json_string(identity.xex_sha256) +
         ",\"base_address\":" + std::to_string(identity.base_address) +
         ",\"ghidra_project\":" + json_string(identity.ghidra_project) +
         ",\"ghidra_language\":" + json_string(identity.ghidra_language) +
         "}}\n";
}

[[nodiscard]] std::string state_hex(const XamInputState &state) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result(state.size() * 2U, '0');
  for (std::size_t index = 0U; index < state.size(); ++index) {
    const auto byte = std::to_integer<std::uint8_t>(state[index]);
    result[index * 2U] = kHex[byte >> 4U];
    result[index * 2U + 1U] = kHex[byte & 0x0FU];
  }
  return result;
}

[[nodiscard]] std::string event_line(const XamInputObservation &event) {
  return "{\"kind\":\"XamInputGetState\",\"ordinal\":" +
         std::to_string(event.ordinal) +
         ",\"guest_tick\":" + std::to_string(event.guest_tick) +
         ",\"guest_thread\":" + std::to_string(event.guest_thread) +
         ",\"caller_lr\":" + std::to_string(event.caller_lr) +
         ",\"user\":" + std::to_string(event.user) +
         ",\"flags\":" + std::to_string(event.flags) + ",\"state_ptr_null\":" +
         std::string(event.state_ptr_null ? "true" : "false") +
         ",\"result\":" + std::to_string(event.result) +
         ",\"state16\":" + json_string(state_hex(event.state)) + "}\n";
}

[[nodiscard]] std::string normalized_line(const XamInputObservation &event) {
  return "{\"kind\":\"XamInputGetState\",\"ordinal\":" +
         std::to_string(event.ordinal) +
         ",\"caller_lr\":" + std::to_string(event.caller_lr) +
         ",\"user\":" + std::to_string(event.user) +
         ",\"flags\":" + std::to_string(event.flags) + ",\"state_ptr_null\":" +
         std::string(event.state_ptr_null ? "true" : "false") +
         ",\"result\":" + std::to_string(event.result) +
         ",\"state16\":" + json_string(state_hex(event.state)) + "}\n";
}

[[nodiscard]] std::string footer_base(std::uint64_t count) {
  return "{\"kind\":\"footer\",\"event_count\":" + std::to_string(count) +
         "}\n";
}

[[nodiscard]] std::string footer_line(std::uint64_t count,
                                      std::string_view payload_sha256,
                                      std::string_view normalized_sha256) {
  return "{\"kind\":\"footer\",\"event_count\":" + std::to_string(count) +
         ",\"payload_sha256\":" + json_string(payload_sha256) +
         ",\"normalized_sha256\":" + json_string(normalized_sha256) + "}\n";
}

class Cursor final {
public:
  explicit Cursor(std::string_view input) : input_(input) {}

  [[nodiscard]] bool consume(std::string_view expected) noexcept {
    if (!input_.starts_with(expected)) {
      return false;
    }
    input_.remove_prefix(expected.size());
    return true;
  }

  [[nodiscard]] bool unsigned_value(std::uint64_t maximum,
                                    std::uint64_t &output) noexcept {
    if (input_.empty() || input_.front() < '0' || input_.front() > '9') {
      return false;
    }
    const auto end = input_.find_first_not_of("0123456789");
    const auto length = end == std::string_view::npos ? input_.size() : end;
    const auto digits = input_.substr(0U, length);
    if (digits.size() > 1U && digits.front() == '0') {
      return false;
    }
    std::uint64_t value{};
    const auto parsed =
        std::from_chars(digits.data(), digits.data() + digits.size(), value);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != digits.data() + digits.size() || value > maximum) {
      return false;
    }
    input_.remove_prefix(digits.size());
    output = value;
    return true;
  }

  [[nodiscard]] bool boolean(bool &output) noexcept {
    if (consume("true")) {
      output = true;
      return true;
    }
    if (consume("false")) {
      output = false;
      return true;
    }
    return false;
  }

  [[nodiscard]] bool sha(std::string &output) noexcept {
    if (input_.size() < 66U || input_.front() != '"' || input_[65U] != '"') {
      return false;
    }
    const auto value = input_.substr(1U, 64U);
    if (!lowercase_sha256(value)) {
      return false;
    }
    output.assign(value);
    input_.remove_prefix(66U);
    return true;
  }

  [[nodiscard]] bool state(XamInputState &output) noexcept {
    if (input_.size() < kXamInputStateBytes * 2U + 2U ||
        input_.front() != '"' || input_[kXamInputStateBytes * 2U + 1U] != '"') {
      return false;
    }
    auto nibble = [](char byte) -> int {
      if (byte >= '0' && byte <= '9') {
        return byte - '0';
      }
      if (byte >= 'a' && byte <= 'f') {
        return byte - 'a' + 10;
      }
      return -1;
    };
    for (std::size_t index = 0U; index < output.size(); ++index) {
      const int high = nibble(input_[index * 2U + 1U]);
      const int low = nibble(input_[index * 2U + 2U]);
      if (high < 0 || low < 0) {
        return false;
      }
      output[index] = static_cast<std::byte>((high << 4) | low);
    }
    input_.remove_prefix(kXamInputStateBytes * 2U + 2U);
    return true;
  }

  [[nodiscard]] bool empty() const noexcept { return input_.empty(); }

private:
  std::string_view input_;
};

[[nodiscard]] bool parse_event(std::string_view line, std::uint64_t expected,
                               XamInputObservation &output,
                               std::string &error) {
  Cursor cursor(line);
  std::uint64_t ordinal{}, tick{}, thread{}, caller{}, user{}, flags{},
      result{};
  bool pointer_null{}, has_state{};
  XamInputState state{};
  if (!cursor.consume("{\"kind\":\"XamInputGetState\",\"ordinal\":") ||
      !cursor.unsigned_value(kMaxXamInputMovieEvents, ordinal) ||
      !cursor.consume(",\"guest_tick\":") ||
      !cursor.unsigned_value(std::numeric_limits<std::uint64_t>::max(), tick) ||
      !cursor.consume(",\"guest_thread\":") ||
      !cursor.unsigned_value(std::numeric_limits<std::uint32_t>::max(),
                             thread) ||
      !cursor.consume(",\"caller_lr\":") ||
      !cursor.unsigned_value(std::numeric_limits<std::uint32_t>::max(),
                             caller) ||
      !cursor.consume(",\"user\":") ||
      !cursor.unsigned_value(std::numeric_limits<std::uint32_t>::max(), user) ||
      !cursor.consume(",\"flags\":") ||
      !cursor.unsigned_value(std::numeric_limits<std::uint32_t>::max(),
                             flags) ||
      !cursor.consume(",\"state_ptr_null\":") ||
      !cursor.boolean(pointer_null) || !cursor.consume(",\"result\":") ||
      !cursor.unsigned_value(std::numeric_limits<std::uint32_t>::max(),
                             result) ||
      !cursor.consume(",\"state16\":") || !cursor.state(state) ||
      !cursor.consume("}\n") || !cursor.empty()) {
    return set_error("event type or canonical shape mismatch", error);
  }
  if (ordinal != expected) {
    return set_error("event ordinal mismatch", error);
  }
  has_state = !pointer_null && result == 0U;
  output = XamInputObservation{ordinal,
                               tick,
                               static_cast<std::uint32_t>(thread),
                               static_cast<std::uint32_t>(caller),
                               static_cast<std::uint32_t>(user),
                               static_cast<std::uint32_t>(flags),
                               pointer_null,
                               static_cast<std::uint32_t>(result),
                               has_state,
                               state};
  return true;
}

struct Footer final {
  std::uint64_t count{};
  std::string payload_sha256;
  std::string normalized_sha256;
};

[[nodiscard]] bool parse_footer(std::string_view line, Footer &output,
                                std::string &error) {
  Cursor cursor(line);
  std::uint64_t count{};
  std::string payload;
  std::string normalized;
  if (!cursor.consume("{\"kind\":\"footer\",\"event_count\":") ||
      !cursor.unsigned_value(kMaxXamInputMovieEvents, count) ||
      !cursor.consume(",\"payload_sha256\":") || !cursor.sha(payload) ||
      !cursor.consume(",\"normalized_sha256\":") || !cursor.sha(normalized) ||
      !cursor.consume("}\n") || !cursor.empty()) {
    return set_error("footer canonical shape mismatch", error);
  }
  output = Footer{count, std::move(payload), std::move(normalized)};
  return true;
}

[[nodiscard]] bool split_lines(std::string_view bytes,
                               std::vector<std::string_view> &lines,
                               std::string &error) {
  if (bytes.empty() || bytes.size() > kMaxXamInputMovieBytes ||
      bytes.back() != '\n' || bytes.find('\r') != std::string_view::npos) {
    return set_error("movie framing or byte bound", error);
  }
  for (std::size_t start = 0U; start < bytes.size();) {
    const auto end = bytes.find('\n', start);
    if (end == std::string_view::npos || end - start + 1U > kMaxLineBytes ||
        lines.size() >= kMaxXamInputMovieEvents + 2U) {
      return set_error("movie line or event bound", error);
    }
    lines.push_back(bytes.substr(start, end - start + 1U));
    start = end + 1U;
  }
  if (lines.size() < 2U) {
    return set_error("trace-length mismatch", error);
  }
  return true;
}

} // namespace

void XamInputMovie::reset() noexcept {
  mode_ = XamInputMovieMode::Disabled;
  failed_ = false;
  failure_.clear();
  header_.clear();
  body_.clear();
  normalized_.clear();
  expected_normalized_sha256_.clear();
  replay_events_.clear();
  replay_cursor_ = 0U;
  ordinal_ = 0U;
}

bool XamInputMovie::fail(std::string message, std::string &error) {
  failed_ = true;
  failure_ = std::move(message);
  error = failure_;
  return false;
}

bool XamInputMovie::begin_record(const XamInputMovieIdentity &identity,
                                 std::string &error) {
  reset();
  if (!valid_identity(identity, error)) {
    return false;
  }
  header_ = header(identity);
  body_ = header_;
  mode_ = XamInputMovieMode::Record;
  return true;
}

bool XamInputMovie::begin_replay(std::string_view movie,
                                 const XamInputMovieIdentity &identity,
                                 std::string &error) {
  reset();
  if (!valid_identity(identity, error)) {
    return false;
  }
  const std::string expected_header = header(identity);
  std::vector<std::string_view> lines;
  if (!split_lines(movie, lines, error)) {
    return false;
  }
  if (lines.front() != expected_header) {
    return set_error("movie target/project/language identity mismatch", error);
  }
  Footer footer;
  if (!parse_footer(lines.back(), footer, error) || footer.count == 0U ||
      footer.count != lines.size() - 2U) {
    return set_error("trace-length mismatch", error);
  }
  const auto footer_offset = movie.size() - lines.back().size();
  std::string digest_input(movie.substr(0U, footer_offset));
  digest_input += footer_base(footer.count);
  if (sha256(digest_input) != footer.payload_sha256) {
    return set_error("payload digest mismatch", error);
  }
  replay_events_.reserve(static_cast<std::size_t>(footer.count));
  for (std::size_t index = 1U; index + 1U < lines.size(); ++index) {
    XamInputObservation parsed;
    if (!parse_event(lines[index], index - 1U, parsed, error)) {
      reset();
      return false;
    }
    normalized_ += normalized_line(parsed);
    replay_events_.emplace_back(lines[index]);
  }
  if (sha256(normalized_) != footer.normalized_sha256) {
    reset();
    return set_error("normalized digest mismatch", error);
  }
  normalized_.clear();
  expected_normalized_sha256_ = std::move(footer.normalized_sha256);
  header_ = expected_header;
  mode_ = XamInputMovieMode::Replay;
  return true;
}

bool XamInputMovie::record(const XamInputObservation &observation,
                           std::string &error) {
  if (mode_ != XamInputMovieMode::Record || failed_) {
    return fail("record mode", error);
  }
  if (observation.ordinal != ordinal_ || ordinal_ >= kMaxXamInputMovieEvents ||
      observation.has_state !=
          (!observation.state_ptr_null && observation.result == 0U)) {
    return fail("record event invariant", error);
  }
  const std::string line = event_line(observation);
  if (body_.size() + line.size() > kMaxXamInputMovieBytes) {
    return fail("movie byte bound", error);
  }
  body_ += line;
  normalized_ += normalized_line(observation);
  ++ordinal_;
  return true;
}

bool XamInputMovie::replay(const XamInputReplayGuards &guards,
                           XamInputReplayValue &output, std::string &error) {
  if (mode_ != XamInputMovieMode::Replay || failed_) {
    return fail("replay mode", error);
  }
  if (replay_cursor_ >= replay_events_.size()) {
    return fail("trace-length mismatch: unexpected XamInputGetState", error);
  }
  XamInputObservation observation;
  if (!parse_event(replay_events_[replay_cursor_], ordinal_, observation,
                   error)) {
    const std::string detail = error;
    return fail(detail, error);
  }
  auto mismatch = [this, &error](std::string_view field) {
    return fail("strict replay mismatch: " + std::string(field) +
                    " at ordinal " + std::to_string(ordinal_),
                error);
  };
  if (observation.caller_lr != guards.caller_lr) {
    return mismatch("caller_lr");
  }
  if (observation.user != guards.user) {
    return mismatch("user");
  }
  if (observation.flags != guards.flags) {
    return mismatch("flags");
  }
  if (observation.state_ptr_null != guards.state_ptr_null) {
    return mismatch("state_ptr_null");
  }
  output = XamInputReplayValue{observation.result, observation.has_state,
                               observation.state};
  normalized_ += normalized_line(observation);
  ++replay_cursor_;
  ++ordinal_;
  return true;
}

bool XamInputMovie::finalize(std::string &sealed_movie, std::string &error) {
  sealed_movie.clear();
  if (failed_) {
    return set_error(failure_, error);
  }
  if (mode_ == XamInputMovieMode::Disabled) {
    return true;
  }
  if (mode_ == XamInputMovieMode::Replay) {
    if (replay_cursor_ != replay_events_.size()) {
      return fail("trace-length mismatch: unconsumed events", error);
    }
    if (sha256(normalized_) != expected_normalized_sha256_) {
      return fail("normalized XAM event stream mismatch", error);
    }
    reset();
    return true;
  }
  if (ordinal_ == 0U) {
    return fail("empty movie", error);
  }
  const std::string digest_input = body_ + footer_base(ordinal_);
  sealed_movie =
      body_ + footer_line(ordinal_, sha256(digest_input), sha256(normalized_));
  if (sealed_movie.size() > kMaxXamInputMovieBytes) {
    sealed_movie.clear();
    return fail("sealed movie byte bound", error);
  }
  reset();
  return true;
}

} // namespace ac6xbox360
