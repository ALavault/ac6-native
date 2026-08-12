#include "ac6/retail_projection_receipt.h"

#include "ac6/sha256.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ac6::retail {
namespace {

constexpr std::uint64_t kMaximumReceiptBytes = 64u * 1024u;
constexpr std::uint64_t kMaximumReplayBytes =
    121u + RetailSessionReplay::kMaximumFrames * 9u +
    RetailSessionReplay::kMaximumCheckpoints * 36u;
constexpr std::size_t kMaximumJsonDepth = 16u;
constexpr std::size_t kMaximumJsonNodes = 1024u;
constexpr std::size_t kMaximumJsonMembers = 128u;
constexpr std::size_t kMaximumJsonArrayItems = 256u;
constexpr std::size_t kMaximumJsonStringBytes = 4096u;
constexpr std::string_view kSchema =
    "ac6.native-controller-projection-receipt.v1";
constexpr std::array<std::uint8_t, 9> kReplayMagic{'A', 'C', '6', 'R', 'T',
                                                   'P', 'L', 'Y', 0};

enum class JsonKind : std::uint8_t {
  Null,
  Boolean,
  Integer,
  String,
  Array,
  Object,
};

struct JsonValue final {
  JsonKind kind{JsonKind::Null};
  bool boolean{};
  std::uint64_t integer{};
  std::string string;
  std::vector<JsonValue> array;
  std::vector<std::pair<std::string, JsonValue>> object;
};

enum class JsonStatus : std::uint8_t {
  Ok,
  Invalid,
  Bound,
  NonCanonical,
};

void append_utf8(std::string &output, std::uint32_t codepoint) {
  if (codepoint <= 0x7fu) {
    output.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ffu) {
    output.push_back(static_cast<char>(0xc0u | (codepoint >> 6u)));
    output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
  } else if (codepoint <= 0xffffu) {
    output.push_back(static_cast<char>(0xe0u | (codepoint >> 12u)));
    output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
    output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
  } else {
    output.push_back(static_cast<char>(0xf0u | (codepoint >> 18u)));
    output.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3fu)));
    output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
    output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
  }
}

class CanonicalJsonParser final {
public:
  explicit CanonicalJsonParser(std::string_view source) : source_(source) {}

  JsonStatus parse(JsonValue &output) {
    if (source_.size() < 2u || source_.back() != '\n' ||
        source_.find('\n') != source_.size() - 1u ||
        source_.find('\r') != std::string_view::npos) {
      return JsonStatus::NonCanonical;
    }
    limit_ = source_.size() - 1u;
    if (!parse_value(output, 0u) || cursor_ != limit_) {
      if (status_ == JsonStatus::Ok)
        status_ = JsonStatus::Invalid;
      return status_;
    }
    return status_;
  }

private:
  bool fail(JsonStatus status) {
    if (status_ == JsonStatus::Ok)
      status_ = status;
    return false;
  }

  bool add_node(std::size_t depth) {
    if (depth > kMaximumJsonDepth || ++nodes_ > kMaximumJsonNodes) {
      return fail(JsonStatus::Bound);
    }
    return true;
  }

  bool parse_value(JsonValue &output, std::size_t depth) {
    if (!add_node(depth) || cursor_ >= limit_)
      return false;
    const char token = source_[cursor_];
    if (token == '{')
      return parse_object(output, depth);
    if (token == '[')
      return parse_array(output, depth);
    if (token == '"') {
      output.kind = JsonKind::String;
      return parse_string(output.string);
    }
    if (token >= '0' && token <= '9')
      return parse_integer(output);
    if (match("true")) {
      output.kind = JsonKind::Boolean;
      output.boolean = true;
      return true;
    }
    if (match("false")) {
      output.kind = JsonKind::Boolean;
      output.boolean = false;
      return true;
    }
    if (match("null")) {
      output.kind = JsonKind::Null;
      return true;
    }
    if (token == ' ' || token == '\t') {
      return fail(JsonStatus::NonCanonical);
    }
    return fail(JsonStatus::Invalid);
  }

  bool match(std::string_view text) {
    if (source_.substr(cursor_, text.size()) != text)
      return false;
    cursor_ += text.size();
    return true;
  }

