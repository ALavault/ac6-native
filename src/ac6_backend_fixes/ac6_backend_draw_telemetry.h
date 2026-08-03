#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ac6::backend {

enum class BackendDrawApi : uint8_t {
  kVulkan = 0,
  kD3D12 = 1,
  kCount,
};

struct BackendDrawTelemetrySnapshot {
  uint64_t host_issue_called = 0;
  uint64_t backend_success = 0;
  uint64_t host_draw_emitted = 0;
  std::array<uint64_t, static_cast<std::size_t>(BackendDrawApi::kCount)>
      host_issue_called_by_api{};
  std::array<uint64_t, static_cast<std::size_t>(BackendDrawApi::kCount)>
      backend_success_by_api{};
  std::array<uint64_t, static_cast<std::size_t>(BackendDrawApi::kCount)>
      host_draw_emitted_by_api{};
};

inline BackendDrawTelemetrySnapshot BackendDrawTelemetryDelta(
    const BackendDrawTelemetrySnapshot &current,
    const BackendDrawTelemetrySnapshot &previous) noexcept {
  BackendDrawTelemetrySnapshot delta;
  for (std::size_t i = 0; i < static_cast<std::size_t>(BackendDrawApi::kCount);
       ++i) {
    auto subtract_or_restart = [](uint64_t value, uint64_t old_value) {
      return value >= old_value ? value - old_value : value;
    };
    delta.host_issue_called_by_api[i] =
        subtract_or_restart(current.host_issue_called_by_api[i],
                            previous.host_issue_called_by_api[i]);
    delta.backend_success_by_api[i] = subtract_or_restart(
        current.backend_success_by_api[i], previous.backend_success_by_api[i]);
    delta.host_draw_emitted_by_api[i] =
        subtract_or_restart(current.host_draw_emitted_by_api[i],
                            previous.host_draw_emitted_by_api[i]);
    delta.host_issue_called += delta.host_issue_called_by_api[i];
    delta.backend_success += delta.backend_success_by_api[i];
    delta.host_draw_emitted += delta.host_draw_emitted_by_api[i];
  }
  return delta;
}

// Cumulative backend draw markers. A successful IssueDraw may intentionally
// emit no host draw command (copy, no-effect and async-placeholder paths), so
// the three counters must remain independent.
class BackendDrawTelemetry {
public:
  void ReportHostIssueCalled(BackendDrawApi api) noexcept {
    host_issue_called_[Index(api)].fetch_add(1, std::memory_order_relaxed);
  }

  void ReportBackendResult(BackendDrawApi api, bool success) noexcept {
    if (success) {
      backend_success_[Index(api)].fetch_add(1, std::memory_order_relaxed);
    }
  }

  void ReportHostDrawEmitted(BackendDrawApi api) noexcept {
    host_draw_emitted_[Index(api)].fetch_add(1, std::memory_order_relaxed);
  }

  BackendDrawTelemetrySnapshot Snapshot() const noexcept {
    BackendDrawTelemetrySnapshot snapshot;
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(BackendDrawApi::kCount); ++i) {
      snapshot.host_issue_called_by_api[i] =
          host_issue_called_[i].load(std::memory_order_relaxed);
      snapshot.backend_success_by_api[i] =
          backend_success_[i].load(std::memory_order_relaxed);
      snapshot.host_draw_emitted_by_api[i] =
          host_draw_emitted_[i].load(std::memory_order_relaxed);
      snapshot.host_issue_called += snapshot.host_issue_called_by_api[i];
      snapshot.backend_success += snapshot.backend_success_by_api[i];
      snapshot.host_draw_emitted += snapshot.host_draw_emitted_by_api[i];
    }
    return snapshot;
  }

  void Reset() noexcept {
    for (std::size_t i = 0;
         i < static_cast<std::size_t>(BackendDrawApi::kCount); ++i) {
      host_issue_called_[i].store(0, std::memory_order_relaxed);
      backend_success_[i].store(0, std::memory_order_relaxed);
      host_draw_emitted_[i].store(0, std::memory_order_relaxed);
    }
  }

private:
  static constexpr std::size_t Index(BackendDrawApi api) noexcept {
    const std::size_t index = static_cast<std::size_t>(api);
    return index < static_cast<std::size_t>(BackendDrawApi::kCount) ? index : 0;
  }

  std::array<std::atomic<uint64_t>,
             static_cast<std::size_t>(BackendDrawApi::kCount)>
      host_issue_called_{};
  std::array<std::atomic<uint64_t>,
             static_cast<std::size_t>(BackendDrawApi::kCount)>
      backend_success_{};
  std::array<std::atomic<uint64_t>,
             static_cast<std::size_t>(BackendDrawApi::kCount)>
      host_draw_emitted_{};
};

} // namespace ac6::backend
