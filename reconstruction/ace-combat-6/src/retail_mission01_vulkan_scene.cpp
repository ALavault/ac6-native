#include "ac6/retail_mission01_vulkan_scene.h"

#include <cmath>
#include <utility>

namespace ac6::retail {
namespace {

bool finite_matrix(const std::array<float, 16>& matrix) noexcept {
  for (const float value : matrix) {
    if (!std::isfinite(value)) return false;
  }
  return true;
}

bool valid_dimensions(const std::uint32_t width,
                      const std::uint32_t height) noexcept {
  return width != 0U && height != 0U;
}

std::array<float, 16> with_placement_translation(
    const std::array<float, 16>& object_to_clip,
    const Mission01MapDrawInstance& instance) noexcept {
  const std::array<float, 16> local_to_world{
      1.0F, 0.0F, 0.0F, instance.world_x,
      0.0F, 1.0F, 0.0F, instance.world_y,
      0.0F, 0.0F, 1.0F, instance.world_z,
      0.0F, 0.0F, 0.0F, 1.0F};
  std::array<float, 16> result{};
  for (std::size_t row = 0U; row < 4U; ++row) {
    for (std::size_t column = 0U; column < 4U; ++column) {
      float value = 0.0F;
      for (std::size_t inner = 0U; inner < 4U; ++inner) {
        value += object_to_clip[row * 4U + inner] *
                 local_to_world[inner * 4U + column];
      }
      result[row * 4U + column] = value;
    }
  }
  return result;
}

std::string material_id_for(const std::uint32_t draw_instance_index) {
  return "retail-m01-material-" + std::to_string(draw_instance_index);
}

std::string mesh_id_for(const std::uint32_t draw_instance_index) {
  return "retail-m01-mesh-" + std::to_string(draw_instance_index);
}

std::string texture_id_for(const std::uint32_t identifier) {
  return "retail-m01-texture-" + std::to_string(identifier);
}

}  // namespace

RetailMission01VulkanScene::RetailMission01VulkanScene(
    RetailMission01MapRenderAssets assets,
    VulkanMission01ClipTexturedUpload upload,
    std::vector<std::uint32_t> vertex_spirv,
    std::vector<std::uint32_t> fragment_spirv, RenderScene scene,
    RetailMission01VulkanSceneReport report, std::string material_id) noexcept
    : assets_(std::move(assets)),
      upload_(std::move(upload)),
      vertex_spirv_(std::move(vertex_spirv)),
      fragment_spirv_(std::move(fragment_spirv)),
      scene_(std::move(scene)),
      report_(report),
      material_id_(std::move(material_id)) {}

std::optional<RetailMission01VulkanScene> RetailMission01VulkanScene::open(
    const RetailContentStore& store, const std::uint32_t draw_instance_index,
    const bool swap_16, const std::array<float, 16>& object_to_clip,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const std::uint64_t vertex_shader_hash,
    const std::uint64_t fragment_shader_hash, const std::uint32_t width,
    const std::uint32_t height) {
  std::optional<RetailMission01MapRenderAssets> assets =
      RetailMission01MapRenderAssets::open(store);
  if (!assets.has_value() || !assets->store_backed()) return std::nullopt;
  return build(std::move(*assets), draw_instance_index, swap_16,
               object_to_clip, vertex_spirv, fragment_spirv,
               vertex_shader_hash, fragment_shader_hash, width, height);
}

std::optional<RetailMission01VulkanScene>
RetailMission01VulkanScene::build_for_testing(
    RetailMission01MapRenderAssets assets,
    const std::uint32_t draw_instance_index, const bool swap_16,
    const std::array<float, 16>& object_to_clip,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const std::uint64_t vertex_shader_hash,
    const std::uint64_t fragment_shader_hash, const std::uint32_t width,
    const std::uint32_t height) {
  return build(std::move(assets), draw_instance_index, swap_16,
               object_to_clip, vertex_spirv, fragment_spirv,
               vertex_shader_hash, fragment_shader_hash, width, height);
}

std::optional<RetailMission01VulkanScene> RetailMission01VulkanScene::build(
    RetailMission01MapRenderAssets assets,
    const std::uint32_t draw_instance_index, const bool swap_16,
    const std::array<float, 16>& object_to_clip,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const std::uint64_t vertex_shader_hash,
    const std::uint64_t fragment_shader_hash, const std::uint32_t width,
    const std::uint32_t height) {
  if (!finite_matrix(object_to_clip) || !valid_dimensions(width, height) ||
      vertex_spirv.empty() || fragment_spirv.empty() ||
      vertex_shader_hash == 0U || fragment_shader_hash == 0U ||
      draw_instance_index >= assets.draw_instances().size()) {
    return std::nullopt;
  }

  const Mission01MapDrawInstance& instance =
      assets.draw_instances()[draw_instance_index];
  const Mission01MapPrimitive* primitive = assets.primitive_for(instance);
  if (primitive == nullptr || primitive->geometry.positions.empty() ||
      primitive->geometry.texcoords.size() !=
          primitive->geometry.positions.size() ||
      primitive->geometry.indices.empty()) {
    return std::nullopt;
  }
  NtxrRefusal refusal = NtxrRefusal::None;
  const std::optional<DecodedTexture> texture = assets.decode_texture(
      primitive->texture_identifier, swap_16, &refusal);
  if (!texture.has_value()) return std::nullopt;

  const std::string mesh_id = mesh_id_for(draw_instance_index);
  const std::string texture_id = texture_id_for(primitive->texture_identifier);
  const std::array<float, 16> placed_object_to_clip =
      with_placement_translation(object_to_clip, instance);
  const std::optional<VulkanMission01ClipTexturedUpload> upload =
      make_vulkan_mission01_clip_textured_upload(
          mesh_id, texture_id, primitive->geometry.positions,
          primitive->geometry.texcoords, primitive->geometry.indices,
          placed_object_to_clip, *texture);
  if (!upload.has_value()) return std::nullopt;

  try {
    std::vector<std::uint32_t> vertex_copy(vertex_spirv.begin(),
                                           vertex_spirv.end());
    std::vector<std::uint32_t> fragment_copy(fragment_spirv.begin(),
                                              fragment_spirv.end());
    const std::string material_id = material_id_for(draw_instance_index);

    RenderScene scene;
    scene.tick = 1U;
    scene.mission_id = 1U;
    // Clip vertices are already projected. This camera is metadata required by
    // RenderScene and is intentionally not consumed by the Vulkan clip path.
    scene.camera = {};
    scene.surface.width = width;
    scene.surface.height = height;
    scene.surface.sample_count = 1U;
    scene.surface.color_format = "rgba8_unorm";
    scene.surface.depth_format = "none";
    scene.surface.present_mode = RenderSurfaceRequirements::PresentMode::Headless;
    scene.passes.push_back(
        {"retail-m01-world-direct", 0U, {0.0F, 0.0F, 0.0F, 1.0F},
         1.0F, true, false});
    scene.materials.push_back(
        {material_id, vertex_shader_hash, fragment_shader_hash,
         {{0U, texture_id}}, {}, 0U, 0U, 1.0F});
    DrawPacket packet;
    packet.mesh_id = mesh_id;
    packet.material_id = material_id;
    packet.texture_ids.push_back(texture_id);
    packet.index_count = static_cast<std::uint32_t>(upload->indices.size());
    packet.depth.test = false;
    packet.depth.write = false;
    packet.blend.enabled = false;
    packet.sort_key = draw_instance_index;
    scene.draw_packets.push_back(std::move(packet));
    scene.refresh_digest();
    if (!scene.valid()) return std::nullopt;

    RetailMission01VulkanSceneReport report;
    report.content_index_sha256 = assets.content_index_sha256();
    report.draw_instance_index = draw_instance_index;
    report.selector = instance.selector;
    report.record_index = instance.record_index;
    report.texture_identifier = primitive->texture_identifier;
    report.vertex_count = upload->vertices.size();
    report.index_count = upload->indices.size();
    report.store_backed = assets.store_backed();
    report.clip_matrix_supplied = true;
    report.placement_translation_applied = true;
    report.shader_bytes_supplied = true;
    report.jv_eligible = false;
    return RetailMission01VulkanScene(
        std::move(assets), std::move(*upload), std::move(vertex_copy),
        std::move(fragment_copy), std::move(scene), report, material_id);
  } catch (...) {
    return std::nullopt;
  }
}

VulkanSceneClipTexturedMeshUpload
RetailMission01VulkanScene::mesh_upload() const noexcept {
  return {upload_.mesh_id, upload_.vertices, upload_.indices};
}

VulkanSceneTexturedMaterialUpload
RetailMission01VulkanScene::material_upload() const noexcept {
  return {material_id_, vertex_spirv_, fragment_spirv_, {}, true};
}

VulkanSceneTextureUpload RetailMission01VulkanScene::texture_upload()
    const noexcept {
  return {upload_.texture_id, upload_.texture_width, upload_.texture_height,
          upload_.rgba8};
}

}  // namespace ac6::retail