  bool parse_integer(JsonValue &output) {
    const std::size_t first = cursor_;
    if (source_[cursor_] == '0') {
      ++cursor_;
      if (cursor_ < limit_ && source_[cursor_] >= '0' &&
          source_[cursor_] <= '9') {
        return fail(JsonStatus::NonCanonical);
      }
    } else {
      while (cursor_ < limit_ && source_[cursor_] >= '0' &&
             source_[cursor_] <= '9') {
        ++cursor_;
      }
    }
    const char *begin = source_.data() + first;
    const char *end = source_.data() + cursor_;
    std::uint64_t value = 0;
    const auto result = std::from_chars(begin, end, value, 10);
    if (result.ec != std::errc{} || result.ptr != end) {
      return fail(JsonStatus::Bound);
    }
    output.kind = JsonKind::Integer;
    output.integer = value;
    return true;
  }

  bool parse_object(JsonValue &output, std::size_t depth) {
    output.kind = JsonKind::Object;
    ++cursor_;
    if (cursor_ < limit_ && source_[cursor_] == '}') {
      ++cursor_;
      return true;
    }
    std::string previous;
    while (cursor_ < limit_) {
      if (output.object.size() >= kMaximumJsonMembers ||
          source_[cursor_] != '"') {
        return fail(output.object.size() >= kMaximumJsonMembers
                        ? JsonStatus::Bound
                        : JsonStatus::Invalid);
      }
      std::string key;
      if (!parse_string(key))
        return false;
      if (!previous.empty() && key <= previous) {
        return fail(JsonStatus::NonCanonical);
      }
      previous = key;
      if (cursor_ >= limit_ || source_[cursor_++] != ':') {
        return fail(cursor_ < limit_ && (source_[cursor_ - 1u] == ' ' ||
                                         source_[cursor_ - 1u] == '\t')
                        ? JsonStatus::NonCanonical
                        : JsonStatus::Invalid);
      }
      JsonValue value;
      if (!parse_value(value, depth + 1u))
        return false;
      output.object.emplace_back(std::move(key), std::move(value));
      if (cursor_ >= limit_)
        return fail(JsonStatus::Invalid);
      const char delimiter = source_[cursor_++];
      if (delimiter == '}')
        return true;
      if (delimiter != ',') {
        return fail(delimiter == ' ' || delimiter == '\t'
                        ? JsonStatus::NonCanonical
                        : JsonStatus::Invalid);
      }
    }
    return fail(JsonStatus::Invalid);
  }

  bool parse_array(JsonValue &output, std::size_t depth) {
    output.kind = JsonKind::Array;
    ++cursor_;
    if (cursor_ < limit_ && source_[cursor_] == ']') {
      ++cursor_;
      return true;
    }
    while (cursor_ < limit_) {
      if (output.array.size() >= kMaximumJsonArrayItems) {
        return fail(JsonStatus::Bound);
      }
      JsonValue value;
      if (!parse_value(value, depth + 1u))
        return false;
      output.array.push_back(std::move(value));
      if (cursor_ >= limit_)
        return fail(JsonStatus::Invalid);
      const char delimiter = source_[cursor_++];
      if (delimiter == ']')
        return true;
      if (delimiter != ',') {
        return fail(delimiter == ' ' || delimiter == '\t'
                        ? JsonStatus::NonCanonical
                        : JsonStatus::Invalid);
      }
    }
    return fail(JsonStatus::Invalid);
  }

  int lower_hex(char digit) {
    if (digit >= '0' && digit <= '9')
      return digit - '0';
    if (digit >= 'a' && digit <= 'f')
      return digit - 'a' + 10;
    if (digit >= 'A' && digit <= 'F') {
      fail(JsonStatus::NonCanonical);
      return -1;
    }
    fail(JsonStatus::Invalid);
    return -1;
  }

  bool parse_hex16(std::uint32_t &value) {
    if (cursor_ + 4u > limit_)
      return fail(JsonStatus::Invalid);
    value = 0;
    for (std::size_t index = 0; index < 4u; ++index) {
      const int nibble = lower_hex(source_[cursor_++]);
      if (nibble < 0)
        return false;
      value = (value << 4u) | static_cast<std::uint32_t>(nibble);
    }
    return true;
  }

