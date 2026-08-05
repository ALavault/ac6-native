#include "ac6/product_runtime.h"
#include "ac6/mission01_compare.h"
#include "ac6/sdl_input.h"

#include <SDL3/SDL.h>

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string_view>
#include <vector>

namespace {

bool present_target(const ac6::NativeRenderTarget& target) {
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO) || !SDL_Vulkan_LoadLibrary(nullptr)) return false;
  std::vector<const char*> extensions;
  if (!ac6::SdlVulkanSurface::required_instance_extensions(extensions)) {
    SDL_Vulkan_UnloadLibrary();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return false;
  }
  ac6::VulkanInstance instance;
  ac6::SdlWindow window;
  ac6::SdlVulkanSurface surface;
  ac6::VulkanDevice device;
  ac6::VulkanSwapchain swapchain;
  ac6::VulkanFramePresenter presenter;
  bool ok = instance.create(extensions) && window.create("ac6-native", 640, 360, true, false) &&
            surface.create(window, instance.handle()) && device.create(instance.handle(), surface.handle()) &&
            swapchain.create(device, surface.handle(), 640, 360) && presenter.create(device, swapchain) &&
            presenter.present_frame(target);
  SDL_Event event{};
  while (SDL_PollEvent(&event)) if (event.type == SDL_EVENT_QUIT) ok = false;
  presenter.destroy();
  swapchain.destroy();
  device.destroy();
  surface.destroy();
  window.destroy();
  instance.destroy();
  SDL_Vulkan_UnloadLibrary();
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
  return ok;
}

bool parse_u32(std::string_view text, std::uint32_t& value) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool same_world_frame(const ac6::WorldFrame& a, const ac6::WorldFrame& b) {
  return a.tick == b.tick && a.mission_id == b.mission_id &&
      a.mission_ready == b.mission_ready && a.position_x == b.position_x &&
      a.position_y == b.position_y && a.position_z == b.position_z &&
      a.pitch == b.pitch && a.roll == b.roll && a.yaw == b.yaw &&
      a.active_units == b.active_units && a.player_entity == b.player_entity &&
      a.camera_x == b.camera_x && a.camera_y == b.camera_y && a.camera_z == b.camera_z &&
      a.camera_target_x == b.camera_target_x && a.camera_target_y == b.camera_target_y &&
      a.camera_target_z == b.camera_target_z && a.input == b.input;
}

}  // namespace

int main(int argc, char** argv) {
  ac6::MissionRuntime runtime(1);
  const auto frame = runtime.tick(1.0f / 60.0f, {});
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
    ac6::MissionSequenceDirector* sequence =
        services.has_sequence ? &services.sequence : nullptr;
    const ac6::InputMappingDatabase* input = services.has_input ? &services.input : nullptr;
    ac6::MissionExecution execution(*definition, &assets, objectives, radios,
                                     nullptr, nullptr, sequence, input);
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
    return resumed_frame.tick == expected.tick &&
                   resumed_frame.position_x == expected.position_x &&
                   resumed_frame.position_y == expected.position_y &&
                   resumed_frame.position_z == expected.position_z
               ? 0
               : 39;
  }
  if ((argc >= 4 && argc <= 6) && std::string_view(argv[1]) == "--present-manifest") {
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
    if (!loader.load_render(argv[2], render, drawables, transforms, materials,
                            textures, shaders, targets, passes, resolves, buffers,
                            geometries)) return 10;
    const ac6::MissionDefinition* definition = catalog.find(mission_id);
    const ac6::MissionRenderDefinition* render_definition = render.find(mission_id);
    const ac6::MissionLaunchDefinition* launch = launches.find(mission_id);
    const ac6::MissionRenderPass* pass = passes.find(mission_id, "world");
    if (definition == nullptr || render_definition == nullptr || launch == nullptr || pass == nullptr) return 11;
    const ac6::MissionObjectiveDatabase* objective_database =
        services.has_objectives ? &services.objectives : nullptr;
    const ac6::RadioMessageDatabase* radio_database =
        services.has_radios ? &services.radios : nullptr;
    ac6::MissionSequenceDirector* sequence =
        services.has_sequence ? &services.sequence : nullptr;
    const ac6::InputMappingDatabase* input = services.has_input ? &services.input : nullptr;
    ac6::MissionExecution execution(*definition, &assets, objective_database, radio_database,
                                    nullptr, nullptr, sequence, input);
    if (!execution.launch(*launch)) return 13;
    const ac6::WorldFrame world = execution.tick(1.0f / 60.0f, {});
    const ac6::MissionRenderTargetDefinition* target_definition =
        targets.find(mission_id, pass->color_target);
    if (target_definition == nullptr) return 14;
    ac6::NativeRenderTarget target;
    if (!target.resize(target_definition->width, target_definition->height) ||
        !target.clear(pass->clear_color, pass->clear_depth)) return 15;
    ac6::VulkanRenderer renderer;
    const ac6::VulkanRenderer::RenderAssets render_assets{
        &assets, render_definition, &drawables, &buffers, &geometries, &transforms,
        &materials, &textures, &shaders, &targets, &passes, &resolves,
        cameras.find(mission_id)};
    if (!renderer.render(world, render_assets, &target)) return 16;
    if (std::getenv("AC6_RENDER_STATS") != nullptr) {
      std::fprintf(stderr, "render_stats geometry=%u triangles=%u writes=%llu coverage=%u\n",
                   target.geometry_calls(), target.raster_triangles(),
                   static_cast<unsigned long long>(target.raster_writes()),
                   target.readback().color_coverage);
    }
    if (argc >= 5 && !target.write_ppm(argv[4])) return 18;
    if (argc == 6 && !target.write_depth_f32(argv[5])) return 18;
    return present_target(target) ? 0 : 17;
  }
  if (argc < 2 || std::string_view(argv[1]) != "--present-smoke") {
    (void)frame;
    return 0;
  }
  ac6::NativeRenderTarget target;
  if (!target.resize(320, 180) || !target.clear(0xFF10243Au, 1.0f)) return 18;
  return present_target(target) ? 0 : 19;
}
