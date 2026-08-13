#include "ac6/retail_commands.h"

#include "ac6/campaign_progression.h"
#include "ac6/execution_trace.h"
#include "ac6/native_hud.h"
#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_camera_table.h"
#include "ac6/retail_content.h"
#include "ac6/retail_frontend_resources.h"
#include "ac6/retail_mission01_cpu_compositor.h"
#include "ac6/retail_projection_receipt.h"
#include "ac6/retail_session.h"
#include "ac6/retail_session_replay.h"
#include "ac6/sdl_input.h"
#include "ac6/vulkan_backend.h"
#include "ac6/vulkan_scene_resource_cache.h"
#include "ac6/retail_mission01_vulkan_scene.h"
#include "vulkan_retail_shaders.h"

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
  std::filesystem::path projection_receipt;
  std::filesystem::path report;
  std::filesystem::path capture;
  std::filesystem::path scene_capture;
  std::filesystem::path scene_report;
  std::filesystem::path trace;
  bool projection_receipt_seen{};
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
    else if (option == "--projection-receipt" &&
             !options.projection_receipt_seen) {
      if (value.empty()) return false;
      options.projection_receipt = value;
      options.projection_receipt_seen = true;
    } else if (option == "--report" && options.report.empty()) options.report = value;
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

  bool present_rgba8(std::span<const std::uint8_t> pixels,
                     std::uint32_t width, std::uint32_t height) noexcept {
    return presenter.valid() && presenter.present_rgba8(pixels, width, height);
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

std::array<float, 16> preview_object_to_clip(
    const retail::Mission01MapPrimitive& primitive,
    const retail::Mission01MapDrawInstance& instance) noexcept {
  if (primitive.geometry.positions.empty()) {
    return {1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F};
  }
  std::array<float, 3> minimum{primitive.geometry.positions.front().x,
                                primitive.geometry.positions.front().y,
                                primitive.geometry.positions.front().z};
  std::array<float, 3> maximum = minimum;
  for (const retail::NdxrPosition& position : primitive.geometry.positions) {
    minimum[0] = std::min(minimum[0], position.x);
    minimum[1] = std::min(minimum[1], position.y);
    minimum[2] = std::min(minimum[2], position.z);
    maximum[0] = std::max(maximum[0], position.x);
    maximum[1] = std::max(maximum[1], position.y);
    maximum[2] = std::max(maximum[2], position.z);
  }
  const std::array<float, 3> centre{
      instance.world_x + (minimum[0] + maximum[0]) * 0.5F,
      instance.world_y + (minimum[1] + maximum[1]) * 0.5F,
      instance.world_z + (minimum[2] + maximum[2]) * 0.5F};
  const float span_x = std::max(maximum[0] - minimum[0], 1.0F);
  const float span_y = std::max(maximum[1] - minimum[1], 1.0F);
  const float span_z = std::max(maximum[2] - minimum[2], 1.0F);
  const float extent = std::max({span_x, span_y, span_z});
  const float scale = 1.6F / extent;
  return {scale, 0.0F, 0.0F, -centre[0] * scale,
          0.0F, scale, 0.0F, -centre[1] * scale,
          0.0F, 0.0F, scale, -centre[2] * scale,
          0.0F, 0.0F, 0.0F, 1.0F};
}

class NativeMission01GpuRenderer final {
 public:
  NativeMission01GpuRenderer() = default;
  NativeMission01GpuRenderer(const NativeMission01GpuRenderer&) = delete;
  NativeMission01GpuRenderer& operator=(const NativeMission01GpuRenderer&) = delete;

  bool initialize(const RetailContentStore& store) {
    auto created = VulkanBackend::create();
    if (!created) return false;
    backend_ = std::move(created.backend);
    target_ = backend_->create_render_target(1280U, 720U, false);
    if (!target_) return false;

    std::optional<retail::RetailMission01MapRenderAssets> assets =
        retail::RetailMission01MapRenderAssets::open(store);
    if (!assets.has_value() || assets->draw_instances().empty()) return false;
    std::uint32_t draw_index = 0U;
    const retail::Mission01MapPrimitive* primitive = nullptr;
    for (std::uint32_t index = 0U;
         index < assets->draw_instances().size(); ++index) {
      primitive = assets->primitive_for(assets->draw_instances()[index]);
      if (primitive != nullptr && !primitive->geometry.positions.empty() &&
          !primitive->geometry.indices.empty()) {
        draw_index = index;
        break;
      }
      primitive = nullptr;
    }
    if (primitive == nullptr) return false;
    const std::array<float, 16> object_to_clip = preview_object_to_clip(
        *primitive, assets->draw_instances()[draw_index]);
    scene_ = retail::RetailMission01VulkanScene::open_assets(
        std::move(*assets), draw_index, true, object_to_clip,
        detail::kRetailClipVertexSpirv,
        detail::kRetailTexturedFragmentSpirv,
        0x6d30315f636c6970ULL, 0x6d30315f74657874ULL, 1280U, 720U);
    if (!scene_.has_value()) return false;
    resources_ = std::make_unique<VulkanSceneResourceCache>(*backend_);
    const VulkanSceneClipTexturedMeshUpload mesh = scene_->mesh_upload();
    const VulkanSceneTexturedMaterialUpload material = scene_->material_upload();
    const VulkanSceneTextureUpload texture = scene_->texture_upload();
    const std::array<VulkanSceneClipTexturedMeshUpload, 1> meshes{mesh};
    const std::array<VulkanSceneTexturedMaterialUpload, 1> materials{material};
    const std::array<VulkanSceneTextureUpload, 1> textures{texture};
    return resources_->build_clip_textured(scene_->scene(), target_, meshes,
                                           materials, textures);
  }

  bool render(std::vector<std::uint8_t>& pixels) noexcept {
    if (!resources_ || !scene_.has_value() || !resources_->render(scene_->scene())) {
      return false;
    }
    pixels = backend_->readback_rgba8(target_);
    return pixels.size() == 1280U * 720U * 4U;
  }

  const retail::RetailMission01VulkanSceneReport* report() const noexcept {
    return scene_.has_value() ? &scene_->report() : nullptr;
  }

 private:
  std::unique_ptr<VulkanBackend> backend_;
  std::unique_ptr<VulkanSceneResourceCache> resources_;
  std::optional<retail::RetailMission01VulkanScene> scene_;
  VulkanRenderTargetHandle target_{};
};

bool render_frame(NativeGraphics& graphics, NativeMission01GpuRenderer& renderer,
                  std::vector<std::uint8_t>& pixels,
                  const retail::RetailSessionFrame& frame) {
  (void)frame;
  return renderer.render(pixels) && graphics.present_rgba8(pixels, 1280U, 720U);
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

bool write_rgba8_ppm(const std::filesystem::path& path,
                     std::span<const std::uint8_t> pixels,
                     std::uint32_t width, std::uint32_t height) {
  if (width == 0U || height == 0U ||
      pixels.size() != static_cast<std::size_t>(width) * height * 4U) {
    return false;
  }
  std::error_code error;
  const std::filesystem::path parent = path.parent_path();
  if (!parent.empty()) std::filesystem::create_directories(parent, error);
  if (error) return false;
  std::ofstream output(path, std::ios::binary);
  if (!output) return false;
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (std::size_t index = 0U; index < pixels.size(); index += 4U) {
    output.put(static_cast<char>(pixels[index]));
    output.put(static_cast<char>(pixels[index + 1U]));
    output.put(static_cast<char>(pixels[index + 2U]));
  }
  return static_cast<bool>(output);
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
                                   retail::RetailScriptDrive::QualifiedRuntime});
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
  std::optional<NativeRenderTarget> diagnostic_target;
  if (!options.scene_capture.empty()) {
    diagnostic_target.emplace();
    if (!diagnostic_target->resize(1280, 720)) return 126;
  }
  std::optional<NativeMission01GpuRenderer> gpu_renderer;
  if (options.scene_capture.empty()) {
    gpu_renderer.emplace();
    if (!gpu_renderer->initialize(store)) {
      std::fprintf(stderr,
                   "ac6_retail=fail error=vulkan_scene_unavailable "
                   "detail=mission01_clip_upload\n");
      return 126;
    }
  }
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
  std::vector<std::uint8_t> gpu_pixels;
  if (options.scene_capture.empty()) {
    if (!gpu_renderer.has_value() ||
        !render_frame(graphics, *gpu_renderer, gpu_pixels, frame)) return 128;
  } else if (!present_retail_scene_capture(
                 graphics, *diagnostic_target, hud, *session, frame, *scene_compositor,
                 loadout, options.scene_capture, options.scene_report)) {
    return 128;
  }
  bool capture_written = false;
  const auto capture = [&]() -> bool {
    if (options.capture.empty() || capture_written) return true;
    if (options.scene_capture.empty()) {
      if (!write_rgba8_ppm(options.capture, gpu_pixels, 1280U, 720U)) return false;
    } else if (!diagnostic_target.has_value() ||
               !diagnostic_target->write_ppm(options.capture)) {
      return false;
    }
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
    if (stepped && (!gpu_renderer.has_value() ||
                    !render_frame(graphics, *gpu_renderer, gpu_pixels, frame))) {
      return 128;
    }
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
        !saves.save(1, {1, store.index_sha256(), session->execution().snapshot(),
                        session->campaign_snapshot(), checkpoint}) ||
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

std::uint64_t hash_combat_state(std::uint64_t hash,
                                const MissionExecution& execution) noexcept {
  const std::vector<CombatUnitState> units = execution.combat().snapshot_units();
  hash_u64(hash, units.size());
  for (const CombatUnitState& unit : units) {
    hash_u32(hash, unit.entity);
    hash_u32(hash, unit.faction);
    for (const float value : {unit.position.x, unit.position.y, unit.position.z,
                              unit.health, unit.max_health,
                              unit.collision_radius}) {
      hash_float(hash, value);
    }
    hash_u32(hash, unit.active ? 1U : 0U);
  }
  hash_u64(hash, execution.combat().active_projectiles());
  hash_u64(hash, execution.combat().damage_events());
  hash_u32(hash, execution.locked_target());
  hash_u32(hash, execution.primary_weapon_id());
  hash_u32(hash, execution.weapon_count());
  return hash;
}

std::uint64_t hash_campaign_state(std::uint64_t hash,
                                  const retail::RetailSession& session) noexcept {
  const CampaignSaveSnapshot snapshot = session.campaign_snapshot();
  hash_u64(hash, snapshot.completed.size());
  for (const CampaignSaveSnapshot::Record& record : snapshot.completed) {
    hash_u32(hash, record.mission_id);
    hash_u32(hash, record.objective_mask);
    hash_u32(hash, static_cast<std::uint32_t>(record.state));
    hash_u32(hash, record.loadout.aircraft_id);
    hash_u32(hash, record.loadout.weapon_id);
    hash_u32(hash, record.loadout.capability_data_valid ? 1u : 0u);
  }
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
                                   retail::RetailScriptDrive::QualifiedRuntime});
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
    result.semantic_hash = hash_combat_state(result.semantic_hash,
                                             session->execution());
    result.semantic_hash = hash_campaign_state(result.semantic_hash, *session);
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
  std::optional<retail::RetailProjectionReceiptPreflight> projection_preflight;
  if (options.projection_receipt_seen) {
    projection_preflight = retail::preflight_retail_projection_receipt(
        options.projection_receipt, options.replay, replay, store.index_sha256());
    if (!projection_preflight->passed()) {
      std::fprintf(
          stderr,
          "ac6_retail=fail error=projection_receipt_%s detail=%s receipt=%s "
          "replay=%s\n",
          retail::retail_projection_receipt_error_name(
              projection_preflight->error),
          projection_preflight->detail.c_str(),
          options.projection_receipt.string().c_str(),
          options.replay.string().c_str());
      return 140;
    }
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
         << "  \"projection_receipt_provided\": "
         << (options.projection_receipt_seen ? "true" : "false") << ",\n"
         << "  \"native_output_verified\": "
         << (projection_preflight.has_value() &&
                     projection_preflight->native_output_verified
                 ? "true"
                 : "false")
         << ",\n"
         << "  \"source_lineage_verified\": "
         << (projection_preflight.has_value() &&
                     projection_preflight->source_lineage_verified
                 ? "true"
                 : "false")
         << ",\n";
  if (projection_preflight.has_value()) {
    output << "  \"projection_receipt_sha256\": \""
           << sha256_hex(projection_preflight->receipt_sha256) << "\",\n"
           << "  \"projection_replay_sha256\": \""
           << sha256_hex(projection_preflight->replay_sha256) << "\",\n";
  }
  output << "  \"replay_frames\": " << replay.frames.size() << ",\n"
         << "  \"random_seed\": " << replay.random_seed << ",\n"
         << "  \"replay_final_tick\": " << replay.final_tick << ",\n"
         << "  \"checkpoint_count\": " << replay.checkpoints.size() << ",\n"
         << "  \"input_digest\": \"" << sha256_hex(replay.input_digest()) << "\",\n"
         << "  \"final_digest\": \"" << sha256_hex(replay.final_digest) << "\",\n"
         << "  \"final_digest_basis\": \"world_script_combat_v1\",\n"
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
         << "  \"script_drive\": \"qualified_runtime\",\n"
         << "  \"script_advance_each_tick\": true\n"
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
