#include "ac6/sdl_input.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

namespace ac6 {

bool SdlInputProfile::load_manifest(const std::filesystem::path& path,
                                    SdlAxisMapping& axes,
                                    SdlKeyboardMapping& keyboard) noexcept {
  std::ifstream input(path);
  if (!input) return false;
  SdlAxisMapping parsed_axes{};
  SdlKeyboardMapping parsed_keyboard{};
  std::uint16_t seen = 0;
  std::string line;
  auto parse_uint = [](std::string_view value, std::uint32_t& out) {
    if (value.empty()) return false;
    const char* first = value.data();
    const char* last = first + value.size();
    auto result = std::from_chars(first, last, out, 10);
    return result.ec == std::errc{} && result.ptr == last;
  };
  while (std::getline(input, line)) {
    std::string_view row(line);
    if (!row.empty() && row.back() == '\r') row.remove_suffix(1);
    const auto first = row.find_first_not_of(" \t");
    if (first == std::string_view::npos || row[first] == '#') continue;
    row.remove_prefix(first);
    const auto tab = row.find('\t');
    if (tab == std::string_view::npos || row.find('\t', tab + 1) != std::string_view::npos) return false;
    const std::string_view key = row.substr(0, tab);
    const std::string_view value = row.substr(tab + 1);
    std::uint32_t number = 0;
    if (!parse_uint(value, number)) return false;
    std::uint16_t bit = 0;
    if (key == "pitch_axis") { bit = 1u << 0; parsed_axes.pitch_axis = static_cast<std::uint8_t>(number); }
    else if (key == "roll_axis") { bit = 1u << 1; parsed_axes.roll_axis = static_cast<std::uint8_t>(number); }
    else if (key == "yaw_axis") { bit = 1u << 2; parsed_axes.yaw_axis = static_cast<std::uint8_t>(number); }
    else if (key == "throttle_axis") { bit = 1u << 3; parsed_axes.throttle_axis = static_cast<std::uint8_t>(number); }
    else if (key == "invert_pitch") { bit = 1u << 4; if (number > 1) return false; parsed_axes.invert_pitch = number != 0; }
    else if (key == "invert_roll") { bit = 1u << 5; if (number > 1) return false; parsed_axes.invert_roll = number != 0; }
    else if (key == "invert_yaw") { bit = 1u << 6; if (number > 1) return false; parsed_axes.invert_yaw = number != 0; }
    else if (key == "pitch_up") { bit = 1u << 7; parsed_keyboard.pitch_up = static_cast<SDL_Scancode>(number); }
    else if (key == "pitch_down") { bit = 1u << 8; parsed_keyboard.pitch_down = static_cast<SDL_Scancode>(number); }
    else if (key == "roll_left") { bit = 1u << 9; parsed_keyboard.roll_left = static_cast<SDL_Scancode>(number); }
    else if (key == "roll_right") { bit = 1u << 10; parsed_keyboard.roll_right = static_cast<SDL_Scancode>(number); }
    else if (key == "yaw_left") { bit = 1u << 11; parsed_keyboard.yaw_left = static_cast<SDL_Scancode>(number); }
    else if (key == "yaw_right") { bit = 1u << 12; parsed_keyboard.yaw_right = static_cast<SDL_Scancode>(number); }
    else if (key == "throttle_up") { bit = 1u << 13; parsed_keyboard.throttle_up = static_cast<SDL_Scancode>(number); }
    else if (key == "throttle_down") { bit = 1u << 14; parsed_keyboard.throttle_down = static_cast<SDL_Scancode>(number); }
    else return false;
    if ((seen & bit) != 0) return false;
    seen = static_cast<std::uint16_t>(seen | bit);
  }
  if (seen != 0x7fffu || !parsed_axes.valid() || !parsed_keyboard.valid()) return false;
  axes = parsed_axes;
  keyboard = parsed_keyboard;
  return true;
}

bool SdlAxisMapping::valid() const noexcept {
  const auto valid_axis = [](std::uint8_t axis) {
    return axis < SDL_GAMEPAD_AXIS_COUNT;
  };
  return valid_axis(pitch_axis) && valid_axis(roll_axis) && valid_axis(yaw_axis) &&
         valid_axis(throttle_axis);
}

bool SdlKeyboardMapping::valid() const noexcept {
  const SDL_Scancode keys[] = {pitch_up, pitch_down, roll_left, roll_right,
                               yaw_left, yaw_right, throttle_up, throttle_down};
  for (const SDL_Scancode key : keys) {
    if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_SCANCODE_COUNT) return false;
  }
  for (std::size_t i = 0; i < std::size(keys); ++i) {
    for (std::size_t j = i + 1; j < std::size(keys); ++j) {
      if (keys[i] == keys[j]) return false;
    }
  }
  return true;
}

