#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6/native_guest_input.h"

#include <cassert>

int main() {
  auto& service = ac6::native::native_guest_input_service();

  // This sandbox has no real controller attached, so every user index must
  // report "not connected" -- a real, expected result (ERROR_DEVICE_NOT_
  // CONNECTED at the import boundary), not an error in the service itself.
  assert(!service.is_connected(0u));
  ac6::native::NativeGuestInputService::GamepadState state{};
  assert(!service.get_state(0u, state));
  assert(!service.set_vibration(0u, 0xffffu, 0xffffu));

  // Out-of-range user indices (XInput supports exactly 4) must also report
  // "not connected" without touching SDL at all.
  assert(!service.is_connected(4u));
  assert(!service.get_state(4u, state));
  assert(!service.set_vibration(4u, 0u, 0u));

  return 0;
}
