#include "ac6demo/input.hpp"

#include "ac6demo/content.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>

namespace ac6demo {

namespace {

constexpr std::uint32_t kErrorSuccess = 0U;
constexpr std::uint32_t kErrorDeviceNotConnected = 1167U;
constexpr std::uint32_t kErrorEmpty = 4306U;
constexpr std::uint32_t kControllerVtable = 0x8202A8DCU;
constexpr std::uint32_t kPlayerZeroNormalizedButtons = 0x827B37E0U;
constexpr std::uint32_t kTitleLogicalCurrent = 0x82798480U;
constexpr std::uint32_t kTitleLogicalPrevious = 0x82798484U;
constexpr std::uint32_t kTitleLogicalPressed = 0x82798488U;
constexpr std::uint32_t kGamepadFlag = 0x00000001U;
constexpr std::uint32_t kAnyUserFlag = 0x40000000U;
constexpr std::uint32_t kUserIndexAny = 0xFFU;
constexpr std::uint16_t kGamepadButtons = 0xF3FFU;

[[nodiscard]] bool parse_unsigned(std::string_view value, std::uint64_t maximum,
                                  std::uint64_t *output) noexcept {
  if (value.empty() || output == nullptr) {
    return false;
  }
  std::uint64_t parsed{};
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
      parsed > maximum) {
    return false;
  }
  *output = parsed;
  return true;
}

[[nodiscard]] bool parse_signed(std::string_view value, std::int64_t minimum,
                                std::int64_t maximum,
                                std::int64_t *output) noexcept {
  if (value.empty() || output == nullptr) {
    return false;
  }
  std::int64_t parsed{};
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
      parsed < minimum || parsed > maximum) {
    return false;
  }
  *output = parsed;
  return true;
}

} // namespace

ac6xbox360::XamInputMovieIdentity qualified_xam_input_movie_identity() {
  return ac6xbox360::XamInputMovieIdentity{"ac6-demo-xbox360-pal",
                                           "Default.xex",
                                           std::string(kQualifiedXexSha256),
                                           0x82000000U,
                                           "ghidra-projects/ace-combat-6-demo",
                                           "PowerPC:BE:64:Xenon"};
}

bool parse_scheduled_input(std::string_view value,
                           ScheduledInputFrame *output) noexcept {
  if (output == nullptr) {
    return false;
  }
  std::array<std::string_view, 9> fields{};
  for (std::size_t field_index = 0U; field_index + 1U < fields.size();
       ++field_index) {
    const auto separator = value.find(',');
    if (separator == std::string_view::npos) {
      return false;
    }
    fields[field_index] = value.substr(0U, separator);
    value.remove_prefix(separator + 1U);
  }
  fields.back() = value;
  if (fields.back().find(',') != std::string_view::npos) {
    return false;
  }

  std::uint64_t tick{};
  std::uint64_t buttons{};
  std::uint64_t left_trigger{};
  std::uint64_t right_trigger{};
  std::int64_t left_x{};
  std::int64_t left_y{};
  std::int64_t right_x{};
  std::int64_t right_y{};
  std::uint64_t connected{};
  constexpr auto kAxisMinimum = std::numeric_limits<std::int16_t>::min();
  constexpr auto kAxisMaximum = std::numeric_limits<std::int16_t>::max();
  if (!parse_unsigned(fields[0], std::numeric_limits<std::uint64_t>::max(),
                      &tick) ||
      !parse_unsigned(fields[1], std::numeric_limits<std::uint16_t>::max(),
                      &buttons) ||
      !parse_unsigned(fields[2], std::numeric_limits<std::uint8_t>::max(),
                      &left_trigger) ||
      !parse_unsigned(fields[3], std::numeric_limits<std::uint8_t>::max(),
                      &right_trigger) ||
      !parse_signed(fields[4], kAxisMinimum, kAxisMaximum, &left_x) ||
      !parse_signed(fields[5], kAxisMinimum, kAxisMaximum, &left_y) ||
      !parse_signed(fields[6], kAxisMinimum, kAxisMaximum, &right_x) ||
      !parse_signed(fields[7], kAxisMinimum, kAxisMaximum, &right_y) ||
      !parse_unsigned(fields[8], 1U, &connected)) {
    return false;
  }
  output->tick = tick;
  output->frame = InputFrame{static_cast<std::uint16_t>(buttons),
                             static_cast<std::uint8_t>(left_trigger),
                             static_cast<std::uint8_t>(right_trigger),
                             static_cast<std::int16_t>(left_x),
                             static_cast<std::int16_t>(left_y),
                             static_cast<std::int16_t>(right_x),
                             static_cast<std::int16_t>(right_y),
                             connected != 0U};
  return true;
}

