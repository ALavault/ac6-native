#include "ac6/cli.h"
#include "ac6/product_runtime.h"
#include "ac6/mission01_compare.h"
#include "ac6/native_hud.h"
#include "ac6/retail_session.h"
#include "ac6/sdl_input.h"
#include "ac6/interactive.h"

#define parse_u32 ac6::parse_u32

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

namespace {

bool same_world_frame(const ac6::WorldFrame& a, const ac6::WorldFrame& b) {
  return a.tick == b.tick && a.mission_id == b.mission_id &&
      a.mission_ready == b.mission_ready && a.position_x == b.position_x &&
      a.position_y == b.position_y && a.position_z == b.position_z &&
      a.pitch == b.pitch && a.roll == b.roll && a.yaw == b.yaw && a.speed == b.speed &&
      a.active_units == b.active_units && a.player_entity == b.player_entity &&
      a.camera_x == b.camera_x && a.camera_y == b.camera_y && a.camera_z == b.camera_z &&
      a.camera_target_x == b.camera_target_x && a.camera_target_y == b.camera_target_y &&
      a.camera_target_z == b.camera_target_z && a.input == b.input;
}

std::filesystem::path resolve_manifest_path(std::filesystem::path path) {
  std::error_code error;
  if (std::filesystem::is_directory(path, error)) path /= "manifest.tsv";
  return path;
}

struct PlayAssets final {
  ac6::MissionManifestLoader loader;
  ac6::MissionManifestPaths paths;
  ac6::MissionCatalog catalog;
  ac6::MissionAssetDatabase assets;
  ac6::MissionLaunchDatabase launches;
  ac6::MissionRuntimeServices services;
  ac6::MissionRenderDatabase render;
  ac6::MissionDrawableDatabase drawables;
  ac6::MissionTransformDatabase transforms;
  ac6::MissionMaterialDatabase materials;
  ac6::MissionTextureDatabase textures;
  ac6::ShaderPermutationDatabase shaders;
  ac6::MissionRenderTargetDatabase targets;
  ac6::MissionRenderPassDatabase passes;
  ac6::MissionRenderResolveDatabase resolves;
  ac6::QualifiedBufferDatabase buffers;
  ac6::NativeGeometryDatabase geometries;
  ac6::MissionCameraDatabase cameras;
  ac6::VulkanRenderer renderer;
  std::uint32_t mission_id{};
  const ac6::MissionDefinition* definition{};
  const ac6::MissionLaunchDefinition* launch{};
  const ac6::MissionRenderDefinition* render_definition{};
  const ac6::MissionRenderPass* world_pass{};
  const ac6::MissionRenderTargetDefinition* color_target{};

  bool load(const std::filesystem::path& input, std::uint32_t requested_mission) {
    mission_id = requested_mission;
    const std::filesystem::path manifest = resolve_manifest_path(input);
    if (!loader.load_paths(manifest, paths) || !paths.render_valid() ||
        !loader.load_runtime(manifest, catalog, assets, launches, services) ||
        !loader.load_render(manifest, render, drawables, transforms, materials, textures,
                            shaders, targets, passes, resolves, buffers, geometries)) {
      return false;
    }
    if (!paths.camera.empty() && !loader.load_camera(manifest, cameras)) return false;
    definition = catalog.find(mission_id);
    launch = launches.find(mission_id);
    render_definition = render.find(mission_id);
    world_pass = passes.find(mission_id, "world");
    if (definition == nullptr || launch == nullptr || render_definition == nullptr ||
        world_pass == nullptr) return false;
    color_target = targets.find(mission_id, world_pass->color_target);
    return color_target != nullptr && color_target->width != 0 && color_target->height != 0;
  }

  ac6::VulkanRenderer::RenderAssets render_assets() const noexcept {
    return {&assets, render_definition, &drawables, &buffers, &geometries, &transforms,
            &materials, &textures, &shaders, &targets, &passes, &resolves,
            cameras.find(mission_id)};
  }
};

struct NativeGraphics final {
  bool video_initialized{};
  bool vulkan_loaded{};
  ac6::VulkanInstance instance;
  ac6::SdlWindow window;
  ac6::SdlVulkanSurface surface;
  ac6::VulkanDevice device;
  ac6::VulkanSwapchain swapchain;
  ac6::VulkanFramePresenter presenter;

  bool initialize(std::uint32_t width, std::uint32_t height, bool hidden) noexcept {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return false;
    video_initialized = true;
    if (!SDL_Vulkan_LoadLibrary(nullptr)) return false;
    vulkan_loaded = true;
    std::vector<const char*> extensions;
    if (!ac6::SdlVulkanSurface::required_instance_extensions(extensions) ||
        !instance.create(extensions) || !window.create("ac6-native", width, height, true, hidden) ||
        !surface.create(window, instance.handle()) || !device.create(instance.handle(), surface.handle()) ||
        !swapchain.create(device, surface.handle(), width, height) ||
        !presenter.create(device, swapchain)) {
      return false;
    }
    return true;
  }

  bool present(const ac6::NativeRenderTarget& target) noexcept {
    return presenter.valid() && presenter.present_frame(target);
  }

  void shutdown() noexcept {
    presenter.destroy();
    swapchain.destroy();
    device.destroy();
    surface.destroy();
    window.destroy();
    instance.destroy();
    if (vulkan_loaded) {
      SDL_Vulkan_UnloadLibrary();
      vulkan_loaded = false;
    }
    if (video_initialized) {
      SDL_QuitSubSystem(SDL_INIT_VIDEO);
      video_initialized = false;
    }
  }

  ~NativeGraphics() { shutdown(); }
};

bool render_and_present(PlayAssets& assets, NativeGraphics& graphics,
                        ac6::NativeRenderTarget& target, const ac6::WorldFrame& frame,
                        const ac6::MissionExecution* execution = nullptr,
                        ac6::NativeHudRenderer* hud = nullptr) {
  const bool reuse_last_world = !frame.mission_ready && execution != nullptr &&
      execution->scenario().state() != ac6::ScenarioState::Gameplay;
  if (frame.mission_ready) {
    if (!target.clear(assets.world_pass->clear_color, assets.world_pass->clear_depth) ||
        !assets.renderer.render(frame, assets.render_assets(), &target)) return false;
  } else if (!reuse_last_world) {
    return false;
  }
  if (execution != nullptr && hud != nullptr && !hud->render(target, frame, *execution)) return false;
  return graphics.present(target);
}

void hash_bytes(std::uint64_t& hash, const void* data, std::size_t size) noexcept {
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }
}

template <typename T>
void hash_value(std::uint64_t& hash, const T& value) noexcept {
  hash_bytes(hash, &value, sizeof(value));
}

