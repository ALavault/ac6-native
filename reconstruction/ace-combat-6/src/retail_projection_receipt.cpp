#include "ac6/retail_projection_receipt.h"

#include "ac6/sha256.h"
#include "retail_projection_replay_bytes.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace ac6::retail {
namespace {

constexpr std::uint64_t kMaximumReceiptBytes = 64u * 1024u;
constexpr std::uint64_t kMaximumReplayBytes =
    121u + RetailSessionReplay::kMaximumFrames * 9u +
    RetailSessionReplay::kMaximumCheckpoints * 36u;
constexpr std::uint64_t kMaximumPreflightBytes =
    kMaximumReceiptBytes + kMaximumReplayBytes;
constexpr std::size_t kMaximumJsonDepth = 16u;
constexpr std::size_t kMaximumJsonNodes = 1024u;
constexpr std::size_t kMaximumJsonMembers = 128u;
constexpr std::size_t kMaximumJsonArrayItems = 256u;
constexpr std::size_t kMaximumJsonStringBytes = 4096u;
constexpr std::string_view kSchemaV3 =
    "ac6.native-controller-projection-receipt.v3";
constexpr std::string_view kSchemaV4 =
    "ac6.native-controller-projection-receipt.v4";
constexpr std::string_view kRawSchemaV4 = "ac6.controller-input-replay.v4";
constexpr std::string_view kCadenceCensusSchemaV1 =
    "ac6.controller-cadence-census.v1";
constexpr std::string_view kCadenceCensusSchemaV2 =
    "ac6.controller-cadence-census.v2";
constexpr std::string_view kCadenceMethod = "uniform_marker_interval_v1";
constexpr std::string_view kCadenceIntegrityLevel =
    "integrity_only_runtime_census";
constexpr std::string_view kNativeClockSchema =
    "ac6.native-simulation-clock.v1";
constexpr std::string_view kPalXexSha256 =
    "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde";
constexpr std::string_view kNtscUjXexSha256 =
    "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc";
constexpr std::string_view kNtscUjMarkerCodeSha256 =
    "a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d";
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
  const int descriptor =
      ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
  if (descriptor < 0)
    return false;
  struct Descriptor final {
    int value;
    ~Descriptor() { ::close(value); }
  } owner{descriptor};

  struct stat status{};
  if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) ||
      status.st_size < 0) {
    return false;
  }
  const std::uint64_t initial_size = static_cast<std::uint64_t>(status.st_size);
  if (initial_size > maximum) {
    exceeded = true;
    return false;
  }

  bytes.clear();
  bytes.reserve(static_cast<std::size_t>(initial_size));
  std::array<std::uint8_t, 64u * 1024u> chunk{};
  for (;;) {
    const ssize_t count = ::read(descriptor, chunk.data(), chunk.size());
    if (count == 0)
      return true;
    if (count < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    const std::uint64_t chunk_size = static_cast<std::uint64_t>(count);
    if (bytes.size() > maximum || chunk_size > maximum - bytes.size()) {
      exceeded = true;
      bytes.clear();
      return false;
    }
    bytes.insert(bytes.end(), chunk.begin(),
                 chunk.begin() + static_cast<std::ptrdiff_t>(count));
  }
}

bool validate_lineage(const JsonValue &source, std::uint64_t &marker_count) {
  constexpr std::array window_keys{std::string_view{"marker_count"},
                                   std::string_view{"start_marker"}};
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
         start_marker <= 500000u - marker_count + 1u;
}

bool validate_source_v3(const JsonValue &source, std::uint64_t &marker_count) {
  constexpr std::array keys{
      std::string_view{"parent_payload_sha256"},
      std::string_view{"parent_replay_sha256"},
      std::string_view{"parent_window"},
      std::string_view{"raw_payload_sha256"},
      std::string_view{"raw_replay_sha256"},
  };
  return exact_keys(source, keys) && validate_lineage(source, marker_count);
}