void XamInputDevice::set_frame(InputFrame frame) noexcept {
  if (frame != frame_) {
    ++packet_number_;
    frame_ = frame;
  }
}

bool XamInputDevice::begin_xam_movie_record(std::string &error) {
  return movie_.begin_record(qualified_xam_input_movie_identity(), error);
}

bool XamInputDevice::begin_xam_movie_replay(std::string_view movie,
                                            std::string &error) {
  return movie_.begin_replay(movie, qualified_xam_input_movie_identity(),
                             error);
}

bool XamInputDevice::finalize_xam_movie(std::string &sealed_movie,
                                        std::string &error) {
  return movie_.finalize(sealed_movie, error);
}

void XamInputDevice::require_gamepad_query(std::uint32_t user_index,
                                           std::uint32_t flags) const {
  if (user_index > 3U || (flags != 0U && flags != kGamepadFlag)) {
    throw RuntimeTrap("unqualified XAM gamepad query", tick_, 0, user_index);
  }
}

std::uint32_t
XamInputDevice::connection_result(std::uint32_t user_index) const {
  return user_index == 0U && frame_.connected ? kErrorSuccess
                                              : kErrorDeviceNotConnected;
}

std::uint32_t XamInputDevice::get_state(std::uint32_t user_index,
                                        std::uint32_t flags,
                                        GuestMemory &memory,
                                        std::uint32_t output,
                                        XamInputCallsite callsite) {
  ++polls_.state;
  if (movie_.mode() == ac6xbox360::XamInputMovieMode::Replay) {
    ac6xbox360::XamInputReplayValue value;
    std::string error;
    if (!movie_.replay(ac6xbox360::XamInputReplayGuards{callsite.caller_lr,
                                                        user_index, flags,
                                                        output == 0U},
                       value, error)) {
      throw RuntimeTrap("XAM movie replay refused: " + error, tick_,
                        callsite.caller_lr, output);
    }
    if (value.has_state) {
      if (!memory.mapped(output, state_size)) {
        throw RuntimeTrap("XAM movie replay output is not mapped", tick_,
                          callsite.caller_lr, output);
      }
      memory.store_bytes(output, value.state);
    }
    return value.result;
  }

  require_gamepad_query(user_index, flags);
  if (!memory.mapped(output, state_size)) {
    throw RuntimeTrap("XamInputGetState output is not mapped", tick_, 0,
                      output);
  }
  const auto result = connection_result(user_index);
  if (result == kErrorSuccess) {
    memory.store_u32(output + 0U, packet_number_);
    memory.store_u16(output + 4U, static_cast<std::uint16_t>(frame_.buttons));
    memory.store_u8(output + 6U, frame_.left_trigger);
    memory.store_u8(output + 7U, frame_.right_trigger);
    memory.store_u16(output + 8U, static_cast<std::uint16_t>(frame_.left_x));
    memory.store_u16(output + 10U, static_cast<std::uint16_t>(frame_.left_y));
    memory.store_u16(output + 12U, static_cast<std::uint16_t>(frame_.right_x));
    memory.store_u16(output + 14U, static_cast<std::uint16_t>(frame_.right_y));
  }

  if (movie_.mode() == ac6xbox360::XamInputMovieMode::Record) {
    ac6xbox360::XamInputState state{};
    const auto bytes = memory.load_bytes(output, state_size);
    std::ranges::copy(bytes, state.begin());
    std::string error;
    if (!movie_.record(
            ac6xbox360::XamInputObservation{
                movie_.ordinal(), callsite.guest_tick, callsite.guest_thread,
                callsite.caller_lr, user_index, flags, false, result,
                result == kErrorSuccess,
                state},
            error)) {
      throw RuntimeTrap("XAM movie record refused: " + error, tick_,
                        callsite.caller_lr, output);
    }
  }
  return result;
}

std::uint32_t XamInputDevice::get_capabilities(std::uint32_t user_index,
                                               std::uint32_t flags,
                                               GuestMemory &memory,
                                               std::uint32_t output) {
  require_gamepad_query(user_index, flags);
  if (!memory.mapped(output, capabilities_size)) {
    throw RuntimeTrap("XamInputGetCapabilities output is not mapped", tick_, 0,
                      output);
  }
  ++polls_.capabilities;
  const auto result = connection_result(user_index);
  if (result != kErrorSuccess) {
    return result;
  }
  memory.store_u8(output + 0U, 1U);  // XINPUT_DEVTYPE_GAMEPAD
  memory.store_u8(output + 1U, 1U);  // XINPUT_DEVSUBTYPE_GAMEPAD
  memory.store_u16(output + 2U, 1U); // XINPUT_CAPS_FFB_SUPPORTED
  memory.store_u16(output + 4U, kGamepadButtons);
  memory.store_u8(output + 6U, 0xFFU);
  memory.store_u8(output + 7U, 0xFFU);
  for (std::uint32_t offset = 8U; offset < 16U; offset += 2U) {
    memory.store_u16(output + offset, 0xFFFFU);
  }
  memory.store_u16(output + 16U, 0xFFFFU);
  memory.store_u16(output + 18U, 0xFFFFU);
  return kErrorSuccess;
}

