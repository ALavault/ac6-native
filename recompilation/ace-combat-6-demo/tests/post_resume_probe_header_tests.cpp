#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "../src/guest_bridge/event_post_set_trace.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <tuple>
#include <vector>

namespace {
using ac6demo::guest_bridge_detail::arm_post_resume_access;
using ac6demo::guest_bridge_detail::initialize_post_resume_watch;
using ac6demo::guest_bridge_detail::post_resume_capture_attempts;
using ac6demo::guest_bridge_detail::post_resume_probe_state;
using ac6demo::guest_bridge_detail::record_post_resume_bytes;
using ac6demo::guest_bridge_detail::record_post_resume_scalar;
using ac6demo::guest_bridge_detail::refuse_post_resume_atomic;
using ac6demo::guest_bridge_detail::reset_post_resume_probe_for_tests;

void arm() {
  arm_post_resume_access(0xE000004CU, 0xE0000048U, 1U, 0x821A69CCU, 7U);
  assert(post_resume_probe_state.phase.load() == 2U);
}

void enable() {
  reset_post_resume_probe_for_tests();
  initialize_post_resume_watch();
  assert(ac6demo::guest_bridge_detail::post_resume_watch_enabled_fast());
}
}  // namespace

int main() {
  setenv("AC6_DEMO_WATCH_POST_RESUME_ACCESS", "0", 1);
  reset_post_resume_probe_for_tests();
  initialize_post_resume_watch();
  assert(!ac6demo::guest_bridge_detail::post_resume_watch_enabled_fast());
  arm_post_resume_access(0xE000004CU, 0xE0000048U, 1U, 0x821A69CCU, 1U);
  record_post_resume_scalar("load32", 0x10U, 4U, 0x01020304U, 2U, 1U,
                            0x821A69CCU, "off", 1U);
  assert(post_resume_capture_attempts() == 0U);
  assert(post_resume_probe_state.phase.load() == 0U);

  setenv("AC6_DEMO_WATCH_POST_RESUME_ACCESS", "1", 1);
  enable();
  arm();
  std::vector<std::thread> contenders;
  for (unsigned index = 0U; index < 16U; ++index) {
    contenders.emplace_back([] {
      record_post_resume_scalar("load32", 0x10U, 4U, 0x01020304U, 8U, 1U,
                                0x821A69CCU, "concurrent", 2U);
    });
  }
  for (auto &contender : contenders) {
    contender.join();
  }
  assert(post_resume_probe_state.phase.load() == 3U);
  assert(post_resume_capture_attempts() >= 1U);

  const std::array<std::uint8_t, 16U> vector_bytes{
      0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
      0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU};
  for (const auto [kind, size, value] :
       {std::tuple{"load8", 1U, 0xABULL},
        std::tuple{"load16", 2U, 0xABCDULL},
        std::tuple{"load32", 4U, 0xABCDEF01ULL},
        std::tuple{"load64", 8U, 0x0123456789ABCDEFULL},
        std::tuple{"store8", 1U, 0x12ULL},
        std::tuple{"store16", 2U, 0x1234ULL},
        std::tuple{"store32", 4U, 0x12345678ULL},
        std::tuple{"store64", 8U, 0x0123456789ABCDEFULL}}) {
    enable();
    arm();
    record_post_resume_scalar(kind, 0x20U, size, value, 9U, 1U, 0x821A69CCU,
                              "width", size);
    assert(post_resume_probe_state.phase.load() == 3U);
  }
  enable();
  arm();
  record_post_resume_bytes("load128", 0x40U, 16U, vector_bytes.data(), 10U,
                           1U, 0x821A69CCU, "vector", 16U);
  assert(post_resume_probe_state.phase.load() == 3U);

  for (const auto [kind, size, reason] :
       {std::tuple{"stwcx", 4U, "atomic_failed"},
        std::tuple{"stwcx", 4U, "atomic_success"},
        std::tuple{"stdcx", 8U, "atomic_failed"},
        std::tuple{"stdcx", 8U, "atomic_success"}}) {
    enable();
    arm();
    refuse_post_resume_atomic(kind, 0x50U, size, 11U, 1U, 0x821A69CCU,
                              reason);
    assert(post_resume_probe_state.phase.load() == 3U);
  }
  return 0;
}