  bool append_unicode_escape(std::string &output) {
    std::uint32_t codepoint = 0;
    if (!parse_hex16(codepoint))
      return false;
    if (codepoint == 0x08u || codepoint == 0x09u || codepoint == 0x0au ||
        codepoint == 0x0cu || codepoint == 0x0du ||
        (codepoint >= 0x20u && codepoint <= 0x7eu)) {
      return fail(JsonStatus::NonCanonical);
    }
    if (codepoint >= 0xd800u && codepoint <= 0xdbffu) {
      if (cursor_ + 6u > limit_ || source_[cursor_] != '\\' ||
          source_[cursor_ + 1u] != 'u') {
        return fail(JsonStatus::Invalid);
      }
      cursor_ += 2u;
      std::uint32_t low = 0;
      if (!parse_hex16(low) || low < 0xdc00u || low > 0xdfffu) {
        return fail(JsonStatus::Invalid);
      }
      codepoint = 0x10000u + ((codepoint - 0xd800u) << 10u) + (low - 0xdc00u);
    } else if (codepoint >= 0xdc00u && codepoint <= 0xdfffu) {
      return fail(JsonStatus::Invalid);
    }
    append_utf8(output, codepoint);
    return output.size() <= kMaximumJsonStringBytes || fail(JsonStatus::Bound);
  }

  bool parse_escape(std::string &output) {
    if (cursor_ >= limit_)
      return fail(JsonStatus::Invalid);
    const char escaped = source_[cursor_++];
    switch (escaped) {
    case '"':
      output.push_back('"');
      break;
    case '\\':
      output.push_back('\\');
      break;
    case 'b':
      output.push_back('\b');
      break;
    case 'f':
      output.push_back('\f');
      break;
    case 'n':
      output.push_back('\n');
      break;
    case 'r':
      output.push_back('\r');
      break;
    case 't':
      output.push_back('\t');
      break;
    case 'u':
      return append_unicode_escape(output);
    case '/':
      return fail(JsonStatus::NonCanonical);
    default:
      return fail(JsonStatus::Invalid);
    }
    return true;
  }

  bool parse_string(std::string &output) {
    if (cursor_ >= limit_ || source_[cursor_++] != '"') {
      return fail(JsonStatus::Invalid);
    }
    while (cursor_ < limit_) {
      const unsigned char byte = static_cast<unsigned char>(source_[cursor_++]);
      if (byte == static_cast<unsigned char>('"'))
        return true;
      if (byte == static_cast<unsigned char>('\\')) {
        if (!parse_escape(output))
          return false;
      } else if (byte < 0x20u) {
        return fail(JsonStatus::Invalid);
      } else if (byte >= 0x7fu) {
        return fail(JsonStatus::NonCanonical);
      } else {
        output.push_back(static_cast<char>(byte));
      }
      if (output.size() > kMaximumJsonStringBytes) {
        return fail(JsonStatus::Bound);
      }
    }
    return fail(JsonStatus::Invalid);
  }

  std::string_view source_;
  std::size_t cursor_{};
  std::size_t limit_{};
  std::size_t nodes_{};
  JsonStatus status_{JsonStatus::Ok};
};

const JsonValue *member(const JsonValue &object, std::string_view name) {
  if (object.kind != JsonKind::Object)
    return nullptr;
  const auto found =
      std::lower_bound(object.object.begin(), object.object.end(), name,
                       [](const auto &entry, std::string_view key) {
                         return entry.first < key;
                       });
  if (found == object.object.end() || found->first != name)
    return nullptr;
  return &found->second;
}

template <std::size_t Size>
bool exact_keys(const JsonValue &object,
                const std::array<std::string_view, Size> &keys) {
  if (object.kind != JsonKind::Object || object.object.size() != keys.size()) {
    return false;
  }
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (object.object[index].first != keys[index])
      return false;
  }
  return true;
}

bool string_is(const JsonValue *value, std::string_view expected) {
  return value != nullptr && value->kind == JsonKind::String &&
         value->string == expected;
}

bool integer_is(const JsonValue *value, std::uint64_t expected) {
  return value != nullptr && value->kind == JsonKind::Integer &&
         value->integer == expected;
}

bool boolean_is(const JsonValue *value, bool expected) {
  return value != nullptr && value->kind == JsonKind::Boolean &&
         value->boolean == expected;
}

bool integer_value(const JsonValue *value, std::uint64_t &output) {
  if (value == nullptr || value->kind != JsonKind::Integer)
    return false;
  output = value->integer;
  return true;
}