std::uint64_t semantic_hash(const ac6::WorldFrame& frame,
                            const ac6::RenderReadback& readback) noexcept {
  std::uint64_t hash = 1469598103934665603ull;
  hash_value(hash, frame.tick);
  hash_value(hash, frame.mission_id);
  hash_value(hash, frame.mission_ready);
  hash_value(hash, frame.position_x);
  hash_value(hash, frame.position_y);
  hash_value(hash, frame.position_z);
  hash_value(hash, frame.pitch);
  hash_value(hash, frame.roll);
  hash_value(hash, frame.yaw);
  hash_value(hash, frame.speed);
  hash_value(hash, frame.active_units);
  hash_value(hash, frame.player_entity);
  hash_value(hash, frame.camera_x);
  hash_value(hash, frame.camera_y);
  hash_value(hash, frame.camera_z);
  hash_value(hash, frame.camera_target_x);
  hash_value(hash, frame.camera_target_y);
  hash_value(hash, frame.camera_target_z);
  hash_value(hash, frame.input.pitch);
  hash_value(hash, frame.input.roll);
  hash_value(hash, frame.input.yaw);
  hash_value(hash, frame.input.throttle);
  hash_value(hash, frame.input.buttons);
  hash_value(hash, readback.width);
  hash_value(hash, readback.height);
  hash_value(hash, readback.color_coverage);
  hash_value(hash, readback.depth_coverage);
  hash_value(hash, readback.color_hash);
  hash_value(hash, readback.depth_hash);
  return hash;
}

bool same_readback(const ac6::RenderReadback& left,
                   const ac6::RenderReadback& right) noexcept {
  return left.width == right.width && left.height == right.height &&
      left.color_coverage == right.color_coverage && left.depth_coverage == right.depth_coverage &&
      left.color_hash == right.color_hash && left.depth_hash == right.depth_hash;
}

ac6::InputFrame generated_headless_input(std::size_t tick) noexcept {
  ac6::InputFrame input{};
  input.pitch = static_cast<std::int16_t>((tick % 120u < 60u) ? 4096 : -2048);
  input.roll = static_cast<std::int16_t>((tick % 180u < 90u) ? 1024 : -1024);
  input.yaw = static_cast<std::int16_t>((tick % 240u < 120u) ? 768 : -768);
  input.throttle = static_cast<std::uint8_t>(160u + (tick % 64u));
  return input;
}

bool write_headless_report(const std::filesystem::path& output_dir,
                           std::uint32_t mission_id,
                           const ac6::WorldFrame& frame,
                           const ac6::RenderReadback& readback,
                           const ac6::NativeRenderTarget& target,
                           const ac6::NativeHudSnapshot& hud,
                           ac6::AssetId player_asset_id,
                           std::uint64_t hash,
                           bool deterministic,
                           bool pause_stable,
                           bool save_resume_stable,
                           bool restart_stable) {
  std::ofstream output(output_dir / "native-session.json");
  if (!output) return false;
  output << "{\n"
         << "  \"schema\": \"ac6.native-session.v3\",\n"
         << "  \"mission_id\": " << mission_id << ",\n"
         << "  \"ticks\": " << frame.tick << ",\n"
         << "  \"mission_ready\": " << (frame.mission_ready ? "true" : "false") << ",\n"
         << "  \"player_entity\": " << frame.player_entity << ",\n"
         << "  \"active_units\": " << frame.active_units << ",\n"
         << "  \"position_x\": " << frame.position_x << ",\n"
         << "  \"position_y\": " << frame.position_y << ",\n"
         << "  \"position_z\": " << frame.position_z << ",\n"
         << "  \"pitch\": " << frame.pitch << ",\n"
         << "  \"roll\": " << frame.roll << ",\n"
         << "  \"yaw\": " << frame.yaw << ",\n"
         << "  \"speed\": " << frame.speed << ",\n"
         << "  \"camera_x\": " << frame.camera_x << ",\n"
         << "  \"camera_y\": " << frame.camera_y << ",\n"
         << "  \"camera_z\": " << frame.camera_z << ",\n"
         << "  \"camera_target_x\": " << frame.camera_target_x << ",\n"
         << "  \"camera_target_y\": " << frame.camera_target_y << ",\n"
         << "  \"camera_target_z\": " << frame.camera_target_z << ",\n"
         << "  \"input_pitch\": " << frame.input.pitch << ",\n"
         << "  \"input_roll\": " << frame.input.roll << ",\n"
         << "  \"input_yaw\": " << frame.input.yaw << ",\n"
         << "  \"input_throttle\": " << static_cast<unsigned>(frame.input.throttle) << ",\n"
         << "  \"player_asset_id\": " << player_asset_id << ",\n"
         << "  \"geometry_calls\": " << target.geometry_calls() << ",\n"
         << "  \"raster_triangles\": " << target.raster_triangles() << ",\n"
         << "  \"raster_writes\": " << target.raster_writes() << ",\n"
         << "  \"diagnostic_point_writes\": " << target.diagnostic_point_writes() << ",\n"
         << "  \"filled_fragment_writes\": " << target.filled_fragment_writes() << ",\n"
         << "  \"hud_pixel_writes\": " << hud.pixel_writes << ",\n"
         << "  \"hud_unique_pixels\": " << hud.unique_pixels << ",\n"
         << "  \"hud_objective_count\": " << hud.objective_count << ",\n"
         << "  \"hud_active_objective_id\": " << hud.active_objective_id << ",\n"
         << "  \"hud_target_entity\": " << hud.target_entity << ",\n"
         << "  \"hud_target_locked\": " << (hud.target_locked ? "true" : "false") << ",\n"
         << "  \"hud_primary_weapon_id\": " << hud.primary_weapon_id << ",\n"
         << "  \"hud_weapon_count\": " << hud.weapon_count << ",\n"
         << "  \"hud_radio_message_id\": " << hud.radio_message_id << ",\n"
         << "  \"hud_reticle_visible\": " << (hud.reticle_visible ? "true" : "false") << ",\n"
         << "  \"hud_telemetry_visible\": " << (hud.telemetry_visible ? "true" : "false") << ",\n"
         << "  \"hud_weapon_visible\": " << (hud.weapon_visible ? "true" : "false") << ",\n"
         << "  \"hud_radar_visible\": " << (hud.radar_visible ? "true" : "false") << ",\n"
         << "  \"hud_target_visible\": " << (hud.target_visible ? "true" : "false") << ",\n"
         << "  \"hud_objective_visible\": " << (hud.objective_visible ? "true" : "false") << ",\n"
         << "  \"hud_radio_visible\": " << (hud.radio_visible ? "true" : "false") << ",\n"
         << "  \"hud_pause_visible\": " << (hud.pause_visible ? "true" : "false") << ",\n"
         << "  \"hud_outcome_visible\": " << (hud.outcome_visible ? "true" : "false") << ",\n"
         << "  \"hud_completed_objectives\": " << hud.completed_objectives << ",\n"
         << "  \"hud_failed_objectives\": " << hud.failed_objectives << ",\n"
         << "  \"hud_outcome\": " << static_cast<unsigned>(hud.outcome) << ",\n"
         << "  \"color_coverage\": " << readback.color_coverage << ",\n"
         << "  \"depth_coverage\": " << readback.depth_coverage << ",\n"
         << "  \"color_hash\": " << readback.color_hash << ",\n"
         << "  \"depth_hash\": " << readback.depth_hash << ",\n"
         << "  \"semantic_hash\": \"" << std::hex << hash << std::dec << "\",\n"
         << "  \"deterministic_replay\": " << (deterministic ? "true" : "false") << ",\n"
         << "  \"pause_stable\": " << (pause_stable ? "true" : "false") << ",\n"
         << "  \"save_resume_stable\": " << (save_resume_stable ? "true" : "false") << ",\n"
         << "  \"restart_stable\": " << (restart_stable ? "true" : "false") << "\n"
         << "}\n";
  return static_cast<bool>(output);
}

