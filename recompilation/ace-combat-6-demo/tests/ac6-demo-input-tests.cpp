#include "ac6demo/input.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

int main() {
  ac6demo::ScheduledInputFrame scheduled;
  assert(ac6demo::parse_scheduled_input("252,4096,7,9,-1000,2000,-3000,4000,1",
                                        &scheduled));
  assert(scheduled.tick == 252U);
  assert(scheduled.frame.buttons == 4096U);
  assert(scheduled.frame.left_trigger == 7U);
  assert(scheduled.frame.left_x == -1000);
  assert(scheduled.frame.right_y == 4000);
  assert(scheduled.frame.connected);
  assert(
      !ac6demo::parse_scheduled_input("252,65536,0,0,0,0,0,0,1", &scheduled));
  assert(!ac6demo::parse_scheduled_input("252,4096,0,0,-32769,0,0,0,1",
                                         &scheduled));
  assert(!ac6demo::parse_scheduled_input("252,4096,0,0,0,0,0,0,2", &scheduled));
  assert(!ac6demo::parse_scheduled_input("252,4096", &scheduled));
  assert(
      !ac6demo::parse_scheduled_input("252,4096,0,0,0,0,0,0,1,99", &scheduled));

  ac6demo::GuestMemory memory;
  memory.map_zero(0x1000U, 0x1000U);
  memory.map_zero(0x827B3000U, 0x1000U);
  memory.map_zero(0x82798000U, 0x1000U);

  ac6demo::XamInputDevice input;
  input.set_tick(17U);
  input.set_frame(
      ac6demo::InputFrame{0x9011U, 7U, 9U, -1000, 2000, -3000, 4000, true});
  assert(input.packet_number() == 1U);
  assert(input.get_state(0U, 0U, memory, 0x1000U) == 0U);
  assert(memory.load_u32(0x1000U) == 1U);
  assert(memory.load_u16(0x1004U) == 0x9011U);
  assert(memory.load_u8(0x1006U) == 7U);
  assert(memory.load_u8(0x1007U) == 9U);
  assert(static_cast<std::int16_t>(memory.load_u16(0x1008U)) == -1000);
  assert(static_cast<std::int16_t>(memory.load_u16(0x100AU)) == 2000);
  assert(static_cast<std::int16_t>(memory.load_u16(0x100CU)) == -3000);
  assert(static_cast<std::int16_t>(memory.load_u16(0x100EU)) == 4000);

  assert(input.get_capabilities(0U, 1U, memory, 0x1020U) == 0U);
  assert(memory.load_u8(0x1020U) == 1U);
  assert(memory.load_u8(0x1021U) == 1U);
  assert(memory.load_u16(0x1022U) == 1U);
  assert(memory.load_u16(0x1024U) == 0xF3FFU);
  assert(memory.load_u16(0x1030U) == 0xFFFFU);
  assert(memory.load_u16(0x1032U) == 0xFFFFU);

  memory.store_u16(0x1040U, 0x1234U);
  memory.store_u16(0x1042U, 0x5678U);
  assert(input.set_vibration(0U, 0U, memory, 0x1040U) == 0U);
  assert(input.get_keystroke(0U, 1U, memory, 0x1050U) == 4306U);
  assert(input.get_state(1U, 0U, memory, 0x1000U) == 1167U);

  const auto polls = input.consume_poll_stats(memory);
  assert(polls.state == 2U);
  assert(polls.capabilities == 1U);
  assert(polls.vibration == 1U);
  assert(polls.keystroke == 1U);
  assert(polls.left_motor == 0x1234U);
  assert(polls.right_motor == 0x5678U);
  assert(input.consume_poll_stats(memory).state == 0U);

  memory.store_u32(0x1100U, 0x8202A8DCU);
  memory.store_u32(0x1114U, 0x0010U);
  memory.store_u32(0x1118U, 0U);
  memory.store_u32(0x111CU, 0x0010U);
  memory.store_u32(0x1174U, 0x0010U);
  memory.store_u32(0x827B37E0U, 0x0400U);
  memory.store_u32(0x82798480U, 0x0010U);
  memory.store_u32(0x82798484U, 0U);
  memory.store_u32(0x82798488U, 0x0010U);
  input.observe_controller_state_output(0x822F616CU, 0x1144U);
  const auto snapshot = input.consume_poll_stats(memory);
  assert(snapshot.has_controller_snapshot);
  assert(snapshot.controller_object == 0x1100U);
  assert(snapshot.controller_vtable == 0x8202A8DCU);
  assert(snapshot.current_buttons == 0x0010U);
  assert(snapshot.pressed_buttons == 0x0010U);
  assert(snapshot.released_buttons == 0U);
  assert(snapshot.previous_buttons == 0x0010U);
  assert(snapshot.normalized_buttons == 0x0400U);
  assert(snapshot.has_logical_snapshot);
  assert(snapshot.logical_current == 0x0010U);
  assert(snapshot.logical_previous == 0U);
  assert(snapshot.logical_pressed == 0x0010U);
  input.observe_controller_state_output(0x822F60A8U, 0x1144U);
  assert(!input.consume_poll_stats(memory).has_controller_snapshot);

  bool rejected = false;
  try {
    (void)input.get_state(0U, 2U, memory, 0x1000U);
  } catch (const ac6demo::RuntimeTrap &trap) {
    rejected = trap.tick() == 17U;
  }
  assert(rejected);

  rejected = false;
  try {
    (void)input.get_capabilities(0U, 0U, memory, 0x3000U);
  } catch (const ac6demo::RuntimeTrap &trap) {
    rejected = trap.address() == 0x3000U;
  }
  assert(rejected);

  ac6demo::XamInputDevice recorder;
  recorder.set_tick(100U);
  recorder.set_frame(ac6demo::InputFrame{0x1000U, 3U, 4U, -5, 6, -7, 8, true});
  std::string movie_error;
  assert(recorder.begin_xam_movie_record(movie_error));
  assert(recorder.get_state(0U, 1U, memory, 0x1200U,
                            ac6demo::XamInputCallsite{100U, 7U, 0x822F616CU}) ==
         0U);
  assert(recorder.get_state(1U, 0U, memory, 0x1220U,
                            ac6demo::XamInputCallsite{101U, 8U, 0x822F60A8U}) ==
         1167U);
  std::string movie;
  assert(recorder.finalize_xam_movie(movie, movie_error));
  assert(movie.find("ac6.xam-input-movie.v1") != std::string::npos);
  assert(movie.find("ac6-demo-xbox360-pal") != std::string::npos);
  assert(movie.find("ghidra-projects/ace-combat-6-demo") !=
         std::string::npos);
  assert(movie.find("PowerPC:BE:64:Xenon") != std::string::npos);
  assert(movie.find("\"guest_tick\":100,\"guest_thread\":7") !=
         std::string::npos);
  assert(movie.find("\"state16\":null") == std::string::npos);

  ac6demo::XamInputDevice movie_replay;
  movie_replay.set_tick(900U);
  // This disconnected host frame would fail the same poll if HID were not
  // bypassed by the exact-call movie.
  movie_replay.set_frame(ac6demo::InputFrame{0U, 0U, 0U, 0, 0, 0, 0, false});
  assert(movie_replay.begin_xam_movie_replay(movie, movie_error));
  assert(movie_replay.get_state(
             0U, 1U, memory, 0x1240U,
             ac6demo::XamInputCallsite{900U, 99U, 0x822F616CU}) == 0U);
  assert(memory.load_u32(0x1240U) == 1U);
  assert(memory.load_u16(0x1244U) == 0x1000U);
  assert(memory.load_u8(0x1246U) == 3U);
  memory.store_u32(0x1260U, 0xA5A5A5A5U);
  assert(movie_replay.get_state(
             1U, 0U, memory, 0x1260U,
             ac6demo::XamInputCallsite{901U, 99U, 0x822F60A8U}) == 1167U);
  assert(memory.load_u32(0x1260U) == 0xA5A5A5A5U);
  std::string replay_output;
  assert(movie_replay.finalize_xam_movie(replay_output, movie_error));
  assert(replay_output.empty());

  std::cout << "ac6-demo-input-tests: ok\n";
  return 0;
}