bool digest_value(const JsonValue *value, Sha256Digest &output) {
  if (value == nullptr || value->kind != JsonKind::String ||
      value->string.size() != 64u ||
      !std::all_of(value->string.begin(), value->string.end(), [](char digit) {
        return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
      })) {
    return false;
  }
  return parse_sha256(value->string, output);
}

bool fixed_hex(const JsonValue *value, std::size_t size, bool uppercase) {
  if (value == nullptr || value->kind != JsonKind::String ||
      value->string.size() != size) {
    return false;
  }
  return std::all_of(value->string.begin(), value->string.end(),
                     [uppercase](char digit) {
                       if (digit >= '0' && digit <= '9')
                         return true;
                       return uppercase ? digit >= 'A' && digit <= 'F'
                                        : digit >= 'a' && digit <= 'f';
                     });
}

bool xex_version(const JsonValue *value) {
  if (value == nullptr || value->kind != JsonKind::String ||
      value->string.size() < 8u || value->string.front() != 'v') {
    return false;
  }
  std::size_t cursor = 1u;
  for (std::size_t component = 0; component < 4u; ++component) {
    const std::size_t first = cursor;
    while (cursor < value->string.size() && value->string[cursor] >= '0' &&
           value->string[cursor] <= '9') {
      ++cursor;
    }
    if (cursor == first || cursor - first > 10u ||
        (cursor - first > 1u && value->string[first] == '0')) {
      return false;
    }
    if (component == 3u)
      return cursor == value->string.size();
    if (cursor >= value->string.size() || value->string[cursor++] != '.') {
      return false;
    }
  }
  return false;
}

RetailProjectionReceiptPreflight fail(RetailProjectionReceiptError error,
                                      std::string detail) {
  RetailProjectionReceiptPreflight result;
  result.error = error;
  result.detail = std::move(detail);
  return result;
}

bool read_bounded(const std::filesystem::path &path, std::uint64_t maximum,
                  std::vector<std::uint8_t> &bytes, bool &exceeded) {
  exceeded = false;
  std::error_code error;
  const std::uintmax_t size = std::filesystem::file_size(path, error);
  if (error)
    return false;
  if (size > maximum || size > std::numeric_limits<std::size_t>::max()) {
    exceeded = true;
    return false;
  }
  bytes.resize(static_cast<std::size_t>(size));
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return false;
  if (!bytes.empty()) {
    input.read(reinterpret_cast<char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!input)
      return false;
  }
  char extra = 0;
  if (input.read(&extra, 1))
    return false;
  return input.eof();
}

bool validate_source(const JsonValue &source, std::uint64_t &marker_count) {
  constexpr std::array keys{
      std::string_view{"parent_payload_sha256"},
      std::string_view{"parent_replay_sha256"},
      std::string_view{"parent_window"},
      std::string_view{"raw_payload_sha256"},
      std::string_view{"raw_replay_sha256"},
  };
  constexpr std::array window_keys{std::string_view{"marker_count"},
                                   std::string_view{"start_marker"}};
  if (!exact_keys(source, keys))
    return false;
  Sha256Digest ignored{};
  if (!digest_value(member(source, "parent_payload_sha256"), ignored) ||
      !digest_value(member(source, "parent_replay_sha256"), ignored) ||
      !digest_value(member(source, "raw_payload_sha256"), ignored) ||
      !digest_value(member(source, "raw_replay_sha256"), ignored)) {
    return false;
  }
  const JsonValue *window = member(source, "parent_window");
  std::uint64_t start_marker = 0;
  return window != nullptr && exact_keys(*window, window_keys) &&
         integer_value(member(*window, "marker_count"), marker_count) &&
         integer_value(member(*window, "start_marker"), start_marker) &&
         marker_count > 0u && marker_count <= 500000u && start_marker > 0u &&
         start_marker <= 500000u;
}

