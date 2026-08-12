#include "ac6/sdl_input.h"
#include "test_fixtures.h"

#include <array>
#include <cstdint>
#include <vector>

namespace {

struct ButtonCase {
  SDL_GamepadButton sdl_button;
  std::uint16_t xinput_mask;
};

constexpr std::array<ButtonCase, 14> kButtonCases{{
    {SDL_GAMEPAD_BUTTON_DPAD_UP, 0x0001u},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0x0002u},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0x0004u},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0x0008u},
    {SDL_GAMEPAD_BUTTON_START, 0x0010u},
    {SDL_GAMEPAD_BUTTON_BACK, 0x0020u},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, 0x0040u},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0x0080u},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0x0100u},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0x0200u},
    {SDL_GAMEPAD_BUTTON_SOUTH, 0x1000u},
    {SDL_GAMEPAD_BUTTON_EAST, 0x2000u},
    {SDL_GAMEPAD_BUTTON_WEST, 0x4000u},
    {SDL_GAMEPAD_BUTTON_NORTH, 0x8000u},
}};

SDL_Event button_event(const Uint32 type, const SDL_GamepadButton button) {
  SDL_Event event{};
  event.type = type;
  event.gbutton.button = static_cast<Uint8>(button);
  return event;
}

}  // namespace

int main() {
  constexpr ac6::EntityId kSubject = 0x1234u;
  const ac6::SdlInputAdapter adapter;

  for (const ButtonCase& button_case : kButtonCases) {
    REQUIRE(ac6::sdl_gamepad_button_xinput_mask(button_case.sdl_button) ==
            button_case.xinput_mask);

    ac6::InputFrame frame{};
    std::uint16_t buttons = 0;
    ac6::InputMappingDatabase mappings;
    REQUIRE(mappings.add({button_case.xinput_mask, ac6::EventType::Pause}));
    std::vector<ac6::Event> events;

    const SDL_Event down =
        button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, button_case.sdl_button);
    REQUIRE(adapter.apply(down, frame, buttons, mappings, kSubject, events));
    REQUIRE(buttons == button_case.xinput_mask);
    REQUIRE(frame.buttons == buttons);
    REQUIRE(events.size() == 1u);
    REQUIRE(events.front().type == ac6::EventType::Pause);
    REQUIRE(events.front().subject == kSubject);

    const SDL_Event up =
        button_event(SDL_EVENT_GAMEPAD_BUTTON_UP, button_case.sdl_button);
    REQUIRE(adapter.apply(up, frame, buttons, mappings, kSubject, events));
    REQUIRE(buttons == 0u);
    REQUIRE(frame.buttons == buttons);
    REQUIRE(events.size() == 1u);

    frame = {123, -456, 789, 42u, button_case.xinput_mask};
    buttons = button_case.xinput_mask;
    adapter.reset(frame, buttons);
    REQUIRE(frame == ac6::InputFrame{});
    REQUIRE(buttons == 0u);
  }

  ac6::InputFrame frame{123, -456, 789, 42u, 0x1000u};
  std::uint16_t buttons = frame.buttons;
  const ac6::InputMappingDatabase mappings;
  std::vector<ac6::Event> events;

  REQUIRE(ac6::sdl_gamepad_button_xinput_mask(SDL_GAMEPAD_BUTTON_GUIDE) == 0u);
  const SDL_Event unsupported =
      button_event(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_GUIDE);
  REQUIRE(!adapter.apply(unsupported, frame, buttons, mappings, kSubject, events));
  REQUIRE(buttons == 0x1000u);
  REQUIRE((frame == ac6::InputFrame{123, -456, 789, 42u, 0x1000u}));
  REQUIRE(events.empty());

  constexpr auto kUnknownButton = static_cast<SDL_GamepadButton>(0xff);
  REQUIRE(ac6::sdl_gamepad_button_xinput_mask(kUnknownButton) == 0u);
  const SDL_Event unknown =
      button_event(SDL_EVENT_GAMEPAD_BUTTON_UP, kUnknownButton);
  REQUIRE(!adapter.apply(unknown, frame, buttons, mappings, kSubject, events));
  REQUIRE(buttons == 0x1000u);
  REQUIRE((frame == ac6::InputFrame{123, -456, 789, 42u, 0x1000u}));
  REQUIRE(events.empty());
  return 0;
}
