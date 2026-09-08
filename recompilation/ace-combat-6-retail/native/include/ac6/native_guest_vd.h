#pragma once

#include "ac6/native_vulkan_backend.h"
#include "ac6/native_xenos.h"

#include <atomic>
#include <cstdint>
#include <thread>
#include <mutex>
#include <vector>

namespace ac6::native {

class VulkanOffscreenTarget;

// Guest-facing Vd service used by the build-only Xenon import boundary. It
// observes the guest ring, decodes complete PM4 packets through VdBridge and
// publishes the consumed read pointer only after the native backend accepts
// the packets. Unknown packets therefore leave the guest readback untouched.
class NativeGuestVdService final {
 public:
  NativeGuestVdService() = default;
  NativeGuestVdService(const NativeGuestVdService&) = delete;
  NativeGuestVdService& operator=(const NativeGuestVdService&) = delete;

  void bind(std::uint8_t* base, MmioBus& bus, VdBridge& bridge,
            XenosState& state, VulkanBackend& backend) noexcept;
  // Optional non-owning offscreen target: when bound, every PRESENT packet
  // accepted by submit() is executed on the target (opaque black) before
  // the guest read-pointer writeback. Null (default) keeps pure-validation
  // behavior. A failed present executes nothing and skips the guest
  // writeback; the bus decode cursor has already advanced, exactly as with
  // any accepted decode whose backend step rejects.
  void bind_offscreen(VulkanOffscreenTarget* target) noexcept;
  void unbind() noexcept;
  void register_allocation(std::uint8_t* base, GuestAddress guest_address,
                           std::uint32_t bytes) noexcept;
  void initialize_ring(std::uint8_t* base, GuestAddress physical_base,
                       std::uint32_t log2_size) noexcept;
  void enable_readback(std::uint8_t* base,
                       GuestAddress physical_address) noexcept;
  void publish_write_address(std::uint8_t* base,
                             GuestAddress guest_address) noexcept;

 private:
  [[nodiscard]] bool resolve_physical_locked(GuestAddress physical,
                                             GuestAddress& guest) const noexcept;
  void start_poller_locked() noexcept;
  void poll_loop() noexcept;
  void poll_once() noexcept;
  void drain_locked() noexcept;
  [[nodiscard]] bool discover_write_index_locked(
      std::uint32_t& write_index) noexcept;

  struct Allocation final {
    GuestAddress guest{};
    GuestAddress physical{};
    std::uint32_t bytes{};
  };

  std::mutex mutex_;
  std::uint8_t* base_{};
  MmioBus* bus_{};
  VdBridge* bridge_{};
  XenosState* state_{};
  VulkanBackend* backend_{};
  VulkanOffscreenTarget* present_target_{};
  GuestAddress ring_base_{};
  GuestAddress ring_guest_base_{};
  std::uint32_t ring_size_{};
  GuestAddress readback_{};
  GuestAddress producer_object_{};
  std::vector<std::uint32_t> ring_words_{};
  std::vector<Allocation> allocations_{};
  bool poller_started_{};
  std::atomic<bool> stop_poller_{false};
};

NativeGuestVdService& native_guest_vd_service() noexcept;

}  // namespace ac6::native