bool validate_target(const JsonValue &target) {
  constexpr std::array keys{
      std::string_view{"base_version"},
      std::string_view{"marker_address"},
      std::string_view{"marker_code_sha256"},
      std::string_view{"media_id"},
      std::string_view{"module"},
      std::string_view{"module_xxh3"},
      std::string_view{"title_id"},
      std::string_view{"xex_sha256"},
      std::string_view{"xex_version"},
  };
  if (!exact_keys(target, keys) ||
      !fixed_hex(member(target, "marker_address"), 8u, true) ||
      !fixed_hex(member(target, "media_id"), 8u, true) ||
      !fixed_hex(member(target, "module_xxh3"), 16u, false) ||
      !fixed_hex(member(target, "title_id"), 8u, true) ||
      !xex_version(member(target, "base_version")) ||
      !xex_version(member(target, "xex_version"))) {
    return false;
  }
  Sha256Digest ignored{};
  if (!digest_value(member(target, "marker_code_sha256"), ignored) ||
      !digest_value(member(target, "xex_sha256"), ignored)) {
    return false;
  }
  const JsonValue *module = member(target, "module");
  return module != nullptr && module->kind == JsonKind::String &&
         !module->string.empty() && module->string.size() <= 128u &&
         module->string.find('/') == std::string::npos &&
         module->string.find('\\') == std::string::npos &&
         module->string.find('\0') == std::string::npos;
}

bool validate_mapping(const JsonValue &mapping) {
  constexpr std::array keys{
      std::string_view{"buttons"}, std::string_view{"pitch"},
      std::string_view{"roll"},    std::string_view{"throttle"},
      std::string_view{"yaw"},
  };
  return exact_keys(mapping, keys) &&
         string_is(member(mapping, "buttons"), "raw_xinput_buttons") &&
         string_is(member(mapping, "pitch"), "thumb_ly") &&
         string_is(member(mapping, "roll"), "thumb_lx") &&
         string_is(member(mapping, "throttle"), "right_trigger") &&
         string_is(member(mapping, "yaw"),
                   "left_shoulder=-32768;right_shoulder=32767;otherwise="
                   "thumb_rx;left_precedes_right");
}

bool validate_cadence(const JsonValue &cadence, std::uint64_t &hold) {
  constexpr std::array keys{
      std::string_view{"hold"},
      std::string_view{"native_hz"},
      std::string_view{"resampling"},
      std::string_view{"source_hz"},
  };
  std::uint64_t native_hz = 0;
  std::uint64_t source_hz = 0;
  if (!exact_keys(cadence, keys) ||
      !integer_value(member(cadence, "hold"), hold) ||
      !integer_value(member(cadence, "native_hz"), native_hz) ||
      !integer_value(member(cadence, "source_hz"), source_hz) || hold == 0u ||
      native_hz == 0u || native_hz > 1000u || source_hz == 0u ||
      source_hz > native_hz || native_hz % source_hz != 0u ||
      hold != native_hz / source_hz) {
    return false;
  }
  const std::string_view policy = hold == 1u ? "identity" : "zero_order_hold";
  return string_is(member(cadence, "resampling"), policy);
}

std::string_view difficulty_name(RetailDifficulty difficulty) {
  switch (difficulty) {
  case RetailDifficulty::Easy:
    return "Easy";
  case RetailDifficulty::Normal:
    return "Normal";
  case RetailDifficulty::Hard:
    return "Hard";
  case RetailDifficulty::Expert:
    return "Expert";
  case RetailDifficulty::Ace:
    return "Ace";
  }
  return {};
}

bool validate_output_shape(const JsonValue &output) {
  constexpr std::array keys{
      std::string_view{"aircraft_id"},
      std::string_view{"capability_data_valid"},
      std::string_view{"checkpoint_count"},
      std::string_view{"difficulty"},
      std::string_view{"difficulty_name"},
      std::string_view{"final_digest_sha256"},
      std::string_view{"final_tick"},
      std::string_view{"format"},
      std::string_view{"frame_count"},
      std::string_view{"input_digest_sha256"},
      std::string_view{"mission_id"},
      std::string_view{"output_sha256"},
      std::string_view{"random_seed"},
      std::string_view{"source_marker_count"},
      std::string_view{"version"},
      std::string_view{"weapon_id"},
  };
  return exact_keys(output, keys) &&
         string_is(member(output, "format"), "AC6RTPLY");
}

