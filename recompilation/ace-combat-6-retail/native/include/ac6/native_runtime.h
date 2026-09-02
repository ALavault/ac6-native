#pragma once

#include "ac6/native_frontend.h"
#include "ac6/native_guest_media.h"
#include "ac6/native_guest_memory.h"
#include "ac6/native_guest_vd.h"
#include "ac6/native_ppc_abi.h"
#include "ac6/native_services.h"
#include "ac6/native_vulkan_backend.h"
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

  [[nodiscard]] const RuntimeDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }
  [[nodiscard]] RuntimeState state() const noexcept { return diagnostics_.state; }
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

 private:
  NativeRuntime(MediaInput media, std::filesystem::path user_data);
  void fail(std::string message) noexcept;

  MediaInput media_;
  std::filesystem::path user_data_;
  RuntimeDiagnostics diagnostics_;
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
};

}  // namespace ac6::native
