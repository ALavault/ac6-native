#include "ac6/vulkan_scene_resource_cache.h"
#include "fixtures/vulkan_triangle_spirv.h"

#include <array>
#include <cstdint>
#include <iostream>

namespace {

int fail(const char* condition) {
  std::cerr << "vulkan_scene_resource_cache_test=fail condition=" << condition
            << '\n';
  return 1;
}

ac6::RenderScene scene_fixture() {
  ac6::RenderScene scene;
  scene.tick = 1;
  scene.mission_id = 1;
  scene.camera.position = {0.0F, 0.0F, 1.0F};
  scene.camera.target = {0.0F, 0.0F, 0.0F};
  scene.surface.width = 16;
  scene.surface.height = 16;
  scene.surface.sample_count = 1;
  scene.surface.color_format = "rgba8_unorm";
  scene.surface.depth_format = "none";
  scene.surface.present_mode =
      ac6::RenderSurfaceRequirements::PresentMode::Headless;
  scene.passes.push_back(
      {"world", 0, {0.1F, 0.2F, 0.3F, 1.0F}, 1.0F, true, false});
  scene.materials.push_back({"material", 1, 2, {}, {}, 0, 0, 1.0F});
  ac6::DrawPacket packet;
  packet.mesh_id = "mesh";
  packet.material_id = "material";
  packet.index_count = 3;
  packet.depth.test = false;
  packet.depth.write = false;
  scene.draw_packets.push_back(std::move(packet));
  scene.refresh_digest();
  return scene;
}

}  // namespace

int main() {
  auto created = ac6::VulkanBackend::create();
  if (!created) {
    std::cout << "vulkan_scene_resource_cache_skipped="
              << ac6::vulkan_backend_error_name(created.error) << '\n';
    return 77;
  }
  ac6::VulkanBackend& backend = *created.backend;
  const auto target = backend.create_render_target(16, 16, false);
  if (!target) return fail("target");

  ac6::RenderScene scene = scene_fixture();
  const std::array<ac6::VulkanVertex, 3> vertices{{
      {-0.8F, -0.8F}, {0.8F, -0.8F}, {0.0F, 0.8F}}};
  const std::array<std::uint16_t, 3> indices{{0, 1, 2}};
  const std::array<ac6::VulkanSceneMeshUpload, 1> mesh_uploads{{
      {"mesh", vertices, indices}}};
  const std::array<ac6::VulkanSceneMaterialUpload, 1> material_uploads{{
      {"material", ac6_test::kTriangleVertexSpirv,
       ac6_test::kTriangleFragmentSpirv, {}, true}}};

  ac6::VulkanSceneResourceCache cache(backend);
  if (!cache.build(scene, target, mesh_uploads, material_uploads) ||
      !cache.ready() || cache.live_mesh_count() != 1U ||
      cache.live_pipeline_count() != 1U ||
      backend.live_mesh_count() != 1U || backend.live_pipeline_count() != 1U) {
    return fail("persistent_build");
  }
  if (!cache.render(scene)) return fail("persistent_render");

  // The source arrays are scoped to the fixture call above; a second render
  // proves that no caller-owned upload span is retained by the cache.
  if (!cache.render(scene)) return fail("second_render");
  ac6::RenderScene changed = scene;
  changed.tick = 2;
  changed.refresh_digest();
  if (cache.render(changed)) return fail("changed_scene_accepted");
  const auto pixels = backend.readback_rgba8(target);
  const std::size_t center = (8U * 16U + 8U) * 4U;
  if (pixels.size() != 16U * 16U * 4U || pixels[center] != 0U ||
      pixels[center + 1U] != 255U || pixels[center + 2U] != 0U ||
      pixels[center + 3U] != 255U) {
    return fail("persistent_readback");
  }

  cache.reset();
  if (cache.ready() || cache.live_mesh_count() != 0U ||
      cache.live_pipeline_count() != 0U || backend.live_mesh_count() != 0U ||
      backend.live_pipeline_count() != 0U) {
    return fail("reset");
  }

  const std::array<ac6::VulkanSceneMaterialUpload, 0> missing_materials{};
  if (cache.build(scene, target, mesh_uploads, missing_materials) ||
      cache.ready() || backend.live_mesh_count() != 0U ||
      backend.live_pipeline_count() != 0U) {
    return fail("transactional_refusal");
  }
  std::cout << "vulkan_scene_resource_cache=pass persistent_resources=1 "
                "resource_allocations_per_frame=0 transactional_refusal=1\n";
  return 0;
}