bool validate_output(const JsonValue &output, const RetailSessionReplay &replay,
                     const Sha256Digest &replay_sha256,
                     std::uint64_t source_marker_count, std::uint64_t hold) {
  if (!validate_output_shape(output) ||
      !integer_is(member(output, "version"), replay.version) ||
      !integer_is(member(output, "mission_id"), replay.mission_id) ||
      !integer_is(member(output, "difficulty"),
                  static_cast<std::uint8_t>(replay.difficulty)) ||
      !string_is(member(output, "difficulty_name"),
                 difficulty_name(replay.difficulty)) ||
      !integer_is(member(output, "aircraft_id"), replay.loadout.aircraft_id) ||
      !integer_is(member(output, "weapon_id"), replay.loadout.weapon_id) ||
      !boolean_is(member(output, "capability_data_valid"),
                  replay.loadout.capability_data_valid) ||
      !integer_is(member(output, "random_seed"), replay.random_seed) ||
      !integer_is(member(output, "checkpoint_count"),
                  replay.checkpoints.size()) ||
      !integer_is(member(output, "frame_count"), replay.frames.size()) ||
      !integer_is(member(output, "final_tick"), replay.final_tick) ||
      !integer_is(member(output, "source_marker_count"), source_marker_count)) {
    return false;
  }
  if (source_marker_count > std::numeric_limits<std::uint64_t>::max() / hold ||
      source_marker_count * hold != replay.frames.size()) {
    return false;
  }
  Sha256Digest input_digest{};
  Sha256Digest final_digest{};
  Sha256Digest output_digest{};
  return digest_value(member(output, "input_digest_sha256"), input_digest) &&
         input_digest == replay.input_digest() &&
         digest_value(member(output, "final_digest_sha256"), final_digest) &&
         final_digest == replay.final_digest &&
         digest_value(member(output, "output_sha256"), output_digest) &&
         output_digest == replay_sha256;
}

class ReplayBytesReader final {
public:
  explicit ReplayBytesReader(std::span<const std::uint8_t> bytes)
      : bytes_(bytes) {}

  bool read_u16(std::uint16_t &value) {
    if (remaining() < 2u)
      return false;
    value = static_cast<std::uint16_t>(bytes_[cursor_]) |
            (static_cast<std::uint16_t>(bytes_[cursor_ + 1u]) << 8u);
    cursor_ += 2u;
    return true;
  }

  bool read_u32(std::uint32_t &value) {
    if (remaining() < 4u)
      return false;
    value = static_cast<std::uint32_t>(bytes_[cursor_]) |
            (static_cast<std::uint32_t>(bytes_[cursor_ + 1u]) << 8u) |
            (static_cast<std::uint32_t>(bytes_[cursor_ + 2u]) << 16u) |
            (static_cast<std::uint32_t>(bytes_[cursor_ + 3u]) << 24u);
    cursor_ += 4u;
    return true;
  }

  bool read_u64(std::uint64_t &value) {
    if (remaining() < 8u)
      return false;
    value = 0;
    for (std::size_t index = 0; index < 8u; ++index) {
      value |= static_cast<std::uint64_t>(bytes_[cursor_ + index])
               << (index * 8u);
    }
    cursor_ += 8u;
    return true;
  }

  bool read_bytes(std::span<std::uint8_t> output) {
    if (remaining() < output.size())
      return false;
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_),
                output.size(), output.begin());
    cursor_ += output.size();
    return true;
  }

  bool consume(std::span<const std::uint8_t> expected) {
    if (remaining() < expected.size() ||
        !std::equal(expected.begin(), expected.end(),
                    bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_))) {
      return false;
    }
    cursor_ += expected.size();
    return true;
  }

  bool finished() const noexcept { return cursor_ == bytes_.size(); }

private:
  std::size_t remaining() const noexcept { return bytes_.size() - cursor_; }

  std::span<const std::uint8_t> bytes_;
  std::size_t cursor_{};
};

