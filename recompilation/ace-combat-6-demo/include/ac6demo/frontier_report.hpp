#pragma once

#include "ac6demo/graphics.hpp"
#include "ac6demo/guest_bridge.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ac6demo {

enum class ProbeUntil : std::uint8_t { Frontend, Mission, Terminal };

[[nodiscard]] bool parse_probe_until(std::string_view value,
                                     ProbeUntil *result) noexcept;
[[nodiscard]] std::string_view probe_until_name(ProbeUntil value) noexcept;

struct ProbeMilestones final {
  std::uint64_t presents{};
  bool frontend{};
  bool mission{};
  bool terminal{};
};

struct FrontierSnapshot final {
  std::string diagnostic;
  std::uint64_t tick{};
  std::uint32_t thread{};
  std::uint32_t lr{};
  std::uint32_t address{};
};

// Convert a bounded probe that did not reach its requested milestone into an
// explicit liveness frontier. This records the observed runnable or blocked
// guest edge only; it never wakes a thread or changes guest state.
[[nodiscard]] std::optional<FrontierSnapshot>
derive_progress_frontier(ProbeUntil until, std::uint64_t completed_ticks,
                         const ProbeMilestones &milestones,
                         const GuestSchedulerSnapshot &scheduler);

struct FrontierReportInput final {
  ProbeUntil until{ProbeUntil::Frontend};
  std::uint64_t max_ticks{};
  GraphicsBackend backend{GraphicsBackend::Headless};
  std::filesystem::path trace_path;
  std::string outcome;
  std::string session_state;
  std::uint64_t completed_ticks{};
  ProbeMilestones milestones{};
  XenosRingSnapshot graphics_ring{};
  VdSwapSnapshot graphics_swap{};
  GuestSchedulerSnapshot scheduler{};
  std::vector<GuestControlFlowEdge> control_flow;
  std::optional<FrontierSnapshot> frontier;
};

void write_frontier_report(const std::filesystem::path &path,
                           const FrontierReportInput &input);

} // namespace ac6demo