void write_json_string(std::ostream& output, std::string_view value) {
  output << '"';
  for (const char character : value) {
    switch (character) {
      case '\\': output << "\\\\"; break;
      case '"': output << "\\\""; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default: output << character; break;
    }
  }
  output << '"';
}

bool write_capture_metrics(const std::filesystem::path& output_dir,
                           const ac6::NativeRenderTarget& target) {
  std::ofstream output(output_dir / "capture-metrics.json");
  if (!output) return false;
  output << std::setprecision(9);
  output << "{\n"
         << "  \"schema\": \"ac6.native-raster-capture.v1\",\n"
         << "  \"width\": " << target.width() << ",\n"
         << "  \"height\": " << target.height() << ",\n"
         << "  \"geometry_calls\": " << target.geometry_calls() << ",\n"
         << "  \"raster_triangles\": " << target.raster_triangles() << ",\n"
         << "  \"raster_writes\": " << target.raster_writes() << ",\n"
         << "  \"diagnostic_point_writes\": " << target.diagnostic_point_writes() << ",\n"
         << "  \"filled_fragment_writes\": " << target.filled_fragment_writes() << ",\n"
         << "  \"hud_pixel_writes\": " << target.hud_pixel_writes() << ",\n"
         << "  \"hud_unique_pixels\": " << target.hud_unique_pixels() << ",\n"
         << "  \"drawables\": [\n";
  const auto& metrics = target.raster_metrics();
  for (std::size_t index = 0; index < metrics.size(); ++index) {
    const auto& metric = metrics[index];
    output << "    {\n      \"stable_id\": ";
    write_json_string(output, metric.stable_id);
    output << ",\n"
            << "      \"object_id\": " << metric.object_id << ",\n"
            << "      \"projected_vertices\": " << metric.projected_vertices << ",\n"
            << "      \"restart_markers\": " << metric.restart_markers << ",\n"
            << "      \"candidate_triangles\": " << metric.candidate_triangles << ",\n"
            << "      \"degenerate_triangles\": " << metric.degenerate_triangles << ",\n"
            << "      \"clip_rejected_triangles\": " << metric.clip_rejected_triangles << ",\n"
            << "      \"nondegenerate_triangles\": " << metric.nondegenerate_triangles << ",\n"
            << "      \"fragment_tests\": " << metric.fragment_tests << ",\n"
            << "      \"inside_fragments\": " << metric.inside_fragments << ",\n"
            << "      \"depth_pass_fragments\": " << metric.depth_pass_fragments << ",\n"
            << "      \"color_writes\": " << metric.color_writes << ",\n"
            << "      \"unique_pixels\": " << metric.unique_pixels << ",\n"
            << "      \"screen_bbox\": ";
    if (!metric.screen_bbox_valid) {
      output << "null,\n";
    } else {
      output << "{\"min_x\": " << metric.bbox_min_x
             << ", \"min_y\": " << metric.bbox_min_y
             << ", \"max_x\": " << metric.bbox_max_x
             << ", \"max_y\": " << metric.bbox_max_y << "},\n";
    }
    output << "      \"depth_min\": " << metric.depth_min << ",\n"
            << "      \"depth_max\": " << metric.depth_max << "\n"
            << "    }" << (index + 1u == metrics.size() ? "\n" : ",\n");
  }
  output << "  ]\n}\n";
  return static_cast<bool>(output);
}

