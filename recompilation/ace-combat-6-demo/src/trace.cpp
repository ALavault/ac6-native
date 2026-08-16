#include "ac6demo/trace.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace ac6demo {

namespace {

[[nodiscard]] std::string json_escape(std::string_view value) {
  std::string result;
  result.reserve(value.size() + 2U);
  for (const char character : value) {
    switch (character) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default: result += character; break;
    }
  }
  return result;
}

[[nodiscard]] std::string json_field(std::string_view line, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const std::size_t start = line.find(needle);
  if (start == std::string_view::npos) {
    throw std::runtime_error("trace field missing: " + std::string(key));
  }
  const std::size_t value_start = start + needle.size();
  if (value_start >= line.size() || line[value_start] != '"') {
    throw std::runtime_error("trace field is not a string: " + std::string(key));
  }
  std::string result;
  bool escaped = false;
  for (std::size_t index = value_start + 1U; index < line.size(); ++index) {
    const char character = line[index];
    if (escaped) {
      switch (character) {
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        default: result += character; break;
      }
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      return result;
    } else {
      result += character;
    }
  }
  throw std::runtime_error("unterminated trace string: " + std::string(key));
}

[[nodiscard]] std::uint64_t json_uint(std::string_view line, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const std::size_t start = line.find(needle);
  if (start == std::string_view::npos) {
    throw RuntimeTrap("replay input number missing: " + std::string(key));
  }
  const char* begin = line.data() + start + needle.size();
  char* end = nullptr;
  const auto value = std::strtoull(begin, &end, 10);
  if (end == begin) {
    throw std::runtime_error("trace number invalid: " + std::string(key));
  }
  return value;
}

[[nodiscard]] std::int64_t json_int(std::string_view line, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const std::size_t start = line.find(needle);
  if (start == std::string_view::npos) {
    throw std::runtime_error("trace number missing: " + std::string(key));
  }
  const char* begin = line.data() + start + needle.size();
  char* end = nullptr;
  const auto value = std::strtoll(begin, &end, 10);
  if (end == begin) {
    throw RuntimeTrap("replay input number invalid: " + std::string(key));
  }
  return value;
}

[[nodiscard]] bool json_bool(std::string_view line, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const std::size_t start = line.find(needle);
  if (start == std::string_view::npos) {
    throw RuntimeTrap("replay input boolean missing: " + std::string(key));
  }
  const std::size_t value_start = start + needle.size();
  if (line.substr(value_start, 4U) == "true") {
    return true;
  }
  if (line.substr(value_start, 5U) == "false") {
    return false;
  }
  throw RuntimeTrap("replay input boolean invalid: " + std::string(key));
}

template <typename T>
[[nodiscard]] T checked_unsigned(std::uint64_t value, std::string_view key) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
    throw RuntimeTrap("replay input value out of range: " + std::string(key));
  }
  return static_cast<T>(value);
}

template <typename T>
[[nodiscard]] T checked_signed(std::int64_t value, std::string_view key) {
  if (value < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
      value > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
    throw RuntimeTrap("replay input value out of range: " + std::string(key));
  }
  return static_cast<T>(value);
}

[[nodiscard]] std::string backend_name(GraphicsBackend backend) {
  return backend == GraphicsBackend::Vulkan ? "vulkan" : "headless";
}

[[nodiscard]] std::string digest_hex(const std::array<std::byte, 32>& digest) {
  std::ostringstream result;
  result << std::hex << std::setfill('0');
  for (const std::byte value : digest) {
    result << std::setw(2) << static_cast<unsigned>(std::to_integer<std::uint8_t>(value));
  }
  return result.str();
}

}  // namespace

std::string_view trace_domain_name(TraceDomain domain) noexcept {
  switch (domain) {
    case TraceDomain::Input: return "input";
    case TraceDomain::Simulation: return "simulation";
    case TraceDomain::Objectives: return "objectives";
    case TraceDomain::Graphics: return "graphics";
    case TraceDomain::Media: return "media";
    case TraceDomain::Hashes: return "hashes";
  }
  return "unknown";
}

bool parse_trace_domain(std::string_view name, TraceDomain& domain) noexcept {
  constexpr TraceDomain domains[] = {TraceDomain::Input, TraceDomain::Simulation,
                                     TraceDomain::Objectives, TraceDomain::Graphics,
                                     TraceDomain::Media, TraceDomain::Hashes};
  for (const TraceDomain candidate : domains) {
    if (trace_domain_name(candidate) == name) {
      domain = candidate;
      return true;
    }
  }
  return false;
}