std::uint32_t XamInputDevice::set_vibration(std::uint32_t user_index,
                                            std::uint32_t flags,
                                            GuestMemory &memory,
                                            std::uint32_t vibration) {
  require_gamepad_query(user_index, flags);
  if (!memory.mapped(vibration, vibration_size)) {
    throw RuntimeTrap("XamInputSetState vibration is not mapped", tick_, 0,
                      vibration);
  }
  ++polls_.vibration;
  const auto result = connection_result(user_index);
  if (result != kErrorSuccess) {
    return result;
  }
  polls_.left_motor = memory.load_u16(vibration + 0U);
  polls_.right_motor = memory.load_u16(vibration + 2U);
  return kErrorSuccess;
}

std::uint32_t XamInputDevice::get_keystroke(std::uint32_t user_index,
                                            std::uint32_t flags,
                                            GuestMemory &memory,
                                            std::uint32_t output) {
  const auto allowed_flags = kGamepadFlag | kAnyUserFlag;
  if ((flags & ~allowed_flags) != 0U ||
      (user_index > 3U && user_index != kUserIndexAny)) {
    throw RuntimeTrap("unqualified XAM keystroke query", tick_, 0, user_index);
  }
  if (!memory.mapped(output, keystroke_size)) {
    throw RuntimeTrap("XamInputGetKeystrokeEx output is not mapped", tick_, 0,
                      output);
  }
  ++polls_.keystroke;
  const auto selected_user = user_index == kUserIndexAny ? 0U : user_index;
  const auto result = connection_result(selected_user);
  return result == kErrorSuccess ? kErrorEmpty : result;
}

void XamInputDevice::observe_controller_state_output(
    std::uint32_t caller_lr, std::uint32_t output) noexcept {
  // static_exact: 0x822F6160 passes controller+0x44 to XamInputGetState;
  // the call returns at 0x822F616C and 0x822F6008 then derives button edges.
  if (caller_lr == 0x822F616CU && output >= 0x44U) {
    observed_state_output_ = output;
  }
}

InputPollStats XamInputDevice::consume_poll_stats(GuestMemory &memory) {
  auto result = polls_;
  if (observed_state_output_ != 0U) {
    const auto object = observed_state_output_ - 0x44U;
    if (!memory.mapped(object, 0x78U)) {
      throw RuntimeTrap("observed controller object is not mapped", tick_,
                        0x822F616CU, object);
    }
    result.has_controller_snapshot = true;
    result.controller_object = object;
    result.controller_vtable = memory.load_u32(object);
    if (result.controller_vtable != kControllerVtable ||
        !memory.mapped(kPlayerZeroNormalizedButtons, sizeof(std::uint32_t))) {
      throw RuntimeTrap(
          "observed controller route differs from qualified layout", tick_,
          0x822F616CU, result.controller_vtable);
    }
    result.pressed_buttons = memory.load_u32(object + 0x14U);
    result.released_buttons = memory.load_u32(object + 0x18U);
    result.current_buttons = memory.load_u32(object + 0x1CU);
    result.previous_buttons = memory.load_u32(object + 0x74U);
    // static_exact: 0x82198C44 copies the 0x40-byte controller state and
    // Function_82198BF0 normalizes player zero's buttons at 0x827B37E0.
    result.normalized_buttons = memory.load_u32(kPlayerZeroNormalizedButtons);
    if (!memory.mapped(kTitleLogicalCurrent, 12U)) {
      throw RuntimeTrap("title logical input bitset is not mapped", tick_,
                        0x821DE990U, kTitleLogicalCurrent);
    }
    // static_exact: 0x821DE990 maps the normalized records into action
    // bitsets; 0x821DE6E0 derives pressed = current & ~previous.
    result.has_logical_snapshot = true;
    result.logical_current = memory.load_u32(kTitleLogicalCurrent);
    result.logical_previous = memory.load_u32(kTitleLogicalPrevious);
    result.logical_pressed = memory.load_u32(kTitleLogicalPressed);
  }
  polls_.state = 0U;
  polls_.capabilities = 0U;
  polls_.vibration = 0U;
  polls_.keystroke = 0U;
  observed_state_output_ = 0U;
  return result;
}

} // namespace ac6demo