int run_play_headless(const std::filesystem::path& manifest_input,
                      std::uint32_t mission_id,
                      const std::filesystem::path& replay_path,
                      const std::filesystem::path& output_dir) {
  constexpr std::size_t kTicks = 1800;
  constexpr float kFixedDt = 1.0f / 60.0f;
  std::error_code error;
  std::filesystem::create_directories(output_dir, error);
  if (error) return 60;

  PlayAssets assets;
  if (!assets.load(manifest_input, mission_id)) return 61;
  ac6::ReplayLog replay;
  const bool replay_exists = std::filesystem::exists(replay_path, error) && !error;
  if (replay_exists) {
    if (!replay.read_file(replay_path)) return 62;
  } else {
    for (std::size_t tick = 0; tick < kTicks; ++tick) replay.append(generated_headless_input(tick));
    if (!replay.write_file(replay_path)) return 63;
  }
  if (replay.frames().size() != kTicks) return 64;

  NativeGraphics graphics;
  if (!graphics.initialize(assets.color_target->width, assets.color_target->height, true)) return 65;
  ac6::NativeRenderTarget target;
  if (!target.resize(assets.color_target->width, assets.color_target->height)) return 66;
  ac6::NativeHudRenderer hud;

  const ac6::MissionObjectiveDatabase* objectives =
      assets.services.has_objectives ? &assets.services.objectives : nullptr;
  const ac6::RadioMessageDatabase* radios =
      assets.services.has_radios ? &assets.services.radios : nullptr;
  ac6::MissionSequenceDirector* sequence =
      assets.services.has_sequence ? &assets.services.sequence : nullptr;
  const ac6::InputMappingDatabase* input =
      assets.services.has_input ? &assets.services.input : nullptr;
  ac6::MissionWaveDirector* waves =
      assets.services.has_waves ? &assets.services.waves : nullptr;
  ac6::MissionAiDirector* ai = assets.services.has_ai ? &assets.services.ai : nullptr;

  auto make_execution = [&]() {
    return std::unique_ptr<ac6::MissionExecution>(new ac6::MissionExecution(
        *assets.definition, &assets.assets, objectives, radios, nullptr, waves, sequence, input, ai));
  };
  std::unique_ptr<ac6::MissionExecution> first = make_execution();
  if (!first->launch(*assets.launch)) return 67;
  ac6::WorldFrame first_frame{};
  ac6::MissionExecution::Checkpoint checkpoint;
  for (std::size_t tick = 0; tick < replay.frames().size(); ++tick) {
    first_frame = first->tick(kFixedDt, replay.frames()[tick]);
    if (tick + 1 == kTicks / 2 && !first->save_checkpoint(checkpoint)) return 69;
  }
  if (!render_and_present(assets, graphics, target, first_frame, first.get(), &hud)) return 68;
  const ac6::RenderReadback first_readback = target.readback();
  ac6::SessionSaveStore save_store;
  if (!save_store.save(1, {mission_id, first->snapshot(), {}, checkpoint}) ||
      !save_store.write_file(output_dir / "session.save")) return 70;

  auto replay_execution = [&](ac6::WorldFrame& final_frame,
                              ac6::RenderReadback& final_readback) -> bool {
    std::unique_ptr<ac6::MissionExecution> execution = make_execution();
    if (!execution->launch(*assets.launch)) return false;
    for (const ac6::InputFrame input_frame : replay.frames()) {
      final_frame = execution->tick(kFixedDt, input_frame);
    }
    ac6::NativeHudRenderer replay_hud;
    if (!render_and_present(assets, graphics, target, final_frame, execution.get(), &replay_hud)) return false;
    final_readback = target.readback();
    return true;
  };
  ac6::WorldFrame second_frame{}, third_frame{};
  ac6::RenderReadback second_readback{}, third_readback{};
  if (!replay_execution(second_frame, second_readback) ||
      !replay_execution(third_frame, third_readback)) return 71;
  const bool deterministic = same_world_frame(first_frame, second_frame) &&
      same_world_frame(first_frame, third_frame) && same_readback(first_readback, second_readback) &&
      same_readback(first_readback, third_readback);
  const bool restart_stable = same_world_frame(first_frame, second_frame) &&
      same_readback(first_readback, second_readback);

  ac6::SessionSaveStore loaded_store;
  if (!loaded_store.read_file(output_dir / "session.save")) return 72;
  const ac6::SessionSaveSnapshot* loaded_snapshot = loaded_store.load(1);
  if (loaded_snapshot == nullptr || !loaded_snapshot->checkpoint.has_value()) return 73;
  std::unique_ptr<ac6::MissionExecution> resumed = make_execution();
  if (!resumed->launch(*assets.launch)) return 74;
  for (std::size_t tick = 0; tick < kTicks / 2; ++tick) {
    if (!resumed->tick(kFixedDt, replay.frames()[tick]).mission_ready) return 75;
  }
  if (!resumed->restore_checkpoint(*loaded_snapshot->checkpoint)) return 76;
  ac6::WorldFrame resumed_frame{};
  for (std::size_t tick = kTicks / 2; tick < kTicks; ++tick) {
    resumed_frame = resumed->tick(kFixedDt, replay.frames()[tick]);
  }
  const bool save_resume_stable = same_world_frame(first_frame, resumed_frame);

  std::unique_ptr<ac6::MissionExecution> paused = make_execution();
  if (!paused->launch(*assets.launch)) return 77;
  const ac6::WorldFrame before_pause = paused->tick(kFixedDt, {});
  if (!paused->dispatch({ac6::EventType::Pause, paused->scenario().player()})) return 78;
  const ac6::WorldFrame during_pause = paused->tick(kFixedDt, {});
  const bool pause_stable = before_pause.tick == during_pause.tick &&
      before_pause.position_x == during_pause.position_x &&
      before_pause.position_y == during_pause.position_y &&
      before_pause.position_z == during_pause.position_z;
  if (!paused->dispatch({ac6::EventType::Resume, paused->scenario().player()})) return 79;
  const ac6::WorldFrame after_resume = paused->tick(kFixedDt, {});
  if (after_resume.tick <= during_pause.tick) return 80;

  ac6::AssetId player_asset_id = 0;
  for (const ac6::UnitRecord& unit : assets.launch->units) {
    if (unit.id == assets.launch->player_entity) player_asset_id = unit.asset;
  }
  if (!target.write_ppm(output_dir / "color.ppm") ||
      !target.write_object_id_ppm(output_dir / "object-id.ppm") ||
      !target.write_depth_f32(output_dir / "depth.f32") ||
      !write_capture_metrics(output_dir, target) ||
      !write_headless_report(output_dir, mission_id, first_frame, first_readback, target,
                             hud.snapshot(),
                             player_asset_id,
                             semantic_hash(first_frame, first_readback), deterministic,
                             pause_stable, save_resume_stable, restart_stable)) return 81;
  return deterministic && pause_stable && save_resume_stable && restart_stable &&
             first_frame.tick == kTicks &&
             first_frame.player_entity != 0 && first_readback.has_world_coverage()
             ? 0
             : 82;
}