bool replay_v3_matches(std::span<const std::uint8_t> bytes,
                       const RetailSessionReplay &replay) {
  ReplayBytesReader reader(bytes);
  std::uint32_t version = 0;
  std::uint32_t mission = 0;
  std::uint32_t difficulty = 0;
  std::uint32_t aircraft = 0;
  std::uint32_t weapon = 0;
  std::uint32_t capability = 0;
  Sha256Digest cache{};
  std::uint64_t seed = 0;
  std::uint32_t checkpoint_count = 0;
  if (!reader.consume(kReplayMagic) || !reader.read_u32(version) ||
      !reader.read_u32(mission) || !reader.read_u32(difficulty) ||
      !reader.read_u32(aircraft) || !reader.read_u32(weapon) ||
      !reader.read_u32(capability) || capability > 1u ||
      !reader.read_bytes(cache) || !reader.read_u64(seed) ||
      !reader.read_u32(checkpoint_count) ||
      version != RetailSessionReplay::kCurrentVersion ||
      version != replay.version || mission != replay.mission_id ||
      difficulty != static_cast<std::uint8_t>(replay.difficulty) ||
      aircraft != replay.loadout.aircraft_id ||
      weapon != replay.loadout.weapon_id ||
      (capability != 0u) != replay.loadout.capability_data_valid ||
      cache != replay.content_index_sha256 || seed != replay.random_seed ||
      checkpoint_count != replay.checkpoints.size()) {
    return false;
  }
  for (const RetailSessionReplay::Checkpoint &expected : replay.checkpoints) {
    std::uint32_t frame_index = 0;
    Sha256Digest digest{};
    if (!reader.read_u32(frame_index) || !reader.read_bytes(digest) ||
        frame_index != expected.frame_index ||
        digest != expected.input_digest) {
      return false;
    }
  }
  std::uint64_t final_tick = 0;
  Sha256Digest final_digest{};
  std::uint32_t frame_count = 0;
  if (!reader.read_u64(final_tick) || !reader.read_bytes(final_digest) ||
      !reader.read_u32(frame_count) || final_tick != replay.final_tick ||
      final_digest != replay.final_digest ||
      frame_count != replay.frames.size()) {
    return false;
  }
  for (const InputFrame expected : replay.frames) {
    std::uint16_t pitch = 0;
    std::uint16_t roll = 0;
    std::uint16_t yaw = 0;
    std::uint8_t throttle = 0;
    std::uint16_t buttons = 0;
    if (!reader.read_u16(pitch) || !reader.read_u16(roll) ||
        !reader.read_u16(yaw) ||
        !reader.read_bytes(std::span<std::uint8_t>(&throttle, 1u)) ||
        !reader.read_u16(buttons) ||
        pitch != static_cast<std::uint16_t>(expected.pitch) ||
        roll != static_cast<std::uint16_t>(expected.roll) ||
        yaw != static_cast<std::uint16_t>(expected.yaw) ||
        throttle != expected.throttle || buttons != expected.buttons) {
      return false;
    }
  }
  return reader.finished();
}

RetailProjectionReceiptError json_error(JsonStatus status) {
  switch (status) {
  case JsonStatus::Bound:
    return RetailProjectionReceiptError::JsonBound;
  case JsonStatus::NonCanonical:
    return RetailProjectionReceiptError::JsonNonCanonical;
  case JsonStatus::Invalid:
    return RetailProjectionReceiptError::JsonInvalid;
  case JsonStatus::Ok:
    break;
  }
  return RetailProjectionReceiptError::JsonInvalid;
}

} // namespace

const char *retail_projection_receipt_error_name(
    RetailProjectionReceiptError error) noexcept {
  switch (error) {
  case RetailProjectionReceiptError::None:
    return "none";
  case RetailProjectionReceiptError::InvalidArgument:
    return "invalid_argument";
  case RetailProjectionReceiptError::ReceiptUnreadable:
    return "receipt_unreadable";
  case RetailProjectionReceiptError::ReceiptByteBound:
    return "receipt_byte_bound";
  case RetailProjectionReceiptError::JsonInvalid:
    return "json_invalid";
  case RetailProjectionReceiptError::JsonBound:
    return "json_bound";
  case RetailProjectionReceiptError::JsonNonCanonical:
    return "json_noncanonical";
  case RetailProjectionReceiptError::SchemaMismatch:
    return "schema_mismatch";
  case RetailProjectionReceiptError::ReplayUnreadable:
    return "replay_unreadable";
  case RetailProjectionReceiptError::ReplayByteBound:
    return "replay_byte_bound";
  case RetailProjectionReceiptError::ReplayIdentityMismatch:
    return "replay_identity_mismatch";
  case RetailProjectionReceiptError::CacheIdentityMismatch:
    return "cache_identity_mismatch";
  case RetailProjectionReceiptError::ReplayMetadataMismatch:
    return "replay_metadata_mismatch";
  }
  return "unknown";
}

