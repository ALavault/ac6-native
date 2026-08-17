#include "ac6/retail_mission01_vulkan_scene.h"

#include "vulkan_retail_shaders.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
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

struct RuntimeTerrainPacket final {
  std::string mesh_id;
  std::string texture_id;
  std::uint32_t index_count{};
  std::uint32_t sort_key{};
};

bool append_terrain_uploads(
    const RetailMission01MapRenderAssets& assets, const bool swap_16,
    std::vector<VulkanMission01WorldTexturedUpload>& world_uploads,
    std::vector<VulkanMission01TextureUpload>& texture_uploads,
    std::set<std::uint32_t>& texture_ids,
    std::vector<RuntimeTerrainPacket>& packets) {
  const Mission01TerrainRenderResource& terrain = assets.terrain_resource();
  if (terrain.draw_instances.size() != 65536U ||
      terrain.topology.vertices.size() != 40U) {
    return false;
  }
  std::array<bool, 8> page_loaded{};
  constexpr std::size_t kCellsPerBatch = 256U;
  constexpr std::size_t kBatchCount = 65536U / kCellsPerBatch;
  for (std::size_t batch = 0U; batch < kBatchCount; ++batch) {
    std::array<std::size_t, 8> upload_for_page{};
    upload_for_page.fill(std::numeric_limits<std::size_t>::max());
    const std::size_t first = batch * kCellsPerBatch;
    const std::size_t last = first + kCellsPerBatch;
    for (std::size_t instance_index = first; instance_index < last;
         ++instance_index) {
      const Mission01TerrainCellInstance& cell =
          terrain.draw_instances[instance_index];
      if (cell.atlas.page >= terrain.atlas_pages.size()) return false;
      const std::size_t page = cell.atlas.page;
      if (upload_for_page[page] == std::numeric_limits<std::size_t>::max()) {
        const Mission01TerrainAtlasPage& atlas = terrain.atlas_pages[page];
        const std::string mesh_id =
            "retail-m01-terrain-batch-" + std::to_string(batch) + "-page-" +
            std::to_string(page);
        const std::string texture_id = texture_id_for(atlas.identifier);
        world_uploads.push_back({});
        VulkanMission01WorldTexturedUpload& upload = world_uploads.back();
        upload.mesh_id = mesh_id;
        upload.texture_id = texture_id;
        upload_for_page[page] = world_uploads.size() - 1U;
        packets.push_back({mesh_id, texture_id, 0U,
                           static_cast<std::uint32_t>(world_uploads.size())});
        if (!page_loaded[page]) {
          NtxrRefusal refusal = NtxrRefusal::None;
          const std::optional<DecodedTexture> texture = decode_ntxr_base_level(
              atlas.source.data(), atlas.source.size(), swap_16, &refusal);
          if (!texture.has_value()) return false;
          if (texture_ids.insert(atlas.identifier).second) {
            std::vector<std::uint8_t> rgba8(texture->pixels.size() * 4U);
            for (std::size_t pixel = 0U; pixel < texture->pixels.size();
                 ++pixel) {
              const std::uint32_t value = texture->pixels[pixel];
              rgba8[pixel * 4U] = static_cast<std::uint8_t>(value & 0xffU);
              rgba8[pixel * 4U + 1U] =
                  static_cast<std::uint8_t>((value >> 8U) & 0xffU);
              rgba8[pixel * 4U + 2U] =
                  static_cast<std::uint8_t>((value >> 16U) & 0xffU);
              rgba8[pixel * 4U + 3U] =
                  static_cast<std::uint8_t>((value >> 24U) & 0xffU);
            }
            texture_uploads.push_back(
                {texture_id, texture->width, texture->height, std::move(rgba8)});
          }
          page_loaded[page] = true;
        }
      }
      VulkanMission01WorldTexturedUpload& upload =
          world_uploads[upload_for_page[page]];
      const std::uint16_t vertex_base =
          static_cast<std::uint16_t>(upload.vertices.size());
      for (std::size_t vertex = 0U; vertex < terrain.topology.vertices.size();
           ++vertex) {
        const std::optional<Mission01TerrainResolvedVertex> resolved =
            terrain.resolve_vertex(instance_index, vertex);
        if (!resolved.has_value()) return false;
        upload.vertices.push_back(
            {resolved->world[0], resolved->world[1], resolved->world[2],
             resolved->uv[0], resolved->uv[1]});
      }
      for (std::size_t fan = 0U; fan < 4U; ++fan) {
        const std::size_t base = fan * 10U;
        for (std::size_t vertex = 1U; vertex + 1U < 10U; ++vertex) {
          upload.indices.push_back(static_cast<std::uint16_t>(
              vertex_base + base));
          upload.indices.push_back(static_cast<std::uint16_t>(
              vertex_base + base + vertex));
          upload.indices.push_back(static_cast<std::uint16_t>(
              vertex_base + base + vertex + 1U));
        }
      }
    }
  }
  for (RuntimeTerrainPacket& packet : packets) {
    const auto found = std::find_if(
        world_uploads.begin(), world_uploads.end(),
        [&packet](const VulkanMission01WorldTexturedUpload& upload) {
          return upload.mesh_id == packet.mesh_id;
        });
    if (found == world_uploads.end() || found->indices.empty() ||
        found->indices.size() > std::numeric_limits<std::uint32_t>::max()) {
      return false;
    }
    packet.index_count = static_cast<std::uint32_t>(found->indices.size());
  }
  return !packets.empty();
}

}  // namespace