// The session command that reads no manifest. Its only input is the retail
// scenario container; everything the session runs on comes out of it.
int run_retail_session(const std::filesystem::path& payload_path,
                       std::uint32_t mission_id,
                       const std::filesystem::path& output_dir) {
  constexpr std::size_t kTicks = 1800;
  constexpr std::size_t kCadence = 300;
  constexpr float kFixedDt = 1.0f / 60.0f;
  std::error_code error;
  std::filesystem::create_directories(output_dir, error);
  if (error) return 111;

  std::ifstream input(payload_path, std::ios::binary);
  if (!input) return 112;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string raw = buffer.str();
  std::unique_ptr<ac6::retail::RetailSession> session = ac6::retail::RetailSession::open(
      std::vector<std::uint8_t>(raw.begin(), raw.end()), {mission_id, {0, 0}});
  if (session == nullptr) return 113;

  ac6::NativeRenderTarget target;
  if (!target.resize(640, 360) || !target.clear(0xFF000000u, 1.0f)) return 114;
  ac6::NativeHudRenderer hud;
  ac6::retail::RetailSessionFrame frame;
  std::size_t advances = 0;
  std::size_t exhausted_at = 0;
  for (std::size_t tick = 1; tick <= kTicks; ++tick) {
    if (tick % kCadence == 0 && !session->script().ended()) {
      advances += 1;
      if (session->advance_script() == ac6::retail::ScriptAdvance::Exhausted) exhausted_at = tick;
    }
    frame = session->tick(kFixedDt, generated_headless_input(tick));
  }
  if (!hud.render(target, frame.world, session->execution())) return 115;
  const ac6::MissionDebrief debrief = session->debrief();
  if (!target.write_ppm(output_dir / "retail-session-hud.ppm")) return 116;

  std::ofstream report(output_dir / "retail-session.json");
  if (!report) return 117;
  report << "{\n"
         << "  \"schema\": \"ac6.native-retail-session.v1\",\n"
         << "  \"mission_id\": " << mission_id << ",\n"
         << "  \"source\": \"retail scenario container only, no manifest\",\n"
         << "  \"ticks\": " << frame.world.tick << ",\n"
         << "  \"units_published\": " << session->world().published << ",\n"
         << "  \"player_entity\": " << session->player_entity() << ",\n"
         << "  \"script_advances\": " << advances << ",\n"
         << "  \"script_exhausted_at_tick\": " << exhausted_at << ",\n"
         << "  \"objectives\": " << debrief.objective_count << ",\n"
         << "  \"objectives_completed\": " << debrief.completed_objectives << ",\n"
         << "  \"hud_pixel_writes\": " << hud.snapshot().pixel_writes << ",\n"
         << "  \"hud_objective_count\": " << hud.snapshot().objective_count << "\n"
         << "}\n";
  if (!report) return 118;
  std::fprintf(stderr,
               "retail_session units=%zu ticks=%llu advances=%zu exhausted_at=%zu completed=%u\n",
               session->world().published,
               static_cast<unsigned long long>(frame.world.tick), advances, exhausted_at,
               debrief.completed_objectives);
  return exhausted_at != 0 && debrief.outcome == ac6::MissionOutcome::Success ? 0 : 119;
}

int run_combat_headless(const std::filesystem::path& manifest_input,
                        std::uint32_t mission_id,
                        const std::filesystem::path& output_path) {
  PlayAssets assets;
  if (!assets.load(manifest_input, mission_id)) return 110;
  const ac6::MissionLaunchDefinition* launch = assets.launch;
  if (launch == nullptr || launch->weapons.empty()) return 111;

  const ac6::UnitRecord* player = nullptr;
  for (const ac6::UnitRecord& unit : launch->units) {
    if (unit.id == launch->player_entity) {
      player = &unit;
      break;
    }
  }
  if (player == nullptr) return 112;

  const ac6::UnitRecord* hostile = nullptr;
  for (const ac6::UnitRecord& unit : launch->units) {
    if (unit.id != launch->player_entity && unit.owner != player->owner) {
      hostile = &unit;
      break;
    }
  }
  if (hostile == nullptr) return 113;

  const ac6::MissionObjectiveDatabase* objectives =
      assets.services.has_objectives ? &assets.services.objectives : nullptr;
  const ac6::RadioMessageDatabase* radios =
      assets.services.has_radios ? &assets.services.radios : nullptr;
  ac6::MissionSequenceDirector* sequence =
      assets.services.has_sequence ? &assets.services.sequence : nullptr;
  const ac6::InputMappingDatabase* input =
      assets.services.has_input ? &assets.services.input : nullptr;
  ac6::MissionWaveDirector* waves =
      assets.services.has_waves ? &assets.services.waves : nullptr;
  ac6::MissionAiDirector* ai = assets.services.has_ai ? &assets.services.ai : nullptr;
  ac6::MissionExecution execution(*assets.definition, &assets.assets, objectives, radios,
                                   nullptr, waves, sequence, input, ai);
  if (!execution.launch(*launch)) return 114;

  const ac6::CombatUnitState* initial_target = execution.combat().unit(hostile->id);
  const std::size_t active_before = execution.combat().active_units();
  if (initial_target == nullptr) return 115;
  const float health_before = initial_target->health;
  const std::uint32_t weapon_id = launch->weapons.front().id;
  const bool target_locked = execution.lock_target(hostile->id);
  bool weapon_fired = false;
  std::uint32_t shots_fired = 0;
  for (std::uint32_t shot = 0; shot < 4 && target_locked; ++shot) {
    if (execution.fire_weapon(weapon_id)) {
      weapon_fired = true;
      ++shots_fired;
    }
    for (int step = 0; step < 4; ++step) {
      (void)execution.tick(0.25f, {});
    }
    const ac6::CombatUnitState* current = execution.combat().unit(hostile->id);
    if (current == nullptr || !current->active) break;
  }
  const ac6::CombatUnitState* final_target = execution.combat().unit(hostile->id);
  const float health_after = final_target == nullptr ? 0.0f : final_target->health;
  const bool target_destroyed = final_target != nullptr && !final_target->active &&
      health_after == 0.0f;
  const std::size_t active_after = execution.combat().active_units();
  const bool damage_applied = execution.combat().damage_events() != 0 &&
      health_after < health_before;
  if (output_path.empty()) return 116;
  std::ofstream output(output_path);
  if (!output) return 117;
  output << "{\n"
         << "  \"schema\": \"ac6.native-combat.v1\",\n"
         << "  \"mission_id\": " << mission_id << ",\n"
         << "  \"player_entity\": " << player->id << ",\n"
         << "  \"target_entity\": " << hostile->id << ",\n"
         << "  \"target_faction\": " << hostile->owner << ",\n"
         << "  \"target_locked\": " << (target_locked ? "true" : "false") << ",\n"
         << "  \"weapon_id\": " << weapon_id << ",\n"
         << "  \"weapon_fired\": " << (weapon_fired ? "true" : "false") << ",\n"
         << "  \"shots_fired\": " << shots_fired << ",\n"
         << "  \"health_before\": " << health_before << ",\n"
         << "  \"health_after\": " << health_after << ",\n"
         << "  \"damage_events\": " << execution.combat().damage_events() << ",\n"
         << "  \"active_units_before\": " << active_before << ",\n"
         << "  \"active_units_after\": " << active_after << ",\n"
         << "  \"damage_applied\": " << (damage_applied ? "true" : "false") << ",\n"
         << "  \"target_destroyed\": " << (target_destroyed ? "true" : "false") << "\n"
         << "}\n";
  if (!output) return 118;
  return target_locked && weapon_fired && damage_applied && target_destroyed &&
             active_after < active_before
         ? 0
         : 119;
}

