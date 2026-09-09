#pragma once

#include "ac6/native_frontend.h"
#include "ac6/native_guest_media.h"
#include "ac6/native_guest_memory.h"
#include "ac6/native_guest_vd.h"
#include "ac6/native_ppc_abi.h"
#include "ac6/native_services.h"
#include "ac6/native_vulkan_backend.h"
#include "ac6/native_vulkan_device.h"
#include "ac6/native_xex.h"
#include "ac6/native_xdvdfs.h"
#include "ac6/native_xenos.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace ac6::native {

// r213: thrown by the ExTerminateThread guest import to unwind the
// current native thread cleanly. ExTerminateThread never returns on real
// hardware (confirmed at this XEX's own real call sites: the compiler
// emits no epilogue after either one). Guest threads run as plain C++
// function calls on their own std::thread (see ExCreateThread), so a
// bare throw would otherwise escape the thread entry point and invoke
// std::terminate() on the whole process for what should only end one
// thread -- every guest thread entry point must catch this by value and
// let the thread return normally afterward.
struct GuestThreadTerminated final {};

// r277: defined in the materialized import-stub translation unit. Sets the
// guest-worker stop flag (the wait/delay/park stubs turn it into
// GuestThreadTerminated, which each worker's entry lambda catches) and
// joins every ExCreateThread worker before the runtime tears down, so the
// stubs' globals and the guest address space outlive their last use.
// The detached _xstart entry thread is not in this registry (its loops
// observe only guest memory), which is why shutdown() also releases the
// guest address space to process exit instead of unmapping it.
void native_guest_threads_stop_and_join() noexcept;

enum class RuntimeState : std::uint8_t {
  kCreated,
  kBooted,
  kRunning,
  kStopped,
  kFailed,
};

struct RuntimeDiagnostics final {
  RuntimeState state{RuntimeState::kCreated};
  std::uint64_t presented_frames{};
  std::uint64_t guest_ticks{};
  std::string error;
};

class NativeRuntime final {
 public:
  static std::unique_ptr<NativeRuntime> open(
      const std::filesystem::path& media,
      std::filesystem::path user_data = xdg_data_directory());

  NativeRuntime(const NativeRuntime&) = delete;
  NativeRuntime& operator=(const NativeRuntime&) = delete;
  ~NativeRuntime();

  [[nodiscard]] bool boot();
  // Bind the generated guest import boundary to this runtime's native Vd
  // bridge. The entry probe calls this once after populating dispatch slots.
  void bind_guest_vd() noexcept;
  [[nodiscard]] bool attach_replay(std::vector<ReplaySample> samples);
  // Submit one bounded packet stream; the bridge places it in a power-of-two
  // ring and publishes the write pointer only for these dwords.
  [[nodiscard]] bool submit_ring(std::span<const std::uint32_t> ring_words);
  [[nodiscard]] ServiceError poll_input(std::uint64_t tick, std::uint32_t user,
                                        ReplaySample& sample) noexcept;
  [[nodiscard]] ServiceError save(std::string_view name,
                                  std::span<const std::uint8_t> bytes);
  [[nodiscard]] ServiceError load(std::string_view name,
                                  std::vector<std::uint8_t>& bytes) const;
  [[nodiscard]] bool shutdown() noexcept;