RetailMission01VulkanScene::RetailMission01VulkanScene(
    RetailMission01MapRenderAssets assets,
    VulkanMission01ClipTexturedUpload upload,
    std::vector<VulkanMission01WorldTexturedUpload> world_uploads,
    std::vector<VulkanMission01TextureUpload> world_texture_uploads,
    std::vector<std::uint32_t> vertex_spirv,
    std::vector<std::uint32_t> fragment_spirv, RenderScene scene,
    RetailMission01VulkanSceneReport report, std::string material_id) noexcept
    : assets_(std::move(assets)),
      upload_(std::move(upload)),
      world_uploads_(std::move(world_uploads)),
      world_texture_uploads_(std::move(world_texture_uploads)),
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
RetailMission01VulkanScene::open_assets(
    RetailMission01MapRenderAssets assets,
    const std::uint32_t draw_instance_index, const bool swap_16,
    const std::array<float, 16>& object_to_clip,
    const std::span<const std::uint32_t> vertex_spirv,
    const std::span<const std::uint32_t> fragment_spirv,
    const std::uint64_t vertex_shader_hash,
    const std::uint64_t fragment_shader_hash, const std::uint32_t width,
    const std::uint32_t height) {
  if (!assets.store_backed()) return std::nullopt;
  return build(std::move(assets), draw_instance_index, swap_16,
               object_to_clip, vertex_spirv, fragment_spirv,
               vertex_shader_hash, fragment_shader_hash, width, height);
}

std::optional<RetailMission01VulkanScene>
RetailMission01VulkanScene::open_runtime(
    const RetailContentStore& store, const SimulationSnapshot& snapshot,
    const bool swap_16, const std::uint32_t width, const std::uint32_t height) {
  std::optional<RetailMission01MapRenderAssets> assets =
      RetailMission01MapRenderAssets::open(store);
  if (!assets.has_value() || !assets->store_backed()) return std::nullopt;
  return build_runtime(std::move(*assets), snapshot, swap_16, width, height);
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
        std::move(assets), std::move(*upload), {}, {}, std::move(vertex_copy),
        std::move(fragment_copy), std::move(scene), report, material_id);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<RetailMission01VulkanScene>