int run_play_interactive(const std::filesystem::path& manifest_input,
                         std::uint32_t mission_id) {
  constexpr float kFixedDt = 1.0f / 60.0f;
  PlayAssets assets;
  if (!assets.load(manifest_input, mission_id)) return 90;
  NativeGraphics graphics;
  if (!graphics.initialize(assets.color_target->width, assets.color_target->height, false)) return 91;
  ac6::NativeRenderTarget target;
  if (!target.resize(assets.color_target->width, assets.color_target->height)) return 92;
  ac6::NativeHudRenderer hud;
  ac6::SdlEventPump pump;
  if (!pump.initialize()) return 93;
  ac6::SdlInputAdapter input_adapter;
  const ac6::MissionObjectiveDatabase* objectives =
      assets.services.has_objectives ? &assets.services.objectives : nullptr;
  const ac6::RadioMessageDatabase* radios =
      assets.services.has_radios ? &assets.services.radios : nullptr;
  ac6::MissionSequenceDirector* sequence =
      assets.services.has_sequence ? &assets.services.sequence : nullptr;
  const ac6::InputMappingDatabase* input_mapping =
      assets.services.has_input ? &assets.services.input : nullptr;
  ac6::MissionWaveDirector* waves =
      assets.services.has_waves ? &assets.services.waves : nullptr;
  ac6::MissionAiDirector* ai = assets.services.has_ai ? &assets.services.ai : nullptr;
  ac6::MissionExecution execution(*assets.definition, &assets.assets, objectives, radios,
                                   nullptr, waves, sequence, input_mapping, ai);
  if (!execution.launch(*assets.launch)) return 94;
  ac6::InputFrame input_frame{};
  std::uint16_t buttons = 0;
  std::vector<ac6::Event> events;
  bool quit = false;
  ac6::WorldFrame frame = execution.tick(kFixedDt, input_frame);
  if (!render_and_present(assets, graphics, target, frame, &execution, &hud)) return 95;
  using Clock = std::chrono::steady_clock;
  auto previous = Clock::now();
  double accumulator = 0.0;
  while (!quit) {
    events.clear();
    if (!pump.pump(input_adapter, input_frame, buttons,
                   assets.services.input, execution.scenario().player(), events, quit)) return 96;
    for (const ac6::Event event : events) (void)execution.dispatch(event);
    const auto now = Clock::now();
    const double elapsed = std::chrono::duration<double>(now - previous).count();
    previous = now;
    accumulator = std::min(accumulator + elapsed, 0.25);
    bool stepped = false;
    while (accumulator >= static_cast<double>(kFixedDt)) {
      frame = execution.tick(kFixedDt, input_frame);
      accumulator -= static_cast<double>(kFixedDt);
      stepped = true;
    }
    if (stepped && !render_and_present(assets, graphics, target, frame, &execution, &hud)) return 97;
    SDL_Delay(1);
  }
  return 0;
}

int run_services_smoke(int argc, char** argv) {
  std::uint32_t mission_id = 0;
  if (!parse_u32(argv[3], mission_id)) return 27;
  ac6::MissionManifestLoader loader;
  ac6::MissionCatalog catalog;
  ac6::MissionAssetDatabase assets;
  ac6::MissionLaunchDatabase launches;
  ac6::MissionRuntimeServices services;
  if (!loader.load_runtime(argv[2], catalog, assets, launches, services)) return 28;
  const ac6::MissionDefinition* definition = catalog.find(mission_id);
  const ac6::MissionLaunchDefinition* launch = launches.find(mission_id);
  if (definition == nullptr || launch == nullptr) return 29;
  const std::vector<ac6::InputFrame> inputs = {
      {12000, -8000, 4000, 192, 0}, {-4000, 1000, 2000, 220, 0},
      {0, 0, 0, 180, 0}, {7000, 0, -3000, 200, 0}};
  const ac6::MissionObjectiveDatabase* objectives =
      services.has_objectives ? &services.objectives : nullptr;
  const ac6::RadioMessageDatabase* radios = services.has_radios ? &services.radios : nullptr;
  ac6::MissionSequenceDirector* sequence = services.has_sequence ? &services.sequence : nullptr;
  const ac6::InputMappingDatabase* input = services.has_input ? &services.input : nullptr;
  ac6::MissionWaveDirector* waves = services.has_waves ? &services.waves : nullptr;
  ac6::MissionAiDirector* ai = services.has_ai ? &services.ai : nullptr;
  ac6::MissionExecution execution(*definition, &assets, objectives, radios,
                                   nullptr, waves, sequence, input, ai);
  if (!execution.launch(*launch)) return 30;
  ac6::ReplayLog replay;
  ac6::SaveStore saves;
  ac6::WorldFrame expected{};
  for (std::size_t i = 0; i < 30; ++i) {
    const ac6::InputFrame input = inputs[i % inputs.size()];
    replay.append(input);
    expected = execution.tick(1.0f / 60.0f, input);
    if (i == 14 && !saves.save(1, execution.snapshot())) return 31;
  }
  if (!saves.write_file(argv[4]) || !replay.write_file(argv[5])) return 32;
  ac6::SaveStore loaded_saves;
  ac6::ReplayLog loaded_replay;
  if (!loaded_saves.read_file(argv[4]) || !loaded_replay.read_file(argv[5]) ||
      loaded_saves.load(1) == nullptr) return 33;
  ac6::MissionExecution replayed(*definition, &assets);
  if (!replayed.launch(*launch)) return 34;
  ac6::WorldFrame replay_frame{};
  for (const ac6::InputFrame input : loaded_replay.frames()) {
    replay_frame = replayed.tick(1.0f / 60.0f, input);
  }
  if (replay_frame.tick != expected.tick || replay_frame.position_x != expected.position_x ||
      replay_frame.position_y != expected.position_y || replay_frame.position_z != expected.position_z) {
    return 35;
  }
  ac6::MissionExecution resumed(*definition, &assets);
  if (!resumed.launch(*launch)) return 36;
  for (std::size_t i = 0; i < 15; ++i) resumed.tick(1.0f / 60.0f, inputs[i % inputs.size()]);
  if (!resumed.restore(*loaded_saves.load(1))) return 37;
  if (resumed.snapshot().tick != loaded_saves.load(1)->tick) return 38;
  ac6::WorldFrame resumed_frame{};
  for (std::size_t i = 15; i < loaded_replay.frames().size(); ++i) {
    resumed_frame = resumed.tick(1.0f / 60.0f, loaded_replay.frames()[i]);
  }
  return resumed_frame.tick == expected.tick && resumed_frame.position_x == expected.position_x &&
                 resumed_frame.position_y == expected.position_y && resumed_frame.position_z == expected.position_z
             ? 0
             : 39;
}

