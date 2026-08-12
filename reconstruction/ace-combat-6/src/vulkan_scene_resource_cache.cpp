#include "ac6/vulkan_scene_resource_cache.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <utility>

namespace ac6 {
namespace {

[[nodiscard]] bool unique_ids(std::span<const std::string> values) noexcept {
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (values[index].empty()) return false;
    for (std::size_t next = index + 1; next < values.size(); ++next) {
      if (values[index] == values[next]) return false;
    }
  }
  return true;
}

[[nodiscard]] bool bounded_scene(const RenderScene& scene) noexcept {
  if (!scene.valid() || scene.passes.size() != 1U || !scene.hud.empty() ||
      scene.surface.sample_count != 1U ||
      scene.surface.color_format != "rgba8_unorm" ||
      scene.surface.depth_format != "none") {
    return false;
  }
  for (const DrawPacket& packet : scene.draw_packets) {
    if (packet.topology != RenderPrimitiveTopology::TriangleList ||
        packet.first_index != 0U || packet.vertex_offset != 0 ||
        packet.texture_ids.size() != 0U) {
      return false;
    }
    constexpr std::array<float, 16> identity{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F};
    if (packet.transform != identity) return false;
  }
  for (const MaterialPipeline& material : scene.materials) {
    if (!material.texture_bindings.empty() || !material.sampler_bindings.empty()) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool bounded_textured_scene(const RenderScene& scene) noexcept {
  if (!scene.valid() || scene.passes.size() != 1U || !scene.hud.empty() ||
      scene.surface.sample_count != 1U ||
      scene.surface.color_format != "rgba8_unorm" ||
      scene.surface.depth_format != "none") {
    return false;
  }
  for (const DrawPacket& packet : scene.draw_packets) {
    if (packet.topology != RenderPrimitiveTopology::TriangleList ||
        packet.first_index != 0U || packet.vertex_offset != 0 ||
        packet.texture_ids.size() != 1U) {
      return false;
    }
    constexpr std::array<float, 16> identity{
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 0.0F, 0.0F, 1.0F};
    if (packet.transform != identity) return false;
  }
  for (const MaterialPipeline& material : scene.materials) {
    if (material.texture_bindings.size() != 1U ||
        material.texture_bindings.front().resource_id.empty()) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::optional<VulkanMission01TexturedUpload>
make_vulkan_mission01_textured_upload(
    const std::string_view mesh_id, const std::string_view texture_id,
    const std::span<const retail::NdxrPosition> positions,
    const std::span<const retail::NdxrTexcoord> texcoords,
    const std::span<const std::uint16_t> indices,
    const retail::DecodedTexture& texture) noexcept {
  if (mesh_id.empty() || texture_id.empty() || positions.empty() ||
      positions.size() != texcoords.size() || indices.empty() ||
      indices.size() % 3U != 0U || texture.width == 0U || texture.height == 0U ||
      texture.pixels.size() != static_cast<std::size_t>(texture.width) *
                                     texture.height) {
    return std::nullopt;
  }
  VulkanMission01TexturedUpload upload;
  upload.mesh_id = std::string(mesh_id);
  upload.texture_id = std::string(texture_id);
  upload.texture_width = texture.width;
  upload.texture_height = texture.height;
  upload.rgba8.resize(texture.pixels.size() * 4U);
  for (std::size_t index = 0U; index < texture.pixels.size(); ++index) {
    const std::uint32_t pixel = texture.pixels[index];
    upload.rgba8[index * 4U] = static_cast<std::uint8_t>(pixel & 0xFFU);
    upload.rgba8[index * 4U + 1U] =
        static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU);
    upload.rgba8[index * 4U + 2U] =
        static_cast<std::uint8_t>((pixel >> 16U) & 0xFFU);
    upload.rgba8[index * 4U + 3U] =
        static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU);
  }
  upload.vertices.reserve(positions.size());
  for (std::size_t index = 0U; index < positions.size(); ++index) {
    const auto& position = positions[index];
    const auto& uv = texcoords[index];
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z) || !std::isfinite(uv.u) ||
        !std::isfinite(uv.v)) {
      return std::nullopt;
    }
    // This first adapter is deliberately 2D: it only accepts already
    // projected X/Y inputs.  A world-space Z/transform path is a later,
    // trace-qualified checkpoint, never silently dropped here.
    if (position.z != 0.0F) return std::nullopt;
    upload.vertices.push_back({position.x, position.y, uv.u, uv.v});
  }
  upload.indices.reserve(indices.size());
  for (const std::uint16_t index : indices) {
    if (index == retail::kStripRestart || index >= upload.vertices.size()) {
      return std::nullopt;
    }
    upload.indices.push_back(index);
  }
  return upload;
}

VulkanSceneResourceCache::VulkanSceneResourceCache(
    VulkanBackend& backend) noexcept
    : backend_(backend), renderer_(backend) {}

VulkanSceneResourceCache::~VulkanSceneResourceCache() { reset(); }

void VulkanSceneResourceCache::reset() noexcept {
  for (const VulkanSceneMaterialBinding& binding : material_bindings_) {
    backend_.release_pipeline(binding.pipeline);
  }
  for (const VulkanSceneMeshBinding& binding : mesh_bindings_) {
    backend_.release_mesh(binding.mesh);
  }
  for (const VulkanSceneTexturedMaterialBinding& binding :
       textured_material_bindings_) {
    backend_.release_pipeline(binding.pipeline);
  }
  for (const VulkanSceneTextureBinding& binding : texture_bindings_) {
    backend_.release_texture(binding.texture);
  }
  for (const VulkanSceneTexturedMeshBinding& binding :
       textured_mesh_bindings_) {
    backend_.release_textured_mesh(binding.mesh);
  }
  material_bindings_.clear();
  mesh_bindings_.clear();
  textured_material_bindings_.clear();
  texture_bindings_.clear();
  textured_mesh_bindings_.clear();
  texture_ids_.clear();
  material_ids_.clear();
  mesh_ids_.clear();
  scene_digest_.fill(0U);
  target_ = {};
  ready_ = false;
  textured_mode_ = false;
}

bool VulkanSceneResourceCache::build(
    const RenderScene& scene, const VulkanRenderTargetHandle target,
    const std::span<const VulkanSceneMeshUpload> meshes,
    const std::span<const VulkanSceneMaterialUpload> materials) noexcept {
  if (!bounded_scene(scene) || !target || !backend_.has_render_target(target) ||
      !render_scene_supported(scene, backend_.caps())) {
    return false;
  }

  std::vector<std::string> required_mesh_ids;
  required_mesh_ids.reserve(scene.draw_packets.size());
  for (const DrawPacket& packet : scene.draw_packets) {
    if (std::find(required_mesh_ids.begin(), required_mesh_ids.end(),
                  packet.mesh_id) == required_mesh_ids.end()) {
      required_mesh_ids.push_back(packet.mesh_id);
    }
  }
  std::vector<std::string> required_material_ids;
  required_material_ids.reserve(scene.materials.size());
  for (const MaterialPipeline& material : scene.materials) {
    required_material_ids.push_back(material.stable_id);
  }
  std::sort(required_mesh_ids.begin(), required_mesh_ids.end());
  required_mesh_ids.erase(
      std::unique(required_mesh_ids.begin(), required_mesh_ids.end()),
      required_mesh_ids.end());
  if (!unique_ids(required_mesh_ids) || !unique_ids(required_material_ids)) {
    return false;
  }
  std::vector<std::string> upload_mesh_ids;
  upload_mesh_ids.reserve(meshes.size());
  for (const VulkanSceneMeshUpload& upload : meshes) {
    upload_mesh_ids.emplace_back(upload.mesh_id);
  }
  std::vector<std::string> upload_material_ids;
  upload_material_ids.reserve(materials.size());
  for (const VulkanSceneMaterialUpload& upload : materials) {
    upload_material_ids.emplace_back(upload.material_id);
  }
  if (!unique_ids(upload_mesh_ids) || !unique_ids(upload_material_ids)) {
    return false;
  }

  reset();
  std::vector<VulkanSceneMeshBinding> new_mesh_bindings;
  std::vector<VulkanSceneMaterialBinding> new_material_bindings;
  std::vector<std::string> new_mesh_ids;
  std::vector<std::string> new_material_ids;
  const auto discard = [&]() noexcept {
    for (const VulkanSceneMaterialBinding& binding : new_material_bindings) {
      backend_.release_pipeline(binding.pipeline);
    }
    for (const VulkanSceneMeshBinding& binding : new_mesh_bindings) {
      backend_.release_mesh(binding.mesh);
    }
    return false;
  };
  try {
    new_mesh_bindings.reserve(required_mesh_ids.size());
    new_mesh_ids.reserve(required_mesh_ids.size());
    for (const std::string& id : required_mesh_ids) {
      const auto found = std::find_if(meshes.begin(), meshes.end(),
                                      [id](const VulkanSceneMeshUpload& upload) {
                                        return upload.mesh_id == id;
                                      });
      if (found == meshes.end() || found->vertices.empty() ||
          found->indices.empty() || found->indices.size() % 3U != 0U) {
        return discard();
      }
      const VulkanMeshHandle handle =
          backend_.create_mesh(found->vertices, found->indices);
      if (!handle) return discard();
      new_mesh_ids.push_back(id);
      new_mesh_bindings.push_back({{}, handle,
                                   static_cast<std::uint32_t>(found->indices.size())});
    }
    new_material_bindings.reserve(required_material_ids.size());
    new_material_ids.reserve(required_material_ids.size());
    for (const std::string& id : required_material_ids) {
      const auto found = std::find_if(
          materials.begin(), materials.end(),
          [id](const VulkanSceneMaterialUpload& upload) {
            return upload.material_id == id;
          });
      const auto packet_it = std::find_if(
          scene.draw_packets.begin(), scene.draw_packets.end(),
          [id](const DrawPacket& packet) {
            return packet.material_id == id;
          });
      if (found == materials.end() || packet_it == scene.draw_packets.end() ||
          found->vertex_spirv.empty() ||
          found->fragment_spirv.empty() || !found->textures_resolved) {
        return discard();
      }
      if (found->state.depth_test != packet_it->depth.test ||
          found->state.depth_write != packet_it->depth.write ||
          found->state.alpha_blend != packet_it->blend.enabled) {
        return discard();
      }
      const VulkanPipelineHandle handle = backend_.create_pipeline(
          target, found->vertex_spirv, found->fragment_spirv, found->state);
      if (!handle) return discard();
      new_material_ids.push_back(id);
      new_material_bindings.push_back({{}, handle, found->state, true});
    }
  } catch (...) {
    return discard();
  }

  mesh_ids_ = std::move(new_mesh_ids);
  material_ids_ = std::move(new_material_ids);
  mesh_bindings_ = std::move(new_mesh_bindings);
  material_bindings_ = std::move(new_material_bindings);
  for (std::size_t index = 0; index < mesh_bindings_.size(); ++index) {
    mesh_bindings_[index].mesh_id = mesh_ids_[index];
  }
  for (std::size_t index = 0; index < material_bindings_.size(); ++index) {
    material_bindings_[index].material_id = material_ids_[index];
  }
  target_ = target;
  scene_digest_ = scene.digest;
  ready_ = true;
  textured_mode_ = false;
  return true;
}

bool VulkanSceneResourceCache::build_textured(
    const RenderScene& scene, const VulkanRenderTargetHandle target,
    const std::span<const VulkanSceneTexturedMeshUpload> meshes,
    const std::span<const VulkanSceneTexturedMaterialUpload> materials,
    const std::span<const VulkanSceneTextureUpload> textures) noexcept {
  if (!bounded_textured_scene(scene) || !target ||
      !backend_.has_render_target(target) ||
      !render_scene_supported(scene, backend_.caps())) {
    return false;
  }
  std::vector<std::string> required_mesh_ids;
  std::vector<std::string> required_material_ids;
  std::vector<std::string> required_texture_ids;
  for (const DrawPacket& packet : scene.draw_packets) {
    required_mesh_ids.push_back(packet.mesh_id);
    required_texture_ids.push_back(packet.texture_ids.front());
  }
  for (const MaterialPipeline& material : scene.materials) {
    required_material_ids.push_back(material.stable_id);
  }
  for (const MaterialPipeline& material : scene.materials) {
    if (material.texture_bindings.size() != 1U ||
        material.texture_bindings.front().slot != 0U ||
        material.texture_bindings.front().resource_id.empty()) {
      return false;
    }
  }
  const auto normalize = [](std::vector<std::string>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
  };
  normalize(required_mesh_ids);
  normalize(required_texture_ids);
  normalize(required_material_ids);
  std::vector<std::string> upload_mesh_ids;
  std::vector<std::string> upload_material_ids;
  std::vector<std::string> upload_texture_ids;
  for (const auto& upload : meshes) upload_mesh_ids.emplace_back(upload.mesh_id);
  for (const auto& upload : materials) upload_material_ids.emplace_back(upload.material_id);
  for (const auto& upload : textures) upload_texture_ids.emplace_back(upload.texture_id);
  if (!unique_ids(required_mesh_ids) || !unique_ids(required_material_ids) ||
      !unique_ids(required_texture_ids) || !unique_ids(upload_mesh_ids) ||
      !unique_ids(upload_material_ids) || !unique_ids(upload_texture_ids)) {
    return false;
  }

  reset();
  std::vector<VulkanSceneTexturedMeshBinding> new_mesh_bindings;
  std::vector<VulkanSceneTexturedMaterialBinding> new_material_bindings;
  std::vector<VulkanSceneTextureBinding> new_texture_bindings;
  std::vector<std::string> new_mesh_ids;
  std::vector<std::string> new_material_ids;
  std::vector<std::string> new_texture_ids;
  const auto discard = [&]() noexcept {
    for (const auto& binding : new_material_bindings) {
      backend_.release_pipeline(binding.pipeline);
    }
    for (const auto& binding : new_texture_bindings) {
      backend_.release_texture(binding.texture);
    }
    for (const auto& binding : new_mesh_bindings) {
      backend_.release_textured_mesh(binding.mesh);
    }
    return false;
  };
  try {
    for (const std::string& id : required_mesh_ids) {
      const auto found = std::find_if(
          meshes.begin(), meshes.end(), [id](const auto& upload) {
            return upload.mesh_id == id;
          });
      if (found == meshes.end() || found->vertices.empty() ||
          found->indices.empty() || found->indices.size() % 3U != 0U) {
        return discard();
      }
      const VulkanTexturedMeshHandle handle =
          backend_.create_textured_mesh(found->vertices, found->indices);
      if (!handle) return discard();
      new_mesh_ids.push_back(id);
      new_mesh_bindings.push_back({{}, handle,
                                   static_cast<std::uint32_t>(found->indices.size())});
    }
    for (const std::string& id : required_texture_ids) {
      const auto found = std::find_if(
          textures.begin(), textures.end(), [id](const auto& upload) {
            return upload.texture_id == id;
          });
      if (found == textures.end()) return discard();
      const VulkanTextureHandle handle = backend_.create_texture_rgba8(
          found->width, found->height, found->rgba8);
      if (!handle) return discard();
      new_texture_ids.push_back(id);
      new_texture_bindings.push_back({{}, handle});
    }
    for (const std::string& id : required_material_ids) {
      const auto found = std::find_if(
          materials.begin(), materials.end(), [id](const auto& upload) {
            return upload.material_id == id;
          });
      const auto material_it = std::find_if(
          scene.materials.begin(), scene.materials.end(),
          [id](const MaterialPipeline& material) { return material.stable_id == id; });
      const auto packet_it = std::find_if(
          scene.draw_packets.begin(), scene.draw_packets.end(),
          [id](const DrawPacket& packet) { return packet.material_id == id; });
      if (found == materials.end() || material_it == scene.materials.end() ||
          packet_it == scene.draw_packets.end() || found->vertex_spirv.empty() ||
          found->fragment_spirv.empty() || !found->textures_resolved ||
          found->state.depth_test != packet_it->depth.test ||
          found->state.depth_write != packet_it->depth.write ||
          found->state.alpha_blend != packet_it->blend.enabled) {
        return discard();
      }
      if (material_it->texture_bindings.front().resource_id !=
          packet_it->texture_ids.front()) {
        return discard();
      }
      const VulkanPipelineHandle handle = backend_.create_textured_pipeline(
          target, found->vertex_spirv, found->fragment_spirv, found->state);
      if (!handle) return discard();
      new_material_ids.push_back(id);
      new_material_bindings.push_back({{}, handle, found->state, true});
    }
  } catch (...) {
    return discard();
  }
  textured_mesh_bindings_ = std::move(new_mesh_bindings);
  textured_material_bindings_ = std::move(new_material_bindings);
  texture_bindings_ = std::move(new_texture_bindings);
  mesh_ids_ = std::move(new_mesh_ids);
  material_ids_ = std::move(new_material_ids);
  texture_ids_ = std::move(new_texture_ids);
  for (std::size_t index = 0; index < textured_mesh_bindings_.size(); ++index) {
    textured_mesh_bindings_[index].mesh_id = mesh_ids_[index];
  }
  for (std::size_t index = 0; index < textured_material_bindings_.size(); ++index) {
    textured_material_bindings_[index].material_id = material_ids_[index];
  }
  for (std::size_t index = 0; index < texture_bindings_.size(); ++index) {
    texture_bindings_[index].texture_id = texture_ids_[index];
  }
  target_ = target;
  scene_digest_ = scene.digest;
  ready_ = true;
  textured_mode_ = true;
  return true;
}

bool VulkanSceneResourceCache::render(const RenderScene& scene) noexcept {
  if (!ready_ || !scene.valid() || scene.digest != scene_digest_) return false;
  if (textured_mode_) {
    return renderer_.render_textured(scene, target_, textured_mesh_bindings_,
                                     textured_material_bindings_, texture_bindings_);
  }
  return renderer_.render(scene, target_, mesh_bindings_, material_bindings_);
}

std::size_t VulkanSceneResourceCache::live_mesh_count() const noexcept {
  return mesh_bindings_.size() + textured_mesh_bindings_.size();
}

std::size_t VulkanSceneResourceCache::live_pipeline_count() const noexcept {
  return material_bindings_.size() + textured_material_bindings_.size();
}

std::size_t VulkanSceneResourceCache::live_texture_count() const noexcept {
  return texture_bindings_.size();
}

}  // namespace ac6
