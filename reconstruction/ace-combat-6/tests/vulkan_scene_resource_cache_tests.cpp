#include "ac6/vulkan_scene_resource_cache.h"
#include "fixtures/vulkan_clip_mesh_spirv.h"
#include "fixtures/vulkan_triangle_spirv.h"
#include "fixtures/vulkan_textured_triangle_spirv.h"
#include "fixtures/vulkan_world_textured_spirv.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>

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

int check_world_textured_path(
    ac6::VulkanBackend& backend, ac6::VulkanSceneResourceCache& cache,
    const ac6::VulkanRenderTargetHandle target,
    const std::array<std::uint16_t, 3>& indices,
    const std::array<std::uint8_t, 4U * 4U * 4U>& texture_pixels,
    const std::size_t textured_center) {
  if (!backend.caps().depth_d32) {
    std::cout << "vulkan_scene_resource_cache_skipped=depth_d32_unavailable\n";
    return 77;
  }
  const auto depth_target = backend.create_render_target(16U, 16U, true);
  if (!depth_target || !backend.render_target_has_d32(depth_target)) {
    return fail("world_depth_target");
  }
  ac6::RenderScene world_scene = scene_fixture();
  world_scene.camera.position = {0.0F, 0.0F, -2.0F};
  world_scene.camera.target = {0.0F, 0.0F, 0.0F};
  world_scene.camera.up = {0.0F, 1.0F, 0.0F};
  world_scene.camera.vertical_fov_radians = 1.0471975512F;
  world_scene.camera.near_plane = 0.1F;
  world_scene.camera.far_plane = 100.0F;
  world_scene.surface.depth_format = "d32_sfloat";
  world_scene.passes[0].clear_depth_enabled = true;
  world_scene.materials[0].texture_bindings.push_back(
      {0U, std::string(ac6::kVulkanDrawPacketTexture0Binding)});
  world_scene.draw_packets.clear();
  const auto transform_at = [](const float translate_x, const float depth) {
    return std::array<float, 16>{1.0F,        0.0F, 0.0F,  0.0F, 0.0F, 1.0F,
                                 0.0F,        0.0F, 0.0F,  0.0F, 1.0F, 0.0F,
                                 translate_x, 0.0F, depth, 1.0F};
  };
  ac6::DrawPacket far_red;
  far_red.mesh_id = "world-mesh";
  far_red.material_id = "material";
  far_red.texture_ids.push_back("red");
  far_red.transform = transform_at(0.0F, 2.0F);
  far_red.index_count = 3U;
  far_red.raster.cull_back_faces = false;
  far_red.sort_key = 0U;
  ac6::DrawPacket near_green = far_red;
  near_green.texture_ids.front() = "green";
  near_green.transform = transform_at(6.0F, 0.0F);
  near_green.sort_key = 1U;
  ac6::DrawPacket farther_red = far_red;
  farther_red.transform = transform_at(6.0F, 4.0F);
  farther_red.sort_key = 2U;
  world_scene.draw_packets = {far_red, near_green, farther_red};
  world_scene.refresh_digest();

  const std::array<ac6::VulkanWorldTexturedVertex, 3> world_vertices{{
      {-6.0F, -6.0F, 10.0F, 0.0F, 0.0F},
      {6.0F, -6.0F, 10.0F, 1.0F, 0.0F},
      {0.0F, 6.0F, 10.0F, 0.5F, 1.0F},
  }};
  const std::array<ac6::VulkanSceneWorldTexturedMeshUpload, 1>
      world_mesh_uploads{{{"world-mesh", world_vertices, indices}}};
  const ac6::VulkanPipelineState world_pipeline_state{true, true, false};
  const std::array<ac6::VulkanSceneTexturedMaterialUpload, 1>
      world_material_uploads{{{"material", ac6_test::kWorldTexturedVertexSpirv,
                               ac6_test::kTexturedTriangleFragmentSpirv,
                               world_pipeline_state, true}}};
  const std::array<std::uint8_t, 4U * 4U * 4U> green_texture_pixels = [] {
    std::array<std::uint8_t, 4U * 4U * 4U> pixels{};
    for (std::size_t index = 0U; index < pixels.size(); index += 4U) {
      pixels[index + 1U] = 255U;
      pixels[index + 3U] = 255U;
    }
    return pixels;
  }();
  const std::array<ac6::VulkanSceneTextureUpload, 2> world_texture_uploads{{
      {"red", 4U, 4U, texture_pixels},
      {"green", 4U, 4U, green_texture_pixels},
  }};

  const auto rejected_world_scene = [&](ac6::RenderScene candidate) {
    candidate.refresh_digest();
    return !cache.build_world_textured(
               candidate, depth_target, world_mesh_uploads,
               world_material_uploads, world_texture_uploads) &&
           !cache.ready() && backend.live_world_textured_mesh_count() == 0U &&
           backend.live_pipeline_count() == 0U &&
           backend.live_texture_count() == 0U;
  };
  ac6::RenderScene nonfinite_world_scene = world_scene;
  nonfinite_world_scene.draw_packets[0].transform[0] =
      std::numeric_limits<float>::quiet_NaN();
  ac6::RenderScene culled_world_scene = world_scene;
  culled_world_scene.draw_packets[0].raster.cull_back_faces = true;
  ac6::RenderScene wireframe_world_scene = world_scene;
  wireframe_world_scene.draw_packets[0].raster.wireframe = true;
  ac6::RenderScene mismatched_texture_scene = world_scene;
  mismatched_texture_scene.materials[0].texture_bindings[0].resource_id = "red";
  ac6::RenderScene sampler_binding_scene = world_scene;
  sampler_binding_scene.materials[0].sampler_bindings.push_back({0U, "linear"});
  ac6::RenderScene material_anisotropy_scene = world_scene;
  material_anisotropy_scene.materials[0].sampler_anisotropy = 2.0F;
  ac6::RenderScene surface_anisotropy_scene = world_scene;
  surface_anisotropy_scene.surface.sampler_anisotropy = 2.0F;
  ac6::RenderScene constant_offset_scene = world_scene;
  constant_offset_scene.materials[0].constant_offset = 16U;
  ac6::RenderScene constant_count_scene = world_scene;
  constant_count_scene.materials[0].constant_count = 4U;
  ac6::RenderScene blend_enabled_scene = world_scene;
  blend_enabled_scene.draw_packets[0].blend.enabled = true;
  ac6::RenderScene blend_source_scene = world_scene;
  blend_source_scene.draw_packets[0].blend.source_factor = 2U;
  ac6::RenderScene blend_destination_scene = world_scene;
  blend_destination_scene.draw_packets[0].blend.destination_factor = 1U;
  ac6::RenderScene coincident_camera_scene = world_scene;
  coincident_camera_scene.camera.target = coincident_camera_scene.camera.position;
  ac6::RenderScene collinear_camera_scene = world_scene;
  collinear_camera_scene.camera.up = {0.0F, 0.0F, 1.0F};
  ac6::RenderScene invalid_fov_scene = world_scene;
  invalid_fov_scene.camera.vertical_fov_radians = 3.5F;
  if (!rejected_world_scene(nonfinite_world_scene) ||
      !rejected_world_scene(culled_world_scene) ||
      !rejected_world_scene(wireframe_world_scene) ||
      !rejected_world_scene(mismatched_texture_scene) ||
      !rejected_world_scene(sampler_binding_scene) ||
      !rejected_world_scene(material_anisotropy_scene) ||
      !rejected_world_scene(surface_anisotropy_scene) ||
      !rejected_world_scene(constant_offset_scene) ||
      !rejected_world_scene(constant_count_scene) ||
      !rejected_world_scene(blend_enabled_scene) ||
      !rejected_world_scene(blend_source_scene) ||
      !rejected_world_scene(blend_destination_scene) ||
      !rejected_world_scene(coincident_camera_scene) ||
      !rejected_world_scene(collinear_camera_scene) ||
      !rejected_world_scene(invalid_fov_scene) ||
      cache.build_world_textured(world_scene, target, world_mesh_uploads,
                                 world_material_uploads,
                                 world_texture_uploads) ||
      cache.ready() || backend.live_world_textured_mesh_count() != 0U) {
    return fail("world_bounded_refusal");
  }
  constexpr std::array<std::uint32_t, 1> invalid_world_spirv{{0U}};
  auto rollback_material_uploads = world_material_uploads;
  rollback_material_uploads[0].vertex_spirv = invalid_world_spirv;
  if (cache.build_world_textured(world_scene, depth_target, world_mesh_uploads,
                                 rollback_material_uploads,
                                 world_texture_uploads) ||
      cache.ready() || backend.live_world_textured_mesh_count() != 0U ||
      backend.live_pipeline_count() != 0U ||
      backend.live_texture_count() != 0U) {
    return fail("world_transactional_rollback");
  }
  if (!cache.build_world_textured(world_scene, depth_target, world_mesh_uploads,
                                  world_material_uploads,
                                  world_texture_uploads) ||
      !cache.ready() || cache.live_mesh_count() != 1U ||
      cache.live_pipeline_count() != 1U || cache.live_texture_count() != 2U ||
      backend.live_world_textured_mesh_count() != 1U ||
      backend.live_pipeline_count() != 1U ||
      backend.live_texture_count() != 2U || !cache.render(world_scene)) {
    return fail("world_persistent_build");
  }
  const auto world_readback = backend.readback_rgba8(depth_target);
  // Authored Z is outside clip space; these pixels prove camera projection.
  const std::size_t translated_center = (8U * 16U + 14U) * 4U;
  if (world_readback.size() != 16U * 16U * 4U ||
      world_readback[textured_center] != 255U ||
      world_readback[textured_center + 1U] != 0U ||
      world_readback[textured_center + 2U] != 0U ||
      world_readback[textured_center + 3U] != 255U ||
      world_readback[translated_center] != 0U ||
      world_readback[translated_center + 1U] != 255U ||
      world_readback[translated_center + 2U] != 0U ||
      world_readback[translated_center + 3U] != 255U) {
    return fail("world_depth_order_readback");
  }
  cache.reset();
  if (cache.ready() || cache.live_mesh_count() != 0U ||
      cache.live_pipeline_count() != 0U || cache.live_texture_count() != 0U ||
      backend.live_world_textured_mesh_count() != 0U ||
      backend.live_pipeline_count() != 0U ||
      backend.live_texture_count() != 0U) {
    return fail("world_reset");
  }
  return 0;
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

  const std::array<ac6::retail::NdxrPosition, 3> qualified_positions{{
      {-0.8F, -0.8F, 0.0F}, {0.8F, -0.8F, 0.0F}, {0.0F, 0.8F, 0.0F}}};
  const std::array<ac6::retail::NdxrTexcoord, 3> qualified_uvs{{
      {0.0F, 0.0F}, {1.0F, 0.0F}, {0.5F, 1.0F}}};
  const std::array<std::uint16_t, 3> qualified_indices{{0U, 1U, 2U}};
  const ac6::retail::DecodedTexture qualified_texture{
      1U, 1U, std::vector<std::uint32_t>{0xFF0000FFU}};
  const auto qualified_upload = ac6::make_vulkan_mission01_textured_upload(
      "qualified-mesh", "qualified-texture", qualified_positions, qualified_uvs,
      qualified_indices, qualified_texture);
  if (!qualified_upload || qualified_upload->vertices.size() != 3U ||
      qualified_upload->indices.size() != 3U ||
      qualified_upload->rgba8.size() != 4U || qualified_upload->rgba8[0] != 255U ||
      qualified_upload->rgba8[3] != 255U) {
    return fail("qualified_upload_adapter");
  }
  const std::array<ac6::retail::NdxrPosition, 3> world_positions{{
      {-0.8F, -0.8F, 1.0F}, {0.8F, -0.8F, 1.0F}, {0.0F, 0.8F, 1.0F}}};
  const std::array<std::uint16_t, 4> strip_indices{{0U, 1U, 2U,
                                                     ac6::retail::kStripRestart}};
  if (ac6::make_vulkan_mission01_textured_upload(
          "world-mesh", "qualified-texture", world_positions, qualified_uvs,
          qualified_indices, qualified_texture) ||
      ac6::make_vulkan_mission01_textured_upload(
          "strip-mesh", "qualified-texture", qualified_positions, qualified_uvs,
          strip_indices, qualified_texture)) {
    return fail("qualified_upload_refusal");
  }
  constexpr std::array<float, 16> identity_clip{
      1.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F,
      0.0F, 0.0F, 1.0F, 0.0F,
      0.0F, 0.0F, 0.0F, 1.0F};
  const auto clip_upload = ac6::make_vulkan_mission01_clip_textured_upload(
      "clip-mesh", "qualified-texture", world_positions, qualified_uvs,
      qualified_indices, identity_clip, qualified_texture);
  const auto clip_strip_upload =
      ac6::make_vulkan_mission01_clip_textured_upload(
          "clip-strip-mesh", "qualified-texture", qualified_positions,
          qualified_uvs, strip_indices, identity_clip, qualified_texture);
  if (!clip_upload || clip_upload->vertices.size() != 3U ||
      clip_upload->vertices[0].z != 1.0F || clip_upload->vertices[0].w != 1.0F ||
      clip_upload->indices.size() != 3U ||
      clip_upload->rgba8.size() != 4U || !clip_strip_upload ||
      clip_strip_upload->indices.size() != 3U) {
    return fail("clip_upload_adapter");
  }
  const auto world_upload = ac6::make_vulkan_mission01_world_textured_upload(
      "world-mesh", "qualified-texture", world_positions, qualified_uvs,
      strip_indices, qualified_texture);
  if (!world_upload || world_upload->vertices.size() != 3U ||
      world_upload->vertices[0].z != 1.0F ||
      world_upload->indices.size() != 3U || world_upload->rgba8.size() != 4U) {
    return fail("world_upload_adapter");
  }

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

  ac6::RenderScene textured_scene = scene_fixture();
  textured_scene.materials[0].texture_bindings.push_back({0U, "texture"});
  textured_scene.draw_packets[0].texture_ids.push_back("texture");
  textured_scene.refresh_digest();
  const std::array<ac6::VulkanTexturedVertex, 3> textured_vertices{{
      {-0.8F, -0.8F, 0.0F, 0.0F},
      {0.8F, -0.8F, 1.0F, 0.0F},
      {0.0F, 0.8F, 0.5F, 1.0F},
  }};
  const std::array<std::uint8_t, 4U * 4U * 4U> texture_pixels = [] {
    std::array<std::uint8_t, 4U * 4U * 4U> pixels{};
    for (std::size_t index = 0U; index < pixels.size(); index += 4U) {
      pixels[index] = 255U;
      pixels[index + 3U] = 255U;
    }
    return pixels;
  }();
  const std::array<ac6::VulkanSceneTexturedMeshUpload, 1> textured_mesh_uploads{{
      {"mesh", textured_vertices, indices}}};
  const std::array<ac6::VulkanSceneTexturedMaterialUpload, 1>
      textured_material_uploads{{
          {"material", ac6_test::kTexturedTriangleVertexSpirv,
           ac6_test::kTexturedTriangleFragmentSpirv, {}, true}}};
  const std::array<ac6::VulkanSceneTextureUpload, 1> texture_uploads{{
      {"texture", 4U, 4U, texture_pixels}}};
  if (!cache.build_textured(textured_scene, target, textured_mesh_uploads,
                            textured_material_uploads, texture_uploads) ||
      !cache.ready() || cache.live_mesh_count() != 1U ||
      cache.live_pipeline_count() != 1U || cache.live_texture_count() != 1U ||
      !cache.render(textured_scene)) {
    return fail("textured_persistent_build");
  }
  const auto textured_readback = backend.readback_rgba8(target);
  const std::size_t textured_center = (8U * 16U + 8U) * 4U;
  if (textured_readback.size() != 16U * 16U * 4U ||
      textured_readback[textured_center] != 255U ||
      textured_readback[textured_center + 1U] != 0U ||
      textured_readback[textured_center + 2U] != 0U ||
      textured_readback[textured_center + 3U] != 255U) {
    return fail("textured_persistent_readback");
  }
  cache.reset();
  if (cache.live_mesh_count() != 0U || cache.live_pipeline_count() != 0U ||
      cache.live_texture_count() != 0U || backend.live_texture_count() != 0U ||
      backend.live_textured_mesh_count() != 0U) {
    return fail("textured_reset");
  }

  const std::array<ac6::VulkanClipTexturedVertex, 3> clip_vertices{{
      {-0.8F, -0.8F, 0.0F, 1.0F, 0.0F, 0.0F},
      {0.8F, -0.8F, 0.0F, 1.0F, 1.0F, 0.0F},
      {0.0F, 0.8F, 0.0F, 1.0F, 0.5F, 1.0F},
  }};
  const std::array<ac6::VulkanSceneClipTexturedMeshUpload, 1>
      clip_mesh_uploads{{{"mesh", clip_vertices, indices}}};
  const std::array<ac6::VulkanSceneTexturedMaterialUpload, 1>
      clip_material_uploads{{{
          "material", ac6_test::kClipMeshVertexSpirv,
          ac6_test::kTexturedTriangleFragmentSpirv, {}, true}}};
  if (!cache.build_clip_textured(textured_scene, target, clip_mesh_uploads,
                                 clip_material_uploads, texture_uploads) ||
      !cache.ready() || cache.live_mesh_count() != 1U ||
      cache.live_pipeline_count() != 1U || cache.live_texture_count() != 1U ||
      !cache.render(textured_scene)) {
    return fail("clip_textured_persistent_build");
  }
  const auto clip_readback = backend.readback_rgba8(target);
  if (clip_readback.size() != 16U * 16U * 4U ||
      clip_readback[textured_center] != 255U ||
      clip_readback[textured_center + 1U] != 0U ||
      clip_readback[textured_center + 2U] != 0U ||
      clip_readback[textured_center + 3U] != 255U) {
    return fail("clip_textured_persistent_readback");
  }
  cache.reset();
  if (cache.live_mesh_count() != 0U || cache.live_pipeline_count() != 0U ||
      cache.live_texture_count() != 0U ||
      backend.live_clip_textured_mesh_count() != 0U) {
    return fail("clip_textured_reset");
  }

  const int world_result = check_world_textured_path(
      backend, cache, target, indices, texture_pixels, textured_center);
  if (world_result != 0) return world_result;

  const std::array<ac6::VulkanSceneMaterialUpload, 0> missing_materials{};
  if (cache.build(scene, target, mesh_uploads, missing_materials) ||
      cache.ready() || backend.live_mesh_count() != 0U ||
      backend.live_pipeline_count() != 0U) {
    return fail("transactional_refusal");
  }
  std::cout << "vulkan_scene_resource_cache=pass persistent_resources=1 "
               "resource_allocations_per_frame=0 world_draws=3 "
               "world_textures=2 depth_order=pass dynamic_texture_binding=1 "
               "state_refusals=15 camera_projection=pass "
               "transactional_rollback=1\n";
  return 0;
}
