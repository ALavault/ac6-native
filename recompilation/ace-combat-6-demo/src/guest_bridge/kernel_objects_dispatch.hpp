// Internal GuestBridge dispatch wrapper; included inside
// dispatch_import_qualified in guest_bridge.cpp.
//
// The original fragment is preserved byte-for-byte in
// kernel_objects_dispatch_original.hpp. Intercept the one import whose ABI was
// previously modelled incorrectly, then delegate every other name unchanged.
if (std::string_view{name} == "KeSetAffinityThread") {
  auto &bridge = require_bridge();
  const auto object = context.r3.u32;
  const auto requested_mask = context.r4.u32;
  if (!bridge.is_guest_thread_object(object) ||
      !ac6demo::valid_xenon_affinity_mask(requested_mask)) {
    return false;
  }

  ac6demo::XenonAffinityTransition transition;
  try {
    transition = ac6demo::make_xenon_affinity_transition(
        bridge.guest_thread_processor(object), requested_mask);
  } catch (const std::exception &) {
    return false;
  }
  if (!bridge.pin_guest_thread_processor(object, requested_mask)) {
    return false;
  }

  // Xbox/Xenon KeSetAffinityThread returns the previous KAFFINITY in r3.
  // r5 is not an out pointer and must remain untouched.
  context.r3.u32 = transition.previous_mask;
  trace_affinity_transition(context, object, transition, context.r3.u32);
  return true;
}

#include "kernel_objects_dispatch_original.hpp"
