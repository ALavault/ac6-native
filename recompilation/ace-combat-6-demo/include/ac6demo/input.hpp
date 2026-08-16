#pragma once

#include "ac6demo/guest_memory.hpp"
#include "ac6demo/runtime_error.hpp"
#include "ac6xbox360/xam_input_movie.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace ac6demo {

struct InputFrame final {
  std::uint16_t buttons{};
  std::uint8_t left_trigger{};
  std::uint8_t right_trigger{};
  std::int16_t left_x{};
  std::int16_t left_y{};
  std::int16_t right_x{};
  std::int16_t right_y{};
  bool connected{true};

  bool operator==(const InputFrame &) const = default;
};

struct ScheduledInputFrame final {
  std::uint64_t tick{};
  InputFrame frame{};
};

struct XamInputCallsite final {
  std::uint64_t guest_tick{};
  std::uint32_t guest_thread{};
  std::uint32_t caller_lr{};
};

[[nodiscard]] ac6xbox360::XamInputMovieIdentity
qualified_xam_input_movie_identity();

[[nodiscard]] bool parse_scheduled_input(std::string_view value,
                                         ScheduledInputFrame *output) noexcept;

struct InputPollStats final {
  std::uint32_t state{};
  std::uint32_t capabilities{};
  std::uint32_t vibration{};
  std::uint32_t keystroke{};
  std::uint16_t left_motor{};
  std::uint16_t right_motor{};
  bool has_controller_snapshot{};
  std::uint32_t controller_object{};
  std::uint32_t controller_vtable{};
  std::uint32_t current_buttons{};
  std::uint32_t pressed_buttons{};
  std::uint32_t released_buttons{};
  std::uint32_t previous_buttons{};
  std::uint32_t normalized_buttons{};
  bool has_logical_snapshot{};
  std::uint32_t logical_current{};
  std::uint32_t logical_previous{};
  std::uint32_t logical_pressed{};
};

class XamInputDevice final {
public:
  static constexpr std::uint32_t state_size = 0x10U;
  static constexpr std::uint32_t capabilities_size = 0x14U;
  static constexpr std::uint32_t vibration_size = 0x04U;
  static constexpr std::uint32_t keystroke_size = 0x08U;

  void set_frame(InputFrame frame) noexcept;
  void set_tick(std::uint64_t tick) noexcept { tick_ = tick; }

  [[nodiscard]] bool begin_xam_movie_record(std::string &error);
  [[nodiscard]] bool begin_xam_movie_replay(std::string_view movie,
                                            std::string &error);
  [[nodiscard]] bool finalize_xam_movie(std::string &sealed_movie,
                                        std::string &error);
  [[nodiscard]] ac6xbox360::XamInputMovieMode xam_movie_mode() const noexcept {
    return movie_.mode();
  }

  [[nodiscard]] std::uint32_t
  get_state(std::uint32_t user_index, std::uint32_t flags, GuestMemory &memory,
            std::uint32_t output, XamInputCallsite callsite = {});
  [[nodiscard]] std::uint32_t get_capabilities(std::uint32_t user_index,
                                               std::uint32_t flags,
                                               GuestMemory &memory,
                                               std::uint32_t output);
  [[nodiscard]] std::uint32_t set_vibration(std::uint32_t user_index,
                                            std::uint32_t flags,
                                            GuestMemory &memory,
                                            std::uint32_t vibration);
  [[nodiscard]] std::uint32_t get_keystroke(std::uint32_t user_index,
                                            std::uint32_t flags,
                                            GuestMemory &memory,
                                            std::uint32_t output);

  void observe_controller_state_output(std::uint32_t caller_lr,
                                       std::uint32_t output) noexcept;
  [[nodiscard]] InputPollStats consume_poll_stats(GuestMemory &memory);
  [[nodiscard]] std::uint32_t packet_number() const noexcept {
    return packet_number_;
  }

private:
  void require_gamepad_query(std::uint32_t user_index,
                             std::uint32_t flags) const;
  [[nodiscard]] std::uint32_t connection_result(std::uint32_t user_index) const;

  InputFrame frame_{};
  InputPollStats polls_{};
  std::uint32_t packet_number_{};
  std::uint32_t observed_state_output_{};
  std::uint64_t tick_{};
  ac6xbox360::XamInputMovie movie_;
};

} // namespace ac6demo
