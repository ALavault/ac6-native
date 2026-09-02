#pragma once

#include <cstdint>
#include <mutex>

namespace ac6::native {

// Real controller state via SDL2's game controller API, exposed to the
// build-only Xenon import boundary (XamInputGetState/SetState/
// GetCapabilities, tools/materialize_native_import_stubs.py). No controller
// physically present for a given user index is a normal, expected result
// (the caller maps it to the real XInput ERROR_DEVICE_NOT_CONNECTED, not a
// fabricated connected state).
class NativeGuestInputService final {
 public:
  NativeGuestInputService() = default;
  NativeGuestInputService(const NativeGuestInputService&) = delete;
  NativeGuestInputService& operator=(const NativeGuestInputService&) = delete;
  ~NativeGuestInputService();

  // Field names and bit layout match the real, Microsoft-published
  // XINPUT_GAMEPAD contract byte-for-byte (a platform-wide protocol
  // struct, identical between Windows and Xbox 360, not specific to this
  // XEX's own compiled layout).
  struct GamepadState final {
    std::uint32_t packet_number{};
    std::uint16_t buttons{};
    std::uint8_t left_trigger{};
    std::uint8_t right_trigger{};
    std::int16_t thumb_lx{};
    std::int16_t thumb_ly{};
    std::int16_t thumb_rx{};
    std::int16_t thumb_ry{};
  };

  [[nodiscard]] bool is_connected(std::uint32_t user_index) noexcept;

  // Returns false if no controller is bound to `user_index` and leaves
  // `state` untouched; the caller maps that to
  // ERROR_DEVICE_NOT_CONNECTED (0x48F), the real XInput contract.
  [[nodiscard]] bool get_state(std::uint32_t user_index,
                               GamepadState& state) noexcept;

  // Best-effort rumble; returns false only for "not connected" (the same
  // shape as get_state), never for an SDL rumble call this host's SDL
  // build or hardware does not support -- a real console always accepts
  // XamInputSetState for a connected pad even on hardware with no motors.
  [[nodiscard]] bool set_vibration(std::uint32_t user_index,
                                   std::uint16_t left_motor,
                                   std::uint16_t right_motor) noexcept;

 private:
  void ensure_initialized_locked() noexcept;

  std::mutex mutex_;
  bool initialized_{};
  bool subsystem_available_{};
  std::uint32_t packet_number_{};
};

NativeGuestInputService& native_guest_input_service() noexcept;

}  // namespace ac6::native
