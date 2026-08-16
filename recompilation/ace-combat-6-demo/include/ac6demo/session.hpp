#pragma once

#include "ac6demo/content.hpp"
#include "ac6demo/graphics.hpp"
#include "ac6demo/guest_bridge.hpp"
#include "ac6demo/input.hpp"
#include "ac6demo/ppc.hpp"
#include "ac6demo/trace.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>

namespace ac6demo {

class DeterministicScheduler final {
public:
  void register_thread(std::uint32_t id);
  void begin_tick();
  void yield(std::uint32_t id);
  void finish_tick();

  [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
  [[nodiscard]] std::uint32_t token() const noexcept { return token_; }

private:
  std::set<std::uint32_t> threads_;
  std::uint64_t tick_{};
  std::uint32_t token_{};
  bool tick_open_{};
};

enum class DemoSessionState : std::uint8_t { Frontend, Guest, Stopped };

class DemoSession final {
public:
  DemoSession(std::filesystem::path store, GraphicsBackend backend);

  void start(const std::filesystem::path &trace_path, bool frontend_only = false);
  void step(InputFrame input = {});
  void begin_xam_movie_record();
  void begin_xam_movie_replay(std::string_view movie);
  [[nodiscard]] std::string finalize_xam_movie();
  void stop();

  [[nodiscard]] DemoSessionState state() const noexcept { return state_; }
  [[nodiscard]] std::uint64_t tick() const noexcept {
    return scheduler_.tick();
  }
  [[nodiscard]] const std::filesystem::path &store() const noexcept {
    return store_;
  }
  [[nodiscard]] GuestSchedulerSnapshot
  guest_scheduler_snapshot() const noexcept {
    return guest_.scheduler_snapshot();
  }
  [[nodiscard]] std::vector<GuestControlFlowEdge>
  control_flow_snapshot() const {
    return guest_.control_flow_snapshot();
  }
  void enable_function_reachability(bool enabled) noexcept {
    guest_.enable_function_reachability(enabled);
  }
  [[nodiscard]] std::vector<GuestFunctionReachability>
  function_reachability_snapshot() const {
    return guest_.function_reachability_snapshot();
  }
  [[nodiscard]] std::uint64_t graphics_present_count() const noexcept {
    return guest_.graphics_present_count();
  }
  [[nodiscard]] XenosRingSnapshot xenos_ring_snapshot() const noexcept {
    return guest_.xenos_ring_snapshot();
  }
  [[nodiscard]] std::vector<XenosCommand> consume_renderer_commands() {
    return guest_.consume_xenos_renderer_commands();
  }
  [[nodiscard]] std::vector<std::byte>
  load_guest_bytes(std::uint32_t address, std::size_t length) const {
    return memory_.load_bytes(address, length);
  }
  // Renderer effects may write only into an allocation already owned by the
  // guest bridge.  This is intentionally not a general host framebuffer
  // upload: an unowned or partially mapped destination traps.
  void store_guest_bytes(std::uint32_t address,
                         std::span<const std::byte> bytes) {
    if (!guest_.owns_allocation(address, bytes.size()) ||
        !memory_.mapped(address, bytes.size())) {
      throw RuntimeTrap("renderer destination is not a qualified guest allocation",
                        tick(), 0, address);
    }
    memory_.store_bytes(address, bytes);
  }
  [[nodiscard]] const VdSwapSnapshot &vd_swap_snapshot() const noexcept {
    return guest_.vd_swap_snapshot();
  }

private:
  std::filesystem::path store_;
  std::unique_ptr<VfsMount> mount_;
  GraphicsProfile profile_;
  GuestMemory memory_;
  PpcContext context_;
  PpcRuntimeHooks hooks_;
  GuestBridge guest_;
  DeterministicScheduler scheduler_;
  TraceWriter trace_;
  std::uint32_t guest_entry_point_{};
  bool guest_started_{};
  bool frontend_only_{};
  bool has_scheduler_snapshot_{};
  GuestSchedulerSnapshot scheduler_snapshot_{};
  DemoSessionState state_{DemoSessionState::Stopped};
};

} // namespace ac6demo
