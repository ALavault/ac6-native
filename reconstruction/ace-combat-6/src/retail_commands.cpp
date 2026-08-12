#include "ac6/retail_commands.h"

#include "ac6/campaign_progression.h"
#include "ac6/execution_trace.h"
#include "ac6/native_hud.h"
#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_camera_table.h"
#include "ac6/retail_content.h"
#include "ac6/retail_frontend_resources.h"
#include "ac6/retail_mission01_cpu_compositor.h"
#include "ac6/retail_session.h"
#include "ac6/retail_session_replay.h"
#include "ac6/sdl_input.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace ac6::retail_cli {

namespace detail {

bool append_native_replay_trace_sample(
    ExecutionTraceJsonlWriter& writer, std::uint64_t frame_index,
    InputFrame input, const WorldFrame& simulation,
    TraceMissionObjectives mission, TraceGraphicsSubmission graphics) {
  if (frame_index == std::numeric_limits<std::uint64_t>::max()) return false;
  return writer.append(frame_index + 1u, input, simulation, std::move(mission),
                       graphics);
}

}  // namespace detail

namespace {

struct Options final {
  std::filesystem::path cache;
  std::filesystem::path save;
  std::filesystem::path resume;
  std::filesystem::path replay;
  std::filesystem::path report;
  std::filesystem::path capture;
  std::filesystem::path scene_capture;
  std::filesystem::path scene_report;
  std::filesystem::path trace;
  std::uint32_t aircraft{1};
  std::uint32_t weapon{1};
  retail::RetailDifficulty difficulty{retail::RetailDifficulty::Normal};
  std::uint32_t frames{};
};

bool parse_u32(std::string_view text, std::uint32_t& value) noexcept {
  if (text.empty()) return false;
  std::uint32_t parsed = 0;
  for (const char digit : text) {
    if (digit < '0' || digit > '9' || parsed > (UINT32_MAX - static_cast<std::uint32_t>(digit - '0')) / 10u) {
      return false;
    }
    parsed = parsed * 10u + static_cast<std::uint32_t>(digit - '0');
  }
  value = parsed;
  return true;
}

bool parse_play_options(int argc, char** argv, Options& options) {
  bool aircraft_seen = false;
  bool weapon_seen = false;
  bool difficulty_seen = false;
  bool frames_seen = false;
  for (int index = 2; index < argc; ++index) {
    const std::string_view option(argv[index]);
    if (index + 1 >= argc) return false;
    const std::filesystem::path value(argv[++index]);
    if (option == "--cache" && options.cache.empty()) options.cache = value;
    else if (option == "--save" && options.save.empty()) options.save = value;
    else if (option == "--resume" && options.resume.empty()) options.resume = value;
    else if (option == "--replay" && options.replay.empty()) options.replay = value;
    else if (option == "--capture" && options.capture.empty()) options.capture = value;
    else if (option == "--scene-capture" && options.scene_capture.empty()) {
      options.scene_capture = value;
    } else if (option == "--scene-report" && options.scene_report.empty()) {
      options.scene_report = value;
    }
    else if (option == "--frames" && !frames_seen) {
      if (!parse_u32(value.string(), options.frames) || options.frames > 600000u) {
        return false;
      }
      frames_seen = true;
    }
    else if (option == "--aircraft" && !aircraft_seen) {
      if (!parse_u32(value.string(), options.aircraft)) return false;
      aircraft_seen = true;
    } else if (option == "--weapon" && !weapon_seen) {
      if (!parse_u32(value.string(), options.weapon)) return false;
      weapon_seen = true;
    } else if (option == "--difficulty" && !difficulty_seen) {
      std::uint32_t raw = 0;
      if (!parse_u32(value.string(), raw) ||
          raw > static_cast<std::uint32_t>(retail::RetailDifficulty::Ace)) {
        return false;
      }
      options.difficulty = static_cast<retail::RetailDifficulty>(raw);
      difficulty_seen = true;
    } else {
      return false;
    }
  }
  return !options.cache.empty() &&
      (options.scene_report.empty() || !options.scene_capture.empty());
}

bool parse_replay_options(int argc, char** argv, Options& options) {
  for (int index = 2; index < argc; ++index) {
    const std::string_view option(argv[index]);
    if (index + 1 >= argc) return false;
    const std::filesystem::path value(argv[++index]);
    if (option == "--cache" && options.cache.empty()) options.cache = value;
    else if (option == "--replay" && options.replay.empty()) options.replay = value;
    else if (option == "--report" && options.report.empty()) options.report = value;
    else if (option == "--trace" && options.trace.empty()) options.trace = value;
    else return false;
  }
  return !options.cache.empty() && !options.replay.empty() && !options.report.empty();
}

bool open_store(const std::filesystem::path& cache, RetailContentStore& store) {
  if (cache.empty() || !store.open(cache)) {
    std::fprintf(stderr, "ac6_retail=fail error=%s detail=%s cache=%s\n",
                 retail_content_error_name(store.error()), store.detail().c_str(),
                 cache.string().c_str());
    return false;
  }
  return true;
}

bool frontend_resources_qualified(const RetailContentStore& store) {
  return retail::RetailFrontendResources::open(store).has_value();
}

bool loadout_qualified(const RetailContentStore& store,
                       const CampaignLoadout& loadout) {
  const std::optional<retail::RetailCampaignBundle> common =
      retail::RetailCampaignBundle::open_entry(store, retail::kRetailCameraTableEntry);
  if (!common.has_value()) return false;
  const std::optional<retail::RetailCameraTable> cameras =
      retail::RetailCameraTable::open(*common);
  return cameras.has_value() && cameras->record_for_loadout(loadout, 1u) != nullptr;
}

struct NativeGraphics final {
  bool video_initialized{};
  bool vulkan_loaded{};
  VulkanInstance instance;
  SdlWindow window;
  SdlVulkanSurface surface;
  VulkanDevice device;
  VulkanSwapchain swapchain;
  VulkanFramePresenter presenter;

