#include "ac6/vulkan_scene_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace ac6 {
namespace {

[[nodiscard]] bool identity_transform(const std::array<float, 16>& transform) noexcept {
  constexpr std::array<float, 16> identity{
      1.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F,
      0.0F, 0.0F, 1.0F, 0.0F,
      0.0F, 0.0F, 0.0F, 1.0F};
  return transform == identity;
}

[[nodiscard]] bool world_material_supported(
    const MaterialPipeline& material) noexcept {
  return material.texture_bindings.size() == 1U &&
         material.texture_bindings.front().slot == 0U &&
         material.texture_bindings.front().resource_id ==
             kVulkanDrawPacketTexture0Binding &&
         material.sampler_bindings.empty() && material.constant_offset == 0U &&
         material.constant_count == 0U && material.sampler_anisotropy == 1.0F;
}

struct CameraVector final {
  double x{};
  double y{};
  double z{};
};

[[nodiscard]] CameraVector cross(const CameraVector left,
                                 const CameraVector right) noexcept {
  return {left.y * right.z - left.z * right.y,
          left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

[[nodiscard]] double dot(const CameraVector left,
                         const CameraVector right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

[[nodiscard]] bool normalize(CameraVector& value) noexcept {
  const double length_squared = dot(value, value);
  if (!std::isfinite(length_squared) || length_squared <= 0.0) return false;
  const double inverse_length = 1.0 / std::sqrt(length_squared);
  if (!std::isfinite(inverse_length)) return false;
  value.x *= inverse_length;
  value.y *= inverse_length;
  value.z *= inverse_length;
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

[[nodiscard]] bool multiply_row_major(const std::array<float, 16>& left,
                                      const std::array<float, 16>& right,
                                      std::array<float, 16>& result) noexcept {
  for (std::size_t row = 0U; row < 4U; ++row) {
    for (std::size_t column = 0U; column < 4U; ++column) {
      double value = 0.0;
      for (std::size_t inner = 0U; inner < 4U; ++inner) {
        value += static_cast<double>(left[row * 4U + inner]) *
                 static_cast<double>(right[inner * 4U + column]);
      }
      result[row * 4U + column] = static_cast<float>(value);
      if (!std::isfinite(result[row * 4U + column])) return false;
    }
  }
  return true;
}

[[nodiscard]] bool make_world_to_clip(
    const RenderCamera& camera, const RenderSurfaceRequirements& surface,
    std::array<float, 16>& world_to_clip) noexcept {
  if (!camera.valid() || surface.width == 0U || surface.height == 0U ||
      camera.vertical_fov_radians >= std::numbers::pi_v<float>) {
    return false;
  }
  CameraVector forward{
      static_cast<double>(camera.target[0]) - camera.position[0],
      static_cast<double>(camera.target[1]) - camera.position[1],
      static_cast<double>(camera.target[2]) - camera.position[2]};
  const CameraVector supplied_up{camera.up[0], camera.up[1], camera.up[2]};
  if (!normalize(forward)) return false;
  CameraVector right = cross(supplied_up, forward);
  if (!normalize(right)) return false;
  CameraVector up = cross(forward, right);
  if (!normalize(up)) return false;

  const double aspect = static_cast<double>(surface.width) / surface.height;
  const double focal =
      1.0 / std::tan(static_cast<double>(camera.vertical_fov_radians) * 0.5);
  const double depth_range =
      static_cast<double>(camera.far_plane) - camera.near_plane;
  if (!std::isfinite(aspect) || aspect <= 0.0 || !std::isfinite(focal) ||
      focal <= 0.0 || !std::isfinite(depth_range) || depth_range <= 0.0) {
    return false;
  }
  const CameraVector position{camera.position[0], camera.position[1],
                              camera.position[2]};
  const std::array<float, 16> view{static_cast<float>(right.x),
                                   static_cast<float>(up.x),
                                   static_cast<float>(forward.x),
                                   0.0F,
                                   static_cast<float>(right.y),
                                   static_cast<float>(up.y),
                                   static_cast<float>(forward.y),
                                   0.0F,
                                   static_cast<float>(right.z),
                                   static_cast<float>(up.z),
                                   static_cast<float>(forward.z),
                                   0.0F,
                                   static_cast<float>(-dot(position, right)),
                                   static_cast<float>(-dot(position, up)),
                                   static_cast<float>(-dot(position, forward)),
                                   1.0F};
  const double depth_scale =
      static_cast<double>(camera.far_plane) / depth_range;
  const std::array<float, 16> projection{
      static_cast<float>(focal / aspect),
      0.0F,
      0.0F,
      0.0F,
      0.0F,
      static_cast<float>(-focal),
      0.0F,
      0.0F,
      0.0F,
      0.0F,
      static_cast<float>(depth_scale),
      1.0F,
      0.0F,
      0.0F,
      static_cast<float>(-static_cast<double>(camera.near_plane) * depth_scale),
      0.0F};
  for (const float value : view) {
    if (!std::isfinite(value)) return false;
  }
  for (const float value : projection) {
    if (!std::isfinite(value)) return false;
  }
  return multiply_row_major(view, projection, world_to_clip);
}

}  // namespace

bool VulkanSceneRenderer::render(
    const RenderScene& scene, const VulkanRenderTargetHandle target,
    const std::span<const VulkanSceneMeshBinding> meshes,
    const std::span<const VulkanSceneMaterialBinding> materials) noexcept {
  if (!scene.valid() || !target || !backend_.has_render_target(target) ||
      !render_scene_supported(scene, backend_.caps()) || scene.passes.size() != 1U ||
      scene.hud.size() != 0U || scene.surface.sample_count != 1U ||
      scene.surface.color_format != "rgba8_unorm" || scene.surface.depth_format != "none") {
    return false;
  }
  const RenderPass& pass = scene.passes.front();
  if (!pass.clear_color_enabled || pass.clear_depth_enabled ||
      !backend_.clear_render_target(target, pass.clear_color, pass.clear_depth)) {
    return false;
  }

  for (const DrawPacket& packet : scene.draw_packets) {
    if (packet.topology != RenderPrimitiveTopology::TriangleList ||
        packet.first_index != 0U || packet.vertex_offset != 0 ||
        !identity_transform(packet.transform) || packet.texture_ids.size() != 0U) {
      return false;
    }
    const auto mesh = std::find_if(meshes.begin(), meshes.end(), [&](const auto& binding) {
      return binding.mesh_id == packet.mesh_id;
    });
    const auto material = std::find_if(materials.begin(), materials.end(), [&](const auto& binding) {
      return binding.material_id == packet.material_id;
    });
    if (mesh == meshes.end() || material == materials.end() || !mesh->mesh ||
        !material->pipeline || mesh->index_count != packet.index_count ||
        !backend_.has_mesh(mesh->mesh) || !backend_.has_pipeline(material->pipeline) ||
        !material->textures_resolved || material->state.depth_test != packet.depth.test ||
        material->state.depth_write != packet.depth.write ||
        material->state.alpha_blend != packet.blend.enabled ||
        !backend_.draw_indexed(target, material->pipeline, mesh->mesh)) {
      return false;
    }
  }
  return true;
}

bool VulkanSceneRenderer::render_textured(
    const RenderScene& scene, const VulkanRenderTargetHandle target,
    const std::span<const VulkanSceneTexturedMeshBinding> meshes,
    const std::span<const VulkanSceneTexturedMaterialBinding> materials,
    const std::span<const VulkanSceneTextureBinding> textures) noexcept {
  if (!scene.valid() || !target || !backend_.has_render_target(target) ||
      !render_scene_supported(scene, backend_.caps()) || scene.passes.size() != 1U ||
      !scene.hud.empty() || scene.surface.sample_count != 1U ||
      scene.surface.color_format != "rgba8_unorm" || scene.surface.depth_format != "none") {
    return false;
  }
  const RenderPass& pass = scene.passes.front();
  if (!pass.clear_color_enabled || pass.clear_depth_enabled ||
      !backend_.clear_render_target(target, pass.clear_color, pass.clear_depth)) {
    return false;
  }
  for (const DrawPacket& packet : scene.draw_packets) {
    if (packet.topology != RenderPrimitiveTopology::TriangleList ||
        packet.first_index != 0U || packet.vertex_offset != 0 ||
        packet.texture_ids.size() != 1U || !identity_transform(packet.transform)) {
      return false;
    }
    const auto mesh = std::find_if(meshes.begin(), meshes.end(), [&](const auto& binding) {
      return binding.mesh_id == packet.mesh_id;
    });
    const auto material = std::find_if(materials.begin(), materials.end(),
                                       [&](const auto& binding) {
                                         return binding.material_id == packet.material_id;
                                       });
    const auto texture = std::find_if(textures.begin(), textures.end(),
                                      [&](const auto& binding) {
                                        return binding.texture_id == packet.texture_ids.front();
                                      });
    if (mesh == meshes.end() || material == materials.end() ||
        texture == textures.end() || !mesh->mesh || !material->pipeline ||
        !texture->texture || mesh->index_count != packet.index_count ||
        !backend_.has_textured_mesh(mesh->mesh) ||
        !backend_.has_pipeline(material->pipeline) ||
        !backend_.has_texture(texture->texture) || !material->textures_resolved ||
        material->state.depth_test != packet.depth.test ||
        material->state.depth_write != packet.depth.write ||
        material->state.alpha_blend != packet.blend.enabled ||
        !backend_.draw_textured_indexed(target, material->pipeline, mesh->mesh,
                                        texture->texture)) {
      return false;
    }
  }
  return true;
}

bool VulkanSceneRenderer::render_clip_textured(
    const RenderScene& scene, const VulkanRenderTargetHandle target,
    const std::span<const VulkanSceneClipTexturedMeshBinding> meshes,
    const std::span<const VulkanSceneTexturedMaterialBinding> materials,
    const std::span<const VulkanSceneTextureBinding> textures) noexcept {
  if (!scene.valid() || !target || !backend_.has_render_target(target) ||
      !render_scene_supported(scene, backend_.caps()) || scene.passes.size() != 1U ||
      !scene.hud.empty() || scene.surface.sample_count != 1U ||
      scene.surface.color_format != "rgba8_unorm" ||
      scene.surface.depth_format != "none") {
    return false;
  }
  const RenderPass& pass = scene.passes.front();
  if (!pass.clear_color_enabled || pass.clear_depth_enabled ||
      !backend_.clear_render_target(target, pass.clear_color, pass.clear_depth)) {
    return false;
  }
  for (const DrawPacket& packet : scene.draw_packets) {
    if (packet.topology != RenderPrimitiveTopology::TriangleList ||
        packet.first_index != 0U || packet.vertex_offset != 0 ||
        packet.texture_ids.size() != 1U || !identity_transform(packet.transform)) {
      return false;
    }
    const auto mesh = std::find_if(meshes.begin(), meshes.end(), [&](const auto& binding) {
      return binding.mesh_id == packet.mesh_id;
    });
    const auto material = std::find_if(materials.begin(), materials.end(),
                                       [&](const auto& binding) {
                                         return binding.material_id == packet.material_id;
                                       });
    const auto texture = std::find_if(textures.begin(), textures.end(),
                                      [&](const auto& binding) {
                                        return binding.texture_id == packet.texture_ids.front();
                                      });
    if (mesh == meshes.end() || material == materials.end() ||
        texture == textures.end() || !mesh->mesh || !material->pipeline ||
        !texture->texture || mesh->index_count != packet.index_count ||
        !backend_.has_clip_textured_mesh(mesh->mesh) ||
        !backend_.has_pipeline(material->pipeline) ||
        !backend_.has_texture(texture->texture) || !material->textures_resolved ||
        material->state.depth_test != packet.depth.test ||
        material->state.depth_write != packet.depth.write ||
        material->state.alpha_blend != packet.blend.enabled ||
        !backend_.draw_clip_textured_indexed(target, material->pipeline, mesh->mesh,
                                             texture->texture)) {
      return false;
    }
  }
  return true;
}

bool VulkanSceneRenderer::render_world_textured(
    const RenderScene& scene, const VulkanRenderTargetHandle target,
    const std::span<const VulkanSceneWorldTexturedMeshBinding> meshes,
    const std::span<const VulkanSceneTexturedMaterialBinding> materials,
    const std::span<const VulkanSceneTextureBinding> textures) noexcept {
  if (!scene.valid() || !target || !backend_.render_target_has_d32(target) ||
      !render_scene_supported(scene, backend_.caps()) ||
      scene.passes.size() != 1U || !scene.hud.empty() ||
      scene.surface.sample_count != 1U ||
      scene.surface.color_format != "rgba8_unorm" ||
      scene.surface.depth_format != "d32_sfloat" ||
      scene.surface.sampler_anisotropy != 1.0F ||
      !std::all_of(scene.materials.begin(), scene.materials.end(),
                   world_material_supported)) {
    return false;
  }
  std::array<float, 16> world_to_clip{};
  if (!make_world_to_clip(scene.camera, scene.surface, world_to_clip)) {
    return false;
  }
  const RenderPass& pass = scene.passes.front();
  if (!pass.clear_color_enabled || !pass.clear_depth_enabled ||
      !backend_.clear_render_target(target, pass.clear_color,
                                    pass.clear_depth)) {
    return false;
  }
  for (const DrawPacket& packet : scene.draw_packets) {
    if (packet.topology != RenderPrimitiveTopology::TriangleList ||
        packet.first_index != 0U || packet.vertex_offset != 0 ||
        packet.texture_ids.size() != 1U || !packet.depth.test ||
        !packet.depth.write || packet.depth.reverse_z ||
        packet.raster.cull_back_faces || packet.raster.wireframe ||
        packet.blend.enabled || packet.blend.source_factor != 1U ||
        packet.blend.destination_factor != 0U) {
      return false;
    }
    std::array<float, 16> object_to_clip{};
    if (!multiply_row_major(packet.transform, world_to_clip, object_to_clip)) {
      return false;
    }
    // With culling disabled front_counter_clockwise has no raster effect.
    const auto mesh = std::find_if(
        meshes.begin(), meshes.end(),
        [&](const auto& binding) { return binding.mesh_id == packet.mesh_id; });
    const auto material = std::find_if(
        materials.begin(), materials.end(), [&](const auto& binding) {
          return binding.material_id == packet.material_id;
        });
    const auto texture = std::find_if(
        textures.begin(), textures.end(), [&](const auto& binding) {
          return binding.texture_id == packet.texture_ids.front();
        });
    if (mesh == meshes.end() || material == materials.end() ||
        texture == textures.end() || !mesh->mesh || !material->pipeline ||
        !texture->texture || mesh->index_count != packet.index_count ||
        !backend_.has_world_textured_mesh(mesh->mesh) ||
        !backend_.has_pipeline(material->pipeline) ||
        !backend_.has_texture(texture->texture) ||
        !material->textures_resolved ||
        material->state.depth_test != packet.depth.test ||
        material->state.depth_write != packet.depth.write ||
        material->state.alpha_blend != packet.blend.enabled ||
        !backend_.draw_world_textured_indexed(target, material->pipeline,
                                              mesh->mesh, texture->texture,
                                              object_to_clip)) {
      return false;
    }
  }
  return true;
}

}  // namespace ac6