TraceWriter::~TraceWriter() {
  if (output_.is_open()) {
    close();
  }
}

void TraceWriter::open(const std::filesystem::path& path, GraphicsBackend backend,
                       std::string_view xex_sha256) {
  if (output_.is_open()) {
    close();
  }
  std::error_code error;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      throw std::runtime_error("cannot create trace directory: " + error.message());
    }
  }
  output_.open(path, std::ios::binary | std::ios::trunc);
  if (!output_) {
    throw std::runtime_error("cannot open trace: " + path.string());
  }
  count_ = 0;
  previous_tick_ = 0;
  has_tick_ = false;
  digest_.reset();
  const std::string header =
      "{\"magic\":\"AC6RTPLY\",\"version\":4,\"xex_sha256\":\"" +
      json_escape(xex_sha256) + "\",\"backend\":\"" + backend_name(backend) +
      "\",\"domains\":[\"input\",\"simulation\",\"objectives\",\"graphics\",\"media\",\"hashes\"]}";
  write_line(header, true);
}

void TraceWriter::write_line(std::string_view line, bool include_in_hash) {
  if (!output_.is_open()) {
    throw std::runtime_error("trace writer is not open");
  }
  output_ << line << '\n';
  if (!output_) {
    throw std::runtime_error("trace write failed");
  }
  if (include_in_hash) {
    const auto* data = reinterpret_cast<const std::byte*>(line.data());
    digest_.update(std::span<const std::byte>(data, line.size()));
    const std::byte newline = std::byte{'\n'};
    digest_.update(std::span<const std::byte>(&newline, 1U));
  }
}

void TraceWriter::append(std::uint64_t tick, TraceDomain domain, std::string payload) {
  if (domain == TraceDomain::Hashes || (has_tick_ && tick < previous_tick_)) {
    throw std::runtime_error("invalid trace event order");
  }
  if (payload.empty()) {
    payload = "null";
  }
  if (payload.front() != '{' && payload.front() != '[' && payload.front() != '"' &&
      payload != "null" && payload != "true" && payload != "false") {
    payload = "\"" + json_escape(payload) + "\"";
  }
  ++count_;
  previous_tick_ = tick;
  has_tick_ = true;
  const std::string line = "{\"type\":\"event\",\"sequence\":" +
                           std::to_string(count_) + ",\"tick\":" + std::to_string(tick) +
                           ",\"domain\":\"" + std::string(trace_domain_name(domain)) +
                           "\",\"payload\":" + payload + "}";
  write_line(line, true);
}

void TraceWriter::close() {
  if (!output_.is_open()) {
    return;
  }
  const auto result = digest_.finish();
  std::ostringstream hash;
  hash << std::hex << std::setfill('0');
  for (const std::byte value : result) {
    hash << std::setw(2) << static_cast<unsigned>(std::to_integer<std::uint8_t>(value));
  }
  write_line("{\"type\":\"hashes\",\"event_count\":" + std::to_string(count_) +
                 ",\"sha256\":\"" + hash.str() + "\"}",
             false);
  output_.flush();
  output_.close();
}

