// The demo view. These tests do NOT verify retail -- nothing here is ported.
// They verify that the picture responds to the contracted attitude, that it
// carries its caption, and that it does not crash on hostile input.

#include "ac6/demo_flight_view.h"
#include "ac6/retail_flight_session.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int failures = 0;
void check(bool c, const char* w) { if (!c) { std::printf("FAIL  %s\n", w); ++failures; } }

using ac6::demo::caption;
using ac6::demo::DemoCamera;
using ac6::demo::draw_flight_view;
using ac6::demo::Image;
using namespace ac6::retail;

constexpr float kFrame = 0.016666668F;

Image canvas(int w = 320, int h = 180) {
  Image image; image.width = w; image.height = h;
  image.clear(0, 0, 0);
  return image;
}

FlightModelConfig config() {
  FlightModelConfig c{};
  c.limits = FlightRotationLimits{5.0F, 1.399999976158142F, 5.400000095367432F};
  c.rates304 = LiveAxisRates{4.0F, 2.0F};
  c.rates308 = LiveAxisRates{3.0F, 1.5F};
  c.rates312 = LiveAxisRates{5.0F, 2.5F};
  c.servo304 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
  c.servo308 = c.servo304; c.servo312 = c.servo304;
  c.rampRate952 = 3.0F; c.rampRate956 = 3.0F; c.rampThreshold404 = 0.5F;
  return c;
}

std::size_t nonbackground(const Image& image) {
  std::size_t count = 0;
  for (std::size_t i = 0; i + 2 < image.rgb.size(); i += 3) {
    if (!(image.rgb[i] == 24 && image.rgb[i + 1] == 32 && image.rgb[i + 2] == 56)) {
      ++count;
    }
  }
  return count;
}

void the_caption_says_all_three_things() {
  const std::string text = caption();
  check(text.find("micro-execution") != std::string::npos,
        "the caption says the attitude is measured");
  check(text.find("invented") != std::string::npos,
        "the caption says the camera and scene are invented");
  check(text.find("does not move") != std::string::npos,
        "the caption says the aircraft does not move");
}

void a_level_attitude_draws_something() {
  Image image = canvas();
  draw_flight_view(image, identity_basis(), DemoCamera{});
  check(nonbackground(image) > 100,
        "a level attitude draws a grid and a horizon");
}

void the_picture_follows_the_contracted_attitude() {
  FlightSessionState state{};
  Image level = canvas();
  draw_flight_view(level, state.basis, DemoCamera{});

  for (int i = 0; i < 90; ++i) {
    FlightStick s{};
    s.target14 = -0.8F; s.increment14 = 1.0F;   // roll
    step_flight_session(state, config(), s, kFrame);
  }
  Image rolled = canvas();
  draw_flight_view(rolled, state.basis, DemoCamera{});

  check(!(level.rgb == rolled.rgb),
        "ninety frames of stick input change the picture");
  check(nonbackground(rolled) > 100, "and it still draws something");
}

void a_command_the_model_discards_leaves_the_picture_alone() {
  // The one-degree tolerance of the command setters, seen from the far end: a
  // stick input retail throws away must not move a single pixel.
  FlightSessionState state{};
  Image before = canvas();
  draw_flight_view(before, state.basis, DemoCamera{});
  for (int i = 0; i < 90; ++i) {
    FlightStick s{};
    s.target12 = kOneDegree * 0.25F; s.increment12 = 1.0F;
    step_flight_session(state, config(), s, kFrame);
  }
  Image after = canvas();
  draw_flight_view(after, state.basis, DemoCamera{});
  check(before.rgb == after.rgb,
        "a quarter-degree command changes nothing, all the way to the pixels");
}

void hostile_input_does_not_crash_or_hang() {
  Image image = canvas(64, 48);
  RetailBasis broken{};
  for (auto& row : broken.rows) {
    for (float& value : row) { value = 0.0F; }
  }
  draw_flight_view(image, broken, DemoCamera{});
  DemoCamera narrow{};
  narrow.invented_fov_y = 0.0001F;
  draw_flight_view(image, identity_basis(), narrow);
  check(true, "a degenerate basis and a pinhole camera both return");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--emit-frames") {
    FlightSessionState state{};
    const std::string base = argv[2];
    // THIRTY SECONDS, not three. The base constructor's rate limits are 5.0,
    // 1.4 and 5.4 -- degrees per second once the chain has scaled them -- so a
    // three-second manoeuvre moves the attitude by about four degrees and the
    // picture barely changes. That is retail's number, read at cycle 1377, and
    // the honest response is a longer manoeuvre rather than a larger one.
    for (int frame = 0; frame < 1800; ++frame) {
      FlightStick s{};
      if (frame < 600) { s.target12 = 0.8F; s.increment12 = 1.0F; }
      else if (frame < 1200) { s.target14 = -0.6F; s.increment14 = 1.0F; }
      step_flight_session(state, config(), s, kFrame);
      if (frame % 200 == 0) {
        Image image = canvas(480, 270);
        draw_flight_view(image, state.basis, DemoCamera{});
        char path[512];
        std::snprintf(path, sizeof(path), "%s/frame-%03d.ppm", base.c_str(), frame);
        image.write_ppm(path);
      }
    }
    std::printf("%s\n", caption().c_str());
    return 0;
  }
  the_caption_says_all_three_things();
  a_level_attitude_draws_something();
  the_picture_follows_the_contracted_attitude();
  a_command_the_model_discards_leaves_the_picture_alone();
  hostile_input_does_not_crash_or_hang();
  if (failures != 0) {
    std::printf("demo_flight_view: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("demo_flight_view: all cases passed\n");
  return 0;
}
