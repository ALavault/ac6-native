#include "ac6/vulkan_scene_renderer.h"

#include <algorithm>
#include <array>

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

}  // namespace ac6