  bool initialize(std::uint32_t width, std::uint32_t height) noexcept {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;
    video_initialized = true;
    if (!SDL_Vulkan_LoadLibrary(nullptr)) return false;
    vulkan_loaded = true;
    std::vector<const char*> extensions;
    if (!SdlVulkanSurface::required_instance_extensions(extensions) ||
        !instance.create(extensions) ||
        !window.create("ac6-native", width, height, true, false) ||
        !surface.create(window, instance.handle()) ||
        !device.create(instance.handle(), surface.handle()) ||
        !swapchain.create(device, surface.handle(), width, height) ||
        !presenter.create(device, swapchain)) return false;
    return true;
  }

  bool present(const NativeRenderTarget& target) noexcept {
    return presenter.valid() && presenter.present_frame(target);
  }

  ~NativeGraphics() {
    presenter.destroy();
    swapchain.destroy();
    device.destroy();
    surface.destroy();
    window.destroy();
    instance.destroy();
    if (vulkan_loaded) SDL_Vulkan_UnloadLibrary();
    if (video_initialized) SDL_QuitSubSystem(SDL_INIT_VIDEO);
  }
};

bool render_frame(NativeGraphics& graphics, NativeRenderTarget& target,
                  NativeHudRenderer& hud, const retail::RetailSession& session,
                  const retail::RetailSessionFrame& frame) {
  if (!target.clear(0xFF071018u, 1.0f)) return false;
  (void)session.render_world_markers(target, frame.world, 0.0f);
  return hud.render(target, frame.world, session.execution()) && graphics.present(target);
}

std::optional<retail::Mission01CpuFrame> render_retail_scene(
    retail::RetailMission01CpuCompositor& compositor,
    const CampaignLoadout& loadout) {
  // This stable map view uses the validated mode-2 base offset only. The live
  // player pose/camera producers are not qualified yet, so this is explicitly
  // a diagnostic scene capture and never an accepted JV frame.
  constexpr std::array<float, 3> kSceneEye{1000.0F, 420.0F, -24000.0F};
  retail::Mission01CpuFrameRequest request;
  request.width = 1280;
  request.height = 720;
  request.loadout = loadout;
  request.view_mode = 2;
  request.camera_mode_selection = retail::resolve_retail_camera_mode(2);
  const retail::RetailCameraRecord* camera =
      compositor.camera_record(loadout, request.view_mode);
  if (camera == nullptr) return std::nullopt;
  const std::optional<std::array<float, 4>> offset = camera->offset(0);
  if (!offset.has_value()) return std::nullopt;

  retail::RetailMode2CameraState camera_state;
  camera_state.player_basis = retail::identity_basis();
  for (std::size_t lane = 0; lane < kSceneEye.size(); ++lane) {
    camera_state.player_position[lane] = kSceneEye[lane] - (*offset)[lane];
  }
  request.mode2_camera_state = camera_state;
  request.texture_swap_16 = true;
  request.sampler_address = retail::Mission01CpuSamplerAddress::Repeat;
  return compositor.render(request);
}

bool present_retail_scene_capture(
    NativeGraphics& graphics, NativeRenderTarget& target, NativeHudRenderer& hud,
    const retail::RetailSession& session,
    const retail::RetailSessionFrame& session_frame,
    retail::RetailMission01CpuCompositor& compositor,
    const CampaignLoadout& loadout, const std::filesystem::path& capture,
    const std::filesystem::path& report) {
  const std::optional<retail::Mission01CpuFrame> scene =
      render_retail_scene(compositor, loadout);
  if (!scene.has_value() ||
      !target.blit_argb32(
          scene->report().width, scene->report().height,
          std::span<const std::uint32_t>(scene->pixels()),
          std::span<const float>(scene->depth()), scene->report().far_plane)) {
    return false;
  }
  if (!report.empty()) {
    std::error_code error;
    const std::filesystem::path parent = report.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error || !scene->write_report_json(report)) return false;
  }
  if (!hud.render(target, session_frame.world, session.execution())) return false;
  std::error_code error;
  const std::filesystem::path parent = capture.parent_path();
  if (!parent.empty()) std::filesystem::create_directories(parent, error);
  if (error || !target.write_ppm(capture)) return false;
  return graphics.present(target);
}

