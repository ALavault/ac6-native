#pragma once

#include "ac6demo/graphics.hpp"
#include "ac6demo/hash.hpp"
#include "ac6demo/content.hpp"
#include "ac6demo/input.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace ac6demo {

enum class TraceDomain : std::uint8_t {
  Input,
  Simulation,
  Objectives,
  Graphics,
  Media,
  Hashes,
};

[[nodiscard]] std::string_view trace_domain_name(TraceDomain domain) noexcept;
[[nodiscard]] bool parse_trace_domain(std::string_view name, TraceDomain& domain) noexcept;

struct TraceHeader final {
  std::string xex_sha256;
  GraphicsBackend backend{GraphicsBackend::Headless};
  std::uint32_t version{4};
};

struct TraceEvent final {
  std::uint64_t sequence{};
  std::uint64_t tick{};
  TraceDomain domain{TraceDomain::Input};
  std::string payload;
};

struct TraceInputEvent final {
  std::uint64_t tick{};
  InputFrame frame{};

  bool operator==(const TraceInputEvent&) const = default;
};

class TraceWriter final {
 public:
  TraceWriter() = default;
  ~TraceWriter();
  TraceWriter(const TraceWriter&) = delete;
  TraceWriter& operator=(const TraceWriter&) = delete;

  void open(const std::filesystem::path& path, GraphicsBackend backend,
            std::string_view xex_sha256 = kQualifiedXexSha256);
  void append(std::uint64_t tick, TraceDomain domain, std::string payload);
  void close();
  [[nodiscard]] bool open() const noexcept { return output_.is_open(); }
  [[nodiscard]] std::uint64_t count() const noexcept { return count_; }

 private:
  void write_line(std::string_view line, bool include_in_hash);

  std::ofstream output_;
  std::uint64_t count_{};
  std::uint64_t previous_tick_{};
  bool has_tick_{};
  Sha256 digest_;
};

class TraceReader final {
 public:
  static TraceReader read(const std::filesystem::path& path);

  [[nodiscard]] const TraceHeader& header() const noexcept { return header_; }
  [[nodiscard]] const std::vector<TraceEvent>& events() const noexcept { return events_; }
  [[nodiscard]] std::vector<TraceInputEvent> input_events() const;
  void validate(std::string_view expected_xex_sha256 = kQualifiedXexSha256) const;

 private:
  TraceHeader header_;
  std::vector<TraceEvent> events_;
  std::string integrity_hash_;
  std::uint64_t declared_event_count_{};
  bool has_integrity_hash_{};
};

}  // namespace ac6demo