bool SdlInputAdapter::apply(const SDL_Event& event, InputFrame& frame,
                            std::uint16_t& buttons,
                            const InputMappingDatabase& mappings, EntityId subject,
                            std::vector<Event>& events) const noexcept {
  if (!mapping_.valid() || !keyboard_.valid()) return false;
  if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
    const auto axis_value = [](Sint16 value, bool invert) -> std::int16_t {
      const std::int32_t clamped = std::clamp<std::int32_t>(value, -32768, 32767);
      const std::int32_t adjusted = invert ? -clamped : clamped;
      return static_cast<std::int16_t>(std::clamp(adjusted, -32768, 32767));
    };
    if (event.gaxis.axis == mapping_.pitch_axis) frame.pitch = axis_value(event.gaxis.value, mapping_.invert_pitch);
    else if (event.gaxis.axis == mapping_.roll_axis) frame.roll = axis_value(event.gaxis.value, mapping_.invert_roll);
    else if (event.gaxis.axis == mapping_.yaw_axis) frame.yaw = axis_value(event.gaxis.value, mapping_.invert_yaw);
    else if (event.gaxis.axis == mapping_.throttle_axis) {
      const std::int32_t value = std::clamp<std::int32_t>(event.gaxis.value, -32768, 32767);
      frame.throttle = static_cast<std::uint8_t>((value + 32768) * 255 / 65535);
    } else return false;
    return true;
  }
  if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
    const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
    const SDL_Scancode key = event.key.scancode;
    std::size_t key_index = 8;
    if (key == keyboard_.pitch_up) key_index = 0;
    else if (key == keyboard_.pitch_down) key_index = 1;
    else if (key == keyboard_.roll_left) key_index = 2;
    else if (key == keyboard_.roll_right) key_index = 3;
    else if (key == keyboard_.yaw_left) key_index = 4;
    else if (key == keyboard_.yaw_right) key_index = 5;
    else if (key == keyboard_.throttle_up) key_index = 6;
    else if (key == keyboard_.throttle_down) key_index = 7;
    else return false;
    held_keys_[key_index] = pressed;
    const auto signed_axis = [](bool positive, bool negative, std::int16_t positive_value,
                                std::int16_t negative_value) {
      if (positive == negative) return static_cast<std::int16_t>(0);
      return positive ? positive_value : negative_value;
    };
    frame.pitch = signed_axis(held_keys_[0], held_keys_[1], 32767, -32768);
    frame.roll = signed_axis(held_keys_[3], held_keys_[2], 32767, -32768);
    frame.yaw = signed_axis(held_keys_[5], held_keys_[4], 32767, -32768);
    if (held_keys_[6]) frame.throttle = 255u;
    else if (held_keys_[7]) frame.throttle = 0u;
    else frame.throttle = 0u;
    return true;
  }
  if (event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
      event.type != SDL_EVENT_GAMEPAD_BUTTON_UP) return false;
  if (event.gbutton.button >= 16) return false;
  const std::uint16_t mask = static_cast<std::uint16_t>(1u << event.gbutton.button);
  if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
    buttons = static_cast<std::uint16_t>(buttons | mask);
    const InputBinding* binding = mappings.resolve(buttons);
    if (binding != nullptr) events.push_back({binding->event, subject});
  } else {
    buttons = static_cast<std::uint16_t>(buttons & ~mask);
  }
  return true;
}

void SdlInputAdapter::reset(InputFrame& frame, std::uint16_t& buttons) const noexcept {
  held_keys_.fill(false);
  frame = {};
  buttons = 0;
}