  // r431: presented_frames/state used to be synced from backend_'s own
  // present_count() only inside submit_ring() -- dead code (r292/r418),
  // never called by the real runtime, so this diagnostic stayed 0 even
  // after r430 made the real present path (NativeGuestVdService, via
  // VdSwap) actually increment backend_'s counter. Sync here instead,
  // read-time rather than write-time: real presents happen asynchronously
  // on whatever thread calls VdSwap, with no direct hook back into this
  // object, so catching up whenever a caller asks is simpler and just as
  // correct as trying to push an update from there.
  [[nodiscard]] const RuntimeDiagnostics& diagnostics() const noexcept {
    // r454/r455: presents now split across two counters -- VulkanBackend's
    // (the plain-clear path, still used whenever native_guest_vd falls back
    // from an unpinned-shader rejection, or when no pinned runtime is
    // bound at all) and PinnedShaderRuntime's own (the real-render path).
    // Neither call site ever presents through both for the same packet
    // (native_guest_vd.cpp's pinned_handled_present/use_pinned guards), so
    // the two counters are additive, not overlapping.
    const std::uint64_t total_presented =
        backend_.present_count() +
        (pinned_runtime_ ? pinned_runtime_->present_count() : 0u);
    if (total_presented != diagnostics_.presented_frames) {
      diagnostics_.presented_frames = total_presented;
      if (diagnostics_.state == RuntimeState::kBooted) {
        diagnostics_.state = RuntimeState::kRunning;
      }
    }
    return diagnostics_;
  }
  // r431: routed through diagnostics() so a caller of state() alone still
  // observes the same read-time presented_frames/state sync, instead of
  // risking a stale kBooted if nothing has called diagnostics() yet.
  [[nodiscard]] RuntimeState state() const noexcept { return diagnostics().state; }
  [[nodiscard]] GuestAddressSpace& guest_address_space() noexcept {
    return guest_address_space_;
  }
  [[nodiscard]] const GuestAddressSpace& guest_address_space() const noexcept {
    return guest_address_space_;
  }
  [[nodiscard]] std::size_t loaded_guest_image_size() const noexcept {
    return guest_image_.size();
  }
  [[nodiscard]] const XexMetadata* xex_metadata() const noexcept {
    return xex_metadata_.has_value() ? &*xex_metadata_ : nullptr;
  }
  // r295: true once bind_guest_vd() has successfully wired a real present
  // target into NativeGuestVdService. False on a host with no usable
  // Vulkan device -- not fatal to boot, but a PresentPacket the guest
  // issues will be silently dropped while this is false (see r294).
  [[nodiscard]] bool has_offscreen_present_target() const noexcept {
    return offscreen_target_ != nullptr;
  }
  // r455: read back the offscreen target's current pixels for diagnostic
  // visual verification (e.g. confirming real content replaced the r430
  // placeholder clear, not just that present_count() advanced). Empty
  // vector when there is no target, matching VulkanOffscreenTarget::
  // readback()'s own empty-on-failure contract.
  [[nodiscard]] std::vector<std::uint8_t> readback_offscreen_pixels() const noexcept {
    if (offscreen_target_ == nullptr) return {};
    return offscreen_target_->readback();
  }
  [[nodiscard]] const std::string& offscreen_error() const noexcept {
    static const std::string kNoTarget = "no offscreen target bound";
    return offscreen_target_ != nullptr ? offscreen_target_->error() : kNoTarget;
  }

 private:
  NativeRuntime(MediaInput media, std::filesystem::path user_data);
  void fail(std::string message) noexcept;

  MediaInput media_;
  std::filesystem::path user_data_;
  // r431: mutable so the const diagnostics() accessor can lazily sync
  // presented_frames/state from backend_.present_count() on read -- see
  // that accessor's own comment for why read-time sync, not write-time.
  mutable RuntimeDiagnostics diagnostics_;
  MmioBus bus_;
  VdBridge bridge_;
  XenosState xenos_state_;
  VulkanBackend backend_;
  GuestAddressSpace guest_address_space_;
  GuestMemory guest_memory_;
  std::vector<std::uint8_t> guest_image_;
  PpcDispatcher dispatcher_;
  std::optional<XexMetadata> xex_metadata_;
  std::unique_ptr<AtomicSaveStore> saves_;
  std::unique_ptr<StrictInputReplay> replay_;
  // r295: bind_guest_vd() constructs these so NativeGuestVdService has a
  // real present target -- previously nothing in the real runtime ever
  // called bind_offscreen(), so a PresentPacket could never be observed
  // or acted on even if the guest issued one (found r294). Null when the
  // host has no usable Vulkan device (matches this project's existing
  // headless-skip contract in native_xenos_tests.cpp); declared in this
  // order so the target (which borrows the device) is destroyed first.
  std::unique_ptr<VulkanDevice> offscreen_device_;
  std::unique_ptr<VulkanOffscreenTarget> offscreen_target_;
  // r454: constructed alongside offscreen_device_/offscreen_target_ (same
  // null-on-no-Vulkan-device contract) and bound to native_guest_vd_service()
  // so the live VdSwap/ring-drain path can route real draws through it
  // instead of VulkanBackend's validate-only submit()/plain-clear present.
  // Declared after offscreen_device_ (which it borrows) so it is destroyed
  // first.
  std::unique_ptr<PinnedShaderRuntime> pinned_runtime_;
};

}  // namespace ac6::native
