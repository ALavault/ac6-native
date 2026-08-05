#include "ac6/product_runtime.h"
#include "ac6/native_hud.h"
#include "ac6/sdl_input.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace {

void require(bool condition, const char* expression, const char* file, int line) {
  if (!condition) {
    std::fprintf(stderr, "REQUIRE failed at %s:%d: %s\n", file, line, expression);
    std::abort();
  }
}

std::uint64_t fnv64(std::string_view bytes) {
  std::uint64_t hash = 1469598103934665603ull;
  for (const char byte : bytes) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

void write_le_u16_at(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset + 0] = static_cast<char>(value & 0xffu);
  bytes[offset + 1] = static_cast<char>((value >> 8u) & 0xffu);
}

void write_le_u32_at(std::string& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset + 0] = static_cast<char>(value & 0xffu);
  bytes[offset + 1] = static_cast<char>((value >> 8u) & 0xffu);
  bytes[offset + 2] = static_cast<char>((value >> 16u) & 0xffu);
  bytes[offset + 3] = static_cast<char>((value >> 24u) & 0xffu);
}

void write_le_f32_at(std::string& bytes, std::size_t offset, float value) {
  std::uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  write_le_u32_at(bytes, offset, raw);
}

void write_be_u16_at(std::string& bytes, std::size_t offset, std::uint16_t value) {
  bytes[offset + 0] = static_cast<char>((value >> 8u) & 0xffu);
  bytes[offset + 1] = static_cast<char>(value & 0xffu);
}

void write_be_u32_at(std::string& bytes, std::size_t offset, std::uint32_t value) {
  bytes[offset + 0] = static_cast<char>((value >> 24u) & 0xffu);
  bytes[offset + 1] = static_cast<char>((value >> 16u) & 0xffu);
  bytes[offset + 2] = static_cast<char>((value >> 8u) & 0xffu);
  bytes[offset + 3] = static_cast<char>(value & 0xffu);
}

void write_be_f32_at(std::string& bytes, std::size_t offset, float value) {
  std::uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  write_be_u32_at(bytes, offset, raw);
}

std::string make_ndxr_be_strip_fixture() {
  constexpr std::size_t object_table = 0x30;
  constexpr std::size_t polygon_descriptor = object_table + 0x30;
  constexpr std::size_t header_size = 0x60;
  constexpr std::size_t polygon_base = 0x30 + header_size;
  constexpr std::size_t polygon_size = 10;
  constexpr std::size_t vertex_base = polygon_base + polygon_size;
  constexpr std::size_t vertex_stride = 32;
  constexpr std::size_t vertex_size = 4 * vertex_stride;
  const std::size_t total_size = vertex_base + vertex_size;
  std::string bytes(total_size, '\0');
  bytes.replace(0, 4, "NDXR");
  write_be_u32_at(bytes, 4, static_cast<std::uint32_t>(total_size));
  write_be_u16_at(bytes, 0x0a, 1);
  write_be_u32_at(bytes, 0x10, static_cast<std::uint32_t>(header_size));
  write_be_u32_at(bytes, 0x14, static_cast<std::uint32_t>(polygon_size));
  write_be_u32_at(bytes, 0x18, static_cast<std::uint32_t>(vertex_size));
  write_be_u32_at(bytes, 0x1c, 0);
  write_be_u16_at(bytes, object_table + 0x2a, 1);
  write_be_u32_at(bytes, polygon_descriptor + 0, 0);
  write_be_u32_at(bytes, polygon_descriptor + 4, 0);
  write_be_u16_at(bytes, polygon_descriptor + 0x0c, 4);
  write_be_u16_at(bytes, polygon_descriptor + 0x0e, 0x0613);
  write_be_u16_at(bytes, polygon_descriptor + 0x20, 5);
  const std::uint16_t indices[] = {0, 1, 2, 3, 0xffffu};
  for (std::size_t i = 0; i < 5; ++i) write_be_u16_at(bytes, polygon_base + i * 2, indices[i]);
  const float vertices[][3] = {{-1, 0, 2}, {1, 0, 2.5f}, {-1, 1, 2.75f}, {1, 1, 3}};
  for (std::size_t i = 0; i < 4; ++i) {
    write_be_f32_at(bytes, vertex_base + i * vertex_stride + 0, vertices[i][0]);
    write_be_f32_at(bytes, vertex_base + i * vertex_stride + 4, vertices[i][1]);
    write_be_f32_at(bytes, vertex_base + i * vertex_stride + 8, vertices[i][2]);
    write_be_f32_at(bytes, vertex_base + i * vertex_stride + 20, vertices[i][0]);
    write_be_f32_at(bytes, vertex_base + i * vertex_stride + 24, vertices[i][1]);
  }
  return bytes;
}

std::string make_ndxr_fixture(std::uint32_t vertex_count, std::uint32_t index_count,
                              std::uint32_t primitive_count, std::uint32_t vertex_stride,
                              std::uint32_t index_size) {
  std::string bytes = "NDXR\t1\t" + std::to_string(vertex_count) + "\t" +
      std::to_string(index_count) + "\t" + std::to_string(primitive_count) +
      "\nVTX\t" + std::to_string(vertex_count) + "\t" + std::to_string(vertex_stride) +
      "\nIDX\t" + std::to_string(index_count) + "\t" + std::to_string(index_size) +
      "\nPOLY\t" + std::to_string(primitive_count) + "\t0\nDATA\n";
  const std::size_t payload_offset = bytes.size();
  bytes.resize(payload_offset + static_cast<std::size_t>(vertex_count) * vertex_stride +
                   static_cast<std::size_t>(index_count) * index_size,
               '\0');

  for (std::uint32_t i = 0; i < std::min(vertex_count, 4u); ++i) {
    const std::size_t offset = payload_offset + static_cast<std::size_t>(i) * vertex_stride;
    write_le_f32_at(bytes, offset + 0, static_cast<float>(i));
    write_le_f32_at(bytes, offset + 4, static_cast<float>(i + 1));
    write_le_f32_at(bytes, offset + 8, static_cast<float>(i + 2));
  }

  const std::size_t index_offset =
      payload_offset + static_cast<std::size_t>(vertex_count) * vertex_stride;
  for (std::uint32_t i = 0; i < std::min(index_count, 8u); ++i) {
    const std::uint32_t value = vertex_count == 0 ? 0 : i % vertex_count;
    const std::size_t offset = index_offset + static_cast<std::size_t>(i) * index_size;
    if (index_size == 2) {
      write_le_u16_at(bytes, offset, static_cast<std::uint16_t>(value));
    } else {
      write_le_u32_at(bytes, offset, value);
    }
  }

  return bytes;
}

std::size_t ndxr_payload_offset(const std::string& bytes) {
  return bytes.find("DATA\n") + 5u;
}

struct RasterFixture final {
  ac6::MissionDrawable drawable;
  ac6::NativeGeometryMetadata geometry;
  ac6::DecodedGeometry decoded;
};

RasterFixture make_raster_fixture(const char* stable_id,
                                  std::vector<ac6::DecodedVertex> vertices,
                                  std::vector<std::uint32_t> indices,
                                  ac6::NativeIndexTopology topology) {
  RasterFixture fixture;
  fixture.drawable = {1, stable_id, "terrain", 119, 1, stable_id,
                      static_cast<std::uint32_t>(vertices.size()),
                      static_cast<std::uint32_t>(indices.size()), stable_id};
  fixture.geometry = {stable_id, "NDXR", topology,
                      static_cast<std::uint32_t>(vertices.size()),
                      static_cast<std::uint32_t>(indices.size()), 1,
                      static_cast<std::uint32_t>(vertices.size()),
                      static_cast<std::uint32_t>(indices.size()), 1, 32, 2,
                      static_cast<std::uint64_t>(vertices.size()) * 32u,
                      static_cast<std::uint64_t>(indices.size()) * 2u};
  fixture.decoded.buffer_id = stable_id;
  fixture.decoded.vertices = std::move(vertices);
  fixture.decoded.indices = std::move(indices);
  for (const ac6::DecodedVertex& vertex : fixture.decoded.vertices) {
    if (!fixture.decoded.bounds.valid) {
      fixture.decoded.bounds = {vertex.x, vertex.y, vertex.z,
                                vertex.x, vertex.y, vertex.z, true};
    } else {
      fixture.decoded.bounds.min_x = std::min(fixture.decoded.bounds.min_x, vertex.x);
      fixture.decoded.bounds.min_y = std::min(fixture.decoded.bounds.min_y, vertex.y);
      fixture.decoded.bounds.min_z = std::min(fixture.decoded.bounds.min_z, vertex.z);
      fixture.decoded.bounds.max_x = std::max(fixture.decoded.bounds.max_x, vertex.x);
      fixture.decoded.bounds.max_y = std::max(fixture.decoded.bounds.max_y, vertex.y);
      fixture.decoded.bounds.max_z = std::max(fixture.decoded.bounds.max_z, vertex.z);
    }
  }
  return fixture;
}

}  // namespace

#define REQUIRE(condition) require(static_cast<bool>(condition), #condition, __FILE__, __LINE__)