int run_play_impl(const Options& options) {
  RetailContentStore store;
  if (!open_store(options.cache, store)) return 120;
  if (!frontend_resources_qualified(store)) {
    std::fprintf(stderr,
                 "ac6_retail=fail error=cache_incomplete detail=frontend_font_resources\n");
    return 121;
  }
  const CampaignLoadout loadout{options.aircraft, options.weapon, true};
  if (!loadout.valid()) return 122;
  if (!loadout_qualified(store, loadout)) {
    std::fprintf(stderr,
                 "ac6_retail=fail error=cache_incomplete detail=loadout_capability_table\n");
    return 123;
  }
  std::optional<retail::RetailMission01CpuCompositor> scene_compositor;
  if (!options.scene_capture.empty()) {
    scene_compositor = retail::RetailMission01CpuCompositor::open(store);
    if (!scene_compositor.has_value()) {
      std::fprintf(stderr,
                   "ac6_retail=fail error=scene_resources_unavailable "
                   "detail=mission01_cpu_compositor\n");
      return 124;
    }
  }
  std::unique_ptr<retail::RetailSession> session =
      retail::RetailSession::open(store, loadout,
                                  {1, {0, 0}, retail::kRetailOpeningCameraModeWord,
                                   options.difficulty,
                                   retail::RetailScriptDrive::ExternalProbe});
  if (session == nullptr) return 125;
  if (!options.resume.empty()) {
    SessionSaveStore saves;
    const SessionSaveSnapshot* snapshot = nullptr;
    if (!saves.read_file(options.resume) || (snapshot = saves.load(1)) == nullptr ||
        !session->restore_save(*snapshot)) {
      std::fprintf(stderr,
                   "ac6_retail=fail error=save_incompatible detail=cache_or_script_state\n");
      return 132;
    }
  }
  NativeGraphics graphics;
  if (!graphics.initialize(1280, 720)) {
    std::fprintf(stderr,
                 "ac6_retail=fail error=vulkan_unavailable detail=interactive_backend\n");
    return 125;
  }
  NativeRenderTarget target;
  if (!target.resize(1280, 720)) return 126;
  NativeHudRenderer hud;
  SdlEventPump pump;
  if (!pump.initialize()) return 127;
  SdlInputAdapter input_adapter;
  InputMappingDatabase mappings;
  InputFrame input_frame{};
  std::uint16_t buttons = 0;
  std::vector<Event> events;
  bool quit = false;
  retail::RetailSessionReplay replay;
  replay.mission_id = 1;
  replay.difficulty = options.difficulty;
  replay.loadout = loadout;
  replay.content_index_sha256 = store.index_sha256();
  retail::RetailSessionFrame frame = session->tick(1.0f / 60.0f, input_frame);
  if (options.scene_capture.empty()) {
    if (!render_frame(graphics, target, hud, *session, frame)) return 128;
  } else if (!present_retail_scene_capture(
                 graphics, target, hud, *session, frame, *scene_compositor,
                 loadout, options.scene_capture, options.scene_report)) {
    return 128;
  }
  bool capture_written = false;
  const auto capture = [&]() -> bool {
    if (options.capture.empty() || capture_written) return true;
    std::error_code error;
    const std::filesystem::path parent = options.capture.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error || !target.write_ppm(options.capture)) return false;
    capture_written = true;
    return true;
  };
  if (!capture()) return 129;
  const std::uint32_t requested_frames =
      options.scene_capture.empty()
          ? options.frames
          : (options.frames == 0 ? 1u : options.frames);
  using Clock = std::chrono::steady_clock;
  auto previous = Clock::now();
  double accumulator = 0.0;
  while (!quit &&
         (requested_frames == 0 || replay.frames.size() < requested_frames)) {
    events.clear();
    (void)pump.pump(input_adapter, input_frame, buttons, mappings,
                    session->player_entity(), events, quit);
    for (const Event event : events) (void)session->execution().dispatch(event);
    const auto now = Clock::now();
    accumulator = std::min(accumulator +
                               std::chrono::duration<double>(now - previous).count(),
                           0.25);
    previous = now;
    bool stepped = false;
    while (accumulator >= 1.0 / 60.0) {
      frame = session->tick(1.0f / 60.0f, input_frame);
      replay.frames.push_back(input_frame);
      if (replay.frames.size() % 600u == 0u) {
        const auto frame_index = static_cast<std::uint32_t>(replay.frames.size());
        replay.checkpoints.push_back(
            {frame_index, replay.input_digest(frame_index)});
      }
      accumulator -= 1.0 / 60.0;
      stepped = true;
    }
    if (stepped && !render_frame(graphics, target, hud, *session, frame)) return 128;
    if (stepped && !capture()) return 129;
    SDL_Delay(1);
  }
  replay.final_tick = replay.frames.size();
  replay.final_digest = replay.input_digest();
  if (!options.replay.empty() && !replay.write_file(options.replay)) return 130;
  if (!options.save.empty()) {
    SessionSaveStore saves;
    MissionExecution::Checkpoint checkpoint;
    if (!session->save_checkpoint(checkpoint) ||
        !saves.save(1, {1, store.index_sha256(), session->execution().snapshot(), {}, checkpoint}) ||
        !saves.write_file(options.save)) return 131;
  }
  std::fprintf(stdout,
               "ac6_retail=pass command=play mission=1 ticks=%llu replay_frames=%zu "
               "difficulty=%u cache_index_sha256=%s\n",
               static_cast<unsigned long long>(frame.world.tick), replay.frames.size(),
               static_cast<unsigned>(options.difficulty),
               sha256_hex(store.index_sha256()).c_str());
  return 0;
}

