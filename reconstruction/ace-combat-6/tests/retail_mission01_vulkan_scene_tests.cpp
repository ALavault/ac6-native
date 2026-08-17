#include "ac6/retail_mission01_vulkan_scene.h"
#include "ac6/vulkan_scene_resource_cache.h"
#include "fixtures/vulkan_clip_mesh_spirv.h"
#include "fixtures/vulkan_textured_triangle_spirv.h"

#include <array>
#include <cstdlib>
#include <iostream>

int main() {
  const char* cache_root = std::getenv("AC6_RETAIL_CACHE");
  if (cache_root == nullptr || *cache_root == '\0') {
    std::cout << "retail_mission01_vulkan_scene_skipped=no_cache\n";
    return 77;
  }
  ac6::RetailContentStore store;
  if (!store.open(cache_root)) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=cache\n";
    return 1;
  }
  constexpr std::array<float, 16> identity_clip{
      1.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F,
      0.0F, 0.0F, 1.0F, 0.0F,
      0.0F, 0.0F, 0.0F, 1.0F};
  const auto scene = ac6::retail::RetailMission01VulkanScene::open(
      store, 0U, true, identity_clip, ac6_test::kClipMeshVertexSpirv,
      ac6_test::kTexturedTriangleFragmentSpirv, 1U, 2U, 64U, 64U);
  if (!scene.has_value() || !scene->report().store_backed ||
      !scene->report().clip_matrix_supplied ||
      !scene->report().placement_translation_applied ||
      !scene->report().shader_bytes_supplied || scene->report().jv_eligible ||
      scene->scene().draw_packets.size() != 1U ||
      scene->scene().draw_packets.front().index_count !=
          scene->report().index_count) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=qualified_draw\n";
    return 1;
  }
  const auto mesh = scene->mesh_upload();
  const auto material = scene->material_upload();
  const auto texture = scene->texture_upload();
  if (mesh.vertices.size() != scene->report().vertex_count ||
      mesh.indices.size() != scene->report().index_count ||
      material.vertex_spirv.empty() || material.fragment_spirv.empty() ||
      texture.width == 0U || texture.height == 0U ||
      texture.rgba8.size() != static_cast<std::size_t>(texture.width) *
                                    texture.height * 4U) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=owned_uploads\n";
    return 1;
  }
  auto backend_result = ac6::VulkanBackend::create();
  if (!backend_result) {
    std::cout << "retail_mission01_vulkan_scene_skipped=vulkan_backend\n";
    return 77;
  }
  ac6::VulkanBackend& backend = *backend_result.backend;
  const ac6::VulkanRenderTargetHandle target =
      backend.create_render_target(scene->scene().surface.width,
                                   scene->scene().surface.height, false);
  if (!target) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=target\n";
    return 1;
  }
  ac6::VulkanSceneResourceCache cache(backend);
  const std::array<ac6::VulkanSceneClipTexturedMeshUpload, 1> meshes{{mesh}};
  const std::array<ac6::VulkanSceneTexturedMaterialUpload, 1> materials{{material}};
  const std::array<ac6::VulkanSceneTextureUpload, 1> textures{{texture}};
  if (!cache.build_clip_textured(scene->scene(), target, meshes, materials,
                                 textures) ||
      !cache.ready() || cache.live_mesh_count() != 1U ||
      cache.live_pipeline_count() != 1U || cache.live_texture_count() != 1U ||
      !cache.render(scene->scene()) || !cache.render(scene->scene())) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=persistent_submit\n";
    return 1;
  }
  const std::vector<std::uint8_t> readback = backend.readback_rgba8(target);
  if (readback.size() != static_cast<std::size_t>(scene->scene().surface.width) *
                              scene->scene().surface.height * 4U) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=readback\n";
    return 1;
  }
  cache.reset();
  if (cache.ready() || backend.live_clip_textured_mesh_count() != 0U ||
      backend.live_pipeline_count() != 0U || backend.live_texture_count() != 0U) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=reset\n";
    return 1;
  }
  if (ac6::retail::RetailMission01VulkanScene::open(
          store, 0U, true, identity_clip, {},
          ac6_test::kTexturedTriangleFragmentSpirv, 1U, 2U, 64U, 64U)) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=missing_shader\n";
    return 1;
  }
  ac6::WorldFrame frame;
  frame.mission_id = 1U;
  frame.mission_ready = true;
  frame.player_entity = 1U;
  frame.camera_x = 0.0F;
  frame.camera_y = 0.0F;
  frame.camera_z = -100.0F;
  frame.camera_target_x = 0.0F;
  frame.camera_target_y = 0.0F;
  frame.camera_target_z = 0.0F;
  const ac6::SimulationSnapshot snapshot = ac6::make_simulation_snapshot(
      frame, ac6::ScenarioState::Gameplay, 1U, 0U, false);
  const auto runtime = ac6::retail::RetailMission01VulkanScene::open_runtime(
      store, snapshot, true, 1280U, 720U);
  if (!runtime.has_value() || runtime->report().runtime_draw_instances != 4226U ||
      runtime->report().terrain_draw_instances != 65536U ||
      runtime->report().runtime_meshes == 0U ||
      runtime->report().runtime_textures == 0U ||
      runtime->report().complete_render_scene || runtime->report().jv_eligible) {
    std::cerr << "retail_mission01_vulkan_scene=fail condition=runtime_scene\n";
    return 1;
  }
  std::cout << "retail_mission01_vulkan_scene=pass draw=1 jv_eligible=0"
            << " runtime_draw=" << runtime->report().runtime_draw_instances
            << " runtime_meshes=" << runtime->report().runtime_meshes
            << " runtime_textures=" << runtime->report().runtime_textures
            << " terrain_draw=" << runtime->report().terrain_draw_instances
            << "\n";
  return 0;
}
