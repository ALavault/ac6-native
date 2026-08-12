#include "ac6/vulkan_scene_renderer.h"
#include "fixtures/vulkan_triangle_spirv.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

int fail(const char* condition) {
  std::cerr << "vulkan_scene_renderer_test=fail condition=" << condition << '\n';
  return 1;
}

ac6::RenderScene triangle_scene() {
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
  scene.surface.present_mode = ac6::RenderSurfaceRequirements::PresentMode::Headless;
  scene.passes.push_back({"world", 0, {0.125F, 0.25F, 0.5F, 1.0F}, 1.0F, true, false});
  ac6::MaterialPipeline material;
  material.stable_id = "material";
  material.vertex_shader_hash = 1;
  material.fragment_shader_hash = 2;
  scene.materials.push_back(std::move(material));
  ac6::DrawPacket packet;
  packet.mesh_id = "mesh";
  packet.material_id = "material";
  packet.index_count = 3;
  packet.topology = ac6::RenderPrimitiveTopology::TriangleList;
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
    std::cout << "vulkan_scene_renderer_skipped="
              << ac6::vulkan_backend_error_name(created.error) << '\n';
    return 77;
  }
  ac6::VulkanBackend& backend = *created.backend;
  const std::array<ac6::VulkanVertex, 3> vertices{{
      {-0.8F, -0.8F}, {0.8F, -0.8F}, {0.0F, 0.8F}}};
  const std::array<std::uint16_t, 3> indices{{0, 1, 2}};
  const auto mesh = backend.create_mesh(vertices, indices);
  const auto target = backend.create_render_target(16, 16, false);
  const auto pipeline = backend.create_pipeline(
      target, ac6_test::kTriangleVertexSpirv, ac6_test::kTriangleFragmentSpirv,
      {});
  if (!mesh || !target || !pipeline) return fail("gpu_resources");

  ac6::VulkanSceneRenderer renderer(backend);
  const ac6::VulkanSceneMeshBinding mesh_binding{"mesh", mesh, 3};
  const ac6::VulkanSceneMaterialBinding material_binding{
      "material", pipeline, {}, true};
  const std::array<ac6::VulkanSceneMeshBinding, 1> meshes{{mesh_binding}};
  const std::array<ac6::VulkanSceneMaterialBinding, 1> materials{{material_binding}};
  ac6::RenderScene scene = triangle_scene();
  if (!renderer.render(scene, target, meshes, materials)) return fail("direct_scene_submit");
  const auto pixels = backend.readback_rgba8(target);
  const std::size_t center = (8U * 16U + 8U) * 4U;
  if (pixels.size() != 16U * 16U * 4U || pixels[center] != 0U ||
      pixels[center + 1U] != 255U || pixels[center + 2U] != 0U ||
      pixels[center + 3U] != 255U) {
    return fail("direct_scene_readback");
  }

  ac6::RenderScene unsupported = scene;
  unsupported.draw_packets[0].topology = ac6::RenderPrimitiveTopology::TriangleStripRestart;
  unsupported.refresh_digest();
  if (renderer.render(unsupported, target, meshes, materials)) {
    return fail("unsupported_topology_accepted");
  }
  unsupported = scene;
  unsupported.draw_packets[0].transform[12] = 0.25F;
  unsupported.refresh_digest();
  if (renderer.render(unsupported, target, meshes, materials)) {
    return fail("unsupported_transform_accepted");
  }
  unsupported = scene;
  unsupported.hud.push_back({"hud", {0.0F, 0.0F, 1.0F, 1.0F}, 0xFFFFFFFFU, true});
  unsupported.refresh_digest();
  if (renderer.render(unsupported, target, meshes, materials)) {
    return fail("unsupported_hud_accepted");
  }
  std::cout << "vulkan_scene_renderer=pass direct_gpu_draw=1 cpu_target=0\n";
  return 0;
}