bool same_world_frame(const WorldFrame& a, const WorldFrame& b) {
  return a.tick == b.tick && a.mission_id == b.mission_id &&
      a.mission_ready == b.mission_ready && a.position_x == b.position_x &&
      a.position_y == b.position_y && a.position_z == b.position_z &&
      a.pitch == b.pitch && a.roll == b.roll && a.yaw == b.yaw &&
      a.speed == b.speed && a.active_units == b.active_units &&
      a.player_entity == b.player_entity && a.camera_x == b.camera_x &&
      a.camera_y == b.camera_y && a.camera_z == b.camera_z &&
      a.camera_target_x == b.camera_target_x &&
      a.camera_target_y == b.camera_target_y &&
      a.camera_target_z == b.camera_target_z && a.input == b.input;
}

void hash_u32(std::uint64_t& hash, std::uint32_t value) noexcept {
  for (unsigned shift = 0; shift < 32u; shift += 8u) {
    hash ^= static_cast<std::uint8_t>(value >> shift);
    hash *= 1099511628211ull;
  }
}

void hash_u64(std::uint64_t& hash, std::uint64_t value) noexcept {
  for (unsigned shift = 0; shift < 64u; shift += 8u) {
    hash ^= static_cast<std::uint8_t>(value >> shift);
    hash *= 1099511628211ull;
  }
}

