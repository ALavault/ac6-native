#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Frontend and renderer state, read-only and opt-in. Extracted from
// run_entry, which the source budget caps at 220 lines.
inline void trace_frontend_state(ac6demo::GuestMemory &memory,
                                 std::uint64_t tick) {
  // Frontend state, read-only and opt-in. The startup mode task's own state
  // word and the mode manager's transition request are the two numbers that
  // say whether the frontend is stalled or merely looping, and neither was
  // observable before. Addresses: CModeTaskStartUpDemoOffline is 0x2E7F0080
  // and its update sub_8218A4A0 switches on [this+12]; the manager pointer is
  // the global at 0x827435F8, which that update loads before storing 1 at
  // [manager+24].
  if (std::getenv("AC6_DEMO_WATCH_MODE_STATE") != nullptr) {
    static std::uint32_t previous_state = 0xFFFFFFFFU;
    static std::uint32_t previous_request = 0xFFFFFFFFU;
    constexpr std::uint32_t kStartUpTask = 0x2E7F0080U;
    if (memory.mapped(kStartUpTask, 96U)) {
      const auto state = memory.load_u32(kStartUpTask + 12U);
      if (state != previous_state) {
        std::fprintf(stderr,
                     "AC6_MODE_STATE tick=%llu state=0x%08X counter=0x%08X\n",
                     static_cast<unsigned long long>(tick), state,
                     memory.load_u32(kStartUpTask + 68U));
        previous_state = state;
      }
    }
    if (memory.mapped(0x827435F8U, 4U)) {
      const auto manager = memory.load_u32(0x827435F8U);
      if (manager != 0U && memory.mapped(manager, 64U)) {
        const auto request = memory.load_u32(manager + 24U);
        // The manager's +0x08 is the running mode and +0x0C the one it just
        // left. Printing the mode object with its vtable on every change is
        // what turns "the frontend does nothing" into a named sequence.
        {
          static std::uint32_t current_mode = 0U;
          const auto mode = memory.load_u32(manager + 8U);
          if (mode != current_mode) {
            const auto vtable = (mode != 0U && memory.mapped(mode, 4U))
                                    ? memory.load_u32(mode)
                                    : 0U;
            std::fprintf(stderr,
                         "AC6_MODE_SWITCH tick=%llu mode=0x%08X vtable=0x%08X "
                         "previous=0x%08X\n",
                         static_cast<unsigned long long>(tick), mode, vtable,
                         memory.load_u32(manager + 12U));
            current_mode = mode;
          }
        }
        if (request != previous_request) {
          std::fprintf(stderr,
                       "AC6_MODE_REQUEST tick=%llu manager=0x%08X "
                       "vtable=0x%08X request=0x%08X\n",
                       static_cast<unsigned long long>(tick), manager,
                       memory.load_u32(manager), request);
          previous_request = request;
        }
      }
    }
  }
  // The renderer's own state machine. sub_821AD378 runs once per frame and
  // dispatches on this word through a jump table, doing nothing at all unless
  // it holds 11..19; it is the switch that decides whether a frame is built.
  if (std::getenv("AC6_DEMO_WATCH_RENDER_STATE") != nullptr) {
    static std::uint32_t previous = 0xFFFFFFFFU;
    constexpr std::uint32_t kRenderStateWord = 0x827AD2F0U;
    if (memory.mapped(kRenderStateWord, 4U)) {
      const auto state = memory.load_u32(kRenderStateWord);
      if (state != previous) {
        std::fprintf(stderr, "AC6_RENDER_STATE tick=%llu state=%u\n",
                     static_cast<unsigned long long>(tick), state);
        previous = state;
      }
    }
  }
}
