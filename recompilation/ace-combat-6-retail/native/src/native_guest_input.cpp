#include "ac6/native_guest_input.h"

#include <SDL.h>

#include <cstdlib>

namespace ac6::native {

namespace {
constexpr int kMaxUsers = 4;

// r474: a probe-only, env-var-gated synthetic pad on user slot 0, for
// AC6_NATIVE_ALLOW_ENTRY_PROBE runs in a headless sandbox with no real
// SDL controller. Pulses the A button on a fixed 60Hz-tick duty cycle
// (held for the first half of every window, released for the second) so
// an unknown confirmation screen (e.g. the "Deploy with this selection?"
// dialog run_gate.py's oracle route needed an explicit A for, r470) gets
// a chance to advance without needing to know its exact tick offset.
// Default OFF: get_state()/is_connected() are unchanged unless set.
bool fake_pad_enabled() noexcept {
  return std::getenv("AC6_NATIVE_INPUT_AUTO_CONFIRM") != nullptr;
}

// Real XInput button bit layout (Microsoft's own published XINPUT_GAMEPAD
// wButtons constants) -- a platform-wide protocol value, not specific to
// this XEX's own compiled layout.
constexpr std::uint16_t kButtonDpadUp = 0x0001u;
constexpr std::uint16_t kButtonDpadDown = 0x0002u;
constexpr std::uint16_t kButtonDpadLeft = 0x0004u;
constexpr std::uint16_t kButtonDpadRight = 0x0008u;
constexpr std::uint16_t kButtonStart = 0x0010u;
constexpr std::uint16_t kButtonBack = 0x0020u;
constexpr std::uint16_t kButtonLeftThumb = 0x0040u;
constexpr std::uint16_t kButtonRightThumb = 0x0080u;
constexpr std::uint16_t kButtonLeftShoulder = 0x0100u;
constexpr std::uint16_t kButtonRightShoulder = 0x0200u;
constexpr std::uint16_t kButtonA = 0x1000u;
constexpr std::uint16_t kButtonB = 0x2000u;
constexpr std::uint16_t kButtonX = 0x4000u;
constexpr std::uint16_t kButtonY = 0x8000u;

void set_button(SDL_GameController* controller, SDL_GameControllerButton id,
                std::uint16_t bit, std::uint16_t& buttons) noexcept {
  if (SDL_GameControllerGetButton(controller, id) != 0) buttons |= bit;
}
}  // namespace

NativeGuestInputService::~NativeGuestInputService() {
  if (subsystem_available_) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void NativeGuestInputService::ensure_initialized_locked() noexcept {
  if (initialized_) return;
  initialized_ = true;
  subsystem_available_ = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
}

bool NativeGuestInputService::is_connected(std::uint32_t user_index) noexcept {
  if (user_index == 0u && fake_pad_enabled()) return true;
  std::lock_guard<std::mutex> lock(mutex_);
  ensure_initialized_locked();
  if (!subsystem_available_ || user_index >= static_cast<std::uint32_t>(kMaxUsers)) {
    return false;
  }
  return SDL_IsGameController(static_cast<int>(user_index)) == SDL_TRUE;
}

bool NativeGuestInputService::get_state(std::uint32_t user_index,
                                        GamepadState& state) noexcept {
  if (user_index == 0u && fake_pad_enabled()) {
    std::lock_guard<std::mutex> lock(mutex_);
    constexpr std::uint32_t kWindowTicks = 30u;  // ~0.5s at 60Hz
    const bool press = (packet_number_ % kWindowTicks) < (kWindowTicks / 2u);
    state = GamepadState{};
    state.buttons = press ? kButtonA : 0u;
    state.packet_number = ++packet_number_;
    return true;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  ensure_initialized_locked();
  if (!subsystem_available_ || user_index >= static_cast<std::uint32_t>(kMaxUsers)) {
    return false;
  }
  if (SDL_IsGameController(static_cast<int>(user_index)) != SDL_TRUE) return false;
  SDL_GameController* controller =
      SDL_GameControllerOpen(static_cast<int>(user_index));
  if (controller == nullptr) return false;
  SDL_GameControllerUpdate();

  std::uint16_t buttons = 0u;
  set_button(controller, SDL_CONTROLLER_BUTTON_DPAD_UP, kButtonDpadUp, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN, kButtonDpadDown, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT, kButtonDpadLeft, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, kButtonDpadRight, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_START, kButtonStart, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_BACK, kButtonBack, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_LEFTSTICK, kButtonLeftThumb, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK, kButtonRightThumb, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, kButtonLeftShoulder, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, kButtonRightShoulder, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_A, kButtonA, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_B, kButtonB, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_X, kButtonX, buttons);
  set_button(controller, SDL_CONTROLLER_BUTTON_Y, kButtonY, buttons);

  // SDL's trigger axes are 0..32767, matching XInput's raw magnitude but
  // not its 0..255 byte range; XInput's Y sticks are positive-up, SDL's are
  // positive-down (its own documented convention) -- negated to match.
  const Sint16 left_trigger =
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
  const Sint16 right_trigger =
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
  const Sint16 left_y =
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
  const Sint16 right_y =
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

  state.buttons = buttons;
  state.left_trigger = static_cast<std::uint8_t>(
      static_cast<std::uint16_t>(left_trigger) >> 7);
  state.right_trigger = static_cast<std::uint8_t>(
      static_cast<std::uint16_t>(right_trigger) >> 7);
  state.thumb_lx = static_cast<std::int16_t>(
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX));
  state.thumb_ly = static_cast<std::int16_t>(
      left_y == -32768 ? 32767 : -left_y);
  state.thumb_rx = static_cast<std::int16_t>(
      SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX));
  state.thumb_ry = static_cast<std::int16_t>(
      right_y == -32768 ? 32767 : -right_y);
  state.packet_number = ++packet_number_;
  return true;
}

bool NativeGuestInputService::set_vibration(std::uint32_t user_index,
                                            std::uint16_t left_motor,
                                            std::uint16_t right_motor) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  ensure_initialized_locked();
  if (!subsystem_available_ || user_index >= static_cast<std::uint32_t>(kMaxUsers)) {
    return false;
  }
  if (SDL_IsGameController(static_cast<int>(user_index)) != SDL_TRUE) return false;
  SDL_GameController* controller =
      SDL_GameControllerOpen(static_cast<int>(user_index));
  if (controller == nullptr) return false;
  // Best-effort: a real console accepts XamInputSetState for a connected
  // pad even on hardware without motors, so a rumble failure here is not
  // reported as "not connected" -- only the presence check above is.
  SDL_GameControllerRumble(controller, left_motor, right_motor, 0u);
  return true;
}

NativeGuestInputService& native_guest_input_service() noexcept {
  static NativeGuestInputService service;
  return service;
}

}  // namespace ac6::native