void hash_float(std::uint64_t& hash, float value) noexcept {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  hash_u32(hash, bits);
}

std::uint64_t hash_frame(std::uint64_t hash, const WorldFrame& frame) noexcept {
  hash_u64(hash, frame.tick);
  hash_u32(hash, frame.mission_id);
  hash_u32(hash, frame.mission_ready ? 1u : 0u);
  for (const float value : {frame.position_x, frame.position_y, frame.position_z,
                            frame.pitch, frame.roll, frame.yaw, frame.speed,
                            frame.camera_x, frame.camera_y, frame.camera_z,
                            frame.camera_target_x, frame.camera_target_y,
                            frame.camera_target_z}) hash_float(hash, value);
  hash_u32(hash, frame.active_units);
  hash_u32(hash, frame.player_entity);
  hash_u32(hash, static_cast<std::uint16_t>(frame.input.pitch));
  hash_u32(hash, static_cast<std::uint16_t>(frame.input.roll));
  hash_u32(hash, static_cast<std::uint16_t>(frame.input.yaw));
  hash_u32(hash, frame.input.throttle);
  hash_u32(hash, frame.input.buttons);
  return hash;
}

struct ReplayRun final {
  WorldFrame final_frame{};
  std::uint32_t sub_mission{};
  std::uint32_t step{};
  bool script_ended{};
  std::uint64_t semantic_hash{1469598103934665603ull};
  std::uint64_t trace_events{};
  std::uint64_t trace_samples{};
};

std::optional<ReplayRun> replay_once(const RetailContentStore& store,
                                     const retail::RetailSessionReplay& replay,
                                     const std::filesystem::path& trace = {}) {
  std::unique_ptr<retail::RetailSession> session =
      retail::RetailSession::open(store, replay.loadout,
                                  {replay.mission_id, {0, 0},
                                   retail::kRetailOpeningCameraModeWord,
                                   replay.difficulty,
                                   retail::RetailScriptDrive::ExternalProbe});
  if (session == nullptr) return std::nullopt;
  ExecutionTraceJsonlWriter trace_writer;
  const bool tracing = !trace.empty();
  if (tracing) {
    std::error_code error;
    const std::filesystem::path parent = trace.parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error || !trace_writer.open(trace)) return std::nullopt;
  }
  ReplayRun result;
  for (std::size_t index = 0; index < replay.frames.size(); ++index) {
    const InputFrame input = replay.frames[index];
    const retail::RetailSessionFrame frame = session->tick(1.0f / 60.0f, input);
    result.final_frame = frame.world;
    result.sub_mission = frame.sub_mission;
    result.step = frame.step;
    result.script_ended = frame.script_ended;
    result.semantic_hash = hash_frame(result.semantic_hash, frame.world);
    hash_u32(result.semantic_hash, frame.sub_mission);
    hash_u32(result.semantic_hash, frame.step);
    hash_u32(result.semantic_hash, frame.script_ended ? 1u : 0u);
    if (tracing && !detail::append_native_replay_trace_sample(
                       trace_writer, static_cast<std::uint64_t>(index), input,
                       frame.world,
                       {session->state(), frame.sub_mission, frame.step,
                        frame.script_ended,
                        session->execution().scenario().objectives().snapshot()},
                       {TraceGraphicsBackend::Headless, 0, false})) {
      return std::nullopt;
    }
  }
  if (tracing && !trace_writer.close()) return std::nullopt;
  result.trace_events = tracing ? trace_writer.event_count() : 0;
  result.trace_samples = tracing ? replay.frames.size() : 0;
  return result;
}