int run_present_manifest(int argc, char** argv) {
  std::uint32_t mission_id = 0;
  if (!parse_u32(argv[3], mission_id)) return 8;
  ac6::MissionManifestLoader loader;
  ac6::MissionManifestPaths paths;
  ac6::MissionCatalog catalog;
  ac6::MissionAssetDatabase assets;
  ac6::MissionLaunchDatabase launches;
  ac6::MissionRuntimeServices services;
  if (!loader.load_paths(argv[2], paths) || !paths.render_valid() ||
      !loader.load_runtime(argv[2], catalog, assets, launches, services)) return 9;
  ac6::MissionRenderDatabase render;
  ac6::MissionDrawableDatabase drawables;
  ac6::MissionTransformDatabase transforms;
  ac6::MissionMaterialDatabase materials;
  ac6::MissionTextureDatabase textures;
  ac6::ShaderPermutationDatabase shaders;
  ac6::MissionRenderTargetDatabase targets;
  ac6::MissionRenderPassDatabase passes;
  ac6::MissionRenderResolveDatabase resolves;
  ac6::QualifiedBufferDatabase buffers;
  ac6::NativeGeometryDatabase geometries;
  ac6::MissionCameraDatabase cameras;
  if (!paths.camera.empty() && !loader.load_camera(argv[2], cameras)) return 12;
  if (!loader.load_render(argv[2], render, drawables, transforms, materials, textures,
                          shaders, targets, passes, resolves, buffers, geometries)) return 10;
  const ac6::MissionDefinition* definition = catalog.find(mission_id);
  const ac6::MissionRenderDefinition* render_definition = render.find(mission_id);
  const ac6::MissionLaunchDefinition* launch = launches.find(mission_id);
  const ac6::MissionRenderPass* pass = passes.find(mission_id, "world");
  if (definition == nullptr || render_definition == nullptr || launch == nullptr || pass == nullptr) return 11;
  const ac6::MissionObjectiveDatabase* objective_database =
      services.has_objectives ? &services.objectives : nullptr;
  const ac6::RadioMessageDatabase* radio_database = services.has_radios ? &services.radios : nullptr;
  ac6::MissionSequenceDirector* sequence = services.has_sequence ? &services.sequence : nullptr;
  const ac6::InputMappingDatabase* input = services.has_input ? &services.input : nullptr;
  ac6::MissionWaveDirector* waves = services.has_waves ? &services.waves : nullptr;
  ac6::MissionAiDirector* ai = services.has_ai ? &services.ai : nullptr;
  ac6::MissionExecution execution(*definition, &assets, objective_database, radio_database,
                                   nullptr, waves, sequence, input, ai);
  if (!execution.launch(*launch)) return 13;
  const ac6::WorldFrame world = execution.tick(1.0f / 60.0f, {});
  const ac6::MissionRenderTargetDefinition* target_definition = targets.find(mission_id, pass->color_target);
  if (target_definition == nullptr) return 14;
  ac6::NativeRenderTarget target;
  if (!target.resize(target_definition->width, target_definition->height) ||
      !target.clear(pass->clear_color, pass->clear_depth)) return 15;
  ac6::VulkanRenderer renderer;
  const ac6::VulkanRenderer::RenderAssets render_assets{
      &assets, render_definition, &drawables, &buffers, &geometries, &transforms,
      &materials, &textures, &shaders, &targets, &passes, &resolves, cameras.find(mission_id)};
  if (!renderer.render(world, render_assets, &target)) return 16;
  if (std::getenv("AC6_RENDER_STATS") != nullptr) {
    std::fprintf(stderr, "render_stats geometry=%u triangles=%u writes=%llu coverage=%u\n",
                 target.geometry_calls(), target.raster_triangles(),
                 static_cast<unsigned long long>(target.raster_writes()), target.readback().color_coverage);
  }
  if (argc >= 5 && !target.write_ppm(argv[4])) return 18;
  if (argc == 6 && !target.write_depth_f32(argv[5])) return 18;
  return ac6::interactive::present_target(target) ? 0 : 17;
}

}  // namespace

