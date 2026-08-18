#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Two opt-in probes lifted out of trace_frontend_state, which the
// complexity budget caps at 220 lines per function.
inline void trace_service_registry(ac6demo::GuestMemory &memory,
                                   std::uint64_t tick) {
  // The callback registrar sub_821ADC78 guards every one of its
  // bctrl sites on these two globals; zero indirect edges were
  // recorded inside it, so read them rather than assume.
  {
    static std::uint64_t previous_registry = 0xFFFFFFFFFFFFFFFFULL;
    const auto a = memory.mapped(0x82000610U, 4U)
                       ? memory.load_u32(0x82000610U) : 0U;
    const auto b = memory.mapped(0x820006E4U, 4U)
                       ? memory.load_u32(0x820006E4U) : 0U;
    const auto a0 = (a != 0U && memory.mapped(a, 4U))
                        ? memory.load_u32(a) : 0U;
    const auto b0 = (b != 0U && memory.mapped(b, 32U))
                        ? memory.load_u32(b) : 0U;
    const auto b24 = (b0 != 0U && memory.mapped(b0, 32U))
                         ? memory.load_u32(b0 + 24U) : 0U;
    const std::uint64_t key =
        (static_cast<std::uint64_t>(a0) << 32) ^ b0 ^
        (static_cast<std::uint64_t>(b24) << 8);
    if (key != previous_registry) {
      std::fprintf(stderr,
                   "AC6_SERVICEREG tick=%llu g610=0x%08X [g610]=0x%08X "
                   "g6E4=0x%08X [g6E4]=0x%08X disp=0x%08X\n",
                   static_cast<unsigned long long>(tick), a, a0, b,
                   b0, b24);
      previous_registry = key;
    }
  }
}

// Is the front buffer black, or a static image that never updates? Every
// "black screen" statement in this campaign traces back to submissions=2 and
// never to output. VdSwap reports frontbuffer_address 0x137A0000 at 1280x720.
inline void trace_frontbuffer(ac6demo::GuestMemory &memory, std::uint64_t tick) {
  constexpr std::uint32_t kFrontBuffer = 0x137A0000U;
  constexpr std::uint32_t kBytes = 1280U * 720U * 4U;
  if ((tick % 1000U) != 0U || !memory.mapped(kFrontBuffer, kBytes)) {
    return;
  }
  std::uint64_t hash = 0xCBF29CE484222325ULL;
  std::uint64_t nonzero = 0U;
  for (std::uint32_t offset = 0U; offset < kBytes; offset += 4U) {
    const auto word = memory.load_u32(kFrontBuffer + offset);
    if (word != 0U) {
      ++nonzero;
    }
    hash = (hash ^ word) * 0x100000001B3ULL;
  }
  std::fprintf(stderr,
               "AC6_FRONTBUFFER tick=%llu nonzero=%llu of %u fnv=0x%016llX\n",
               static_cast<unsigned long long>(tick),
               static_cast<unsigned long long>(nonzero), kBytes / 4U,
               static_cast<unsigned long long>(hash));
}

// The ring publisher sub_821B9BC8 gates on bit 1 of device byte 10941, and the
// device is whatever the guest stored through the VdGlobalDevice import slot.
inline void trace_device_flags(ac6demo::GuestMemory &memory,
                               std::uint64_t tick) {
  static std::uint32_t previous = 0xFFFFFFFFU;
  if (!memory.mapped(0x82000608U, 4U)) {
    return;
  }
  const auto slot = memory.load_u32(0x82000608U);
  if (slot == 0U || !memory.mapped(slot, 4U)) {
    return;
  }
  const auto device = memory.load_u32(slot);
  if (device == 0U || !memory.mapped(device, 22300U)) {
    return;
  }
  const auto flags = memory.load_u8(device + 10941U);
  const std::uint32_t key = (device ^ (static_cast<std::uint32_t>(flags) << 24));
  if (key == previous && (tick % 2000U) != 0U) {
    return;
  }
  previous = key;
  std::fprintf(stderr,
               "AC6_DEVFLAGS tick=%llu slot=0x%08X device=0x%08X "
               "byte10941=0x%02X bit1=%d f21508=0x%08X f5460=0x%08X\n",
               static_cast<unsigned long long>(tick), slot, device, flags,
               (flags & 0x02U) ? 1 : 0, memory.load_u32(device + 21508U),
               memory.load_u32(device + 21600U));
  // sub_821AD378, the only reached writer of device+21508, switches on
  // [0x827AD2F0] - 11 and writes nothing unless that lands in 0..8.
  if (memory.mapped(0x827AD2F0U, 4U)) {
    const auto mode = memory.load_u32(0x827AD2F0U);
    std::fprintf(stderr, "AC6_DISPLAYMODE tick=%llu [0x827AD2F0]=%u case=%d\n",
                 static_cast<unsigned long long>(tick), mode,
                 static_cast<int>(mode) - 11);
    // [device+22264] bit 2 gates the only path from the per-frame present into
    // sub_821BB078, whose cone reaches sub_821ACCD0, one of the two writers.
    std::fprintf(stderr, "AC6_DEVGATE tick=%llu f22264=0x%08X bit2=%d\n",
                 static_cast<unsigned long long>(tick),
                 memory.load_u32(device + 22264U),
                 (memory.load_u32(device + 22264U) & 0x4U) ? 1 : 0);
  }
}