int run_replay_impl(const Options& options) {
  RetailContentStore store;
  if (!open_store(options.cache, store)) return 131;
  if (!frontend_resources_qualified(store)) {
    std::fprintf(stderr,
                 "ac6_retail=fail error=cache_incomplete detail=frontend_font_resources\n");
    return 132;
  }
  retail::RetailSessionReplay replay;
  if (!replay.read_file(options.replay)) return 133;
  if (replay.content_index_sha256 != store.index_sha256()) return 134;
  if (replay.final_tick != replay.frames.size() ||
      replay.final_digest != replay.input_digest()) {
    return 134;
  }
  if (!loadout_qualified(store, replay.loadout)) return 135;
  // Replay and play share the same fail-closed drive policy. A trace changes
  // observation only; it cannot enable or disable scenario progression.
  const bool trace_window = !options.trace.empty();
  const std::optional<ReplayRun> first =
      replay_once(store, replay, options.trace);
  const std::optional<ReplayRun> second =
      replay_once(store, replay);
  if (!first.has_value() || !second.has_value()) return 136;
  const bool deterministic = same_world_frame(first->final_frame, second->final_frame) &&
      first->sub_mission == second->sub_mission && first->step == second->step &&
      first->script_ended == second->script_ended &&
      first->semantic_hash == second->semantic_hash;
  const bool trace_complete =
      options.trace.empty() ||
      (first->trace_samples == replay.frames.size() &&
       first->trace_events == first->trace_samples * 5u);
  std::error_code error;
  std::filesystem::create_directories(options.report, error);
  if (error) return 137;
  const std::filesystem::path report = options.report / "retail-replay.json";
  std::ofstream output(report, std::ios::trunc);
  if (!output) return 138;
  output << "{\n"
         << "  \"schema\": \"ac6.native-retail-replay.v3\",\n"
         << "  \"mission_id\": " << replay.mission_id << ",\n"
         << "  \"difficulty\": "
         << static_cast<unsigned>(replay.difficulty) << ",\n"
         << "  \"aircraft_id\": " << replay.loadout.aircraft_id << ",\n"
         << "  \"weapon_id\": " << replay.loadout.weapon_id << ",\n"
         << "  \"cache_index_sha256\": \"" << sha256_hex(store.index_sha256()) << "\",\n"
         << "  \"replay_frames\": " << replay.frames.size() << ",\n"
         << "  \"random_seed\": " << replay.random_seed << ",\n"
         << "  \"replay_final_tick\": " << replay.final_tick << ",\n"
         << "  \"checkpoint_count\": " << replay.checkpoints.size() << ",\n"
         << "  \"input_digest\": \"" << sha256_hex(replay.input_digest()) << "\",\n"
         << "  \"final_digest\": \"" << sha256_hex(replay.final_digest) << "\",\n"
         << "  \"final_digest_basis\": \"input_frames_le_v1\",\n"
         << "  \"final_tick\": " << first->final_frame.tick << ",\n"
         << "  \"final_player_entity\": " << first->final_frame.player_entity << ",\n"
         << "  \"sub_mission\": " << first->sub_mission << ",\n"
         << "  \"step\": " << first->step << ",\n"
         << "  \"script_ended\": " << (first->script_ended ? "true" : "false") << ",\n"
         << "  \"forced_progression\": false,\n"
         << "  \"simulation_hz\": 60,\n"
         << "  \"deterministic\": " << (deterministic ? "true" : "false") << ",\n"
         << "  \"semantic_hash\": \"0x" << std::hex << first->semantic_hash << std::dec << "\",\n"
         << "  \"trace_samples\": " << first->trace_samples << ",\n"
         << "  \"trace_events\": " << first->trace_events << ",\n"
         << "  \"trace_window\": " << (trace_window ? "true" : "false") << ",\n"
         << "  \"script_drive\": \"external_probe\",\n"
         << "  \"script_advance_each_tick\": false\n"
         << "}\n";
  if (!output || !deterministic || !trace_complete) return 139;
  std::fprintf(stdout, "ac6_retail=pass command=replay mission=%u frames=%zu "
                      "deterministic=true semantic_hash=0x%llx report=%s\n",
               replay.mission_id, replay.frames.size(),
               static_cast<unsigned long long>(first->semantic_hash), report.string().c_str());
  return 0;
}

}  // namespace

int run_play(int argc, char** argv) {
  Options options;
  if (!parse_play_options(argc, argv, options)) return 120;
  return run_play_impl(options);
}

int run_replay(int argc, char** argv) {
  Options options;
  if (!parse_replay_options(argc, argv, options)) return 130;
  return run_replay_impl(options);
}

}  // namespace ac6::retail_cli