bool validate_ntsc_uj_oracle_target(const JsonValue &target) {
  constexpr std::array keys{
      std::string_view{"base_version"}, std::string_view{"entry_point"},
      std::string_view{"media_id"},     std::string_view{"module"},
      std::string_view{"module_xxh3"},  std::string_view{"region_mask"},
      std::string_view{"target_id"},    std::string_view{"title_id"},
      std::string_view{"xex_sha256"},   std::string_view{"xex_version"},
  };
  return exact_keys(target, keys) &&
         string_is(member(target, "target_id"), "ac6-ntsc-uj-default-xex") &&
         string_is(member(target, "title_id"), "4E4D07D1") &&
         string_is(member(target, "media_id"), "531C30BE") &&
         string_is(member(target, "module"), "default.xex") &&
         string_is(member(target, "xex_sha256"), kNtscUjXexSha256) &&
         string_is(member(target, "xex_version"), "v0.0.0.8") &&
         string_is(member(target, "base_version"), "v0.0.0.8") &&
         string_is(member(target, "module_xxh3"), "892639B654015428") &&
         string_is(member(target, "entry_point"), "821F5ED0") &&
         string_is(member(target, "region_mask"), "0000FDFF");
}

bool validate_ntsc_uj_marker_contract(const JsonValue &marker) {
  constexpr std::array marker_keys{
      std::string_view{"address"}, std::string_view{"code"},
      std::string_view{"phase"}, std::string_view{"role"}};
  constexpr std::array code_keys{std::string_view{"image_rva"},
                                 std::string_view{"length"},
                                 std::string_view{"sha256"}};
  const JsonValue *code = member(marker, "code");
  return exact_keys(marker, marker_keys) && code != nullptr &&
         exact_keys(*code, code_keys) &&
         string_is(member(marker, "address"), "821CA940") &&
         string_is(member(marker, "phase"), "before_input") &&
         string_is(member(marker, "role"), "ac6_frame_input_stage") &&
         string_is(member(*code, "image_rva"), "001CA940") &&
         integer_is(member(*code, "length"), 328u) &&
         string_is(member(*code, "sha256"), kNtscUjMarkerCodeSha256);
}

bool validate_source_v4(const JsonValue &source, std::uint64_t &marker_count) {
  constexpr std::array keys{
      std::string_view{"oracle"},
      std::string_view{"parent_payload_sha256"},
      std::string_view{"parent_replay_sha256"},
      std::string_view{"parent_window"},
      std::string_view{"raw_payload_sha256"},
      std::string_view{"raw_replay_sha256"},
      std::string_view{"raw_schema"},
  };
  constexpr std::array oracle_keys{std::string_view{"marker_contract"},
                                   std::string_view{"target"}};
  const JsonValue *oracle = member(source, "oracle");
  const JsonValue *target =
      oracle == nullptr ? nullptr : member(*oracle, "target");
  const JsonValue *marker =
      oracle == nullptr ? nullptr : member(*oracle, "marker_contract");
  return exact_keys(source, keys) &&
         string_is(member(source, "raw_schema"), kRawSchemaV4) &&
         validate_lineage(source, marker_count) && oracle != nullptr &&
         exact_keys(*oracle, oracle_keys) && target != nullptr &&
         validate_ntsc_uj_oracle_target(*target) && marker != nullptr &&
         validate_ntsc_uj_marker_contract(*marker);
}

bool validate_native_target_v4(const JsonValue &target) {
  constexpr std::array keys{
      std::string_view{"base_version"}, std::string_view{"media_id"},
      std::string_view{"module"},       std::string_view{"target_id"},
      std::string_view{"title_id"},     std::string_view{"xex_sha256"},
      std::string_view{"xex_version"},
  };
  return exact_keys(target, keys) &&
         string_is(member(target, "target_id"), "ac6-pal-default-xex") &&
         string_is(member(target, "title_id"), "4E4D07D1") &&
         string_is(member(target, "media_id"), "0379EFB3") &&
         string_is(member(target, "module"), "default.xex") &&
         string_is(member(target, "xex_sha256"), kPalXexSha256) &&
         string_is(member(target, "xex_version"), "v0.0.0.11") &&
         string_is(member(target, "base_version"), "v0.0.0.11");
}

