#pragma once

#include "ac6/product_runtime.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ac6 {

enum class TraceGraphicsBackend : std::uint8_t {
  Headless,
  CpuCompatibility,
  VulkanDirect,
};

struct TraceMissionObjectives final {
  ScenarioState state{ScenarioState::Loading};
  std::uint32_t sub_mission{};
  std::uint32_t step{};
  bool script_ended{};
  std::vector<ObjectiveRecord> objectives;
};

struct TraceGraphicsSubmission final {
  TraceGraphicsBackend backend{TraceGraphicsBackend::Headless};
  std::uint32_t draw_packets{};
  bool hud{};
};

// Writes the raw five-domain stream consumed by build_ac6_execution_trace_v2.py.
// It deliberately carries no oracle metadata; sealing commits, binaries, replay
// and probe identities belongs to the evidence tool, outside the product.
class ExecutionTraceJsonlWriter final {
 public:
  ExecutionTraceJsonlWriter() = default;
  ~ExecutionTraceJsonlWriter();
  ExecutionTraceJsonlWriter(const ExecutionTraceJsonlWriter&) = delete;
  ExecutionTraceJsonlWriter& operator=(const ExecutionTraceJsonlWriter&) = delete;

  bool open(const std::filesystem::path& path);
  bool append(std::uint64_t sample_tick, InputFrame input,
              const WorldFrame& simulation,
              TraceMissionObjectives mission,
              TraceGraphicsSubmission graphics);
  bool close();
  bool good() const noexcept { return output_.is_open() && !failed_; }
  std::uint64_t event_count() const noexcept { return sequence_; }

 private:
  bool emit(std::uint64_t tick, const char* domain, const std::string& payload);

  std::ofstream output_;
  std::uint64_t sequence_{};
  std::uint64_t previous_tick_{};
  bool has_tick_{};
  bool failed_{};
};

}  // namespace ac6