RetailProjectionReceiptPreflight
preflight_retail_projection_receipt(const std::filesystem::path &receipt_path,
                                    const std::filesystem::path &replay_path,
                                    const RetailSessionReplay &replay,
                                    const Sha256Digest &cache_index_sha256) {
  if (receipt_path.empty() || replay_path.empty() || !replay.valid()) {
    return fail(RetailProjectionReceiptError::InvalidArgument,
                "paths and an already valid replay are required");
  }

  std::vector<std::uint8_t> receipt_bytes;
  bool exceeded = false;
  if (!read_bounded(receipt_path, kMaximumReceiptBytes, receipt_bytes,
                    exceeded)) {
    return fail(exceeded ? RetailProjectionReceiptError::ReceiptByteBound
                         : RetailProjectionReceiptError::ReceiptUnreadable,
                exceeded ? "receipt exceeds 65536 bytes"
                         : "receipt cannot be read as one stable file");
  }
  const Sha256Digest receipt_sha256 = sha256_bytes(receipt_bytes);
  const std::string_view receipt_text(
      reinterpret_cast<const char *>(receipt_bytes.data()),
      receipt_bytes.size());
  JsonValue root;
  CanonicalJsonParser parser(receipt_text);
  const JsonStatus status = parser.parse(root);
  if (status != JsonStatus::Ok) {
    auto result =
        fail(json_error(status), "receipt is not bounded canonical JSON");
    result.receipt_sha256 = receipt_sha256;
    return result;
  }

  constexpr std::array root_keys{
      std::string_view{"cache_index_sha256"},
      std::string_view{"cadence"},
      std::string_view{"kind"},
      std::string_view{"mapping"},
      std::string_view{"output"},
      std::string_view{"schema"},
      std::string_view{"source"},
      std::string_view{"target"},
  };
  const JsonValue *source = member(root, "source");
  const JsonValue *target = member(root, "target");
  const JsonValue *cadence = member(root, "cadence");
  const JsonValue *mapping = member(root, "mapping");
  const JsonValue *output = member(root, "output");
  std::uint64_t source_marker_count = 0;
  std::uint64_t hold = 0;
  if (!exact_keys(root, root_keys) ||
      !string_is(member(root, "kind"), "native_projection_receipt") ||
      !string_is(member(root, "schema"), kSchema) || source == nullptr ||
      target == nullptr || cadence == nullptr || mapping == nullptr ||
      output == nullptr || !validate_source(*source, source_marker_count) ||
      !validate_target(*target) || !validate_cadence(*cadence, hold) ||
      !validate_mapping(*mapping)) {
    auto result = fail(RetailProjectionReceiptError::SchemaMismatch,
                       "receipt v1 shape or projection contract mismatch");
    result.receipt_sha256 = receipt_sha256;
    return result;
  }

  Sha256Digest receipt_cache{};
  if (!digest_value(member(root, "cache_index_sha256"), receipt_cache) ||
      receipt_cache != cache_index_sha256 ||
      receipt_cache != replay.content_index_sha256) {
    auto result = fail(RetailProjectionReceiptError::CacheIdentityMismatch,
                       "receipt, replay and opened cache identities differ");
    result.receipt_sha256 = receipt_sha256;
    return result;
  }

  std::vector<std::uint8_t> replay_bytes;
  if (!read_bounded(replay_path, kMaximumReplayBytes, replay_bytes, exceeded)) {
    auto result =
        fail(exceeded ? RetailProjectionReceiptError::ReplayByteBound
                      : RetailProjectionReceiptError::ReplayUnreadable,
             exceeded ? "replay exceeds the v3 structural bound"
                      : "replay cannot be read as one stable file");
    result.receipt_sha256 = receipt_sha256;
    return result;
  }
  const Sha256Digest replay_sha256 = sha256_bytes(replay_bytes);
  if (!replay_v3_matches(replay_bytes, replay)) {
    auto result = fail(RetailProjectionReceiptError::ReplayIdentityMismatch,
                       "projection receipt requires an exact AC6RTPLY v3 file");
    result.receipt_sha256 = receipt_sha256;
    result.replay_sha256 = replay_sha256;
    return result;
  }
  if (!validate_output(*output, replay, replay_sha256, source_marker_count,
                       hold)) {
    auto result = fail(RetailProjectionReceiptError::ReplayMetadataMismatch,
                       "receipt output does not describe the loaded replay");
    result.receipt_sha256 = receipt_sha256;
    result.replay_sha256 = replay_sha256;
    return result;
  }

  RetailProjectionReceiptPreflight result;
  result.error = RetailProjectionReceiptError::None;
  result.detail =
      "native output/cache verified; raw and parent replay lineage unverified";
  result.receipt_sha256 = receipt_sha256;
  result.replay_sha256 = replay_sha256;
  result.native_output_verified = true;
  result.source_lineage_verified = false;
  return result;
}

} // namespace ac6::retail