TraceReader TraceReader::read(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("cannot open replay trace: " + path.string());
  }
  TraceReader result;
  Sha256 digest;
  std::string line;
  bool first = true;
  while (std::getline(input, line)) {
    if (first) {
      first = false;
      if (line.find("\"magic\":\"AC6RTPLY\"") == std::string::npos) {
        throw std::runtime_error("trace magic is not AC6RTPLY");
      }
      result.header_.version = static_cast<std::uint32_t>(json_uint(line, "version"));
      result.header_.xex_sha256 = json_field(line, "xex_sha256");
      const std::string backend = json_field(line, "backend");
      if (backend == "vulkan") {
        result.header_.backend = GraphicsBackend::Vulkan;
      } else if (backend != "headless") {
        throw std::runtime_error("trace backend is unknown");
      }
      const std::string expected_domains =
          "\"domains\":[\"input\",\"simulation\",\"objectives\",\"graphics\",\"media\",\"hashes\"]";
      if (result.header_.version == 4U && line.find(expected_domains) == std::string::npos) {
        throw std::runtime_error("AC6RTPLY v4 domain set is incomplete");
      }
      const auto* data = reinterpret_cast<const std::byte*>(line.data());
      digest.update(std::span<const std::byte>(data, line.size()));
      const std::byte newline = std::byte{'\n'};
      digest.update(std::span<const std::byte>(&newline, 1U));
      continue;
    }
    if (line.find("\"type\":\"event\"") != std::string::npos) {
      TraceEvent event;
      event.sequence = json_uint(line, "sequence");
      event.tick = json_uint(line, "tick");
      if (!parse_trace_domain(json_field(line, "domain"), event.domain) ||
          event.domain == TraceDomain::Hashes) {
        throw std::runtime_error("trace event domain is unknown or reserved");
      }
      const std::string payload_key = "\"payload\":";
      const std::size_t payload_start = line.find(payload_key);
      if (payload_start == std::string::npos || line.back() != '}') {
        throw std::runtime_error("trace event payload is missing");
      }
      event.payload = line.substr(payload_start + payload_key.size(),
                                  line.size() - payload_start - payload_key.size() - 1U);
      result.events_.push_back(std::move(event));
      const auto* data = reinterpret_cast<const std::byte*>(line.data());
      digest.update(std::span<const std::byte>(data, line.size()));
      const std::byte newline = std::byte{'\n'};
      digest.update(std::span<const std::byte>(&newline, 1U));
    } else if (line.find("\"type\":\"hashes\"") != std::string::npos) {
      result.declared_event_count_ = json_uint(line, "event_count");
      result.integrity_hash_ = json_field(line, "sha256");
      result.has_integrity_hash_ = true;
    } else if (!line.empty()) {
      throw std::runtime_error("unknown AC6RTPLY record");
    }
  }
  if (first) {
    throw std::runtime_error("empty replay trace");
  }
  if (result.header_.version == 4U) {
    if (!result.has_integrity_hash_ || result.declared_event_count_ != result.events_.size()) {
      throw std::runtime_error("AC6RTPLY v4 integrity trailer is missing or inconsistent");
    }
    if (result.integrity_hash_ != digest_hex(digest.finish())) {
      throw std::runtime_error("AC6RTPLY v4 integrity hash mismatch");
    }
  }
  return result;
}

std::vector<TraceInputEvent> TraceReader::input_events() const {
  std::vector<TraceInputEvent> result;
  for (const auto& event : events_) {
    if (event.domain != TraceDomain::Input) {
      continue;
    }
    const auto& payload = event.payload;
    if (payload.empty() || payload.front() != '{' || payload.back() != '}') {
      throw RuntimeTrap("replay input payload is not an object");
    }
    // The input domain also carries read-only XAM poll observations.  Only
    // records with the canonical frame discriminator are replay inputs.
    if (payload.find("\"buttons\":") == std::string_view::npos) {
      continue;
    }
    InputFrame frame;
    frame.buttons = checked_unsigned<std::uint16_t>(json_uint(payload, "buttons"),
                                                    "buttons");
    frame.left_trigger = checked_unsigned<std::uint8_t>(
        json_uint(payload, "left_trigger"), "left_trigger");
    frame.right_trigger = checked_unsigned<std::uint8_t>(
        json_uint(payload, "right_trigger"), "right_trigger");
    frame.left_x = checked_signed<std::int16_t>(json_int(payload, "lx"), "lx");
    frame.left_y = checked_signed<std::int16_t>(json_int(payload, "ly"), "ly");
    frame.right_x = checked_signed<std::int16_t>(json_int(payload, "rx"), "rx");
    frame.right_y = checked_signed<std::int16_t>(json_int(payload, "ry"), "ry");
    frame.connected = json_bool(payload, "connected");
    result.push_back(TraceInputEvent{event.tick, frame});
  }
  return result;
}

void TraceReader::validate(std::string_view expected_xex_sha256) const {
  if (header_.version != 4U) {
    throw RuntimeTrap("only AC6RTPLY v4 is executable; v2/v3 are inspection-only");
  }
  if (header_.xex_sha256 != expected_xex_sha256) {
    throw RuntimeTrap("replay XEX identity mismatch");
  }
  if (!has_integrity_hash_ || declared_event_count_ != events_.size()) {
    throw RuntimeTrap("replay integrity trailer is missing");
  }
  std::uint64_t previous_tick = 0;
  for (std::size_t index = 0; index < events_.size(); ++index) {
    const auto& event = events_[index];
    if (event.sequence != index + 1U || (index != 0U && event.tick < previous_tick)) {
      throw RuntimeTrap("replay event sequence or scheduler tick is not deterministic");
    }
    previous_tick = event.tick;
  }
}

}  // namespace ac6demo
