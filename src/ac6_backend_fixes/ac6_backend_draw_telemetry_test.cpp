#include "ac6_backend_draw_telemetry.h"

#include <cassert>

int main() {
  using ac6::backend::BackendDrawApi;
  using ac6::backend::BackendDrawTelemetry;

  BackendDrawTelemetry telemetry;
  auto snapshot = telemetry.Snapshot();
  assert(snapshot.host_issue_called == 0);
  assert(snapshot.backend_success == 0);
  assert(snapshot.host_draw_emitted == 0);

  telemetry.ReportHostIssueCalled(BackendDrawApi::kVulkan);
  telemetry.ReportBackendResult(BackendDrawApi::kVulkan, true);
  telemetry.ReportHostDrawEmitted(BackendDrawApi::kVulkan);

  telemetry.ReportHostIssueCalled(BackendDrawApi::kVulkan);
  telemetry.ReportBackendResult(BackendDrawApi::kVulkan, false);

  telemetry.ReportHostIssueCalled(BackendDrawApi::kD3D12);
  telemetry.ReportBackendResult(BackendDrawApi::kD3D12, true);

  snapshot = telemetry.Snapshot();
  assert(snapshot.host_issue_called == 3);
  assert(snapshot.backend_success == 2);
  assert(snapshot.host_draw_emitted == 1);
  assert(snapshot.host_issue_called_by_api[static_cast<std::size_t>(
             BackendDrawApi::kVulkan)] == 2);
  assert(snapshot.backend_success_by_api[static_cast<std::size_t>(
             BackendDrawApi::kVulkan)] == 1);
  assert(snapshot.host_draw_emitted_by_api[static_cast<std::size_t>(
             BackendDrawApi::kVulkan)] == 1);
  assert(snapshot.host_issue_called_by_api[static_cast<std::size_t>(
             BackendDrawApi::kD3D12)] == 1);
  assert(snapshot.backend_success_by_api[static_cast<std::size_t>(
             BackendDrawApi::kD3D12)] == 1);
  assert(snapshot.host_draw_emitted_by_api[static_cast<std::size_t>(
             BackendDrawApi::kD3D12)] == 0);

  ac6::backend::BackendDrawTelemetrySnapshot previous;
  previous.host_issue_called = 1;
  previous.backend_success = 1;
  previous.host_issue_called_by_api[static_cast<std::size_t>(
      BackendDrawApi::kVulkan)] = 1;
  previous.backend_success_by_api[static_cast<std::size_t>(
      BackendDrawApi::kVulkan)] = 1;
  auto delta = ac6::backend::BackendDrawTelemetryDelta(snapshot, previous);
  assert(delta.host_issue_called == 2);
  assert(delta.backend_success == 1);
  assert(delta.host_draw_emitted == 1);

  // A counter reset is treated as a new epoch, never as unsigned underflow.
  ac6::backend::BackendDrawTelemetrySnapshot reset_epoch;
  reset_epoch.host_issue_called = 1;
  reset_epoch.host_issue_called_by_api[static_cast<std::size_t>(
      BackendDrawApi::kVulkan)] = 1;
  delta = ac6::backend::BackendDrawTelemetryDelta(reset_epoch, snapshot);
  assert(delta.host_issue_called == 1);
  assert(delta.backend_success == 0);
  assert(delta.host_draw_emitted == 0);

  telemetry.Reset();
  snapshot = telemetry.Snapshot();
  assert(snapshot.host_issue_called == 0);
  assert(snapshot.backend_success == 0);
  assert(snapshot.host_draw_emitted == 0);
}