RetailMission01VulkanScene::build_runtime(
    RetailMission01MapRenderAssets assets, const SimulationSnapshot& snapshot,
    const bool swap_16, const std::uint32_t width, const std::uint32_t height) {
  if (!snapshot.valid() || !valid_dimensions(width, height) ||
      snapshot.mission_id != 1U || assets.draw_instances().empty()) {
    return std::nullopt;
  }

  try {
    std::vector<VulkanMission01WorldTexturedUpload> world_uploads;
    std::vector<VulkanMission01TextureUpload> texture_uploads;
    std::set<std::string> mesh_ids;
    std::set<std::uint32_t> texture_ids;
    world_uploads.reserve(assets.draw_instances().size());
    for (const Mission01MapDrawInstance& instance : assets.draw_instances()) {
      const Mission01MapPrimitive* primitive = assets.primitive_for(instance);
      if (primitive == nullptr || primitive->geometry.positions.empty() ||
          primitive->geometry.texcoords.size() !=
              primitive->geometry.positions.size() ||
          primitive->geometry.indices.empty()) {
        return std::nullopt;
      }
      const std::string mesh_id =
          "retail-m01-mesh-s" + std::to_string(instance.selector) + "-r" +
          std::to_string(instance.record_index);
      const std::string texture_id =
          texture_id_for(primitive->texture_identifier);
      if (!mesh_ids.insert(mesh_id).second) continue;
      NtxrRefusal refusal = NtxrRefusal::None;
      const std::optional<DecodedTexture> texture = assets.decode_texture(
          primitive->texture_identifier, swap_16, &refusal);
      if (!texture.has_value()) return std::nullopt;
      std::optional<VulkanMission01WorldTexturedUpload> upload =
          make_vulkan_mission01_world_textured_upload(
              mesh_id, texture_id, primitive->geometry.positions,
              primitive->geometry.texcoords, primitive->geometry.indices,
              *texture);
      if (!upload.has_value()) return std::nullopt;
      if (texture_ids.insert(primitive->texture_identifier).second) {
        texture_uploads.push_back(
            {texture_id, texture->width, texture->height, upload->rgba8});
      }
      upload->texture_width = 0U;
      upload->texture_height = 0U;
      upload->rgba8.clear();
      world_uploads.push_back(std::move(*upload));
    }
    std::vector<RuntimeTerrainPacket> terrain_packets;
    if (!append_terrain_uploads(assets, swap_16, world_uploads, texture_uploads,
                                texture_ids, terrain_packets)) {
      return std::nullopt;
    }
    if (world_uploads.empty() || texture_uploads.empty()) return std::nullopt;
    std::map<std::string, std::uint32_t> mesh_index_counts;
    for (const VulkanMission01WorldTexturedUpload& upload : world_uploads) {
      if (upload.indices.empty() ||
          upload.indices.size() > std::numeric_limits<std::uint32_t>::max() ||
          !mesh_index_counts
               .emplace(upload.mesh_id,
                        static_cast<std::uint32_t>(upload.indices.size()))
               .second) {
        return std::nullopt;
      }
    }

    std::vector<std::uint32_t> vertex_spirv(
        ac6::retail_cli::detail::kRetailWorldVertexSpirv.begin(),
        ac6::retail_cli::detail::kRetailWorldVertexSpirv.end());
    std::vector<std::uint32_t> fragment_spirv(
        ac6::retail_cli::detail::kRetailWorldFragmentSpirv.begin(),
        ac6::retail_cli::detail::kRetailWorldFragmentSpirv.end());
    constexpr std::uint64_t kWorldVertexHash = 0x6d30315f77767478ULL;
    constexpr std::uint64_t kWorldFragmentHash = 0x6d30315f77666778ULL;
    const std::string material_id = "retail-m01-world-material";

    RenderScene scene;
    scene.tick = snapshot.tick;
    scene.mission_id = snapshot.mission_id;
    scene.camera = snapshot.camera;
    scene.surface.width = width;
    scene.surface.height = height;
    scene.surface.sample_count = 1U;
    scene.surface.color_format = "rgba8_unorm";
    scene.surface.depth_format = "d32_sfloat";
    scene.surface.present_mode = RenderSurfaceRequirements::PresentMode::Headless;
    scene.passes.push_back(
        {"retail-m01-world", 0U, {0.0F, 0.0F, 0.0F, 1.0F}, 1.0F, true, true});
    scene.materials.push_back(
        {material_id, kWorldVertexHash, kWorldFragmentHash,
         {{0U, std::string(kVulkanDrawPacketTexture0Binding)}}, {}, 0U, 0U,
         1.0F});
    scene.draw_packets.reserve(assets.draw_instances().size());
    for (std::size_t index = 0U; index < assets.draw_instances().size(); ++index) {
      const Mission01MapDrawInstance& instance = assets.draw_instances()[index];
      const Mission01MapPrimitive* primitive = assets.primitive_for(instance);
      if (primitive == nullptr) return std::nullopt;
      DrawPacket packet;
      packet.mesh_id =
          "retail-m01-mesh-s" + std::to_string(instance.selector) + "-r" +
          std::to_string(instance.record_index);
      packet.material_id = material_id;
      packet.texture_ids.push_back(texture_id_for(primitive->texture_identifier));
      packet.transform[3] = instance.world_x;
      packet.transform[7] = instance.world_y;
      packet.transform[11] = instance.world_z;
      const auto mesh_count = mesh_index_counts.find(packet.mesh_id);
      if (mesh_count == mesh_index_counts.end()) return std::nullopt;
      packet.index_count = mesh_count->second;
      packet.raster.cull_back_faces = false;
      packet.sort_key = static_cast<std::uint32_t>(index);
      scene.draw_packets.push_back(std::move(packet));
    }
    for (const RuntimeTerrainPacket& terrain_packet : terrain_packets) {
      DrawPacket packet;
      packet.mesh_id = terrain_packet.mesh_id;
      packet.material_id = material_id;
      packet.texture_ids.push_back(terrain_packet.texture_id);
      packet.index_count = terrain_packet.index_count;
      packet.raster.cull_back_faces = false;
      packet.sort_key = 4226U + terrain_packet.sort_key;
      scene.draw_packets.push_back(std::move(packet));
    }
    scene.refresh_digest();
    if (!scene.valid()) return std::nullopt;

    const Mission01MapDrawInstance& first = assets.draw_instances().front();
    const Mission01MapPrimitive* first_primitive = assets.primitive_for(first);
    if (first_primitive == nullptr) return std::nullopt;
    RetailMission01VulkanSceneReport report;
    report.content_index_sha256 = assets.content_index_sha256();
    report.draw_instance_index = 0U;
    report.selector = first.selector;
    report.record_index = first.record_index;
    report.texture_identifier = first_primitive->texture_identifier;
    report.vertex_count = world_uploads.front().vertices.size();
    report.index_count = world_uploads.front().indices.size();
    report.store_backed = assets.store_backed();
    report.clip_matrix_supplied = false;
    report.placement_translation_applied = true;
    report.shader_bytes_supplied = false;
    report.jv_eligible = false;
    report.runtime_draw_instances = assets.draw_instances().size();
    report.runtime_meshes = world_uploads.size();
    report.runtime_textures = texture_uploads.size();
    report.terrain_draw_instances = assets.terrain_resource().draw_instances.size();
    report.water_lookup_entries = assets.report().water_lookup_entries;
    report.complete_render_scene = false;
    return RetailMission01VulkanScene(
        std::move(assets), {}, std::move(world_uploads),
        std::move(texture_uploads), std::move(vertex_spirv),
        std::move(fragment_spirv), std::move(scene), report, material_id);
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

std::vector<VulkanSceneWorldTexturedMeshUpload>
RetailMission01VulkanScene::world_mesh_uploads() const {
  std::vector<VulkanSceneWorldTexturedMeshUpload> result;
  result.reserve(world_uploads_.size());
  for (const VulkanMission01WorldTexturedUpload& upload : world_uploads_) {
    result.push_back({upload.mesh_id, upload.vertices, upload.indices});
  }
  return result;
}

VulkanSceneTexturedMaterialUpload
RetailMission01VulkanScene::world_material_upload() const noexcept {
  return {material_id_, vertex_spirv_, fragment_spirv_, {true, true, false},
          true};
}

std::vector<VulkanSceneTextureUpload>
RetailMission01VulkanScene::world_texture_uploads() const {
  std::vector<VulkanSceneTextureUpload> result;
  result.reserve(world_texture_uploads_.size());
  for (const VulkanMission01TextureUpload& upload : world_texture_uploads_) {
    result.push_back({upload.texture_id, upload.texture_width,
                      upload.texture_height, upload.rgba8});
  }
  return result;
}

bool RetailMission01VulkanScene::update_snapshot(
    const SimulationSnapshot& snapshot) noexcept {
  if (!snapshot.valid() || snapshot.mission_id != scene_.mission_id) return false;
  scene_.tick = snapshot.tick;
  scene_.camera = snapshot.camera;
  scene_.refresh_digest();
  return scene_.valid();
}

}  // namespace ac6::retail