bool validate_target_v3(const JsonValue &target) {
  constexpr std::array keys{
      std::string_view{"base_version"},
      std::string_view{"marker_address"},
      std::string_view{"marker_code_length"},
      std::string_view{"marker_code_offset"},
      std::string_view{"marker_code_sha256"},
      std::string_view{"media_id"},
      std::string_view{"module"},
      std::string_view{"module_xxh3"},
      std::string_view{"title_id"},
      std::string_view{"xex_sha256"},
      std::string_view{"xex_version"},
  };
  if (!exact_keys(target, keys) ||
      !string_is(member(target, "title_id"), "4E4D07D1") ||
      !string_is(member(target, "media_id"), "0379EFB3") ||
      !string_is(member(target, "module"), "default.xex") ||
      !string_is(member(target, "xex_sha256"), kPalXexSha256) ||
      !string_is(member(target, "xex_version"), "v0.0.0.11") ||
      !string_is(member(target, "base_version"), "v0.0.0.11") ||
      !fixed_hex(member(target, "marker_address"), 8u, true) ||
      !fixed_hex(member(target, "media_id"), 8u, true) ||
      !fixed_hex(member(target, "module_xxh3"), 16u, false) ||
      !fixed_hex(member(target, "title_id"), 8u, true) ||
      !xex_version(member(target, "base_version")) ||
      !xex_version(member(target, "xex_version"))) {
    return false;
  }
  std::uint64_t code_offset = 0;
  std::uint64_t code_length = 0;
  if (!integer_value(member(target, "marker_code_offset"), code_offset) ||
      !integer_value(member(target, "marker_code_length"), code_length) ||
      code_offset > 0xffffffffu || code_length == 0u || code_length > 4096u ||
      code_offset > 0xffffffffu - code_length + 1u) {
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

bool validate_cadence_common(const JsonValue &cadence,
                             std::string_view census_schema,
                             std::uint64_t source_marker_count,
                             std::uint64_t &hold) {
  constexpr std::array census_keys{
      std::string_view{"file_sha256"},    std::string_view{"integrity_level"},
      std::string_view{"interval_count"}, std::string_view{"method"},
      std::string_view{"payload_sha256"}, std::string_view{"record_count"},
      std::string_view{"schema"},
  };
  constexpr std::array native_clock_keys{
      std::string_view{"clock_id"}, std::string_view{"frequency"},
      std::string_view{"schema"}, std::string_view{"tick_semantics"}};
  constexpr std::array rational_keys{std::string_view{"denominator"},
                                     std::string_view{"numerator"}};
  std::uint64_t native_hz = 0;
  std::uint64_t source_hz = 0;
  if (!string_is(member(cadence, "integrity_level"), kCadenceIntegrityLevel) ||
      !integer_value(member(cadence, "hold"), hold) ||
      !integer_value(member(cadence, "native_hz"), native_hz) ||
      !integer_value(member(cadence, "source_hz"), source_hz) || hold == 0u ||
      native_hz == 0u || native_hz > 1000u || source_hz == 0u ||
      source_hz > native_hz || native_hz % source_hz != 0u ||
      hold != native_hz / source_hz ||
      !((source_hz == 30u && native_hz == 60u && hold == 2u) ||
        (source_hz == 60u && native_hz == 60u && hold == 1u))) {
    return false;
  }
  const std::string_view policy = hold == 1u ? "identity" : "zero_order_hold";
  if (!string_is(member(cadence, "resampling"), policy))
    return false;

  const JsonValue *census = member(cadence, "census");
  Sha256Digest ignored{};
  std::uint64_t record_count = 0;
  std::uint64_t interval_count = 0;
  if (census == nullptr || !exact_keys(*census, census_keys) ||
      !string_is(member(*census, "schema"), census_schema) ||
      !string_is(member(*census, "integrity_level"), kCadenceIntegrityLevel) ||
      !string_is(member(*census, "method"), kCadenceMethod) ||
      !digest_value(member(*census, "file_sha256"), ignored) ||
      !digest_value(member(*census, "payload_sha256"), ignored) ||
      !integer_value(member(*census, "record_count"), record_count) ||
      !integer_value(member(*census, "interval_count"), interval_count) ||
      record_count < 2u || record_count > 500000u ||
      interval_count != record_count - 1u ||
      record_count != source_marker_count) {
    return false;
  }

  const JsonValue *native_clock = member(cadence, "native_clock");
  const JsonValue *frequency =
      native_clock == nullptr ? nullptr : member(*native_clock, "frequency");
  return native_clock != nullptr &&
         exact_keys(*native_clock, native_clock_keys) && frequency != nullptr &&
         exact_keys(*frequency, rational_keys) &&
         string_is(member(*native_clock, "schema"), kNativeClockSchema) &&
         string_is(member(*native_clock, "clock_id"),
                   "ac6_native_fixed_step") &&
         string_is(member(*native_clock, "tick_semantics"),
                   "one_simulation_step") &&
         integer_is(member(*frequency, "numerator"), 60u) &&
         integer_is(member(*frequency, "denominator"), 1u) && native_hz == 60u;
}

bool validate_cadence_marker_v3(const JsonValue &cadence,
                                const JsonValue &target) {
  constexpr std::array marker_keys{
      std::string_view{"address"}, std::string_view{"code"},
      std::string_view{"phase"}, std::string_view{"role"}};
  constexpr std::array code_keys{std::string_view{"length"},
                                 std::string_view{"offset"},
                                 std::string_view{"sha256"}};
  const JsonValue *marker = member(cadence, "marker_contract");
  const JsonValue *code = marker == nullptr ? nullptr : member(*marker, "code");
  Sha256Digest marker_code{};
  Sha256Digest target_marker_code{};
  std::uint64_t code_offset = 0;
  std::uint64_t code_length = 0;
  std::uint64_t target_code_offset = 0;
  std::uint64_t target_code_length = 0;
  if (marker == nullptr || !exact_keys(*marker, marker_keys) ||
      code == nullptr || !exact_keys(*code, code_keys) ||
      (!string_is(member(*marker, "role"), "ac6_frame_input_stage") &&
       !string_is(member(*marker, "role"), "mission_manager_tick")) ||
      (!string_is(member(*marker, "phase"), "before_input") &&
       !string_is(member(*marker, "phase"), "after_input")) ||
      !fixed_hex(member(*marker, "address"), 8u, true) ||
      !integer_value(member(*code, "offset"), code_offset) ||
      !integer_value(member(*code, "length"), code_length) ||
      code_offset > 0xffffffffu || code_length == 0u || code_length > 4096u ||
      code_offset > 0xffffffffu - code_length + 1u ||
      !digest_value(member(*code, "sha256"), marker_code) ||
      !integer_value(member(target, "marker_code_offset"),
                     target_code_offset) ||
      !integer_value(member(target, "marker_code_length"),
                     target_code_length) ||
      !digest_value(member(target, "marker_code_sha256"), target_marker_code)) {
    return false;
  }
  const JsonValue *marker_address = member(*marker, "address");
  const JsonValue *target_address = member(target, "marker_address");
  if (marker_address == nullptr || target_address == nullptr ||
      marker_address->string != target_address->string ||
      code_offset != target_code_offset || code_length != target_code_length ||
      marker_code != target_marker_code) {
    return false;
  }
  return true;
}

bool validate_cadence_v3(const JsonValue &cadence, const JsonValue &target,
                         std::uint64_t source_marker_count,
                         std::uint64_t &hold) {
  constexpr std::array keys{
      std::string_view{"census"},          std::string_view{"hold"},
      std::string_view{"integrity_level"}, std::string_view{"marker_contract"},
      std::string_view{"native_clock"},    std::string_view{"native_hz"},
      std::string_view{"resampling"},      std::string_view{"source_hz"},
  };
  return exact_keys(cadence, keys) &&
         validate_cadence_common(cadence, kCadenceCensusSchemaV1,
                                 source_marker_count, hold) &&
         validate_cadence_marker_v3(cadence, target);
}

bool validate_cadence_v4(const JsonValue &cadence,
                         std::uint64_t source_marker_count,
                         std::uint64_t &hold) {
  constexpr std::array keys{
      std::string_view{"census"},          std::string_view{"hold"},
      std::string_view{"integrity_level"}, std::string_view{"native_clock"},
      std::string_view{"native_hz"},       std::string_view{"resampling"},
      std::string_view{"source_hz"},
  };
  return exact_keys(cadence, keys) &&
         validate_cadence_common(cadence, kCadenceCensusSchemaV2,
                                 source_marker_count, hold);
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

  constexpr std::array root_keys_v3{
      std::string_view{"cache_index_sha256"},
      std::string_view{"cadence"},
      std::string_view{"kind"},
      std::string_view{"mapping"},
      std::string_view{"output"},
      std::string_view{"schema"},
      std::string_view{"source"},
      std::string_view{"target"},
  };
  constexpr std::array root_keys_v4{
      std::string_view{"cache_index_sha256"},
      std::string_view{"cadence"},
      std::string_view{"kind"},
      std::string_view{"mapping"},
      std::string_view{"native_target"},
      std::string_view{"output"},
      std::string_view{"schema"},
      std::string_view{"source"},
  };
  const JsonValue *source = member(root, "source");
  const JsonValue *cadence = member(root, "cadence");
  const JsonValue *mapping = member(root, "mapping");
  const JsonValue *output = member(root, "output");
  std::uint64_t source_marker_count = 0;
  std::uint64_t hold = 0;
  const bool common_shape =
      string_is(member(root, "kind"), "native_projection_receipt") &&
      source != nullptr && cadence != nullptr && mapping != nullptr &&
      output != nullptr && validate_mapping(*mapping);
  const bool receipt_v3 = string_is(member(root, "schema"), kSchemaV3);
  const bool receipt_v4 = string_is(member(root, "schema"), kSchemaV4);
  bool contract_matches = false;
  if (common_shape && receipt_v3 && exact_keys(root, root_keys_v3)) {
    const JsonValue *target = member(root, "target");
    contract_matches =
        target != nullptr && validate_source_v3(*source, source_marker_count) &&
        validate_target_v3(*target) &&
        validate_cadence_v3(*cadence, *target, source_marker_count, hold);
  } else if (common_shape && receipt_v4 && exact_keys(root, root_keys_v4)) {
    const JsonValue *native_target = member(root, "native_target");
    contract_matches = native_target != nullptr &&
                       validate_source_v4(*source, source_marker_count) &&
                       validate_native_target_v4(*native_target) &&
                       validate_cadence_v4(*cadence, source_marker_count, hold);
  }
  if (!contract_matches) {
    auto result =
        fail(RetailProjectionReceiptError::SchemaMismatch,
             receipt_v3 ? "receipt v3 shape or projection contract mismatch"
                        : "receipt v4 shape or projection contract mismatch");
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
  if (receipt_bytes.size() > kMaximumPreflightBytes - replay_bytes.size()) {
    auto result = fail(RetailProjectionReceiptError::ReplayByteBound,
                       "receipt and replay exceed the total preflight bound");
    result.receipt_sha256 = receipt_sha256;
    return result;
  }
  const Sha256Digest replay_sha256 = sha256_bytes(replay_bytes);
  if (!detail::replay_v3_matches(replay_bytes, replay)) {
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
  result.detail = receipt_v4
                      ? "native output/cache verified for the provisional "
                        "NTSC-U/J oracle to PAL projection; raw, parent and "
                        "cadence-census lineage unverified; not parity evidence"
                      : "native output/cache verified; raw, parent and "
                        "cadence-census lineage unverified";
  result.receipt_sha256 = receipt_sha256;
  result.replay_sha256 = replay_sha256;
  result.native_output_verified = true;
  result.source_lineage_verified = false;
  return result;
}

} // namespace ac6::retail