inline void trace_message_listeners(ac6demo::GuestMemory &memory,
                                    std::uint64_t tick) {
  // SendMsgI broadcasts to the listener array at 0x826DF800 and calls
  // slot +0x20 on each. At the press both callees were 0x820AC748,
  // the shared no-op, so this dumps who is actually registered.
  {
    static std::uint64_t previous_listeners = 0xFFFFFFFFFFFFFFFFULL;
    if (memory.mapped(0x826DF800U, 64U)) {
      std::uint64_t key = 0U;
      char line[512];
      int used = std::snprintf(line, sizeof(line),
                               "AC6_MSGLISTENERS tick=%llu",
                               static_cast<unsigned long long>(tick));
      for (std::uint32_t i = 0U; i < 12U; ++i) {
        const auto entry = memory.load_u32(0x826DF800U + i * 4U);
        if (entry == 0U) {
          break;
        }
        const auto vptr =
            memory.mapped(entry, 4U) ? memory.load_u32(entry) : 0U;
        key = key * 31U + vptr;
        used += std::snprintf(line + used, sizeof(line) - used,
                              " [%u]=0x%08X vptr=0x%08X", i, entry,
                              vptr);
      }
      if (key != previous_listeners) {
        std::fprintf(stderr, "%s\n", line);
        previous_listeners = key;
      }
    }
  }
}

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
        // The running mode's own state word, at +12 like the startup mode's.
        // Whether the title's state moves when START is pressed is what
        // separates "the input is consumed" from "the input has an effect".
        {
          static std::uint32_t previous_mode_state = 0xFFFFFFFFU;
          const auto mode = memory.load_u32(manager + 8U);
          if (mode != 0U && memory.mapped(mode, 16U)) {
            const auto mode_state = memory.load_u32(mode + 12U);
            trace_service_registry(memory, tick);
            trace_message_listeners(memory, tick);
            trace_device_flags(memory, tick);
            trace_frontbuffer(memory, tick);
            // GetCurrentMode / GetCurrentMission / GetCurrentLevel -- the three
            // script commands the movie issues after START -- all read the
            // singleton at [0x823C27E0]: mode is [gs+120], the other two derive
            // from the sub-object at gs+112.
            {
              static std::uint64_t previous_state = 0xFFFFFFFFFFFFFFFFULL;
              if (memory.mapped(0x823C27E0U, 4U)) {
                const auto gs = memory.load_u32(0x823C27E0U);
                if (gs != 0U && memory.mapped(gs, 128U)) {
                  const auto mode = memory.load_u32(gs + 120U);
                  const auto sub0 = memory.load_u32(gs + 112U);
                  const auto sub4 = memory.load_u32(gs + 116U);
                  // sub_820E9300 (GetCurrentMission's validity gate) requires
                  // [sub0+8] == 1 as its first check, forcing mission=16
                  // otherwise. Reading it here answers whether the script's
                  // decline after START correlates with this gate.
                  const auto sub0_field8 =
                      (sub0 != 0U && memory.mapped(sub0, 12U))
                          ? memory.load_u32(sub0 + 8U) : 0xFFFFFFFFU;
                  const std::uint64_t key =
                      (static_cast<std::uint64_t>(mode) << 32) ^ sub0 ^
                      (static_cast<std::uint64_t>(sub4) << 8) ^ sub0_field8;
                  if (key != previous_state) {
                    std::fprintf(stderr,
                                 "AC6_GAMESTATE tick=%llu gs=0x%08X mode=%d "
                                 "sub112=0x%08X sub116=0x%08X sub112f8=0x%08X\n",
                                 static_cast<unsigned long long>(tick), gs,
                                 static_cast<int>(mode), sub0, sub4,
                                 sub0_field8);
                    previous_state = key;
                  }
                }
              }
            }
            // The title's state-1 arm calls slot +0x20 on the sub-object at
            // mode+28, whose first act is to return early when [that+4] is
            // zero. That word is the difference between a poll that works and
            // one that does nothing 5,294 times.
            {
              static std::uint32_t previous_swg = 0xFFFFFFFFU;
              const auto swg_vptr = memory.load_u32(mode + 28U);
              if (swg_vptr != 0U && memory.mapped(mode + 28U, 32U)) {
                const auto field4 = memory.load_u32(mode + 28U + 4U);
                // 0x820CE368 gates on [world+236] and [world+224], skips its
                // frame step unless byte [+236]+9 is set, and steps the frame
                // counter at [+224]+4132 modulo [+224]+4136.
                if (field4 != 0U && memory.mapped(field4, 256U)) {
                  const auto w236 = memory.load_u32(field4 + 236U);
                  const auto w224 = memory.load_u32(field4 + 224U);
                  const auto flag9 =
                      (w236 != 0U && memory.mapped(w236, 16U))
                          ? memory.load_u8(w236 + 9U) : 0xFFU;
                  const auto frame =
                      (w224 != 0U && memory.mapped(w224, 4200U))
                          ? memory.load_u32(w224 + 4132U) : 0xFFFFFFFFU;
                  const auto total =
                      (w224 != 0U && memory.mapped(w224, 4200U))
                          ? memory.load_u32(w224 + 4136U) : 0xFFFFFFFFU;
                  const auto cb8 = (w236 != 0U && memory.mapped(w236, 16U))
                                       ? memory.load_u8(w236 + 8U) : 0xFFU;
                  const auto g48 = memory.mapped(0x826DFC48U, 1U)
                                       ? memory.load_u8(0x826DFC48U) : 0xFFU;
                  const std::uint64_t key =
                      (static_cast<std::uint64_t>(w236) << 32) ^ w224 ^
                      (static_cast<std::uint64_t>(flag9) << 16) ^
                      (static_cast<std::uint64_t>(cb8) << 24) ^
                      (static_cast<std::uint64_t>(g48) << 8);
                  static std::uint64_t previous_key = 0xFFFFFFFFFFFFFFFFULL;
                  if (key != previous_key || (tick % 500U) == 0U) {
                    std::fprintf(stderr,
                                 "AC6_SWGW tick=%llu world=0x%08X anim=0x%08X "
                                 "player=0x%08X step=0x%02X frame=%d of %d "
                                 "avptr=0x%08X pvptr=0x%08X p4124=%d p4128=%d "
                                 "p4140=%d cb8=0x%02X g48=0x%02X g44=0x%08X\n",
                                 static_cast<unsigned long long>(tick), field4,
                                 w236, w224, flag9, static_cast<int>(frame),
                                 static_cast<int>(total),
                                 w236 ? memory.load_u32(w236) : 0U,
                                 w224 ? memory.load_u32(w224) : 0U,
                                 w224 ? memory.load_u32(w224 + 4124U) : 0U,
                                 w224 ? memory.load_u32(w224 + 4128U) : 0U,
                                 w224 ? memory.load_u32(w224 + 4140U) : 0U,
                                 (w236 && memory.mapped(w236, 16U))
                                     ? memory.load_u8(w236 + 8U) : 0xFFU,
                                 memory.mapped(0x826DFC48U, 1U)
                                     ? memory.load_u8(0x826DFC48U) : 0xFFU,
                                 memory.mapped(0x826DFC44U, 4U)
                                     ? memory.load_u32(0x826DFC44U) : 0U);
                    previous_key = key;
                  }
                }
                if (field4 != previous_swg) {
                  std::fprintf(stderr,
                               "AC6_SWG tick=%llu sub=0x%08X vptr=0x%08X "
                               "field4=0x%08X field24=0x%08X\n",
                               static_cast<unsigned long long>(tick),
                               mode + 28U, swg_vptr, field4,
                               memory.load_u32(mode + 28U + 24U));
                  previous_swg = field4;
                }
              }
            }
            if (mode_state != previous_mode_state) {
              std::fprintf(stderr,
                           "AC6_MODE_INNER tick=%llu mode=0x%08X state=0x%08X\n",
                           static_cast<unsigned long long>(tick), mode,
                           mode_state);
              previous_mode_state = mode_state;
            }
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