int run_commands(int argc, char** argv) {
  ac6::MissionRuntime runtime(1);
  const auto frame = runtime.tick(1.0f / 60.0f, {});
  if (argc == 4 && std::string_view(argv[1]) == "--play") {
    std::uint32_t mission_id = 0;
    if (!parse_u32(argv[3], mission_id) || mission_id == 0) return 98;
    return run_play_interactive(argv[2], mission_id);
  }
  if (argc == 6 && std::string_view(argv[1]) == "--play-headless") {
    std::uint32_t mission_id = 0;
    if (!parse_u32(argv[3], mission_id) || mission_id == 0) return 99;
    return run_play_headless(argv[2], mission_id, argv[4], argv[5]);
  }
  if (argc == 5 && std::string_view(argv[1]) == "--retail-session") {
    std::uint32_t mission_id = 0;
    if (!parse_u32(argv[3], mission_id) || mission_id == 0) return 110;
    return run_retail_session(argv[2], mission_id, argv[4]);
  }
  if (argc == 5 && std::string_view(argv[1]) == "--combat-headless") {
    std::uint32_t mission_id = 0;
    if (!parse_u32(argv[3], mission_id) || mission_id == 0) return 109;
    return run_combat_headless(argv[2], mission_id, argv[4]);
  }
  if (argc == 6 && std::string_view(argv[1]) == "--compare-mission01") {
    std::uint32_t mission_id = 0;
    if (!parse_u32(argv[3], mission_id) || mission_id != 1u) return 40;
    ac6::Mission01Reference reference;
    if (!reference.load(argv[4]) || reference.width() != 1280u || reference.height() != 720u) return 41;
    ac6::MissionManifestLoader loader;
    ac6::MissionManifestPaths paths;
    ac6::MissionCatalog catalog;
    ac6::MissionAssetDatabase assets;
    ac6::MissionLaunchDatabase launches;
    if (!loader.load_paths(argv[2], paths) || !paths.render_valid() ||
        !loader.load_runtime(argv[2], catalog, assets, launches)) return 42;
    ac6::MissionRenderDatabase render;
    ac6::MissionDrawableDatabase drawables;
    ac6::MissionTransformDatabase transforms;
    ac6::MissionMaterialDatabase materials;
    ac6::MissionTextureDatabase textures;
    ac6::ShaderPermutationDatabase shaders;
    ac6::MissionRenderTargetDatabase targets;
    ac6::MissionRenderPassDatabase passes;
    ac6::MissionRenderResolveDatabase resolves;
    ac6::QualifiedBufferDatabase buffers;
    ac6::NativeGeometryDatabase geometries;
    ac6::MissionCameraDatabase cameras;
    if (paths.camera.empty() || !loader.load_camera(argv[2], cameras) ||
        cameras.find(mission_id) == nullptr || !cameras.find(mission_id)->qualified) return 43;
    if (!loader.load_render(argv[2], render, drawables, transforms, materials, textures,
                            shaders, targets, passes, resolves, buffers, geometries)) return 43;
    const ac6::MissionDefinition* definition = catalog.find(mission_id);
    const ac6::MissionLaunchDefinition* launch = launches.find(mission_id);
    const ac6::MissionRenderDefinition* render_definition = render.find(mission_id);
    const ac6::MissionRenderPass* pass = passes.find(mission_id, "world");
    if (definition == nullptr || launch == nullptr || render_definition == nullptr || pass == nullptr) return 44;

    const auto run = [&](std::vector<ac6::WorldFrame>& checkpoints, ac6::WorldFrame& final) {
      ac6::MissionExecution execution(*definition, &assets);
      if (!execution.launch(*launch)) return false;
      std::size_t checkpoint_index = 0;
      for (const ac6::InputFrame input : reference.replay().frames()) {
        final = execution.tick(1.0f / 60.0f, input);
        if (checkpoint_index < reference.checkpoints().size() &&
            final.tick == reference.checkpoints()[checkpoint_index].frame.tick) {
          checkpoints.push_back(final);
          ++checkpoint_index;
        }
      }
      return final.tick == 1800u && checkpoint_index == reference.checkpoints().size();
    };
    std::vector<ac6::WorldFrame> first_checkpoints, second_checkpoints, third_checkpoints;
    ac6::WorldFrame first{}, second{}, third{};
    if (!run(first_checkpoints, first) || !run(second_checkpoints, second) ||
        !run(third_checkpoints, third)) return 45;
    bool deterministic = same_world_frame(first, second) && same_world_frame(first, third) &&
        first_checkpoints.size() == second_checkpoints.size() &&
        first_checkpoints.size() == third_checkpoints.size();
    for (std::size_t i = 0; deterministic && i < first_checkpoints.size(); ++i) {
      deterministic = same_world_frame(first_checkpoints[i], second_checkpoints[i]) &&
                      same_world_frame(first_checkpoints[i], third_checkpoints[i]);
    }
    const ac6::MissionRenderTargetDefinition* target_definition =
        targets.find(mission_id, pass->color_target);
    if (target_definition == nullptr || target_definition->width != 1280u ||
        target_definition->height != 720u) return 46;
    ac6::NativeRenderTarget target;
    if (!target.resize(target_definition->width, target_definition->height) ||
        !target.clear(pass->clear_color, pass->clear_depth)) return 47;
    const ac6::VulkanRenderer::RenderAssets render_assets{
        &assets, render_definition, &drawables, &buffers, &geometries, &transforms,
        &materials, &textures, &shaders, &targets, &passes, &resolves,
        cameras.find(mission_id)};
    ac6::VulkanRenderer renderer;
    if (!renderer.render(first, render_assets, &target)) return 48;
    const ac6::Mission01ComparisonResult result = ac6::Mission01Comparator{}.compare(
        reference, first_checkpoints, target, deterministic, argv[5]);
    return result.passed() ? 0 : 49;
  }
  if (argc == 3 && std::string_view(argv[1]) == "--validate-manifest") {
    ac6::MissionManifestLoader loader;
    ac6::MissionManifestPaths paths;
    ac6::MissionCatalog catalog;
    ac6::MissionAssetDatabase assets;
    ac6::MissionLaunchDatabase launches;
    ac6::MissionRuntimeServices services;
    if (!loader.load_paths(argv[2], paths) || !paths.render_valid() ||
        !loader.load_runtime(argv[2], catalog, assets, launches, services)) return 6;
    if (!paths.campaign.empty() && !services.has_campaign) return 6;
    ac6::MissionRenderDatabase render;
    ac6::MissionDrawableDatabase drawables;
    ac6::MissionTransformDatabase transforms;
    ac6::MissionMaterialDatabase materials;
    ac6::MissionTextureDatabase textures;
    ac6::ShaderPermutationDatabase shaders;
    ac6::MissionRenderTargetDatabase targets;
    ac6::MissionRenderPassDatabase passes;
    ac6::MissionRenderResolveDatabase resolves;
    ac6::QualifiedBufferDatabase buffers;
    return loader.load_render(argv[2], render, drawables, transforms, materials,
                              textures, shaders, targets, passes, resolves, buffers) ? 0 : 7;
  }
  if (argc == 4 && std::string_view(argv[1]) == "--frontend-smoke") {
    std::uint32_t mission_id = 0;
    if (!parse_u32(argv[3], mission_id)) return 20;
    ac6::MissionManifestLoader loader;
    ac6::MissionManifestPaths paths;
    ac6::MissionCatalog catalog;
    ac6::MissionAssetDatabase assets;
    ac6::MissionLaunchDatabase launches;
    ac6::MissionRuntimeServices services;
    if (!loader.load_paths(argv[2], paths) || paths.input.empty() ||
        !loader.load_runtime(argv[2], catalog, assets, launches, services) ||
        !services.has_input) return 21;
    if (catalog.find(mission_id) == nullptr || launches.find(mission_id) == nullptr) return 22;
    if (!paths.controls.empty()) {
      ac6::SdlAxisMapping axes;
      ac6::SdlKeyboardMapping keyboard;
      if (!ac6::SdlInputProfile::load_manifest(paths.controls, axes, keyboard)) return 23;
      ac6::SdlInputAdapter qualified_input(axes, keyboard);
      if (!qualified_input.mapping().valid() || !qualified_input.keyboard_mapping().valid()) return 23;
    }
    ac6::FrontendController frontend;
    if (!frontend.configure({ac6::FrontendDifficulty::Normal, ac6::FrontendControls::Normal,
                             ac6::FrontendLanguage::English}) ||
        !frontend.select_mission(catalog, mission_id)) return 24;
    for (int phase = 0; phase < 5; ++phase) {
      if (!frontend.dispatch_buttons(services.input, 1u)) return 25;
    }
    return frontend.state() == ac6::FrontendState::Mission &&
                   frontend.mission_definition(catalog) != nullptr
               ? 0
               : 26;
  }
  if (argc == 6 && std::string_view(argv[1]) == "--services-smoke") {
    return run_services_smoke(argc, argv);
  }
  if ((argc >= 4 && argc <= 6) && std::string_view(argv[1]) == "--present-manifest") {
    return run_present_manifest(argc, argv);
  }
  if (argc < 2 || std::string_view(argv[1]) != "--present-smoke") {
    (void)frame;
    return 0;
  }
  ac6::NativeRenderTarget target;
  if (!target.resize(320, 180) || !target.clear(0xFF10243Au, 1.0f)) return 18;
  return ac6::interactive::present_target(target) ? 0 : 19;
}