int main() {
  ac6::MissionCatalog catalog;
  REQUIRE(catalog.add({1, ac6::MissionFamily::AirIntercept, {9, 119, 165, 199, 210}}));
  REQUIRE(!catalog.add({1, ac6::MissionFamily::Strike, {9}}));
  REQUIRE(!catalog.add({2, ac6::MissionFamily::Unknown, {9}}));
  REQUIRE(!catalog.add({3, ac6::MissionFamily::Strike, {9, 9}}));
  REQUIRE(catalog.find(1) && catalog.find(1)->family == ac6::MissionFamily::AirIntercept);
  const char* catalog_manifest = "ac6-test-catalog.tsv";
  {
    std::ofstream out(catalog_manifest);
    out << "# mission_id family required_asset_ids\n";
    out << "1\tair_intercept\t9,119,165,199,210\n";
    out << "2\tstrike\t9,119\n";
  }
  ac6::MissionCatalog loaded_catalog;
  REQUIRE(loaded_catalog.load_manifest(catalog_manifest));
  REQUIRE(loaded_catalog.find(1) && loaded_catalog.find(1)->asset_ids.size() == 5);
  REQUIRE(loaded_catalog.find(2) && loaded_catalog.find(2)->family == ac6::MissionFamily::Strike);
  std::remove(catalog_manifest);
  const char* bad_catalog_manifest = "ac6-test-bad-catalog.tsv";
  { std::ofstream out(bad_catalog_manifest); out << "3\tunknown\t9\n"; }
  ac6::MissionCatalog bad_catalog;
  REQUIRE(!bad_catalog.load_manifest(bad_catalog_manifest));
  std::remove(bad_catalog_manifest);
  const char* partial_catalog_manifest = "ac6-test-partial-catalog.tsv";
  { std::ofstream out(partial_catalog_manifest);
    out << "2\tstrike\t9\n" << "bad\tunknown\t9\n"; }
  ac6::MissionCatalog atomic_catalog;
  REQUIRE(atomic_catalog.add({77, ac6::MissionFamily::Escort, {210}}));
  REQUIRE(!atomic_catalog.load_manifest(partial_catalog_manifest));
  REQUIRE(atomic_catalog.find(77) && atomic_catalog.find(2) == nullptr);
  std::remove(partial_catalog_manifest);

  ac6::InputMappingDatabase input_mappings;
  REQUIRE(input_mappings.add({0x0010u, ac6::EventType::Pause}));
  REQUIRE(input_mappings.add({0x1000u, ac6::EventType::Resume}));
  REQUIRE(input_mappings.add({0x2000u, ac6::EventType::StartMission}));
  REQUIRE(!input_mappings.add({0x0010u, ac6::EventType::Abort}));
  REQUIRE(!input_mappings.add({0, ac6::EventType::Pause}));
  REQUIRE(input_mappings.resolve(0x0010u) &&
          input_mappings.resolve(0x0010u)->event == ac6::EventType::Pause);
  REQUIRE(input_mappings.resolve(0x0011u) &&
          input_mappings.resolve(0x0011u)->event == ac6::EventType::Pause);
  REQUIRE(input_mappings.resolve(0x0001u) == nullptr);
  ac6::MissionScenario mapped_scenario(1);
  REQUIRE(mapped_scenario.dispatch(ac6::Event{ac6::EventType::StartMission, 0}));
  REQUIRE(mapped_scenario.dispatch_buttons(input_mappings, 0x0010u));
  REQUIRE(mapped_scenario.state() == ac6::ScenarioState::Paused);
  REQUIRE(mapped_scenario.dispatch_buttons(input_mappings, 0x1000u));
  REQUIRE(mapped_scenario.state() == ac6::ScenarioState::Gameplay);
  REQUIRE(!mapped_scenario.dispatch_buttons(input_mappings, 0x0001u));
  ac6::FrontendController mapped_frontend;
  REQUIRE(!mapped_frontend.configure({ac6::FrontendDifficulty::Hard,
                                      ac6::FrontendControls::Normal,
                                      ac6::FrontendLanguage::English}));
  REQUIRE(mapped_frontend.configure({ac6::FrontendDifficulty::Normal,
                                     ac6::FrontendControls::Normal,
                                     ac6::FrontendLanguage::English}));
  REQUIRE(mapped_frontend.settings().valid());
  REQUIRE(mapped_frontend.dispatch_buttons(input_mappings, 0x2000u));
  REQUIRE(mapped_frontend.state() == ac6::FrontendState::NewGame);
  REQUIRE(mapped_frontend.dispatch_buttons(input_mappings, 0x2000u));
  REQUIRE(mapped_frontend.dispatch_buttons(input_mappings, 0x2000u));
  REQUIRE(mapped_frontend.dispatch_buttons(input_mappings, 0x2000u));
  REQUIRE(mapped_frontend.dispatch_buttons(input_mappings, 0x2000u));
  REQUIRE(mapped_frontend.state() == ac6::FrontendState::Mission);
  REQUIRE(mapped_frontend.dispatch_buttons(input_mappings, 0x0010u) == false);
  REQUIRE(mapped_frontend.dispatch({ac6::EventType::Abort, 0}));
  REQUIRE(mapped_frontend.state() == ac6::FrontendState::Title);
  ac6::SdlInputAdapter sdl_input;
  REQUIRE(sdl_input.mapping().valid());
  const char* controls_manifest = "ac6-test-controls.tsv";
  {
    std::ofstream out(controls_manifest);
    out << "pitch_axis\t1\nroll_axis\t0\nyaw_axis\t2\nthrottle_axis\t5\n"
           "invert_pitch\t0\ninvert_roll\t1\ninvert_yaw\t0\n"
           "pitch_up\t26\npitch_down\t22\nroll_left\t4\nroll_right\t7\n"
           "yaw_left\t20\nyaw_right\t8\nthrottle_up\t21\nthrottle_down\t9\n";
  }
  ac6::SdlAxisMapping qualified_axes;
  ac6::SdlKeyboardMapping qualified_keyboard;
  REQUIRE(ac6::SdlInputProfile::load_manifest(controls_manifest, qualified_axes,
                                                qualified_keyboard));
  REQUIRE(qualified_axes.pitch_axis == 1 && qualified_axes.roll_axis == 0 &&
          qualified_axes.throttle_axis == 5 && !qualified_axes.invert_pitch &&
          qualified_axes.invert_roll);
  REQUIRE(qualified_keyboard.pitch_up == static_cast<SDL_Scancode>(26));
  std::remove(controls_manifest);
  const char* bad_controls_manifest = "ac6-test-bad-controls.tsv";
  { std::ofstream out(bad_controls_manifest); out << "pitch_axis\t1\nunknown\t0\n"; }
  REQUIRE(!ac6::SdlInputProfile::load_manifest(bad_controls_manifest, qualified_axes,
                                                qualified_keyboard));
  std::remove(bad_controls_manifest);
  ac6::InputFrame sdl_frame{};
  std::uint16_t sdl_buttons = 0;
  std::vector<ac6::Event> sdl_events;
  SDL_Event axis_event{};
  axis_event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
  axis_event.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
  axis_event.gaxis.value = -10000;
  REQUIRE(sdl_input.apply(axis_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.pitch == 10000 && sdl_events.empty());
  axis_event.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
  axis_event.gaxis.value = 32767;
  REQUIRE(sdl_input.apply(axis_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.throttle == 255);
  axis_event.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHTX;
  axis_event.gaxis.value = static_cast<Sint16>(-32768);
  REQUIRE(sdl_input.apply(axis_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.yaw == -32768);
  axis_event.gaxis.axis = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
  axis_event.gaxis.value = static_cast<Sint16>(-32768);
  REQUIRE(sdl_input.apply(axis_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.throttle == 0);
  ac6::SdlAxisMapping invalid_axis_mapping;
  invalid_axis_mapping.pitch_axis = SDL_GAMEPAD_AXIS_COUNT;
  ac6::SdlInputAdapter invalid_sdl_input(invalid_axis_mapping);
  REQUIRE(!invalid_sdl_input.mapping().valid());
  REQUIRE(!invalid_sdl_input.apply(axis_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_input.keyboard_mapping().valid());
  SDL_Event key_event{};
  key_event.type = SDL_EVENT_KEY_DOWN;
  key_event.key.scancode = SDL_SCANCODE_W;
  REQUIRE(sdl_input.apply(key_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.pitch == 32767);
  key_event.type = SDL_EVENT_KEY_UP;
  REQUIRE(sdl_input.apply(key_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.pitch == 0);
  key_event.type = SDL_EVENT_KEY_DOWN;
  key_event.key.scancode = SDL_SCANCODE_S;
  REQUIRE(sdl_input.apply(key_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.pitch == -32768);
  key_event.type = SDL_EVENT_KEY_UP;
  key_event.key.scancode = SDL_SCANCODE_W;
  REQUIRE(sdl_input.apply(key_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.pitch == -32768);
  key_event.key.scancode = SDL_SCANCODE_S;
  REQUIRE(sdl_input.apply(key_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.pitch == 0);
  key_event.type = SDL_EVENT_KEY_DOWN;
  key_event.key.scancode = SDL_SCANCODE_R;
  REQUIRE(sdl_input.apply(key_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_frame.throttle == 255);
  sdl_input.reset(sdl_frame, sdl_buttons);
  REQUIRE(sdl_frame.pitch == 0 && sdl_frame.roll == 0 && sdl_frame.yaw == 0 &&
          sdl_frame.throttle == 0 && sdl_buttons == 0);
  SDL_Event button_event{};
  button_event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  button_event.gbutton.button = 13;
  REQUIRE(sdl_input.apply(button_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_buttons == 0x2000u && sdl_events.size() == 1 &&
          sdl_events.front().type == ac6::EventType::StartMission && sdl_events.front().subject == 7);
  button_event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
  REQUIRE(sdl_input.apply(button_event, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events));
  REQUIRE(sdl_buttons == 0 && sdl_events.size() == 1);
  ac6::SdlEventPump pump;
  REQUIRE(!pump.initialized());
  bool pre_quit = false;
  REQUIRE(!pump.pump(sdl_input, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events, pre_quit));
  REQUIRE(pump.initialize());
  sdl_frame.pitch = 32767;
  sdl_buttons = 0x2000u;
  SDL_Event removed_event{};
  removed_event.type = SDL_EVENT_GAMEPAD_REMOVED;
  REQUIRE(SDL_PushEvent(&removed_event) == 1);
  bool removed_quit = false;
  REQUIRE(pump.pump(sdl_input, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events,
                    removed_quit));
  REQUIRE(sdl_frame.pitch == 0 && sdl_buttons == 0 && !removed_quit);
  SDL_Event quit_event{};
  quit_event.type = SDL_EVENT_QUIT;
  REQUIRE(SDL_PushEvent(&quit_event) == 1);
  bool quit = false;
  REQUIRE(pump.pump(sdl_input, sdl_frame, sdl_buttons, input_mappings, 7, sdl_events, quit));
  REQUIRE(quit);
  pump.shutdown();
  REQUIRE(!pump.initialized());
  REQUIRE(pump.initialize());
  ac6::SdlWindow window;
  REQUIRE(window.create("ac6-native-test", 320, 180, false, true));
  REQUIRE(window.valid() && window.native_handle() != nullptr);
  ac6::SdlVulkanSurface surface;
  REQUIRE(!surface.valid());
  REQUIRE(!surface.create(window, VK_NULL_HANDLE));
  surface.destroy();
  ac6::VulkanInstance instance;
  const std::vector<const char*> no_extensions;
  REQUIRE(instance.create(no_extensions));
  REQUIRE(instance.valid() && instance.handle() != VK_NULL_HANDLE);
  REQUIRE(!instance.create(no_extensions));
  instance.destroy();
  const char* duplicate_extension = "VK_KHR_surface";
  const std::vector<const char*> duplicate_extensions{duplicate_extension, duplicate_extension};
  REQUIRE(!instance.create(duplicate_extensions));
  window.destroy();
  REQUIRE(!window.valid());
  pump.shutdown();
  ac6::SdlAudioDevice audio;
  if (audio.initialize()) {
    const std::array<float, 4> silence{};
    REQUIRE(audio.queue(silence.data(), static_cast<int>(sizeof(silence))));
    audio.shutdown();
  }
  REQUIRE(!audio.initialized());
  const char* input_manifest = "ac6-test-input-mappings.tsv";
  {
    std::ofstream out(input_manifest);
    out << "# button_mask event\n";
    out << "16\tpause\n";
    out << "4096\tresume\n";
  }
  ac6::InputMappingDatabase loaded_input_mappings;
  REQUIRE(loaded_input_mappings.load_manifest(input_manifest));
  REQUIRE(loaded_input_mappings.resolve(0x1000u)->event == ac6::EventType::Resume);
  std::remove(input_manifest);
  const char* bad_input_manifest = "ac6-test-bad-input-mappings.tsv";
  { std::ofstream out(bad_input_manifest); out << "16\tunknown\n"; }
  ac6::InputMappingDatabase bad_input_mappings;
  REQUIRE(!bad_input_mappings.load_manifest(bad_input_manifest));
  std::remove(bad_input_manifest);

  ac6::SaveStore saves;
  REQUIRE(!saves.save(0, {1, 0, 0, 0}));
  REQUIRE(saves.save(1, {42, 1.0f, 2.0f, 3.0f}));
  REQUIRE(saves.load(1) && saves.load(1)->tick == 42);
  REQUIRE(saves.load(2) == nullptr);
  ac6::MissionRuntime save_runtime(1);
  save_runtime.tick(1.0f / 60.0f, {1200, 0, 0, 10, 0});
  const auto checkpoint = save_runtime.snapshot();
  REQUIRE(saves.save(2, checkpoint));
  save_runtime.tick(1.0f / 60.0f, {3200, 0, 0, 20, 0});
  REQUIRE(save_runtime.restore(*saves.load(2)));
  REQUIRE(save_runtime.snapshot().tick == checkpoint.tick &&
         save_runtime.snapshot().position_x == checkpoint.position_x &&
         save_runtime.snapshot().pitch == checkpoint.pitch &&
         save_runtime.snapshot().roll == checkpoint.roll &&
         save_runtime.snapshot().yaw == checkpoint.yaw);
  save_runtime.tick(1.0f / 120.0f, {});
  const auto partial_checkpoint = save_runtime.snapshot();
  REQUIRE(partial_checkpoint.tick == checkpoint.tick && partial_checkpoint.fixed_accumulator > 0.0f);
  REQUIRE(saves.save(3, partial_checkpoint));
  const char* save_file = "ac6-test-save.ac6s";
  REQUIRE(saves.write_file(save_file));
  REQUIRE(!std::filesystem::exists(std::string(save_file) + ".tmp"));
  ac6::SaveStore loaded_saves;
  REQUIRE(loaded_saves.read_file(save_file));
  REQUIRE(loaded_saves.load(1) && loaded_saves.load(1)->tick == 42);
  REQUIRE(loaded_saves.load(2) && loaded_saves.load(2)->position_z == checkpoint.position_z &&
          loaded_saves.load(2)->pitch == checkpoint.pitch &&
          loaded_saves.load(2)->roll == checkpoint.roll &&
          loaded_saves.load(2)->yaw == checkpoint.yaw);
  REQUIRE(loaded_saves.load(3) &&
          loaded_saves.load(3)->fixed_accumulator == partial_checkpoint.fixed_accumulator);
  std::remove(save_file);
  const char* bad_save_file = "ac6-test-bad-save.ac6s";
  { std::ofstream out(bad_save_file, std::ios::binary); out << "bad"; }
  ac6::SaveStore bad_saves;
  REQUIRE(!bad_saves.read_file(bad_save_file));
  REQUIRE(!bad_saves.save(3, {0, 0.0f, 0.0f, 0.0f}));
  REQUIRE(!saves.write_file("ac6-missing-save-directory/save.ac6s"));
  REQUIRE(!std::filesystem::exists("ac6-missing-save-directory/save.ac6s.tmp"));
  std::remove(bad_save_file);
  ac6::ReplayLog replay;
  replay.append({1, 2, 3, 4, 0});
  replay.append({5, 6, 7, 8, 1});
  REQUIRE(replay.frames().size() == 2 && replay.frames()[1].throttle == 8);
  replay.clear();
  REQUIRE(replay.frames().empty());
  replay.append({12000, -8000, 4000, 192, 0});
  replay.append({-4000, 1000, 2000, 220, 0});
  const char* replay_file = "ac6-test-replay.ac6r";
  REQUIRE(replay.write_file(replay_file));
  ac6::ReplayLog loaded_replay;
  REQUIRE(loaded_replay.read_file(replay_file));
  REQUIRE(loaded_replay.frames() == replay.frames());
  std::remove(replay_file);
  const char* bad_replay_file = "ac6-test-bad-replay.ac6r";
  { std::ofstream out(bad_replay_file, std::ios::binary); out << "bad"; }
  ac6::ReplayLog bad_replay;
  REQUIRE(!bad_replay.read_file(bad_replay_file));
  REQUIRE(replay.write_file(bad_replay_file));
  { std::ofstream out(bad_replay_file, std::ios::binary | std::ios::app); out.put('x'); }
  REQUIRE(!bad_replay.read_file(bad_replay_file));
  REQUIRE(replay.write_file(bad_replay_file));
  std::filesystem::resize_file(bad_replay_file, 10u);
  REQUIRE(!bad_replay.read_file(bad_replay_file));
  std::remove(bad_replay_file);
  ac6::MissionRuntime replay_runtime_a(1), replay_runtime_b(1);
  const auto replay_frame_a = replay_runtime_a.run_replay(1.0f / 60.0f, replay);
  const auto replay_frame_b = replay_runtime_b.run_replay(1.0f / 60.0f, replay);
  REQUIRE(replay_frame_a.tick == replay_frame_b.tick &&
         replay_frame_a.position_x == replay_frame_b.position_x &&
         replay_frame_a.position_z == replay_frame_b.position_z);

  ac6::FrontendController frontend;
  REQUIRE(frontend.state() == ac6::FrontendState::Title);
  REQUIRE(!frontend.select_mission(loaded_catalog, 99));
  REQUIRE(frontend.select_mission(loaded_catalog, 1));
  REQUIRE(frontend.selected_mission() == 1);
  REQUIRE(frontend.mission_definition(loaded_catalog) == nullptr);
  for (int i = 0; i < 5; ++i) REQUIRE(frontend.advance());
  REQUIRE(frontend.state() == ac6::FrontendState::Mission);
  const ac6::MissionDefinition* selected_definition = frontend.mission_definition(loaded_catalog);
  REQUIRE(selected_definition && selected_definition->id == 1);
  REQUIRE(!frontend.advance());

  ac6::CampaignProgression frontend_campaign;
  REQUIRE(frontend_campaign.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(frontend_campaign.finalize());
  ac6::FrontendController campaign_frontend;
  campaign_frontend.set_campaign(&frontend_campaign);
  REQUIRE(campaign_frontend.select_mission(loaded_catalog, 1));
  REQUIRE(campaign_frontend.advance() && campaign_frontend.state() == ac6::FrontendState::NewGame);
  REQUIRE(campaign_frontend.advance() &&
          campaign_frontend.state() == ac6::FrontendState::Briefing &&
          frontend_campaign.status(1)->state == ac6::CampaignMissionState::Briefing);
  REQUIRE(!campaign_frontend.set_loadout({7, 8, true}));
  REQUIRE(campaign_frontend.advance() && campaign_frontend.state() == ac6::FrontendState::Hangar);
  REQUIRE(campaign_frontend.set_loadout({7, 8, true}));
  REQUIRE(campaign_frontend.advance() && campaign_frontend.state() == ac6::FrontendState::Loading);
  REQUIRE(frontend_campaign.status(1)->state == ac6::CampaignMissionState::Active);
  REQUIRE(campaign_frontend.advance() && campaign_frontend.state() == ac6::FrontendState::Mission);
  ac6::CampaignProgression locked_frontend_campaign;
  REQUIRE(locked_frontend_campaign.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(locked_frontend_campaign.add({2, {2, 10, 10}, 1, {1}}));
  REQUIRE(locked_frontend_campaign.finalize());
  ac6::FrontendController locked_frontend;
  locked_frontend.set_campaign(&locked_frontend_campaign);
  REQUIRE(!locked_frontend.select_mission(loaded_catalog, 2));

  const char* launch_manifest = "ac6-test-launch.tsv";
  {
    std::ofstream out(launch_manifest);
    out << "# mission_id player_entity unit_id:owner:asset\n";
    out << "1\t4097\t4097:1:9,4098:1:119\t7:60.0:20.0:0.25:100.0\n";
  }
  ac6::MissionLaunchDatabase launches;
  REQUIRE(launches.load_manifest(launch_manifest));
  const ac6::MissionLaunchDefinition* launch = launches.find(1);
  REQUIRE(launch && launch->player_entity == 4097 && launch->units.size() == 2 &&
          launch->weapons.size() == 1 && launch->weapons[0].id == 7);
  std::remove(launch_manifest);
  const char* partial_launch_manifest = "ac6-test-partial-launch.tsv";
  { std::ofstream out(partial_launch_manifest);
    out << "2\t4097\t4097:1:9\n" << "bad\t4097\t4097:1:9\n"; }
  ac6::MissionLaunchDatabase atomic_launches;
  REQUIRE(atomic_launches.add(*launch));
  REQUIRE(!atomic_launches.load_manifest(partial_launch_manifest));
  REQUIRE(atomic_launches.find(1) && atomic_launches.find(2) == nullptr);
  std::remove(partial_launch_manifest);
  const char* bad_launch_manifest = "ac6-test-bad-launch.tsv";
  { std::ofstream out(bad_launch_manifest); out << "1\t8192\t4097:1:9\n"; }
  ac6::MissionLaunchDatabase bad_launches;
  REQUIRE(!bad_launches.load_manifest(bad_launch_manifest));
  std::remove(bad_launch_manifest);
  const char* runtime_manifest = "ac6-test-runtime-manifest.tsv";
  const char* runtime_catalog = "ac6-test-runtime-catalog.tsv";
  const char* runtime_assets = "ac6-test-runtime-assets.tsv";
  const char* runtime_launches = "ac6-test-runtime-launches.tsv";
  const char* runtime_radios = "ac6-test-runtime-radios.tsv";
  const char* runtime_campaign = "ac6-test-runtime-campaign.tsv";
  const char* runtime_objectives = "ac6-test-runtime-objectives.tsv";
  const char* runtime_waves = "ac6-test-runtime-waves.tsv";
  const char* runtime_ai = "ac6-test-runtime-ai.tsv";
  const char* runtime_sequence = "ac6-test-runtime-sequence.tsv";
  { std::ofstream out(runtime_catalog);
    out << "1\tair_intercept\t9\n";
    out << "2\tstrike\t9,119\n";
    out << "3\tescort\t9,165\n"; }
  { std::ofstream out(runtime_assets);
    out << "9\tDATA00.PAC@qualified\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n";
    out << "119\tDATA00.PAC@qualified\tabcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789\n";
    out << "165\tDATA00.PAC@qualified\t89abcdef0123456789abcdef0123456789abcdef0123456789abcdef01234567\n";
  }
  { std::ofstream out(runtime_launches);
    out << "1\t4097\t4097:1:9\t7:10:100:0:1000\n";
    out << "2\t4098\t4098:1:119\n";
    out << "3\t4099\t4099:1:165\n"; }
  { std::ofstream out(runtime_radios); out << "1\t10\talpha_warning\tAWACS\t9\t119\n"; }
  { std::ofstream out(runtime_campaign); out << "1\t1\t9\t9\t1\t-\n"; }
  { std::ofstream out(runtime_objectives); out << "1\t1\tintercept_primary\t1\n"; }
  { std::ofstream out(runtime_waves);
    out << "1\t1\t5000\t2\t119\t2\t20\t0\t0\t100\t100\t1\n"; }
  { std::ofstream out(runtime_ai); out << "1\t1\t1\t4097\t5000\t7\n"; }
  { std::ofstream out(runtime_sequence);
    out << "1\t1\t1\tactivate_objective\t1\t0\n";
    out << "1\t2\t2\tplay_radio\t10\t0.25\n"; }
  { std::ofstream out(runtime_manifest);
    out << "catalog\t" << runtime_catalog << "\n";
    out << "assets\t" << runtime_assets << "\n";
    out << "launches\t" << runtime_launches << "\n";
    out << "campaign\t" << runtime_campaign << "\n";
  }
  ac6::MissionManifestLoader manifest_loader;
  ac6::MissionCatalog manifest_catalog;
  ac6::MissionAssetDatabase manifest_assets;
  ac6::MissionLaunchDatabase manifest_launches;
  REQUIRE(manifest_loader.load_runtime(runtime_manifest, manifest_catalog,
                                       manifest_assets, manifest_launches));
  REQUIRE(manifest_catalog.find(1) &&
          manifest_catalog.find(1)->family == ac6::MissionFamily::AirIntercept);
  REQUIRE(manifest_catalog.find(2) &&
          manifest_catalog.find(2)->family == ac6::MissionFamily::Strike);
  REQUIRE(manifest_catalog.find(3) &&
          manifest_catalog.find(3)->family == ac6::MissionFamily::Escort);
  REQUIRE(manifest_assets.resolve(9) && manifest_assets.resolve(119) &&
          manifest_assets.resolve(165));
  REQUIRE(manifest_launches.find(1) && manifest_launches.find(2) &&
          manifest_launches.find(3));
  const char* bad_optional_input = "ac6-test-bad-optional-input.tsv";
  const char* bad_optional_manifest = "ac6-test-bad-optional-manifest.tsv";
  { std::ofstream out(bad_optional_input); out << "1\tunknown_event\n"; }
  { std::ofstream out(bad_optional_manifest);
    out << "catalog\t" << runtime_catalog << "\nassets\t" << runtime_assets
        << "\nlaunches\t" << runtime_launches << "\ninput\t" << bad_optional_input << "\n";
  }
  REQUIRE(!manifest_loader.load_runtime(bad_optional_manifest, manifest_catalog,
                                        manifest_assets, manifest_launches));
  REQUIRE(manifest_catalog.find(1) && manifest_catalog.find(2) && manifest_catalog.find(3) &&
          manifest_assets.resolve(9) && manifest_assets.resolve(119) &&
          manifest_assets.resolve(165) && manifest_launches.find(1) &&
          manifest_launches.find(2) && manifest_launches.find(3));
  std::remove(bad_optional_input);
  std::remove(bad_optional_manifest);
  const char* render_keys[] = {"render", "drawables", "transforms", "materials", "textures",
                               "shaders", "targets", "passes", "resolves", "buffers"};
  const char* input_manifest_path = "ac6-test-input-manifest.tsv";
  const char* controls_manifest_path = "ac6-test-controls-manifest.tsv";
  { std::ofstream out(input_manifest_path); out << "1\tstart_mission\n"; }
  { std::ofstream out(controls_manifest_path);
    out << "pitch_axis\t1\nroll_axis\t0\nyaw_axis\t2\nthrottle_axis\t5\n"
           "invert_pitch\t0\ninvert_roll\t0\ninvert_yaw\t0\n"
           "pitch_up\t26\npitch_down\t22\nroll_left\t4\nroll_right\t7\n"
           "yaw_left\t20\nyaw_right\t8\nthrottle_up\t21\nthrottle_down\t9\n";
  }
  for (const char* key : render_keys) {
    const std::string key_file = std::string("ac6-test-") + key + ".tsv";
    std::ofstream out(key_file);
    out << key << "\t" << key_file << "\n";
  }
  { std::ofstream out(runtime_manifest);
    out << "catalog\t" << runtime_catalog << "\n";
    out << "assets\t" << runtime_assets << "\n";
    out << "launches\t" << runtime_launches << "\n";
    out << "campaign\t" << runtime_campaign << "\n";
    out << "input\t" << input_manifest_path << "\n";
    out << "controls\t" << controls_manifest_path << "\n";
    out << "objectives\t" << runtime_objectives << "\n";
    out << "radios\t" << runtime_radios << "\n";
    out << "waves\t" << runtime_waves << "\n";
    out << "ai\t" << runtime_ai << "\n";
    out << "sequence\t" << runtime_sequence << "\n";
    for (const char* key : render_keys) out << key << "\tac6-test-" << key << ".tsv\n";
  }
  ac6::MissionRuntimeServices manifest_services;
  REQUIRE(manifest_loader.load_runtime(runtime_manifest, manifest_catalog,
                                       manifest_assets, manifest_launches,
                                       manifest_services));
  REQUIRE(manifest_services.has_input && manifest_services.input.resolve(1) &&
          manifest_services.input.resolve(1)->event == ac6::EventType::StartMission);
  REQUIRE(manifest_services.has_objectives &&
          manifest_services.objectives.find_by_mission(1).size() == 1);
  REQUIRE(manifest_services.has_radios && manifest_services.radios.find(1, 10) != nullptr);
  REQUIRE(manifest_services.has_waves && manifest_services.waves.pending(1) == 1);
  REQUIRE(manifest_services.has_ai && manifest_services.ai.active(1, 1) == 1);
  REQUIRE(manifest_services.has_sequence && manifest_services.sequence.pending(1) == 2);
  REQUIRE(manifest_services.has_campaign &&
          manifest_services.campaign.route_for_selector(1) != nullptr);
  REQUIRE(manifest_services.campaign.enter_briefing(1));
  REQUIRE(manifest_services.campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(manifest_services.campaign.begin(1));
  ac6::MissionExecution services_execution(*manifest_catalog.find(1), &manifest_assets,
                                           &manifest_services.objectives,
                                           &manifest_services.radios,
                                           &manifest_services.campaign,
                                           &manifest_services.waves,
                                           &manifest_services.sequence,
                                           &manifest_services.input,
                                           &manifest_services.ai);
  REQUIRE(services_execution.launch(*manifest_launches.find(1)));
  REQUIRE(services_execution.tick(1.0f / 60.0f, {}).tick == 1);
  REQUIRE(services_execution.scenario().objectives().find(1)->state ==
              ac6::ObjectiveState::Active);
  REQUIRE(manifest_services.waves.pending(1) == 0 &&
          manifest_services.waves.spawned(1) == 1 &&
          services_execution.combat().unit(5000) != nullptr &&
          services_execution.combat().active_projectiles() == 1);
  REQUIRE(services_execution.tick(1.0f / 60.0f, {}).tick == 2);
  REQUIRE(services_execution.radio().playing() && manifest_services.sequence.pending(1) == 0 &&
          manifest_services.ai.active(1, 2) == 1 &&
          services_execution.combat().active_projectiles() >= 1);
  REQUIRE(services_execution.complete_objective(1));
  const auto services_terminal_frame = services_execution.tick(1.0f / 60.0f, {});
  REQUIRE(!services_terminal_frame.mission_ready &&
          services_execution.scenario().state() == ac6::ScenarioState::Complete);
  REQUIRE(manifest_services.campaign.status(1)->state == ac6::CampaignMissionState::Completed);
  REQUIRE(services_execution.debrief().outcome == ac6::MissionOutcome::Success);
  ac6::MissionManifestPaths manifest_paths;
  REQUIRE(manifest_loader.load_paths(runtime_manifest, manifest_paths));
  REQUIRE(manifest_paths.render_valid());
  REQUIRE(manifest_paths.input == input_manifest_path);
  REQUIRE(manifest_paths.controls == controls_manifest_path);
  REQUIRE(manifest_paths.objectives == runtime_objectives);
  REQUIRE(manifest_paths.radios == runtime_radios);
  REQUIRE(manifest_paths.waves == runtime_waves);
  REQUIRE(manifest_paths.ai == runtime_ai);
  REQUIRE(manifest_paths.sequence == runtime_sequence);
  REQUIRE(manifest_paths.campaign == runtime_campaign);
  ac6::CampaignProgression manifest_campaign;
  REQUIRE(manifest_loader.load_campaign(runtime_manifest, manifest_campaign));
  REQUIRE(manifest_campaign.route_for_selector(1) != nullptr);
  ac6::InputMappingDatabase manifest_input;
  REQUIRE(manifest_loader.load_input(runtime_manifest, manifest_input));
  REQUIRE(manifest_input.resolve(1) && manifest_input.resolve(1)->event == ac6::EventType::StartMission);
  const char* bad_service_input = "ac6-test-bad-service-input.tsv";
  const char* bad_service_manifest = "ac6-test-bad-service-manifest.tsv";
  { std::ofstream out(bad_service_input); out << "1\tunknown_event\n"; }
  { std::ofstream out(bad_service_manifest);
    out << "catalog\t" << runtime_catalog << "\nassets\t" << runtime_assets
        << "\nlaunches\t" << runtime_launches << "\ninput\t" << bad_service_input << "\n";
  }
  REQUIRE(!manifest_loader.load_runtime(bad_service_manifest, manifest_catalog,
                                        manifest_assets, manifest_launches,
                                        manifest_services));
  REQUIRE(manifest_services.has_input && manifest_services.input.resolve(1) &&
          manifest_services.input.resolve(1)->event == ac6::EventType::StartMission &&
          manifest_services.has_objectives && manifest_services.has_radios &&
          manifest_services.has_waves && manifest_services.has_ai &&
          manifest_services.has_sequence && manifest_services.has_campaign &&
          manifest_services.campaign.status(1)->state == ac6::CampaignMissionState::Completed);
  std::remove(bad_service_input);
  std::remove(bad_service_manifest);
  std::remove(runtime_manifest);
  std::remove(runtime_catalog);
  std::remove(runtime_assets);
  std::remove(runtime_launches);
  std::remove(runtime_radios);
  std::remove(runtime_campaign);
  std::remove(runtime_objectives);
  std::remove(runtime_waves);
  std::remove(runtime_ai);
  std::remove(runtime_sequence);
  std::remove(input_manifest_path);
  std::remove(controls_manifest_path);
  for (const char* key : render_keys) std::remove((std::string("ac6-test-") + key + ".tsv").c_str());

  ac6::UnitRegistry units;
  ac6::MissionScenario wrong_scenario(2);
  REQUIRE(configure_mission_launch(*launch, units, wrong_scenario) == false);
  ac6::MissionScenario scenario(*selected_definition);
  REQUIRE(configure_mission_launch(*launch, units, scenario));
  REQUIRE(!units.register_unit({0x1001, 0x0001, 9, false}));
  REQUIRE(!units.register_unit({0x1002, 0x1002, 9, false}));
  REQUIRE(units.size() == 2);
  REQUIRE(units.activate(0x1001));
  REQUIRE(units.find(0x1001) && units.find(0x1001)->active);
  REQUIRE(units.active_count() == 2);
  REQUIRE(!units.activate(0x2000));

  REQUIRE(scenario.bind_player(units, 0x1001));
  REQUIRE(!scenario.bind_player(units, 0x2000));
  REQUIRE(scenario.player() == 0x1001);
  REQUIRE(scenario.state() == ac6::ScenarioState::Loading);
  REQUIRE(!scenario.dispatch({ac6::EventType::StartMission, 0x2002}));
  REQUIRE(scenario.dispatch({ac6::EventType::StartMission, 0x1001}));
  REQUIRE(scenario.state() == ac6::ScenarioState::Gameplay);
  REQUIRE(scenario.dispatch({ac6::EventType::Pause, 0}));
  REQUIRE(scenario.dispatch({ac6::EventType::Resume, 0}));
  REQUIRE(scenario.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(!scenario.dispatch({ac6::EventType::Resume, 0}));
  ac6::MissionScenario objective_scenario(1);
  REQUIRE(objective_scenario.add_objective({1, "intercept_primary", true,
                                             ac6::ObjectiveState::Pending}));
  REQUIRE(objective_scenario.dispatch({ac6::EventType::StartMission, 0}));
  REQUIRE(!objective_scenario.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(objective_scenario.objectives().size() == 1);
  REQUIRE(objective_scenario.objectives().find(1)->state == ac6::ObjectiveState::Pending);
  REQUIRE(!objective_scenario.complete_objective(1));
  REQUIRE(objective_scenario.activate_objective(1));
  REQUIRE(objective_scenario.complete_objective(1));
  REQUIRE(objective_scenario.objectives().find(1)->state == ac6::ObjectiveState::Complete);
  REQUIRE(objective_scenario.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(objective_scenario.state() == ac6::ScenarioState::Complete);
  ac6::MissionScenario failed_objective_scenario(1);
  REQUIRE(failed_objective_scenario.add_objective({2, "intercept_secondary", false,
                                                    ac6::ObjectiveState::Pending}));
  REQUIRE(failed_objective_scenario.fail_objective(2));
  REQUIRE(failed_objective_scenario.state() == ac6::ScenarioState::Aborted);
  const char* objective_manifest = "ac6-test-objectives.tsv";
  { std::ofstream out(objective_manifest);
    out << "1\t1\tintercept_primary\t1\n" << "1\t2\tintercept_secondary\t0\n"; }
  ac6::MissionObjectiveDatabase loaded_objectives;
  REQUIRE(loaded_objectives.load_manifest(objective_manifest));
  REQUIRE(loaded_objectives.find_by_mission(1).size() == 2);
  REQUIRE(loaded_objectives.find_by_mission(1)[0]->stable_id == "intercept_primary");
  REQUIRE(loaded_objectives.find_by_mission(2).empty());
  std::remove(objective_manifest);
  const char* radio_manifest = "ac6-test-radios.tsv";
  { std::ofstream out(radio_manifest);
    out << "1\t10\talpha_warning\tAWACS\t199\t210\n"; }
  ac6::RadioMessageDatabase radios;
  REQUIRE(radios.load_manifest(radio_manifest));
  REQUIRE(radios.find(1, 10) && radios.find(1, 10)->speaker == "AWACS");
  REQUIRE(radios.find(2, 10) == nullptr);
  ac6::MissionScenario radio_scenario(1);
  REQUIRE(radio_scenario.dispatch({ac6::EventType::StartMission, 0}));
  REQUIRE(radio_scenario.dispatch_radio(radios, 10));
  REQUIRE(!radio_scenario.dispatch_radio(radios, 11));
  REQUIRE(radio_scenario.radio_history().size() == 1 && radio_scenario.radio_history()[0] == 10);
  ac6::MissionAssetDatabase radio_assets;
  for (const ac6::AssetId id : {9u, 119u, 165u, 199u, 210u}) {
    REQUIRE(radio_assets.add({id, "DATA00.PAC@radio", "radio-hash"}));
  }
  ac6::MissionExecution radio_execution(*selected_definition, &radio_assets, nullptr, &radios);
  REQUIRE(radio_execution.launch(*launch));
  REQUIRE(radio_execution.play_radio(10, 0.5f));
  REQUIRE(radio_execution.radio().playing() &&
          radio_execution.radio().snapshot().audio_asset == 199 &&
          radio_execution.radio().snapshot().subtitle_asset == 210);
  REQUIRE(!radio_execution.play_radio(10, 0.5f));
  REQUIRE(radio_execution.dispatch({ac6::EventType::Pause, 0}));
  radio_execution.tick(0.25f, {});
  REQUIRE(radio_execution.radio().snapshot().elapsed_seconds == 0.0f);
  REQUIRE(radio_execution.dispatch({ac6::EventType::Resume, 0}));
  radio_execution.tick(0.25f, {});
  REQUIRE(radio_execution.radio().snapshot().state == ac6::RadioPlaybackState::Playing);
  radio_execution.tick(0.25f, {});
  REQUIRE(radio_execution.radio().snapshot().state == ac6::RadioPlaybackState::Complete);
  REQUIRE(!radio_execution.play_radio(11, 0.5f));
  ac6::MissionSequenceDirector sequence;
  REQUIRE(sequence.add({1, 1, 1, ac6::MissionSequenceEventType::ActivateObjective, 1, 0.0f}));
  REQUIRE(sequence.add({1, 2, 1, ac6::MissionSequenceEventType::PlayRadio, 10, 0.25f}));
  REQUIRE(sequence.add({1, 3, 1, ac6::MissionSequenceEventType::CompleteObjective, 1, 0.0f}));
  REQUIRE(!sequence.add({1, 1, 1, ac6::MissionSequenceEventType::FailObjective, 1, 0.0f}));
  ac6::MissionExecution sequenced_execution(*selected_definition, &radio_assets,
                                            &loaded_objectives, &radios, nullptr, nullptr,
                                            &sequence);
  REQUIRE(sequenced_execution.launch(*launch));
  REQUIRE(sequenced_execution.tick(1.0f / 60.0f, {}).tick == 1);
  REQUIRE(sequenced_execution.scenario().objectives().find(1)->state ==
              ac6::ObjectiveState::Active && sequence.dispatched(1) == 1);
  sequenced_execution.tick(1.0f / 60.0f, {});
  REQUIRE(sequenced_execution.radio().playing() && sequence.dispatched(1) == 2);
  sequenced_execution.tick(1.0f / 60.0f, {});
  REQUIRE(sequenced_execution.scenario().objectives().find(1)->state ==
              ac6::ObjectiveState::Complete && sequence.pending(1) == 0);
  std::remove(radio_manifest);

  ac6::MissionAssetDatabase assets;
  REQUIRE(assets.add({9, "DATA00.PAC@0x1000+0x200", "aaaaaaaa"}));
  REQUIRE(!assets.add({9, "duplicate", "bbbbbbbb"}));
  REQUIRE(!assets.add({0, "bad", "cccccccc"}));
  const auto* entry = assets.resolve(9);
  REQUIRE(entry && entry->relative_path == "DATA00.PAC@0x1000+0x200");
  REQUIRE(assets.resolve(119) == nullptr);

  const char* manifest = "ac6-test-manifest.tsv";
  { std::ofstream out(manifest); out << "119\tDATA00.PAC@0x2000+0x40\tbbbbbbbb\n"; }
  ac6::MissionAssetDatabase loaded;
  REQUIRE(loaded.load_manifest(manifest));
  REQUIRE(loaded.resolve(119) && loaded.resolve(119)->sha256 == "bbbbbbbb");
  ac6::MissionAssetDatabase qualified_assets;
  REQUIRE(!qualified_assets.load_qualified_manifest(manifest));
  std::remove(manifest);
  const char* partial_asset_manifest = "ac6-test-partial-assets.tsv";
  { std::ofstream out(partial_asset_manifest);
    out << "165\tqualified\thash\n" << "bad\tinvalid\n"; }
  REQUIRE(!loaded.load_manifest(partial_asset_manifest));
  REQUIRE(loaded.resolve(119) && loaded.resolve(165) == nullptr);
  std::remove(partial_asset_manifest);

  const char* legacy_qualified_manifest = "ac6-test-legacy-qualified-assets.tsv";
  { std::ofstream out(legacy_qualified_manifest);
    out << "119\tlegacy\t" << std::string(64, 'a') << "\n"; }
  REQUIRE(qualified_assets.load_qualified_manifest(legacy_qualified_manifest));
  REQUIRE(qualified_assets.resolve(119) && qualified_assets.resolve(119)->byte_size == 0);
  std::remove(legacy_qualified_manifest);

  const char* qualified_a = "ac6-qualified-a.bin";
  const char* qualified_b = "ac6-qualified-b.bin";
  { std::ofstream out(qualified_a, std::ios::binary); out << "asset-a"; }
  { std::ofstream out(qualified_b, std::ios::binary); out << "asset-b"; }
  const char* extended_manifest = "ac6-test-extended-assets.tsv";
  { std::ofstream out(extended_manifest);
    out << "9\tac6-qualified-a.bin\t769b206a3f59eae2ed7455e4a9269121ce20f26534c06e32f9fa6085c7274a63\t7\t-\n";
    out << "119\tac6-qualified-b.bin\t7b4875f447a821ed6920925775a6aa62791a87298e1637fd2ba8f4b9a0d72637\t7\t9\n"; }
  ac6::MissionAssetDatabase extended_assets;
  REQUIRE(extended_assets.load_qualified_manifest(extended_manifest));
  REQUIRE(extended_assets.resolve(119) && extended_assets.resolve(119)->byte_size == 7 &&
          extended_assets.resolve(119)->dependencies == std::vector<ac6::AssetId>{9});

  const char* bad_size_manifest = "ac6-test-bad-size-assets.tsv";
  { std::ofstream out(bad_size_manifest);
    out << "9\tac6-qualified-a.bin\t769b206a3f59eae2ed7455e4a9269121ce20f26534c06e32f9fa6085c7274a63\t8\t-\n";
    out << "119\tac6-qualified-b.bin\t7b4875f447a821ed6920925775a6aa62791a87298e1637fd2ba8f4b9a0d72637\t7\t9\n"; }
  REQUIRE(!extended_assets.load_qualified_manifest(bad_size_manifest));
  REQUIRE(extended_assets.resolve(119) && extended_assets.resolve(119)->relative_path == qualified_b);
  std::remove(bad_size_manifest);

  const char* bad_hash_manifest = "ac6-test-bad-hash-assets.tsv";
  { std::ofstream out(bad_hash_manifest);
    out << "9\tac6-qualified-a.bin\t" << std::string(64, 'b') << "\t7\t-\n";
    out << "119\tac6-qualified-b.bin\t7b4875f447a821ed6920925775a6aa62791a87298e1637fd2ba8f4b9a0d72637\t7\t9\n"; }
  REQUIRE(!extended_assets.load_qualified_manifest(bad_hash_manifest));
  REQUIRE(extended_assets.resolve(9) &&
          extended_assets.resolve(9)->sha256 ==
              "769b206a3f59eae2ed7455e4a9269121ce20f26534c06e32f9fa6085c7274a63");
  std::remove(bad_hash_manifest);

  const char* missing_dependency_manifest = "ac6-test-missing-dependency-assets.tsv";
  { std::ofstream out(missing_dependency_manifest);
    out << "9\tac6-qualified-a.bin\t769b206a3f59eae2ed7455e4a9269121ce20f26534c06e32f9fa6085c7274a63\t7\t119\n"; }
  REQUIRE(!extended_assets.load_qualified_manifest(missing_dependency_manifest));
  std::remove(missing_dependency_manifest);

  const char* cycle_manifest = "ac6-test-cycle-assets.tsv";
  { std::ofstream out(cycle_manifest);
    out << "9\tac6-qualified-a.bin\t769b206a3f59eae2ed7455e4a9269121ce20f26534c06e32f9fa6085c7274a63\t7\t119\n";
    out << "119\tac6-qualified-b.bin\t7b4875f447a821ed6920925775a6aa62791a87298e1637fd2ba8f4b9a0d72637\t7\t9\n"; }
  REQUIRE(!extended_assets.load_qualified_manifest(cycle_manifest));
  std::remove(cycle_manifest);
  std::remove(extended_manifest);
  std::remove(qualified_a);
  std::remove(qualified_b);

  ac6::MissionRuntime runtime(*selected_definition, &assets);
  ac6::UnitRegistry runtime_units;
  ac6::MissionScenario runtime_scenario(*selected_definition);
  REQUIRE(configure_mission_launch(*launch, runtime_units, runtime_scenario));
  runtime.set_units(&runtime_units);
  REQUIRE(!runtime.set_definition(nullptr));
  REQUIRE(!runtime.set_definition(loaded_catalog.find(2)));
  const auto first = runtime.tick(1.0f / 60.0f, {});
  REQUIRE(!first.mission_ready);
  ac6::MissionRuntime scheduler(1);
  REQUIRE(scheduler.tick(1.0f / 120.0f, {}).tick == 0);
  REQUIRE(scheduler.tick(1.0f / 120.0f, {}).tick == 1);
  REQUIRE(scheduler.tick(1.0f / 30.0f, {}).tick == 3);
  REQUIRE(runtime.set_definition(loaded_catalog.find(1)));
  for (const ac6::AssetId id : {119u, 165u, 199u, 210u}) {
    REQUIRE(assets.add({id, "DATA00.PAC@qualified", "hash"}));
  }
  ac6::MissionExecution frontend_execution(*selected_definition, &assets, nullptr, nullptr,
                                            &frontend_campaign);
  REQUIRE(campaign_frontend.launch_selected(loaded_catalog, launches, frontend_execution));
  REQUIRE(frontend_execution.launched() &&
          frontend_execution.scenario().state() == ac6::ScenarioState::Gameplay);
  REQUIRE(!campaign_frontend.launch_selected(loaded_catalog, launches, frontend_execution));
  ac6::MissionExecution debrief_execution(*selected_definition, &assets, &loaded_objectives,
                                           nullptr, &frontend_campaign);
  REQUIRE(debrief_execution.launch(*launch));
  REQUIRE(debrief_execution.activate_objective(1));
  REQUIRE(debrief_execution.complete_objective(1));
  REQUIRE(debrief_execution.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(campaign_frontend.enter_debrief(debrief_execution));
  REQUIRE(campaign_frontend.state() == ac6::FrontendState::Debrief &&
          campaign_frontend.debrief() != nullptr &&
          campaign_frontend.debrief()->outcome == ac6::MissionOutcome::Success);
  REQUIRE(campaign_frontend.return_to_campaign());
  REQUIRE(campaign_frontend.state() == ac6::FrontendState::NewGame &&
          campaign_frontend.selected_mission() == 0 && campaign_frontend.debrief() == nullptr);
  ac6::FrontendController failure_frontend;
  REQUIRE(failure_frontend.configure({ac6::FrontendDifficulty::Normal,
                                      ac6::FrontendControls::Normal,
                                      ac6::FrontendLanguage::English}));
  REQUIRE(failure_frontend.select_mission(loaded_catalog, 1));
  for (int phase = 0; phase < 5; ++phase) REQUIRE(failure_frontend.advance());
  ac6::MissionExecution failure_debrief_execution(*selected_definition, &assets);
  REQUIRE(failure_frontend.launch_selected(loaded_catalog, launches, failure_debrief_execution));
  REQUIRE(failure_debrief_execution.combat().apply_damage(4097, 100.0f));
  REQUIRE(!failure_debrief_execution.tick(1.0f / 60.0f, {}).mission_ready);
  REQUIRE(failure_frontend.enter_debrief(failure_debrief_execution));
  REQUIRE(failure_frontend.debrief()->outcome == ac6::MissionOutcome::Failure);
  REQUIRE(failure_frontend.return_to_campaign());
  ac6::InputMappingDatabase mission_input;
  REQUIRE(mission_input.add({0x0001u, ac6::EventType::Pause}));
  REQUIRE(mission_input.add({0x0002u, ac6::EventType::Resume}));
  ac6::MissionExecution button_execution(*selected_definition, &assets, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, &mission_input);
  REQUIRE(button_execution.launch(*launch));
  REQUIRE(button_execution.tick(1.0f / 60.0f, {}).tick == 1);
  const auto paused_by_button = button_execution.tick(1.0f / 60.0f, {32767, 0, 0, 255, 1});
  REQUIRE(button_execution.scenario().state() == ac6::ScenarioState::Paused);
  REQUIRE(paused_by_button.tick == 1 && paused_by_button.position_x == 0.0f);
  const auto resumed_by_button = button_execution.tick(1.0f / 60.0f, {0, 0, 0, 0, 2});
  REQUIRE(button_execution.scenario().state() == ac6::ScenarioState::Gameplay);
  REQUIRE(resumed_by_button.tick == 2);
  ac6::ReplayLog mission_replay;
  mission_replay.append({0, 0, 0, 0, 0});
  mission_replay.append({32767, 0, 0, 255, 1});
  mission_replay.append({0, 0, 0, 0, 2});
  ac6::MissionExecution replay_execution_a(*selected_definition, &assets, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, &mission_input);
  ac6::MissionExecution replay_execution_b(*selected_definition, &assets, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, &mission_input);
  REQUIRE(replay_execution_a.launch(*launch) && replay_execution_b.launch(*launch));
  const auto mission_replay_frame_a = replay_execution_a.run_replay(1.0f / 60.0f, mission_replay);
  const auto mission_replay_frame_b = replay_execution_b.run_replay(1.0f / 60.0f, mission_replay);
  REQUIRE(mission_replay_frame_a.tick == mission_replay_frame_b.tick &&
          mission_replay_frame_a.position_x == mission_replay_frame_b.position_x &&
          mission_replay_frame_a.position_z == mission_replay_frame_b.position_z &&
          replay_execution_a.scenario().state() == ac6::ScenarioState::Gameplay &&
          replay_execution_b.scenario().state() == ac6::ScenarioState::Gameplay);
  ac6::MissionExecution execution(*selected_definition, &assets);
  REQUIRE(!execution.launched());
  REQUIRE(execution.launch(*launch));
  REQUIRE(execution.launched());
  const auto execution_frame = execution.tick(1.0f / 60.0f, {});
  REQUIRE(execution_frame.mission_ready && execution_frame.active_units == 2 &&
          execution_frame.player_entity == 0x1001);
  ac6::MissionExecution lower_axis_execution(*selected_definition, &assets);
  REQUIRE(lower_axis_execution.launch(*launch));
  const auto lower_axis_frame = lower_axis_execution.tick(
      1.0f / 60.0f, {-32768, -32768, -32768, 0, 0});
  REQUIRE(lower_axis_frame.pitch == -1.0f / 60.0f &&
          lower_axis_frame.roll == -1.0f / 60.0f &&
          lower_axis_frame.yaw == -1.0f / 60.0f);
  ac6::MissionExecution upper_axis_execution(*selected_definition, &assets);
  REQUIRE(upper_axis_execution.launch(*launch));
  const auto upper_axis_frame = upper_axis_execution.tick(
      1.0f / 60.0f, {32767, 32767, 32767, 0, 0});
  REQUIRE(upper_axis_frame.pitch == 1.0f / 60.0f &&
          upper_axis_frame.roll == 1.0f / 60.0f &&
          upper_axis_frame.yaw == 1.0f / 60.0f);
  ac6::MissionWaveDirector waves;
  REQUIRE(waves.add({1, 2, {5000, 2, 119, false},
                     {5000, 2, {25.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}}));
  REQUIRE(!waves.add({1, 2, {5000, 2, 119, false},
                      {5000, 2, {25.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}}));
  ac6::MissionExecution wave_execution(*selected_definition, &assets, nullptr, nullptr,
                                        nullptr, &waves);
  REQUIRE(wave_execution.launch(*launch));
  REQUIRE(waves.pending(1) == 1 && waves.spawned(1) == 0);
  REQUIRE(wave_execution.tick(1.0f / 60.0f, {}).active_units == 2);
  REQUIRE(waves.pending(1) == 1);
  REQUIRE(wave_execution.tick(1.0f / 60.0f, {}).active_units == 3);
  REQUIRE(waves.pending(1) == 0 && waves.spawned(1) == 1 &&
          wave_execution.combat().unit(5000) != nullptr);
  ac6::MissionExecution::Checkpoint wave_checkpoint;
  REQUIRE(wave_execution.save_checkpoint(wave_checkpoint));
  REQUIRE(wave_checkpoint.unit_records.size() == 3 &&
          wave_checkpoint.waves.entries.size() == 1 &&
          wave_checkpoint.waves.entries.front().published);
  REQUIRE(waves.despawn(5000, wave_execution.units(), wave_execution.combat()));
  REQUIRE(wave_execution.combat().active_units() == 2);
  REQUIRE(wave_execution.restore_checkpoint(wave_checkpoint));
  REQUIRE(wave_execution.units().active_count() == 3 &&
          wave_execution.combat().active_units() == 3 &&
          waves.spawned(1) == 1 && waves.pending(1) == 0);
  ac6::CombatWorld combat;
  REQUIRE(combat.add_unit({1, 1, {0.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}));
  REQUIRE(combat.add_unit({2, 2, {10.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}));
  REQUIRE(!combat.add_unit({2, 2, {10.0f, 0.0f, 0.0f}, 100.0f, 100.0f, 1.0f, true}));
  REQUIRE(combat.add_weapon({7, 60.0f, 20.0f, 0.25f, 100.0f}));
  REQUIRE(!combat.add_weapon({7, 10.0f, 20.0f, 0.0f, 100.0f}));
  REQUIRE(combat.lock_target(1, 2) && combat.locked_target(1) == 2);
  REQUIRE(combat.fire(1, 7) && combat.active_projectiles() == 1);
  REQUIRE(!combat.fire(1, 7));
  combat.tick(0.25f);
  REQUIRE(combat.unit(2) && combat.unit(2)->health == 100.0f);
  combat.tick(0.25f);
  REQUIRE(combat.unit(2) && combat.unit(2)->health == 40.0f &&
          combat.damage_events() == 1);
  REQUIRE(combat.fire(1, 7));
  combat.tick(0.25f);
  combat.tick(0.25f);
  REQUIRE(combat.unit(2) && !combat.unit(2)->active && combat.active_units() == 1);
  const ac6::MissionLaunchDefinition weapon_launch{
      1, 4097, {{4097, 1, 9, false}, {4098, 2, 119, false}},
      {{7, 60.0f, 20.0f, 0.25f, 100.0f}}};
  ac6::MissionExecution weapon_execution(*selected_definition, &assets);
  REQUIRE(weapon_execution.launch(weapon_launch));
  REQUIRE(weapon_execution.lock_target(4098));
  REQUIRE(weapon_execution.fire_weapon(7));
  for (int step = 0; step < 4; ++step) weapon_execution.tick(0.25f, {});
  REQUIRE(weapon_execution.combat().unit(4098) != nullptr &&
          weapon_execution.combat().unit(4098)->health == 40.0f);
  ac6::MissionExecution::Checkpoint weapon_checkpoint;
  REQUIRE(weapon_execution.save_checkpoint(weapon_checkpoint));
  REQUIRE(weapon_execution.combat().apply_damage(4098, 30.0f));
  REQUIRE(weapon_execution.restore_checkpoint(weapon_checkpoint));
  REQUIRE(weapon_execution.combat().unit(4098) != nullptr &&
          weapon_execution.combat().unit(4098)->health == 40.0f);
  REQUIRE(weapon_execution.lock_target(4098) && weapon_execution.fire_weapon(7));
  ac6::WorldFrame weapon_final_frame{};
  for (int step = 0; step < 4; ++step) weapon_final_frame = weapon_execution.tick(0.25f, {});
  REQUIRE(weapon_execution.combat().unit(4098) != nullptr &&
          weapon_execution.combat().unit(4098)->health == 0.0f &&
          weapon_final_frame.active_units == 1 && weapon_execution.units().active_count() == 1);
  ac6::MissionAiDirector ai;
  REQUIRE(ai.add({1, 1, 1, 4097, 4098, 7}));
  REQUIRE(!ai.add({1, 1, 1, 4097, 4098, 7}));
  ac6::MissionExecution ai_execution(*selected_definition, &assets, nullptr, nullptr,
                                     nullptr, nullptr, nullptr, nullptr, &ai);
  REQUIRE(ai_execution.launch(weapon_launch));
  REQUIRE(ai_execution.tick(1.0f / 60.0f, {}).tick == 1);
  REQUIRE(ai.active(1, 1) == 1 && ai_execution.combat().active_projectiles() == 1);
  for (int step = 0; step < 4; ++step) ai_execution.tick(0.25f, {});
  REQUIRE(ai_execution.combat().unit(4098) != nullptr &&
          ai_execution.combat().unit(4098)->health == 40.0f);
  REQUIRE(ai_execution.combat().deactivate_unit(4098));
  REQUIRE(ai_execution.tick(1.0f / 60.0f, {}).mission_ready);
  ac6::MissionExecution checkpoint_execution(*selected_definition, &assets,
                                             &loaded_objectives);
  REQUIRE(checkpoint_execution.launch(weapon_launch));
  REQUIRE(checkpoint_execution.activate_objective(1));
  REQUIRE(checkpoint_execution.tick(1.0f / 60.0f, {}).tick == 1);
  ac6::MissionExecution::Checkpoint mission_checkpoint;
  REQUIRE(checkpoint_execution.save_checkpoint(mission_checkpoint));
  REQUIRE(mission_checkpoint.scenario.state == ac6::ScenarioState::Gameplay &&
          mission_checkpoint.scenario.objectives.size() == 2 &&
          mission_checkpoint.combat_units.size() == 2);
  REQUIRE(checkpoint_execution.dispatch({ac6::EventType::Pause, 0}));
  REQUIRE(checkpoint_execution.restore_checkpoint(mission_checkpoint));
  REQUIRE(checkpoint_execution.scenario().state() == ac6::ScenarioState::Gameplay &&
          checkpoint_execution.scenario().objectives().find(1)->state ==
              ac6::ObjectiveState::Active &&
          checkpoint_execution.snapshot() == mission_checkpoint.flight);
  const auto before_bad_checkpoint = checkpoint_execution.snapshot();
  auto invalid_state_checkpoint = mission_checkpoint;
  invalid_state_checkpoint.scenario.state = static_cast<ac6::ScenarioState>(255);
  REQUIRE(!checkpoint_execution.restore_checkpoint(invalid_state_checkpoint));
  REQUIRE(checkpoint_execution.snapshot() == before_bad_checkpoint);
  ac6::CampaignProgression mission_campaign;
  REQUIRE(mission_campaign.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(mission_campaign.finalize());
  REQUIRE(mission_campaign.enter_briefing(1));
  REQUIRE(mission_campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(mission_campaign.begin(1));
  ac6::MissionExecution campaign_execution(*selected_definition, &assets, &loaded_objectives,
                                            nullptr, &mission_campaign);
  REQUIRE(campaign_execution.launch(*launch));
  REQUIRE(campaign_execution.activate_objective(1));
  REQUIRE(campaign_execution.complete_objective(1));
  REQUIRE(campaign_execution.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(mission_campaign.status(1)->state == ac6::CampaignMissionState::Completed);
  const ac6::MissionDebrief success_debrief = campaign_execution.debrief();
  REQUIRE(success_debrief.outcome == ac6::MissionOutcome::Success &&
          success_debrief.objective_count == 2 &&
          success_debrief.completed_objectives == 1 &&
          success_debrief.failed_objectives == 0);
  ac6::CampaignProgression session_campaign;
  REQUIRE(session_campaign.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(session_campaign.finalize());
  REQUIRE(session_campaign.enter_briefing(1));
  REQUIRE(session_campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(session_campaign.begin(1));
  ac6::MissionExecution session_execution(*selected_definition, &assets, &loaded_objectives,
                                           nullptr, &session_campaign);
  REQUIRE(session_execution.launch(weapon_launch));
  REQUIRE(session_execution.activate_objective(1));
  REQUIRE(session_execution.complete_objective(1));
  REQUIRE(session_execution.tick(1.0f / 60.0f, {}).tick == 1);
  ac6::MissionExecution::Checkpoint session_checkpoint;
  REQUIRE(session_execution.save_checkpoint(session_checkpoint));
  ac6::SessionSaveSnapshot mission_session{
      1, session_checkpoint.flight, session_campaign.snapshot(), session_checkpoint};
  ac6::SessionSaveStore mission_sessions;
  REQUIRE(mission_sessions.save(11, mission_session));
  const char* mission_session_path = "ac6-test-mission1-session.ac6sess";
  REQUIRE(mission_sessions.write_file(mission_session_path));
  ac6::SessionSaveStore loaded_mission_sessions;
  REQUIRE(loaded_mission_sessions.read_file(mission_session_path));
  REQUIRE(loaded_mission_sessions.load(11) != nullptr &&
          *loaded_mission_sessions.load(11) == mission_session);
  ac6::CampaignProgression restored_session_campaign;
  REQUIRE(restored_session_campaign.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(restored_session_campaign.finalize());
  REQUIRE(restored_session_campaign.restore(loaded_mission_sessions.load(11)->campaign));
  REQUIRE(restored_session_campaign.status(1)->state == ac6::CampaignMissionState::Active &&
          restored_session_campaign.status(1)->objective_mask == 1u);
  ac6::MissionExecution reloaded_session_execution(*selected_definition, &assets,
                                                    &loaded_objectives, nullptr,
                                                    &restored_session_campaign);
  REQUIRE(reloaded_session_execution.launch(weapon_launch));
  REQUIRE(loaded_mission_sessions.load(11)->checkpoint.has_value());
  auto stale_resource_checkpoint = *loaded_mission_sessions.load(11)->checkpoint;
  REQUIRE(!stale_resource_checkpoint.resource_identities.empty());
  stale_resource_checkpoint.resource_identities.front().relative_path += ".stale";
  REQUIRE(!reloaded_session_execution.restore_checkpoint(stale_resource_checkpoint));
  REQUIRE(reloaded_session_execution.restore_checkpoint(
      *loaded_mission_sessions.load(11)->checkpoint));
  REQUIRE(reloaded_session_execution.scenario().state() == ac6::ScenarioState::Gameplay &&
          reloaded_session_execution.scenario().objectives().find(1)->state ==
              ac6::ObjectiveState::Complete);
  REQUIRE(reloaded_session_execution.lock_target(4098) &&
          reloaded_session_execution.fire_weapon(7));
  for (int step = 0; step < 4; ++step) reloaded_session_execution.tick(0.25f, {});
  REQUIRE(reloaded_session_execution.combat().unit(4098) != nullptr &&
          reloaded_session_execution.combat().unit(4098)->health == 40.0f);
  auto invalid_player_checkpoint = session_checkpoint;
  invalid_player_checkpoint.scenario.player = 9999;
  REQUIRE(!checkpoint_execution.restore_checkpoint(invalid_player_checkpoint));
  auto invalid_player_session = mission_session;
  invalid_player_session.checkpoint->scenario.player = 9999;
  REQUIRE(!mission_sessions.save(12, invalid_player_session));
  std::remove(mission_session_path);
  ac6::CampaignProgression failed_campaign;
  REQUIRE(failed_campaign.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(failed_campaign.finalize());
  REQUIRE(failed_campaign.enter_briefing(1));
  REQUIRE(failed_campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(failed_campaign.begin(1));
  ac6::MissionExecution campaign_failure(*selected_definition, &assets, &loaded_objectives,
                                         nullptr, &failed_campaign);
  REQUIRE(campaign_failure.launch(*launch));
  REQUIRE(campaign_failure.activate_objective(1));
  REQUIRE(campaign_failure.fail_objective(1));
  REQUIRE(failed_campaign.status(1)->state == ac6::CampaignMissionState::Failed);
  const ac6::MissionDebrief failure_debrief = campaign_failure.debrief();
  REQUIRE(failure_debrief.outcome == ac6::MissionOutcome::Failure &&
          failure_debrief.failed_objectives == 1);
  ac6::CampaignProgression destroyed_campaign;
  REQUIRE(destroyed_campaign.add({1, {1, 9, 9}, 1, {}}));
  REQUIRE(destroyed_campaign.finalize());
  REQUIRE(destroyed_campaign.enter_briefing(1));
  REQUIRE(destroyed_campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(destroyed_campaign.begin(1));
  ac6::MissionExecution destroyed_execution(*selected_definition, &assets, nullptr, nullptr,
                                             &destroyed_campaign);
  REQUIRE(destroyed_execution.launch(*launch));
  REQUIRE(destroyed_execution.combat().apply_damage(4097, 100.0f));
  const auto destroyed_frame = destroyed_execution.tick(1.0f / 60.0f, {});
  REQUIRE(!destroyed_frame.mission_ready &&
          destroyed_execution.scenario().state() == ac6::ScenarioState::Aborted &&
          destroyed_campaign.status(1)->state == ac6::CampaignMissionState::Failed &&
          destroyed_execution.debrief().outcome == ac6::MissionOutcome::Failure);
  REQUIRE(!destroyed_execution.dispatch({ac6::EventType::Abort, 0}));
  ac6::MissionExecution expiration_execution(*selected_definition, &assets);
  REQUIRE(expiration_execution.launch(*launch));
  expiration_execution.set_failure_tick(2);
  REQUIRE(expiration_execution.tick(1.0f / 60.0f, {}).tick == 1);
  REQUIRE(!expiration_execution.tick(1.0f / 60.0f, {}).mission_ready &&
          expiration_execution.scenario().state() == ac6::ScenarioState::Aborted &&
          expiration_execution.debrief().outcome == ac6::MissionOutcome::Failure);
  ac6::MissionExecution objective_execution(*selected_definition, &assets, &loaded_objectives);
  REQUIRE(objective_execution.launch(*launch));
  REQUIRE(!objective_execution.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(objective_execution.scenario().objectives().find(1) != nullptr);
  REQUIRE(objective_execution.scenario().objectives().find(1)->state == ac6::ObjectiveState::Pending);
  REQUIRE(objective_execution.dispatch({ac6::EventType::Pause, 0}));
  REQUIRE(objective_execution.dispatch({ac6::EventType::Resume, 0}));
  REQUIRE(objective_execution.activate_objective(1));
  REQUIRE(objective_execution.complete_objective(1));
  REQUIRE(objective_execution.dispatch({ac6::EventType::Complete, 0}));
  ac6::CampaignProgression noncontiguous_campaign;
  REQUIRE(noncontiguous_campaign.add({1, {1, 9, 9}, 2, {}}));
  REQUIRE(noncontiguous_campaign.finalize());
  REQUIRE(noncontiguous_campaign.enter_briefing(1));
  REQUIRE(noncontiguous_campaign.set_loadout(1, {7, 8, true}));
  REQUIRE(noncontiguous_campaign.begin(1));
  ac6::MissionObjectiveDatabase noncontiguous_objectives;
  const char* noncontiguous_manifest = "ac6-test-noncontiguous-objectives.tsv";
  { std::ofstream out(noncontiguous_manifest);
    out << "1\t10\tprimary\t1\n" << "1\t20\tsecondary\t1\n"; }
  REQUIRE(noncontiguous_objectives.load_manifest(noncontiguous_manifest));
  std::remove(noncontiguous_manifest);
  ac6::MissionExecution noncontiguous_execution(*selected_definition, &assets,
                                                 &noncontiguous_objectives, nullptr,
                                                 &noncontiguous_campaign);
  REQUIRE(noncontiguous_execution.launch(*launch));
  REQUIRE(noncontiguous_execution.activate_objective(10));
  REQUIRE(noncontiguous_execution.complete_objective(10));
  REQUIRE(noncontiguous_execution.activate_objective(20));
  REQUIRE(noncontiguous_execution.complete_objective(20));
  REQUIRE(noncontiguous_execution.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(noncontiguous_campaign.status(1)->state == ac6::CampaignMissionState::Completed);
  ac6::MissionExecution failed_execution(*selected_definition, &assets, &loaded_objectives);
  REQUIRE(failed_execution.launch(*launch));
  REQUIRE(failed_execution.activate_objective(1));
  const auto failed_before = failed_execution.tick(1.0f / 60.0f,
                                                    {3200, 0, 0, 255, 0});
  REQUIRE(failed_before.mission_ready && failed_before.tick == 1);
  REQUIRE(failed_execution.fail_objective(1));
  REQUIRE(failed_execution.scenario().state() == ac6::ScenarioState::Aborted);
  const auto failed_after = failed_execution.tick(1.0f / 60.0f,
                                                   {32767, 32767, 32767, 255, 0});
  REQUIRE(failed_after.tick == failed_before.tick &&
          failed_after.position_x == failed_before.position_x &&
          failed_after.position_y == failed_before.position_y &&
          failed_after.position_z == failed_before.position_z &&
          !failed_execution.dispatch({ac6::EventType::Complete, 0}));
  REQUIRE(execution.dispatch({ac6::EventType::Pause, 0}));
  const auto paused_execution_frame = execution.tick(1.0f / 60.0f, {3200, 0, 0, 255, 0});
  REQUIRE(!paused_execution_frame.mission_ready &&
          paused_execution_frame.tick == execution_frame.tick &&
          paused_execution_frame.position_x == execution_frame.position_x &&
          paused_execution_frame.position_z == execution_frame.position_z);
  REQUIRE(execution.dispatch({ac6::EventType::Resume, 0}));
  const auto resumed_execution_frame = execution.tick(1.0f / 60.0f, {});
  REQUIRE(resumed_execution_frame.mission_ready && resumed_execution_frame.tick == execution_frame.tick + 1);
  const auto execution_checkpoint = execution.snapshot();
  execution.tick(1.0f / 60.0f, {1200, -400, 900, 32, 0});
  REQUIRE(execution.restore(execution_checkpoint));
  REQUIRE(execution.snapshot().tick == execution_checkpoint.tick &&
          execution.snapshot().position_x == execution_checkpoint.position_x);
  ac6::MissionExecution resumed_execution(*selected_definition, &assets);
  REQUIRE(resumed_execution.launch(*launch));
  REQUIRE(resumed_execution.restore(execution_checkpoint));
  const auto resumed_a = execution.tick(1.0f / 60.0f, {1200, -400, 900, 32, 0});
  const auto resumed_b = resumed_execution.tick(1.0f / 60.0f, {1200, -400, 900, 32, 0});
  REQUIRE(resumed_a.tick == resumed_b.tick && resumed_a.position_x == resumed_b.position_x &&
          resumed_a.position_y == resumed_b.position_y && resumed_a.position_z == resumed_b.position_z);
  const ac6::MissionDefinition strike_definition{2, ac6::MissionFamily::Strike, {119}};
  const ac6::MissionLaunchDefinition strike_launch{
      2, 0x2001, {{0x2001, 2, 119, false}, {0x2002, 2, 119, false}}};
  ac6::MissionExecution strike_execution(strike_definition, &assets);
  REQUIRE(strike_execution.launch(strike_launch));
  const auto strike_frame = strike_execution.tick(1.0f / 60.0f, {});
  REQUIRE(strike_frame.mission_id == 2 && strike_frame.mission_ready &&
          strike_frame.player_entity == 0x2001 && strike_frame.active_units == 2);
  runtime.set_scenario(&runtime_scenario);
  const auto gated = runtime.tick(1.0f / 60.0f, {});
  REQUIRE(!gated.mission_ready);
  REQUIRE(runtime_scenario.dispatch({ac6::EventType::StartMission, 0x1001}));
  const auto second = runtime.tick(1.0f / 60.0f, {});
  REQUIRE(first.tick == 1);
  REQUIRE(second.mission_id == 1);
  REQUIRE(second.tick == 3);
  REQUIRE(second.mission_ready);
  REQUIRE(second.active_units == 2 && second.player_entity == 0x1001);
  REQUIRE(second.camera_x == second.position_x - 12.0f);
  REQUIRE(second.camera_y == second.position_y + 3.0f);
  REQUIRE(second.camera_target_z == second.position_z);

  const char* render_manifest = "ac6-test-render.tsv";
  {
    std::ofstream out(render_manifest);
    out << "# mission_id required_render_asset_ids\n";
    out << "1\t9,119\n";
    out << "2\t119\n";
  }
  ac6::MissionRenderDatabase render_definitions;
  REQUIRE(render_definitions.load_manifest(render_manifest));
  const ac6::MissionRenderDefinition* render_definition = render_definitions.find(1);
  REQUIRE(render_definition && render_definition->asset_ids.size() == 2);
  std::remove(render_manifest);

  const char* camera_manifest = "ac6-test-camera.tsv";
  {
    std::ofstream out(camera_manifest);
    out << "# mission_id c218-c221\n";
    out << "1\t1\t0\t0\t0\t0\t1\t0\t0\t0\t0\t1\t0\t0\t0\t0\t1\tqualified\tcolumn_major\n";
    out << "2\t1\t0\t0\t0\t0\t1\t0\t0\t0\t0\t1\t0\t0\t0\t0\t1\n";
  }
  ac6::MissionCameraDatabase cameras;
  REQUIRE(cameras.load_manifest(camera_manifest));
  REQUIRE(cameras.find(1) && cameras.find(1)->column_major);
  REQUIRE(cameras.find(1)->qualified);
  REQUIRE(cameras.find(2) && !cameras.find(2)->column_major);
  std::remove(camera_manifest);
  const char* bad_camera_manifest = "ac6-test-bad-camera.tsv";
  { std::ofstream out(bad_camera_manifest);
    out << "1\t1\t0\t0\t0\t0\t1\t0\t0\t0\t0\t1\t0\t0\t0\t0\t1\ttranspose\n";
  }
  ac6::MissionCameraDatabase bad_cameras;
  REQUIRE(!bad_cameras.load_manifest(bad_camera_manifest));
  std::remove(bad_camera_manifest);

  const char* bad_render_manifest = "ac6-test-bad-render.tsv";
  { std::ofstream out(bad_render_manifest); out << "1\t9,9\n"; }
  ac6::MissionRenderDatabase bad_render_definitions;
  REQUIRE(!bad_render_definitions.load_manifest(bad_render_manifest));
  std::remove(bad_render_manifest);

  const char* drawable_manifest = "ac6-test-drawables.tsv";
  {
    std::ofstream out(drawable_manifest);
    out << "# mission_id stable_id kind asset primitive_count buffer_id vertex_count index_count content_hash\n";
    out << "1\tmapobj_m01_l_brg1_n\tmapobj\t9\t3\tGIDX268439850\t1024\t1536\tcycle760-brg1n\n";
    out << "1\tmapparts_m01_l_034_010\tterrain\t119\t5\t021/010_NDXR\t259\t41600\t7209F3DEB7BD097D\n";
    out << "1\tentry119_022_skycloud\tsky_cloud\t119\t7\tentry119/022_FHM/005_FHM\t8\t2048\tcycle774-skycloud\n";
    out << "2\tmission2_placeholder\tterrain\t119\t1\tmission2/placeholder\t1\t3\tplaceholder\n";
  }
  ac6::MissionDrawableDatabase drawables;
  REQUIRE(drawables.load_manifest(drawable_manifest));
  REQUIRE(drawables.find(1, "mapobj_m01_l_brg1_n") &&
          drawables.find(1, "mapobj_m01_l_brg1_n")->primitive_count == 3);
  REQUIRE(drawables.find(1, "mapparts_m01_l_034_010") &&
          drawables.find(1, "mapparts_m01_l_034_010")->kind == "terrain");
  REQUIRE(drawables.find(1, "mapparts_m01_l_034_010")->has_buffer_contract());
  REQUIRE(drawables.find(1, "mapparts_m01_l_034_010")->index_count == 41600);
  REQUIRE(drawables.find_by_asset(1, 9).size() == 1);
  REQUIRE(drawables.find_by_asset(1, 119).size() == 2);
  REQUIRE(drawables.find_by_asset(1, 210).empty());
  std::remove(drawable_manifest);
  const char* bad_drawable_manifest = "ac6-test-bad-drawables.tsv";
  {
    std::ofstream out(bad_drawable_manifest);
    out << "1\tmapobj_m01_l_brg1_n\tmapobj\t9\t3\tGIDX268439850\t1024\t1536\tcycle760-brg1n\n";
    out << "1\tmapobj_m01_l_brg1_n\tmapobj\t9\t4\tGIDX268439851\t1024\t1536\tduplicate\n";
  }
  ac6::MissionDrawableDatabase bad_drawables;
  REQUIRE(!bad_drawables.load_manifest(bad_drawable_manifest));
  std::remove(bad_drawable_manifest);

  const char* transform_manifest = "ac6-test-transforms.tsv";
  {
    std::ofstream out(transform_manifest);
    out << "# mission_id stable_id tx ty tz sx sy sz\n";
    out << "1\tmapobj_m01_l_brg1_n\t10\t0\t-20\t1\t1\t1\n";
    out << "1\tmapparts_m01_l_034_010\t100\t5\t-200\t2\t1\t3\n";
    out << "1\tentry119_022_skycloud\t20\t0\t-75\t4\t2\t4\n";
  }
  ac6::MissionTransformDatabase transforms;
  REQUIRE(transforms.load_manifest(transform_manifest));
  REQUIRE(transforms.find(1, "mapparts_m01_l_034_010"));
  REQUIRE(transforms.find(1, "mapparts_m01_l_034_010")->translate_x == 100.0f);
  REQUIRE(transforms.find(1, "mapparts_m01_l_034_010")->scale_z == 3.0f);
  std::remove(transform_manifest);
  const char* bad_transform_manifest = "ac6-test-bad-transforms.tsv";
  {
    std::ofstream out(bad_transform_manifest);
    out << "1\tmapparts_m01_l_034_010\t0\t0\t0\t1\t1\t0\n";
  }
  ac6::MissionTransformDatabase bad_transforms;
  REQUIRE(!bad_transforms.load_manifest(bad_transform_manifest));
  std::remove(bad_transform_manifest);

  const char* material_manifest = "ac6-test-materials.tsv";
  {
    std::ofstream out(material_manifest);
    out << "# mission_id stable_id shader_permutation depth_test depth_write blend_mode base_color\n";
    out << "1\tmapobj_m01_l_brg1_n\tD5B4.mapobj\t1\t1\topaque\t0xFF805040\n";
    out << "1\tmapparts_m01_l_034_010\tD5B4.terrain\t1\t1\topaque\t0xFF4060A0\n";
    out << "1\tentry119_022_skycloud\tD5B4.skycloud\t0\t0\talpha\t0xAAE0F0FF\n";
  }
  ac6::MissionMaterialDatabase materials;
  REQUIRE(materials.load_manifest(material_manifest));
  REQUIRE(materials.find(1, "mapparts_m01_l_034_010"));
  REQUIRE(materials.find(1, "mapparts_m01_l_034_010")->depth_test);
  REQUIRE(materials.find(1, "entry119_022_skycloud")->blend_mode == "alpha");
  std::remove(material_manifest);
  const char* bad_material_manifest = "ac6-test-bad-materials.tsv";
  {
    std::ofstream out(bad_material_manifest);
    out << "1\tmapparts_m01_l_034_010\tD5B4.terrain\t1\t1\tunknown\t0xFF4060A0\n";
  }
  ac6::MissionMaterialDatabase bad_materials;
  REQUIRE(!bad_materials.load_manifest(bad_material_manifest));
  std::remove(bad_material_manifest);

  const char* shader_manifest = "ac6-test-shaders.tsv";
  {
    std::ofstream out(shader_manifest);
    out << "# shader_permutation vertex_layout texture_fetches constant_count render_target_format\n";
    out << "D5B4.mapobj\tpos_norm_uv\t1\t8\trgba8\n";
    out << "D5B4.terrain\tpos_norm_uv\t1\t12\trgba8\n";
    out << "D5B4.skycloud\tpos_uv_color\t1\t6\trgba8\n";
  }
  ac6::ShaderPermutationDatabase shaders;
  REQUIRE(shaders.load_manifest(shader_manifest));
  REQUIRE(shaders.find("D5B4.terrain"));
  REQUIRE(shaders.find("D5B4.terrain")->constant_count == 12);
  std::remove(shader_manifest);
  const char* bad_shader_manifest = "ac6-test-bad-shaders.tsv";
  {
    std::ofstream out(bad_shader_manifest);
    out << "D5B4.terrain\tpos_norm_uv\t1\t12\tunknown\n";
  }
  ac6::ShaderPermutationDatabase bad_shaders;
  REQUIRE(!bad_shaders.load_manifest(bad_shader_manifest));
  std::remove(bad_shader_manifest);

  const char* render_target_manifest = "ac6-test-render-targets.tsv";
  {
    std::ofstream out(render_target_manifest);
    out << "# mission_id target_id width height sample_count color_format depth_format depth_enabled\n";
    out << "1\tworld_color\t64\t32\t4\trgba8\td24s8\t1\n";
    out << "1\tpresent\t64\t32\t1\trgba8\tnone\t0\n";
  }
  ac6::MissionRenderTargetDatabase render_targets;
  REQUIRE(render_targets.load_manifest(render_target_manifest));
  REQUIRE(render_targets.find(1, "world_color"));
  REQUIRE(render_targets.find(1, "world_color")->width == 64);
  REQUIRE(render_targets.find(1, "world_color")->sample_count == 4);
  REQUIRE(render_targets.find(1, "world_color")->depth_enabled);
  REQUIRE(render_targets.find(1, "present"));
  REQUIRE(render_targets.find(1, "present")->sample_count == 1);
  std::remove(render_target_manifest);
  const char* bad_render_target_manifest = "ac6-test-bad-render-targets.tsv";
  {
    std::ofstream out(bad_render_target_manifest);
    out << "1\tworld_color\t64\t32\t3\trgba8\td24s8\t1\n";
  }
  ac6::MissionRenderTargetDatabase bad_render_targets;
  REQUIRE(!bad_render_targets.load_manifest(bad_render_target_manifest));
  std::remove(bad_render_target_manifest);

  const char* render_pass_manifest = "ac6-test-render-passes.tsv";
  {
    std::ofstream out(render_pass_manifest);
    out << "# mission_id pass_id order color_target depth_target clear_color clear_depth\n";
    out << "1\tworld\t1\tworld_color\tmain_depth\t0x00000000\t1\n";
  }
  ac6::MissionRenderPassDatabase render_passes;
  REQUIRE(render_passes.load_manifest(render_pass_manifest));
  REQUIRE(render_passes.find(1, "world"));
  REQUIRE(render_passes.find(1, "world")->order == 1);
  std::remove(render_pass_manifest);
  const char* bad_render_pass_manifest = "ac6-test-bad-render-passes.tsv";
  {
    std::ofstream out(bad_render_pass_manifest);
    out << "1\tworld\t1\tmain_color\tmain_depth\t0x00000000\t2\n";
  }
  ac6::MissionRenderPassDatabase bad_render_passes;
  REQUIRE(!bad_render_passes.load_manifest(bad_render_pass_manifest));
  std::remove(bad_render_pass_manifest);

  const char* render_resolve_manifest = "ac6-test-render-resolves.tsv";
  {
    std::ofstream out(render_resolve_manifest);
    out << "# mission_id source_pass source_target destination_target mode\n";
    out << "1\tworld\tworld_color\tpresent\tmsaa_resolve\n";
  }
  ac6::MissionRenderResolveDatabase render_resolves;
  REQUIRE(render_resolves.load_manifest(render_resolve_manifest));
  REQUIRE(render_resolves.find(1, "world"));
  REQUIRE(render_resolves.find(1, "world")->mode == "msaa_resolve");
  std::remove(render_resolve_manifest);
  const char* bad_render_resolve_manifest = "ac6-test-bad-render-resolves.tsv";
  {
    std::ofstream out(bad_render_resolve_manifest);
    out << "1\tworld\tmain_color\tbackbuffer\tcopy\n";
  }
  ac6::MissionRenderResolveDatabase bad_render_resolves;
  REQUIRE(!bad_render_resolves.load_manifest(bad_render_resolve_manifest));
  std::remove(bad_render_resolve_manifest);

  // Filled raster end-to-end contract.  The BE NDXR fixture exercises the
  // retail-style strip/restart decoder; the small variants below isolate
  // winding, restart, degeneracy, depth occlusion, and viewport clipping.
  const ac6::MissionRenderTargetDefinition& raster_color_target =
      *render_targets.find(1, "world_color");
  const ac6::MissionRenderTargetDefinition& raster_present_target =
      *render_targets.find(1, "present");
  const ac6::MissionRenderPass& raster_pass = *render_passes.find(1, "world");
  const ac6::MissionRenderResolve& raster_resolve = *render_resolves.find(1, "world");
  const ac6::ShaderPermutation& raster_shader = *shaders.find("D5B4.terrain");
  ac6::WorldFrame raster_frame = second;
  raster_frame.mission_id = 1;
  raster_frame.mission_ready = true;
  raster_frame.camera_x = 0.0f;
  raster_frame.camera_y = 0.0f;
  raster_frame.camera_z = 0.0f;
  raster_frame.camera_target_x = 0.0f;
  raster_frame.camera_target_y = 0.0f;
  raster_frame.camera_target_z = 1.0f;
  ac6::NativeRenderTarget raster_target;
  REQUIRE(raster_target.resize(64, 32));
  REQUIRE(raster_target.raster_mode() == ac6::NativeRasterMode::Filled);
  const auto draw_raster_fixture = [&](const RasterFixture& fixture,
                                       std::uint32_t ordinal = 0u) {
    const ac6::MissionDrawableTransform transform{
        1, fixture.drawable.stable_id, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    const ac6::MissionMaterial material{
        1, fixture.drawable.stable_id, "D5B4.terrain", true, true, "opaque", 0xFFFFFFFFu, 1};
    const ac6::MissionTextureBinding texture{
        1, fixture.drawable.stable_id, "fixture", "nearest", "wrap", 1};
    return raster_target.draw_world_geometry(
        raster_frame, fixture.drawable, fixture.geometry, fixture.decoded, transform,
        material, texture, raster_shader, raster_color_target, raster_present_target,
        raster_pass, raster_resolve, nullptr, nullptr, ordinal);
  };
  const char* raster_be_slice = "ac6-test-raster-be-strip.ndxr";
  const std::string raster_be_bytes = make_ndxr_be_strip_fixture();
  { std::ofstream out(raster_be_slice, std::ios::binary); out << raster_be_bytes; }
  ac6::QualifiedBufferDatabase raster_be_buffers;
  REQUIRE(raster_be_buffers.add({"raster-be-strip", raster_be_slice,
                                 raster_be_bytes.size(), fnv64(raster_be_bytes), false}));
  REQUIRE(raster_be_buffers.verify("raster-be-strip"));
  const ac6::MissionDrawable raster_be_drawable{
      1, "raster-be-strip", "terrain", 119, 1, "raster-be-strip", 4, 5,
      "raster-be-strip"};
  ac6::NativeGeometryDatabase raster_be_geometry;
  REQUIRE(raster_be_geometry.load_verified(raster_be_drawable, raster_be_buffers));
  REQUIRE(raster_be_geometry.find("raster-be-strip")->topology ==
          ac6::NativeIndexTopology::TriangleStripRestart);
  const RasterFixture strip_fixture{
      raster_be_drawable, *raster_be_geometry.find("raster-be-strip"),
      *raster_be_geometry.decoded("raster-be-strip")};
  std::remove(raster_be_slice);
  REQUIRE(raster_target.clear(0, 1.0f));
  REQUIRE(draw_raster_fixture(strip_fixture));
  const ac6::RenderReadback strip_readback = raster_target.readback();
  REQUIRE(strip_readback.color_coverage > 0);
  REQUIRE(strip_readback.depth_coverage > 0);
  REQUIRE(raster_target.diagnostic_point_writes() == 0);
  REQUIRE(raster_target.filled_fragment_writes() > 0);
  REQUIRE(raster_target.raster_metrics().size() == 1);
  const ac6::NativeRasterDrawableMetrics strip_metrics = raster_target.raster_metrics().front();
  REQUIRE(strip_metrics.stable_id == "raster-be-strip");
  REQUIRE(strip_metrics.projected_vertices == 4);
  REQUIRE(strip_metrics.restart_markers == 1);
  REQUIRE(strip_metrics.candidate_triangles == 2);
  REQUIRE(strip_metrics.degenerate_triangles == 0);
  REQUIRE(strip_metrics.nondegenerate_triangles == 2);
  REQUIRE(strip_metrics.fragment_tests > 0 && strip_metrics.inside_fragments > 0);
  REQUIRE(strip_metrics.depth_pass_fragments == strip_metrics.color_writes);
  REQUIRE(strip_metrics.screen_bbox_valid && strip_metrics.depth_min < 1.0f &&
          strip_metrics.depth_max > strip_metrics.depth_min);
  const std::uint64_t bbox_area =
      static_cast<std::uint64_t>(strip_metrics.bbox_max_x - strip_metrics.bbox_min_x + 1u) *
      static_cast<std::uint64_t>(strip_metrics.bbox_max_y - strip_metrics.bbox_min_y + 1u);
  REQUIRE(strip_readback.color_coverage == strip_metrics.unique_pixels);
  REQUIRE(strip_metrics.unique_pixels > bbox_area * 3u / 4u);
  std::vector<std::uint8_t> raster_pixels;
  REQUIRE(raster_target.copy_rgba8(raster_pixels));
  const std::size_t center_pixel = (8u * 64u + 32u) * 4u;
  REQUIRE(raster_pixels[center_pixel] != 0 || raster_pixels[center_pixel + 1u] != 0 ||
          raster_pixels[center_pixel + 2u] != 0 || raster_pixels[center_pixel + 3u] != 0);
  std::vector<std::uint32_t> raster_object_ids;
  REQUIRE(raster_target.copy_object_id(raster_object_ids));
  REQUIRE(raster_object_ids[8u * 64u + 32u] != 0);
  std::vector<float> raster_depth;
  REQUIRE(raster_target.copy_depth(raster_depth));
  REQUIRE(raster_depth[8u * 64u + 32u] < 1.0f);
  REQUIRE(raster_pixels[0] == 0 && raster_pixels[1] == 0 && raster_pixels[2] == 0);

  const auto triangle_vertices = [] (float z) {
    return std::vector<ac6::DecodedVertex>{{-0.8f, -0.8f, z, 0.0f, 0.0f},
                                           {0.0f, 0.8f, z, 0.5f, 1.0f},
                                           {0.8f, -0.8f, z, 1.0f, 0.0f}};
  };
  const RasterFixture cw_fixture = make_raster_fixture(
      "raster-cw", triangle_vertices(2.0f), {0, 1, 2}, ac6::NativeIndexTopology::TriangleList);
  const RasterFixture ccw_fixture = make_raster_fixture(
      "raster-ccw", triangle_vertices(2.0f), {0, 2, 1}, ac6::NativeIndexTopology::TriangleList);
  for (const RasterFixture* fixture : {&cw_fixture, &ccw_fixture}) {
    REQUIRE(raster_target.clear(0, 1.0f));
    REQUIRE(draw_raster_fixture(*fixture));
    REQUIRE(raster_target.filled_fragment_writes() > 0);
    REQUIRE(raster_target.raster_metrics().front().nondegenerate_triangles == 1);
    REQUIRE(raster_target.raster_metrics().front().inside_fragments > 0);
  }

  const RasterFixture restart_fixture = make_raster_fixture(
      "raster-restart",
      {{-0.8f, -0.8f, 2.0f, 0.0f, 0.0f}, {0.0f, 0.8f, 2.0f, 0.5f, 1.0f},
       {0.8f, -0.8f, 2.0f, 1.0f, 0.0f}, {-0.8f, 0.8f, 2.0f, 0.0f, 1.0f}},
      {0, 1, 2, std::numeric_limits<std::uint32_t>::max(), 0, 2, 3},
      ac6::NativeIndexTopology::TriangleStripRestart);
  REQUIRE(raster_target.clear(0, 1.0f));
  REQUIRE(draw_raster_fixture(restart_fixture));
  REQUIRE(raster_target.raster_metrics().front().restart_markers == 1);
  REQUIRE(raster_target.raster_metrics().front().candidate_triangles == 2);
  REQUIRE(raster_target.raster_metrics().front().inside_fragments > 0);

  const RasterFixture degenerate_fixture = make_raster_fixture(
      "raster-degenerate",
      {{-0.8f, 0.0f, 2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 2.0f, 0.5f, 0.0f},
       {0.8f, 0.0f, 2.0f, 1.0f, 0.0f}},
      {0, 1, 2}, ac6::NativeIndexTopology::TriangleList);
  REQUIRE(raster_target.clear(0, 1.0f));
  REQUIRE(draw_raster_fixture(degenerate_fixture));
  REQUIRE(raster_target.raster_metrics().front().degenerate_triangles == 1);
  REQUIRE(raster_target.raster_metrics().front().nondegenerate_triangles == 0);
  REQUIRE(raster_target.filled_fragment_writes() == 0);

  const RasterFixture occlusion_near = make_raster_fixture(
      "raster-occlusion-near", triangle_vertices(2.0f), {0, 1, 2},
      ac6::NativeIndexTopology::TriangleList);
  const RasterFixture occlusion_far = make_raster_fixture(
      "raster-occlusion-far", triangle_vertices(4.0f), {0, 1, 2},
      ac6::NativeIndexTopology::TriangleList);
  REQUIRE(raster_target.clear(0, 1.0f));
  REQUIRE(draw_raster_fixture(occlusion_near, 1));
  REQUIRE(draw_raster_fixture(occlusion_far, 2));
  REQUIRE(raster_target.raster_metrics().size() == 2);
  const ac6::NativeRasterDrawableMetrics occlusion_metrics = raster_target.raster_metrics().back();
  REQUIRE(occlusion_metrics.inside_fragments > 0);
  REQUIRE(occlusion_metrics.depth_pass_fragments < occlusion_metrics.inside_fragments);

  const RasterFixture partial_fixture = make_raster_fixture(
      "raster-partial",
      {{-5.0f, -0.5f, 2.0f, 0.0f, 0.0f}, {5.0f, -0.5f, 2.0f, 1.0f, 0.0f},
       {0.0f, 1.0f, 2.0f, 0.5f, 1.0f}},
      {0, 1, 2}, ac6::NativeIndexTopology::TriangleList);
  REQUIRE(raster_target.clear(0, 1.0f));
  REQUIRE(draw_raster_fixture(partial_fixture));
  REQUIRE(raster_target.raster_metrics().front().projected_vertices == 3);
  REQUIRE(raster_target.raster_metrics().front().nondegenerate_triangles > 0);
  REQUIRE(raster_target.raster_metrics().front().inside_fragments > 0);
  REQUIRE(raster_target.filled_fragment_writes() > 0);

  RasterFixture aircraft_anchor_fixture = make_raster_fixture(
      "aircraft-anchor",
      {{-0.25f, -0.25f, 2.0f, 0.0f, 0.0f}, {0.25f, -0.25f, 2.0f, 1.0f, 0.0f},
       {0.0f, 0.25f, 2.0f, 0.5f, 1.0f}},
      {0, 1, 2}, ac6::NativeIndexTopology::TriangleList);
  aircraft_anchor_fixture.drawable.kind = "aircraft";
  raster_frame.position_x = 0.0f;
  raster_frame.position_y = 0.0f;
  raster_frame.position_z = 0.0f;
  REQUIRE(raster_target.clear(0, 1.0f));
  REQUIRE(draw_raster_fixture(aircraft_anchor_fixture));
  const auto aircraft_origin_bbox = raster_target.raster_metrics().front();
  raster_frame.position_x = 0.5f;
  REQUIRE(raster_target.clear(0, 1.0f));
  REQUIRE(draw_raster_fixture(aircraft_anchor_fixture));
  const auto aircraft_anchored_bbox = raster_target.raster_metrics().front();
  REQUIRE(aircraft_origin_bbox.screen_bbox_valid && aircraft_anchored_bbox.screen_bbox_valid);
  REQUIRE(aircraft_anchored_bbox.bbox_min_x != aircraft_origin_bbox.bbox_min_x);

  const char* texture_manifest = "ac6-test-textures.tsv";
  {
    std::ofstream out(texture_manifest);
    out << "# mission_id stable_id texture_id sampler_filter sampler_address content_hash\n";
    out << "1\tmapobj_m01_l_brg1_n\tntxr.mapobj.diffuse\tlinear\twrap\t0x0102030405060708\n";
    out << "1\tmapparts_m01_l_034_010\tntxr.terrain.diffuse\tlinear\twrap\t0x1112131415161718\n";
    out << "1\tentry119_022_skycloud\tntxr.skycloud.diffuse\tlinear\tclamp\t0x2122232425262728\n";
  }
  ac6::MissionTextureDatabase textures;
  REQUIRE(textures.load_manifest(texture_manifest));
  REQUIRE(textures.find(1, "mapparts_m01_l_034_010"));
  REQUIRE(textures.find(1, "entry119_022_skycloud")->sampler_address == "clamp");
  std::remove(texture_manifest);
  const char* ppm_texture = "ac6-test-texture.ppm";
  const char* ppm_manifest = "ac6-test-texture-ppm.tsv";
  const std::string ppm_bytes = std::string("P6\n2 1\n255\n") +
                                std::string("\xFF\x00\x00\x00\x00\xFF", 6);
  { std::ofstream out(ppm_texture, std::ios::binary); out << ppm_bytes; }
  {
    std::ofstream out(ppm_manifest);
    out << "1\ttextured\tntxr.test\tnearest\twrap\t0x" << std::hex
        << fnv64(ppm_bytes) << std::dec << "\t" << ppm_texture << "\t"
        << ppm_bytes.size() << "\n";
    out << "1\tclamped\tntxr.test\tnearest\tclamp\t0x" << std::hex
        << fnv64(ppm_bytes) << std::dec << "\t" << ppm_texture << "\t"
        << ppm_bytes.size() << "\n";
    out << "1\tlinear\tntxr.test\tlinear\tclamp\t0x" << std::hex
        << fnv64(ppm_bytes) << std::dec << "\t" << ppm_texture << "\t"
        << ppm_bytes.size() << "\n";
  }
  ac6::MissionTextureDatabase ppm_textures;
  REQUIRE(ppm_textures.load_manifest(ppm_manifest));
  std::uint32_t sampled = 0;
  REQUIRE(ppm_textures.sample(1, "textured", 0.1f, 0.5f, sampled));
  REQUIRE(sampled == 0xFFFF0000u);
  REQUIRE(ppm_textures.sample(1, "textured", 0.9f, 0.5f, sampled));
  REQUIRE(sampled == 0xFF0000FFu);
  REQUIRE(ppm_textures.sample(1, "clamped", -0.1f, 0.5f, sampled));
  REQUIRE(sampled == 0xFFFF0000u);
  REQUIRE(ppm_textures.sample(1, "clamped", 1.1f, 0.5f, sampled));
  REQUIRE(sampled == 0xFF0000FFu);
  REQUIRE(ppm_textures.sample(1, "linear", 0.5f, 0.5f, sampled));
  REQUIRE(((sampled >> 16u) & 0xFFu) >= 127u && ((sampled >> 16u) & 0xFFu) <= 128u);
  REQUIRE(((sampled >> 8u) & 0xFFu) == 0u && (sampled & 0xFFu) >= 127u &&
          (sampled & 0xFFu) <= 128u);
  std::remove(ppm_manifest);
  std::remove(ppm_texture);
  const char* bad_texture_manifest = "ac6-test-bad-textures.tsv";
  {
    std::ofstream out(bad_texture_manifest);
    out << "1\tmapparts_m01_l_034_010\tntxr.terrain.diffuse\ttrilinear\twrap\t0x11121314\n";
  }
  ac6::MissionTextureDatabase bad_textures;
  REQUIRE(!bad_textures.load_manifest(bad_texture_manifest));
  std::remove(bad_texture_manifest);

  const char* mapobj_slice = "ac6-buffer-GIDX268439850.slice";
  const char* terrain_slice = "ac6-buffer-021-010_NDXR.slice";
  const char* sky_slice = "ac6-buffer-entry119-022.slice";
  const std::string mapobj_bytes = make_ndxr_fixture(1024, 1536, 3, 32, 2);
  const std::string terrain_bytes = make_ndxr_fixture(259, 41600, 5, 32, 2);
  const std::string sky_bytes = make_ndxr_fixture(8, 2048, 7, 32, 2);
  { std::ofstream out(mapobj_slice, std::ios::binary); out << mapobj_bytes; }
  { std::ofstream out(terrain_slice, std::ios::binary); out << terrain_bytes; }
  { std::ofstream out(sky_slice, std::ios::binary); out << sky_bytes; }
  const char* buffer_manifest = "ac6-test-buffers.tsv";
  {
    std::ofstream out(buffer_manifest);
    out << "GIDX268439850\t" << mapobj_slice << "\t" << mapobj_bytes.size()
        << "\t" << fnv64(mapobj_bytes) << "\n";
    out << "021/010_NDXR\t" << terrain_slice << "\t" << terrain_bytes.size()
        << "\t" << fnv64(terrain_bytes) << "\n";
    out << "entry119/022_FHM/005_FHM\t" << sky_slice << "\t" << sky_bytes.size()
        << "\t" << fnv64(sky_bytes) << "\n";
  }
  ac6::QualifiedBufferDatabase buffers;
  REQUIRE(buffers.load_manifest(buffer_manifest));
  REQUIRE(buffers.find("GIDX268439850") && !buffers.has_verified("GIDX268439850"));
  ac6::QualifiedBufferDatabase unverified_buffers;
  REQUIRE(unverified_buffers.load_manifest(buffer_manifest));
  REQUIRE(buffers.verify("GIDX268439850"));
  REQUIRE(buffers.verify("021/010_NDXR"));
  REQUIRE(buffers.verify("entry119/022_FHM/005_FHM"));
  REQUIRE(buffers.has_verified("entry119/022_FHM/005_FHM"));
  ac6::NativeGeometryDatabase geometries;
  REQUIRE(geometries.load_verified(*drawables.find(1, "mapobj_m01_l_brg1_n"), buffers));
  REQUIRE(geometries.load_verified(*drawables.find(1, "mapparts_m01_l_034_010"), buffers));
  REQUIRE(geometries.load_verified(*drawables.find(1, "entry119_022_skycloud"), buffers));
  REQUIRE(geometries.find("021/010_NDXR") &&
          geometries.find("021/010_NDXR")->index_count == 41600);
  REQUIRE(geometries.find("021/010_NDXR")->source_format == "NDXR");
  REQUIRE(geometries.find("021/010_NDXR")->topology == ac6::NativeIndexTopology::TriangleList);
  REQUIRE(geometries.find("021/010_NDXR")->vertex_section_count == 259);
  REQUIRE(geometries.find("021/010_NDXR")->polygon_descriptor_count == 5);
  REQUIRE(geometries.find("021/010_NDXR")->vertex_stride == 32);
  REQUIRE(geometries.find("021/010_NDXR")->index_size == 2);
  REQUIRE(geometries.find("021/010_NDXR")->vertex_byte_size == 259u * 32u);
  REQUIRE(geometries.find("021/010_NDXR")->index_byte_size == 41600u * 2u);
  REQUIRE(geometries.decoded("021/010_NDXR") &&
          geometries.decoded("021/010_NDXR")->vertices.size() == 259);
  REQUIRE(geometries.decoded("021/010_NDXR")->indices.size() == 41600);
  REQUIRE(geometries.decoded("021/010_NDXR")->vertices[1].x == 1.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->vertices[1].y == 2.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->vertices[1].z == 3.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->indices[7] == 7);
  REQUIRE(geometries.decoded("021/010_NDXR")->bounds.valid);
  REQUIRE(geometries.decoded("021/010_NDXR")->bounds.min_x == 0.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->bounds.min_y == 0.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->bounds.min_z == 0.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->bounds.max_x == 3.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->bounds.max_y == 4.0f);
  REQUIRE(geometries.decoded("021/010_NDXR")->bounds.max_z == 5.0f);

  const char* be_strip_slice = "ac6-buffer-be-strip.ndxr";
  const std::string be_strip_bytes = make_ndxr_be_strip_fixture();
  { std::ofstream out(be_strip_slice, std::ios::binary); out << be_strip_bytes; }
  ac6::QualifiedBufferDatabase be_strip_buffers;
  REQUIRE(be_strip_buffers.add({"be-strip", be_strip_slice, be_strip_bytes.size(),
                                fnv64(be_strip_bytes), false}));
  REQUIRE(be_strip_buffers.verify("be-strip"));
  const ac6::MissionDrawable be_strip_drawable{
      1, "be-strip", "terrain", 119, 1, "be-strip", 4, 5, "be-strip"};
  ac6::NativeGeometryDatabase be_strip_geometry;
  REQUIRE(be_strip_geometry.load_verified(be_strip_drawable, be_strip_buffers));
  REQUIRE(be_strip_geometry.find("be-strip")->source_format == "NDXR_BE");
  REQUIRE(be_strip_geometry.find("be-strip")->topology ==
          ac6::NativeIndexTopology::TriangleStripRestart);
  REQUIRE(be_strip_geometry.decoded("be-strip")->indices.size() == 5);
  REQUIRE(be_strip_geometry.decoded("be-strip")->indices.back() ==
          std::numeric_limits<std::uint32_t>::max());
  std::remove(be_strip_slice);

  // Positive end-to-end loader contract: all render manifests and one
  // qualified geometry are loaded atomically from external files.
  const char* loader_render = "ac6-loader-render.tsv";
  const char* loader_drawables = "ac6-loader-drawables.tsv";
  const char* loader_transforms = "ac6-loader-transforms.tsv";
  const char* loader_materials = "ac6-loader-materials.tsv";
  const char* loader_textures = "ac6-loader-textures.tsv";
  const char* loader_shaders = "ac6-loader-shaders.tsv";
  const char* loader_targets = "ac6-loader-targets.tsv";
  const char* loader_passes = "ac6-loader-passes.tsv";
  const char* loader_resolves = "ac6-loader-resolves.tsv";
  { std::ofstream out(loader_render); out << "1\t9\n"; }
  { std::ofstream out(loader_drawables);
    out << "1\tmapobj\tmapobj\t9\t3\tGIDX268439850\t1024\t1536\tfixture\n"; }
  { std::ofstream out(loader_transforms); out << "1\tmapobj\t0\t0\t-10\t1\t1\t1\n"; }
  { std::ofstream out(loader_materials);
    out << "1\tmapobj\tD5B4.mapobj\t1\t1\topaque\t0xFFFFFFFF\n"; }
  { std::ofstream out(loader_textures);
    out << "1\tmapobj\tfixture\tlinear\twrap\t0x1\n"; }
  { std::ofstream out(loader_shaders); out << "D5B4.mapobj\tpos_norm_uv\t1\t8\trgba8\n"; }
  { std::ofstream out(loader_targets);
    out << "1\tworld_color\t64\t32\t1\trgba8\td24s8\t1\n"; }
  { std::ofstream out(loader_passes);
    out << "1\tworld\t1\tworld_color\tmain_depth\t0x00000000\t1\n"; }
  { std::ofstream out(loader_resolves);
    out << "1\tworld\tworld_color\tpresent\tcopy\n"; }
  const char* loader_catalog = "ac6-loader-catalog.tsv";
  const char* loader_assets = "ac6-loader-assets.tsv";
  const char* loader_launches = "ac6-loader-launches.tsv";
  const char* loader_manifest = "ac6-loader-manifest.tsv";
  { std::ofstream out(loader_catalog); out << "1\tfixture\t9\n"; }
  { std::ofstream out(loader_assets); out << "9\tfixture\tfixture\n"; }
  { std::ofstream out(loader_launches); out << "1\t4097\t4097:1:9\n"; }
  { std::ofstream out(loader_manifest);
    out << "catalog\t" << loader_catalog << "\nassets\t" << loader_assets
        << "\nlaunches\t" << loader_launches << "\nrender\t" << loader_render
        << "\ndrawables\t" << loader_drawables << "\ntransforms\t" << loader_transforms
        << "\nmaterials\t" << loader_materials << "\ntextures\t" << loader_textures
        << "\nshaders\t" << loader_shaders << "\ntargets\t" << loader_targets
        << "\npasses\t" << loader_passes << "\nresolves\t" << loader_resolves
        << "\nbuffers\t" << buffer_manifest << "\n";
  }
  ac6::MissionRenderDatabase loaded_render;
  ac6::MissionDrawableDatabase loaded_drawables;
  ac6::MissionTransformDatabase loaded_transforms;
  ac6::MissionMaterialDatabase loaded_materials;
  ac6::MissionTextureDatabase loaded_textures;
  ac6::ShaderPermutationDatabase loaded_shaders;
  ac6::MissionRenderTargetDatabase loaded_targets;
  ac6::MissionRenderPassDatabase loaded_passes;
  ac6::MissionRenderResolveDatabase loaded_resolves;
  ac6::QualifiedBufferDatabase loaded_buffers;
  ac6::NativeGeometryDatabase loaded_geometries;
  ac6::MissionManifestLoader loader;
  REQUIRE(loader.load_render(loader_manifest, loaded_render, loaded_drawables,
                             loaded_transforms, loaded_materials, loaded_textures,
                             loaded_shaders, loaded_targets, loaded_passes,
                             loaded_resolves, loaded_buffers, loaded_geometries));
  REQUIRE(loaded_render.find(1) && loaded_drawables.find(1, "mapobj"));
  REQUIRE(loaded_geometries.decoded("GIDX268439850"));
  for (const char* path : {loader_render, loader_drawables, loader_transforms,
                           loader_materials, loader_textures, loader_shaders,
                           loader_targets, loader_passes, loader_resolves,
                           loader_catalog, loader_assets, loader_launches,
                           loader_manifest}) {
    std::remove(path);
  }
  std::remove(buffer_manifest);

  const char* legacy_geometry_slice = "ac6-buffer-legacy-geometry.slice";
  const std::string legacy_geometry_bytes = "AC6GEO1\t1024\t1536\t3\nlegacy";
  { std::ofstream out(legacy_geometry_slice, std::ios::binary); out << legacy_geometry_bytes; }
  ac6::QualifiedBufferDatabase legacy_buffers;
  REQUIRE(legacy_buffers.add({"legacy", legacy_geometry_slice, legacy_geometry_bytes.size(),
                              fnv64(legacy_geometry_bytes), false}));
  REQUIRE(legacy_buffers.verify("legacy"));
  ac6::NativeGeometryDatabase legacy_geometries;
  REQUIRE(!legacy_geometries.load_verified({1, "legacy", "mapobj", 9, 3, "legacy", 1024,
                                            1536, "legacy"}, legacy_buffers));
  std::remove(legacy_geometry_slice);

  const char* bad_geometry_slice = "ac6-buffer-bad-geometry.slice";
  const std::string bad_geometry_bytes =
      std::string("NDXR\t1\t1024\t1536\t3\nVTX\t1023\t32\nIDX\t1536\t2\nPOLY\t3\t0\nDATA\n") +
      std::string(1024u * 32u + 1536u * 2u, 'b');
  { std::ofstream out(bad_geometry_slice, std::ios::binary); out << bad_geometry_bytes; }
  ac6::QualifiedBufferDatabase bad_geometry_buffers;
  REQUIRE(bad_geometry_buffers.add({"bad-geometry", bad_geometry_slice, bad_geometry_bytes.size(),
                                    fnv64(bad_geometry_bytes), false}));
  REQUIRE(bad_geometry_buffers.verify("bad-geometry"));
  ac6::NativeGeometryDatabase bad_geometry_db;
  REQUIRE(!bad_geometry_db.load_verified({1, "bad-geometry", "mapobj", 9, 3,
                                          "bad-geometry", 1024, 1536, "bad-geometry"},
                                         bad_geometry_buffers));
  std::remove(bad_geometry_slice);

  const char* short_geometry_slice = "ac6-buffer-short-geometry.slice";
  const std::string short_geometry_bytes =
      "NDXR\t1\t1024\t1536\t3\nVTX\t1024\t32\nIDX\t1536\t2\nPOLY\t3\t0\nDATA\nshort";
  { std::ofstream out(short_geometry_slice, std::ios::binary); out << short_geometry_bytes; }
  ac6::QualifiedBufferDatabase short_geometry_buffers;
  REQUIRE(short_geometry_buffers.add({"short-geometry", short_geometry_slice,
                                      short_geometry_bytes.size(), fnv64(short_geometry_bytes), false}));
  REQUIRE(short_geometry_buffers.verify("short-geometry"));
  ac6::NativeGeometryDatabase short_geometry_db;
  REQUIRE(!short_geometry_db.load_verified({1, "short-geometry", "mapobj", 9, 3,
                                            "short-geometry", 1024, 1536, "short-geometry"},
                                           short_geometry_buffers));
  std::remove(short_geometry_slice);

  const char* out_of_range_index_slice = "ac6-buffer-out-of-range-index.slice";
  std::string out_of_range_index_bytes = make_ndxr_fixture(4, 8, 2, 32, 2);
  const std::size_t first_index_offset = ndxr_payload_offset(out_of_range_index_bytes) + 4u * 32u;
  write_le_u16_at(out_of_range_index_bytes, first_index_offset, 4u);
  { std::ofstream out(out_of_range_index_slice, std::ios::binary); out << out_of_range_index_bytes; }
  ac6::QualifiedBufferDatabase out_of_range_index_buffers;
  REQUIRE(out_of_range_index_buffers.add({"out-of-range-index", out_of_range_index_slice,
                                          out_of_range_index_bytes.size(),
                                          fnv64(out_of_range_index_bytes), false}));
  REQUIRE(out_of_range_index_buffers.verify("out-of-range-index"));
  ac6::NativeGeometryDatabase out_of_range_index_db;
  REQUIRE(!out_of_range_index_db.load_verified({1, "out-of-range-index", "terrain", 119, 2,
                                                "out-of-range-index", 4, 8,
                                                "out-of-range-index"},
                                               out_of_range_index_buffers));
  std::remove(out_of_range_index_slice);

  const char* nonfinite_vertex_slice = "ac6-buffer-nonfinite-vertex.slice";
  std::string nonfinite_vertex_bytes = make_ndxr_fixture(4, 8, 2, 32, 2);
  write_le_u32_at(nonfinite_vertex_bytes, ndxr_payload_offset(nonfinite_vertex_bytes), 0x7FC00000u);
  { std::ofstream out(nonfinite_vertex_slice, std::ios::binary); out << nonfinite_vertex_bytes; }
  ac6::QualifiedBufferDatabase nonfinite_vertex_buffers;
  REQUIRE(nonfinite_vertex_buffers.add({"nonfinite-vertex", nonfinite_vertex_slice,
                                        nonfinite_vertex_bytes.size(),
                                        fnv64(nonfinite_vertex_bytes), false}));
  REQUIRE(nonfinite_vertex_buffers.verify("nonfinite-vertex"));
  ac6::NativeGeometryDatabase nonfinite_vertex_db;
  REQUIRE(!nonfinite_vertex_db.load_verified({1, "nonfinite-vertex", "terrain", 119, 2,
                                              "nonfinite-vertex", 4, 8,
                                              "nonfinite-vertex"},
                                             nonfinite_vertex_buffers));
  std::remove(nonfinite_vertex_slice);

  const char* bad_buffer_manifest = "ac6-test-bad-buffers.tsv";
  {
    std::ofstream out(bad_buffer_manifest);
    out << "bad\t" << mapobj_slice << "\t" << mapobj_bytes.size() << "\t1\n";
  }
  ac6::QualifiedBufferDatabase bad_buffers;
  REQUIRE(bad_buffers.load_manifest(bad_buffer_manifest));
  REQUIRE(!bad_buffers.verify("bad"));
  std::remove(bad_buffer_manifest);

  ac6::VulkanRenderer renderer;
  REQUIRE(renderer.render(first, {&assets, render_definition, &drawables, &buffers}) == false);
  REQUIRE(renderer.render(second, {nullptr, render_definition, &drawables, &buffers}) == false);
  REQUIRE(renderer.render(second, {&assets, nullptr, &drawables, &buffers}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definitions.find(2), &drawables, &buffers}) == false);
  ac6::MissionDrawableDatabase incomplete_drawables;
  REQUIRE(incomplete_drawables.add({1, "mapobj_m01_l_brg1_n", "mapobj", 9, 3,
                                    "GIDX268439850", 1024, 1536, "cycle760-brg1n"}));
  REQUIRE(renderer.render(second, {&assets, render_definition, &incomplete_drawables, &buffers}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &unverified_buffers}) == false);
  ac6::NativeGeometryDatabase incomplete_geometries;
  REQUIRE(incomplete_geometries.load_verified(*drawables.find(1, "mapobj_m01_l_brg1_n"), buffers));
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &geometries}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &incomplete_geometries}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &incomplete_geometries, &transforms}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &geometries, &transforms}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &geometries, &transforms, &materials}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &geometries, &transforms, &materials, &textures}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &geometries, &transforms, &materials, &textures, &shaders}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &geometries, &transforms, &materials, &textures, &shaders,
                                   &render_targets}) == false);
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                   &geometries, &transforms, &materials, &textures, &shaders,
                                   &render_targets, &render_passes}) == false);
  REQUIRE(renderer.submitted_frames() == 0);
  REQUIRE(renderer.last_world_asset_count() == 0);
  REQUIRE(renderer.world_asset_submissions() == 0);
  ac6::NativeRenderTarget synthetic_target;
  REQUIRE(synthetic_target.resize(64, 32));
  REQUIRE(synthetic_target.clear(0, 1.0f));
  synthetic_target.set_raster_mode(ac6::NativeRasterMode::Points);
  ac6::NativeRenderTarget rgba_target;
  REQUIRE(rgba_target.resize(1, 1));
  REQUIRE(rgba_target.clear(0xAABBCCDDu, 1.0f));
  std::vector<std::uint8_t> rgba_pixels;
  REQUIRE(rgba_target.copy_rgba8(rgba_pixels));
  REQUIRE(rgba_pixels == std::vector<std::uint8_t>({0xBBu, 0xCCu, 0xDDu, 0xAAu}));
  ac6::VulkanRenderer synthetic_renderer;
  REQUIRE(synthetic_renderer.render(second, {&assets, render_definition, &drawables, &buffers},
                                    &synthetic_target));
  const auto synthetic_readback = synthetic_target.readback();
  REQUIRE(synthetic_readback.color_coverage == 15);
  REQUIRE(synthetic_readback.depth_coverage == 15);
  REQUIRE(synthetic_target.diagnostic_point_writes() == 15);
  REQUIRE(synthetic_target.filled_fragment_writes() == 0);
  ac6::NativeRenderTarget target;
  REQUIRE(!target.resize(0, 64));
  REQUIRE(target.resize(64, 32));
  REQUIRE(target.clear(0, 1.0f));
  REQUIRE(!target.readback().has_world_coverage());
  REQUIRE(renderer.render(second, {&assets, render_definition, &drawables, &buffers, &geometries,
                                   &transforms, &materials, &textures, &shaders, &render_targets,
                                   &render_passes, &render_resolves},
                          &target));
  REQUIRE(renderer.submitted_frames() == 1);
  REQUIRE(renderer.last_world_asset_count() == 2);
  REQUIRE(renderer.world_asset_submissions() == 2);
  const auto readback = target.readback();
  REQUIRE(readback.width == 64 && readback.height == 32);
  REQUIRE(readback.color_coverage != 0);
  REQUIRE(readback.depth_coverage != 0);
  REQUIRE(readback.color_hash != synthetic_readback.color_hash);
  REQUIRE(readback.depth_hash != synthetic_readback.depth_hash);
  REQUIRE(readback.has_world_coverage());
  REQUIRE(target.geometry_calls() != 0);
  REQUIRE(target.raster_triangles() != 0);
  REQUIRE(target.raster_writes() != 0);
  const char* requested_frame_dump = std::getenv("AC6_NATIVE_FRAME_DUMP");
  const char* frame_dump = requested_frame_dump == nullptr ? "ac6-native-frame-test.ppm" :
      requested_frame_dump;
  REQUIRE(target.write_ppm(frame_dump));
  {
    std::ifstream input(frame_dump, std::ios::binary);
    std::string header;
    std::getline(input, header);
    REQUIRE(header == "P6");
    std::getline(input, header);
    REQUIRE(header == "64 32");
    std::getline(input, header);
    REQUIRE(header == "255");
    input.seekg(0, std::ios::end);
    REQUIRE(input.tellg() == static_cast<std::streamoff>(13 + 64 * 32 * 3));
  }
  if (requested_frame_dump == nullptr) std::remove(frame_dump);
  REQUIRE(!target.write_ppm(""));

  ac6::NativeRenderTarget target_b;
  REQUIRE(target_b.resize(64, 32));
  REQUIRE(target_b.clear(0, 1.0f));
  ac6::VulkanRenderer renderer_b;
  REQUIRE(renderer_b.render(second, {&assets, render_definition, &drawables, &buffers, &geometries,
                                     &transforms, &materials, &textures, &shaders, &render_targets,
                                     &render_passes, &render_resolves},
                            &target_b));
  const auto readback_b = target_b.readback();
  REQUIRE(readback.color_hash == readback_b.color_hash);
  REQUIRE(readback.depth_hash == readback_b.depth_hash);
  ac6::MissionTransformDatabase shifted_transforms;
  REQUIRE(shifted_transforms.add({1, "mapobj_m01_l_brg1_n", 11.0f, 0.0f, -20.0f, 1.0f, 1.0f, 1.0f}));
  REQUIRE(shifted_transforms.add({1, "mapparts_m01_l_034_010", 101.0f, 5.0f, -200.0f, 2.0f, 1.0f, 3.0f}));
  REQUIRE(shifted_transforms.add({1, "entry119_022_skycloud", 21.0f, 0.0f, -75.0f, 4.0f, 2.0f, 4.0f}));
  ac6::NativeRenderTarget shifted_target;
  REQUIRE(shifted_target.resize(64, 32));
  REQUIRE(shifted_target.clear(0, 1.0f));
  ac6::VulkanRenderer shifted_renderer;
  REQUIRE(shifted_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                           &geometries, &shifted_transforms, &materials, &textures,
                                           &shaders, &render_targets, &render_passes, &render_resolves},
                                  &shifted_target));
  const auto shifted_readback = shifted_target.readback();
  REQUIRE(readback.color_hash != shifted_readback.color_hash);
  REQUIRE(readback.depth_hash != shifted_readback.depth_hash);
  ac6::MissionMaterialDatabase recolored_materials;
  REQUIRE(recolored_materials.add({1, "mapobj_m01_l_brg1_n", "D5B4.mapobj", true, true,
                                   "opaque", 0xFF905040u}));
  REQUIRE(recolored_materials.add({1, "mapparts_m01_l_034_010", "D5B4.terrain", true, true,
                                   "opaque", 0xFF5060A0u}));
  REQUIRE(recolored_materials.add({1, "entry119_022_skycloud", "D5B4.skycloud", false, false,
                                   "alpha", 0xAAFFFFFFu}));
  ac6::NativeRenderTarget recolored_target;
  REQUIRE(recolored_target.resize(64, 32));
  REQUIRE(recolored_target.clear(0, 1.0f));
  ac6::VulkanRenderer recolored_renderer;
  REQUIRE(recolored_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                             &geometries, &transforms, &recolored_materials, &textures,
                                             &shaders, &render_targets, &render_passes, &render_resolves},
                                    &recolored_target));
  const auto recolored_readback = recolored_target.readback();
  REQUIRE(readback.color_hash != recolored_readback.color_hash);
  ac6::MissionTextureDatabase retextured;
  REQUIRE(retextured.add({1, "mapobj_m01_l_brg1_n", "ntxr.mapobj.diffuse", "linear", "wrap",
                          0x0102030405060709ull}));
  REQUIRE(retextured.add({1, "mapparts_m01_l_034_010", "ntxr.terrain.diffuse", "linear", "wrap",
                          0x1112131415161719ull}));
  REQUIRE(retextured.add({1, "entry119_022_skycloud", "ntxr.skycloud.diffuse", "nearest", "clamp",
                          0x2122232425262729ull}));
  ac6::NativeRenderTarget retextured_target;
  REQUIRE(retextured_target.resize(64, 32));
  REQUIRE(retextured_target.clear(0, 1.0f));
  ac6::VulkanRenderer retextured_renderer;
  REQUIRE(retextured_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                              &geometries, &transforms, &materials, &retextured,
                                              &shaders, &render_targets, &render_passes, &render_resolves},
                                     &retextured_target));
  const auto retextured_readback = retextured_target.readback();
  REQUIRE(readback.color_hash != retextured_readback.color_hash);
  ac6::ShaderPermutationDatabase relayouted_shaders;
  REQUIRE(relayouted_shaders.add({"D5B4.mapobj", "pos_uv_color", 1, 8, "rgba8"}));
  REQUIRE(relayouted_shaders.add({"D5B4.terrain", "pos_norm_uv", 2, 12, "rgba8"}));
  REQUIRE(relayouted_shaders.add({"D5B4.skycloud", "pos_uv_color", 1, 7, "rgba8"}));
  ac6::NativeRenderTarget relayouted_target;
  REQUIRE(relayouted_target.resize(64, 32));
  REQUIRE(relayouted_target.clear(0, 1.0f));
  ac6::VulkanRenderer relayouted_renderer;
  REQUIRE(relayouted_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                              &geometries, &transforms, &materials, &textures,
                                              &relayouted_shaders, &render_targets, &render_passes,
                                              &render_resolves},
                                     &relayouted_target));
  const auto relayouted_readback = relayouted_target.readback();
  REQUIRE(readback.color_hash != relayouted_readback.color_hash);
  ac6::NativeRenderTarget wrong_size_target;
  REQUIRE(wrong_size_target.resize(32, 32));
  REQUIRE(wrong_size_target.clear(0, 1.0f));
  ac6::VulkanRenderer wrong_size_renderer;
  REQUIRE(!wrong_size_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                               &geometries, &transforms, &materials, &textures,
                                               &shaders, &render_targets, &render_passes,
                                               &render_resolves},
                                      &wrong_size_target));
  ac6::MissionRenderTargetDatabase missing_present_targets;
  REQUIRE(missing_present_targets.add({1, "world_color", 64, 32, 4, "rgba8", "d24s8", true}));
  ac6::NativeRenderTarget missing_present_target;
  REQUIRE(missing_present_target.resize(64, 32));
  REQUIRE(missing_present_target.clear(0, 1.0f));
  ac6::VulkanRenderer missing_present_renderer;
  REQUIRE(!missing_present_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                                    &geometries, &transforms, &materials, &textures,
                                                    &shaders, &missing_present_targets, &render_passes,
                                                    &render_resolves},
                                           &missing_present_target));
  ac6::MissionRenderPassDatabase reordered_passes;
  REQUIRE(reordered_passes.add({1, "world", 2, "world_color", "main_depth", 0x00000000u, 1.0f}));
  ac6::NativeRenderTarget reordered_target;
  REQUIRE(reordered_target.resize(64, 32));
  REQUIRE(reordered_target.clear(0, 1.0f));
  ac6::VulkanRenderer reordered_renderer;
  REQUIRE(reordered_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                             &geometries, &transforms, &materials, &textures,
                                             &shaders, &render_targets, &reordered_passes,
                                             &render_resolves},
                                    &reordered_target));
  const auto reordered_readback = reordered_target.readback();
  REQUIRE(readback.color_hash != reordered_readback.color_hash);
  ac6::MissionRenderResolveDatabase tonemap_resolves;
  REQUIRE(tonemap_resolves.add({1, "world", "world_color", "present", "tonemap"}));
  ac6::NativeRenderTarget tonemap_target;
  REQUIRE(tonemap_target.resize(64, 32));
  REQUIRE(tonemap_target.clear(0, 1.0f));
  ac6::VulkanRenderer tonemap_renderer;
  REQUIRE(tonemap_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                           &geometries, &transforms, &materials, &textures,
                                           &shaders, &render_targets, &render_passes, &tonemap_resolves},
                                  &tonemap_target));
  const auto tonemap_readback = tonemap_target.readback();
  REQUIRE(readback.color_hash != tonemap_readback.color_hash);
  ac6::MissionRenderResolveDatabase copy_msaa_resolves;
  REQUIRE(copy_msaa_resolves.add({1, "world", "world_color", "present", "copy"}));
  ac6::NativeRenderTarget copy_msaa_target;
  REQUIRE(copy_msaa_target.resize(64, 32));
  REQUIRE(copy_msaa_target.clear(0, 1.0f));
  ac6::VulkanRenderer copy_msaa_renderer;
  REQUIRE(!copy_msaa_renderer.render(second, {&assets, render_definition, &drawables, &buffers,
                                              &geometries, &transforms, &materials, &textures,
                                              &shaders, &render_targets, &render_passes,
                                              &copy_msaa_resolves},
                                     &copy_msaa_target));
  ac6::WorldFrame bad_camera_frame = second;
  bad_camera_frame.camera_target_x = bad_camera_frame.camera_x;
  bad_camera_frame.camera_target_y = bad_camera_frame.camera_y;
  bad_camera_frame.camera_target_z = bad_camera_frame.camera_z;
  ac6::NativeRenderTarget bad_camera_target;
  REQUIRE(bad_camera_target.resize(64, 32));
  REQUIRE(bad_camera_target.clear(0, 1.0f));
  ac6::VulkanRenderer bad_camera_renderer;
  REQUIRE(!bad_camera_renderer.render(bad_camera_frame, {&assets, render_definition, &drawables,
                                                         &buffers, &geometries, &transforms,
                                                         &materials, &textures, &shaders,
                                                         &render_targets, &render_passes,
                                                         &render_resolves},
                                      &bad_camera_target));
  std::remove(mapobj_slice);
  std::remove(terrain_slice);
  std::remove(sky_slice);

  ac6::MissionRuntime replay_a(1, &assets);
  ac6::MissionRuntime replay_b(1, &assets);
  const ac6::InputFrame input{12000, -8000, 4000, 192, 0};
  const auto initial_a = replay_a.tick(1.0f / 60.0f, {});
  const auto initial_b = replay_b.tick(1.0f / 60.0f, {});
  REQUIRE(initial_a.input.pitch == 0 && initial_a.input.roll == 0 && initial_a.input.yaw == 0 &&
          initial_a.input.throttle == 0 && initial_a.input.buttons == 0);
  REQUIRE(initial_a.tick == initial_b.tick && initial_a.position_x == initial_b.position_x &&
          initial_a.position_y == initial_b.position_y && initial_a.position_z == initial_b.position_z);
  for (int i = 0; i < 120; ++i) {
    const auto a = replay_a.tick(1.0f / 60.0f, input);
    const auto b = replay_b.tick(1.0f / 60.0f, input);
    REQUIRE(a.tick == b.tick && a.position_x == b.position_x &&
           a.position_y == b.position_y && a.position_z == b.position_z);
    REQUIRE(a.input.pitch == input.pitch && a.input.roll == input.roll &&
            a.input.yaw == input.yaw && a.input.throttle == input.throttle &&
            a.input.buttons == input.buttons);
  }
  const auto neutral = replay_a.tick(1.0f / 60.0f, {});
  REQUIRE(neutral.input.pitch == 0 && neutral.input.roll == 0 && neutral.input.yaw == 0 &&
          neutral.input.throttle == 0 && neutral.input.buttons == 0);

  ac6::MissionAssetDatabase hud_assets;
  for (const ac6::AssetId id : {9u, 119u, 165u, 199u, 210u}) {
    REQUIRE(hud_assets.add({id, "qualified", "hud-test"}));
  }
  ac6::MissionObjectiveDatabase hud_objectives;
  REQUIRE(hud_objectives.add({1, {1, "retail_objective_record", true,
                                  ac6::ObjectiveState::Pending}}));
  ac6::RadioMessageDatabase hud_radios;
  REQUIRE(hud_radios.add({1, 10, "retail_radio_record", "AWACS", 199, 210}));
  ac6::MissionLaunchDefinition hud_launch = *launch;
  hud_launch.units[1].owner = 2;
  ac6::MissionExecution hud_execution(*selected_definition, &hud_assets, &hud_objectives,
                                      &hud_radios);
  REQUIRE(hud_execution.launch(hud_launch));
  REQUIRE(hud_execution.activate_objective(1));
  REQUIRE(hud_execution.lock_target(4098));
  REQUIRE(hud_execution.play_radio(10, 1.0f));
  const ac6::WorldFrame hud_frame = hud_execution.tick(
      1.0f / 60.0f, {8192, 0, 2048, 200, 0});
  REQUIRE(hud_frame.mission_ready && hud_frame.speed > 0.0f);
  ac6::NativeRenderTarget hud_target;
  REQUIRE(hud_target.resize(320, 180) && hud_target.clear(0, 1.0f));
  ac6::NativeHudRenderer hud_renderer;
  REQUIRE(hud_renderer.render(hud_target, hud_frame, hud_execution));
  const ac6::NativeHudSnapshot& hud_snapshot = hud_renderer.snapshot();
  REQUIRE(hud_snapshot.reticle_visible && hud_snapshot.telemetry_visible &&
          hud_snapshot.weapon_visible && hud_snapshot.target_visible &&
          hud_snapshot.target_locked && hud_snapshot.radar_visible &&
          hud_snapshot.objective_visible && hud_snapshot.radio_visible &&
          hud_snapshot.active_objective_id == 1 &&
          hud_snapshot.primary_weapon_id == 7 && hud_snapshot.weapon_count == 1 &&
          hud_snapshot.radio_message_id == 10 && hud_snapshot.pixel_writes > 0 &&
          hud_snapshot.unique_pixels > 0);
  REQUIRE(hud_target.readback().color_coverage > 0 && hud_target.readback().depth_coverage == 0);

  // Objective conditions are explicit native bindings.  The four-column
  // manifest remains compatible with manual objectives; the six-column form
  // is the only form allowed to bind a combat entity.
  const char* condition_manifest = "ac6-test-objective-conditions.tsv";
  {
    std::ofstream out(condition_manifest);
    out << "1\t1\tdestroy_retail_target\t1\tdestroy_unit\t4098\n";
    out << "1\t2\tprotect_retail_target\t1\tprotect_unit\t4098\n";
  }
  ac6::MissionObjectiveDatabase condition_database;
  REQUIRE(condition_database.load_manifest(condition_manifest));
  std::remove(condition_manifest);
  const auto condition_records = condition_database.find_by_mission(1);
  REQUIRE(condition_records.size() == 2 && condition_records[0]->condition ==
              ac6::ObjectiveCondition::DestroyUnit && condition_records[0]->target_entity == 4098 &&
          condition_records[1]->condition == ac6::ObjectiveCondition::ProtectUnit &&
          condition_records[1]->target_entity == 4098);

  const char* legacy_condition_manifest = "ac6-test-objective-manual.tsv";
  {
    std::ofstream out(legacy_condition_manifest);
    out << "1\t3\tmanual_retail_pending\t1\n";
  }
  ac6::MissionObjectiveDatabase legacy_condition_database;
  REQUIRE(legacy_condition_database.load_manifest(legacy_condition_manifest));
  std::remove(legacy_condition_manifest);
  const auto legacy_records = legacy_condition_database.find_by_mission(1);
  REQUIRE(legacy_records.size() == 1 &&
          legacy_records[0]->condition == ac6::ObjectiveCondition::Manual &&
          legacy_records[0]->target_entity == 0);

  const char* invalid_condition_manifest = "ac6-test-objective-invalid.tsv";
  {
    std::ofstream out(invalid_condition_manifest);
    out << "1\t4\tinvalid_target\t1\tdestroy_unit\t0\n";
  }
  REQUIRE(!condition_database.load_manifest(invalid_condition_manifest));
  std::remove(invalid_condition_manifest);
  REQUIRE(condition_database.find_by_mission(1).size() == 2);

  ac6::MissionObjectiveDatabase destroy_database;
  REQUIRE(destroy_database.add({1, {1, "destroy_target", true, ac6::ObjectiveState::Pending,
                                    ac6::ObjectiveCondition::DestroyUnit, 4098}}));
  ac6::MissionExecution destroy_execution(*selected_definition, &hud_assets, &destroy_database);
  REQUIRE(destroy_execution.launch(hud_launch) && destroy_execution.activate_objective(1));
  REQUIRE(destroy_execution.combat().apply_damage(4098, 100.0f));
  const ac6::WorldFrame condition_success_frame = destroy_execution.tick(1.0f / 60.0f, {});
  REQUIRE(!condition_success_frame.mission_ready &&
          destroy_execution.debrief().outcome == ac6::MissionOutcome::Success &&
          destroy_execution.debrief().completed_objectives == 1);

  ac6::MissionObjectiveDatabase protect_database;
  REQUIRE(protect_database.add({1, {1, "protect_target", true, ac6::ObjectiveState::Pending,
                                    ac6::ObjectiveCondition::ProtectUnit, 4098}}));
  ac6::MissionExecution protect_execution(*selected_definition, &hud_assets, &protect_database);
  REQUIRE(protect_execution.launch(hud_launch) && protect_execution.activate_objective(1));
  REQUIRE(protect_execution.combat().apply_damage(4098, 100.0f));
  const ac6::WorldFrame condition_failure_frame = protect_execution.tick(1.0f / 60.0f, {});
  REQUIRE(!condition_failure_frame.mission_ready &&
          protect_execution.debrief().outcome == ac6::MissionOutcome::Failure &&
          protect_execution.debrief().failed_objectives == 1);
  return 0;
}