void SdlEventPump::close_gamepads() noexcept {
  for (SDL_Gamepad* gamepad : gamepads_) {
    if (gamepad != nullptr) SDL_CloseGamepad(gamepad);
  }
  gamepads_.clear();
}

SdlEventPump::~SdlEventPump() { shutdown(); }

bool SdlEventPump::initialize() noexcept {
  if (initialized_) return true;
  if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) return false;
  // A headless run may have no gamepad backend; event polling remains valid.
  (void)SDL_InitSubSystem(SDL_INIT_GAMEPAD);
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  if (ids != nullptr) {
    for (int i = 0; i < count; ++i) {
      if (SDL_Gamepad* gamepad = SDL_OpenGamepad(ids[i]); gamepad != nullptr) {
        gamepads_.push_back(gamepad);
      }
    }
    SDL_free(ids);
  }
  initialized_ = true;
  return true;
}

void SdlEventPump::shutdown() noexcept {
  if (!initialized_) return;
  close_gamepads();
  SDL_QuitSubSystem(SDL_INIT_GAMEPAD | SDL_INIT_EVENTS);
  initialized_ = false;
}

SdlAudioDevice::~SdlAudioDevice() { shutdown(); }

bool SdlAudioDevice::initialize(const SDL_AudioSpec& spec) noexcept {
  if (stream_ != nullptr || spec.format == SDL_AUDIO_UNKNOWN || spec.channels <= 0 ||
      spec.freq <= 0) return false;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
  owns_subsystem_ = true;
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (stream_ == nullptr || !SDL_ResumeAudioStreamDevice(stream_)) {
    shutdown();
    return false;
  }
  return true;
}

bool SdlAudioDevice::queue(const void* samples, int bytes) noexcept {
  return stream_ != nullptr && samples != nullptr && bytes > 0 &&
         SDL_PutAudioStreamData(stream_, samples, bytes);
}

void SdlAudioDevice::shutdown() noexcept {
  if (stream_ != nullptr) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
  }
  if (owns_subsystem_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    owns_subsystem_ = false;
  }
}

bool SdlEventPump::pump(const SdlInputAdapter& adapter, InputFrame& frame,
                        std::uint16_t& buttons,
                        const InputMappingDatabase& mappings, EntityId subject,
                        std::vector<Event>& events, bool& quit) noexcept {
  if (!initialized_) return false;
  bool handled = false;
  SDL_Event event{};
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      quit = true;
      handled = true;
      continue;
    }
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
      if (SDL_Gamepad* gamepad = SDL_OpenGamepad(event.gdevice.which); gamepad != nullptr) {
        gamepads_.push_back(gamepad);
      }
      handled = true;
      continue;
    }
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
      for (auto it = gamepads_.begin(); it != gamepads_.end(); ++it) {
        if (*it != nullptr && SDL_GetGamepadID(*it) == event.gdevice.which) {
          SDL_CloseGamepad(*it);
          gamepads_.erase(it);
          break;
        }
      }
      adapter.reset(frame, buttons);
      handled = true;
      continue;
    }
    handled = adapter.apply(event, frame, buttons, mappings, subject, events) || handled;
  }
  return handled;
}

SdlWindow::~SdlWindow() { destroy(); }

bool SdlWindow::create(const char* title, std::uint32_t width, std::uint32_t height,
                       bool vulkan, bool hidden) noexcept {
  if (window_ != nullptr || title == nullptr || width == 0 || height == 0) return false;
  SDL_WindowFlags flags = vulkan ? SDL_WINDOW_VULKAN : 0u;
  if (hidden) flags |= SDL_WINDOW_HIDDEN;
  window_ = SDL_CreateWindow(title, static_cast<int>(width), static_cast<int>(height), flags);
  return window_ != nullptr;
}

bool SdlWindow::show() noexcept {
  return window_ != nullptr && SDL_ShowWindow(window_);
}

void SdlWindow::destroy() noexcept {
  if (window_ == nullptr) return;
  SDL_DestroyWindow(window_);
  window_ = nullptr;
}

}  // namespace ac6
