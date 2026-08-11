#include "ac6/native_renderer.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace ac6 {

namespace {

std::uint64_t stable_hash(std::string_view value) noexcept {
  std::uint64_t hash = 1469598103934665603ull;
  for (const unsigned char byte : value) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash == 0 ? 1 : hash;
}

WorldFrame compatibility_frame(const SimulationSnapshot& snapshot) noexcept {
  WorldFrame frame;
  frame.tick = snapshot.tick;
  frame.mission_id = snapshot.mission_id;
  frame.mission_ready = snapshot.mission_ready;
  frame.player_entity = snapshot.player_entity;
  frame.position_x = snapshot.player_position[0];
  frame.position_y = snapshot.player_position[1];
  frame.position_z = snapshot.player_position[2];
  frame.pitch = snapshot.player_attitude[0];
  frame.roll = snapshot.player_attitude[1];
  frame.yaw = snapshot.player_attitude[2];
  frame.speed = snapshot.player_speed;
  frame.active_units = snapshot.active_units;
  frame.camera_x = snapshot.camera.position[0];
  frame.camera_y = snapshot.camera.position[1];
  frame.camera_z = snapshot.camera.position[2];
  frame.camera_target_x = snapshot.camera.target[0];
  frame.camera_target_y = snapshot.camera.target[1];
  frame.camera_target_z = snapshot.camera.target[2];
  return frame;
}

std::array<float, 16> transform_matrix(const MissionDrawableTransform& transform) noexcept {
  return {transform.scale_x, 0.0F, 0.0F, 0.0F,
          0.0F, transform.scale_y, 0.0F, 0.0F,
          0.0F, 0.0F, transform.scale_z, 0.0F,
          transform.translate_x, transform.translate_y, transform.translate_z, 1.0F};
}

RenderPrimitiveTopology topology_for(NativeIndexTopology topology) noexcept {
  return topology == NativeIndexTopology::TriangleStripRestart
             ? RenderPrimitiveTopology::TriangleStripRestart
             : RenderPrimitiveTopology::TriangleList;
}

std::array<float, 4> clear_color(std::uint32_t color) noexcept {
  return {static_cast<float>((color >> 16U) & 0xffU) / 255.0F,
          static_cast<float>((color >> 8U) & 0xffU) / 255.0F,
          static_cast<float>(color & 0xffU) / 255.0F,
          static_cast<float>((color >> 24U) & 0xffU) / 255.0F};
}

std::string sampler_id(const MissionTextureBinding& texture) {
  return texture.sampler_filter + "-" + texture.sampler_address;
}

}  // namespace

bool VulkanRenderer::RenderAssets::has(AssetId id) const noexcept {
  return database != nullptr && database->resolve(id) != nullptr;
}

bool VulkanRenderer::RenderAssets::ready_for(const WorldFrame& frame) const noexcept {
  if (definition == nullptr || definition->mission_id != frame.mission_id ||
      definition->asset_ids.empty()) {
    return false;
  }
  for (const AssetId id : definition->asset_ids) {
    if (!has(id)) return false;
    if (drawables != nullptr && drawables->find_by_asset(frame.mission_id, id).empty()) return false;
    if (drawables != nullptr && buffers != nullptr) {
      for (const MissionDrawable* drawable : drawables->find_by_asset(frame.mission_id, id)) {
        if (drawable == nullptr || !buffers->has_verified(drawable->buffer_id)) return false;
        if (geometries != nullptr && geometries->find(drawable->buffer_id) == nullptr) return false;
        if (geometries != nullptr && geometries->decoded(drawable->buffer_id) == nullptr) return false;
        if (geometries != nullptr &&
            (transforms == nullptr || transforms->find(frame.mission_id, drawable->stable_id) == nullptr)) {
          return false;
        }
        if (geometries != nullptr &&
            (materials == nullptr || materials->find(frame.mission_id, drawable->stable_id) == nullptr)) {
          return false;
        }
        if (geometries != nullptr &&
            (textures == nullptr || textures->find(frame.mission_id, drawable->stable_id) == nullptr)) {
          return false;
        }
        if (geometries != nullptr) {
          const MissionMaterial* material =
              materials == nullptr ? nullptr : materials->find(frame.mission_id, drawable->stable_id);
          if (material == nullptr || shaders == nullptr ||
              shaders->find(material->shader_permutation) == nullptr) return false;
        }
        if (geometries != nullptr &&
            (render_passes == nullptr || render_passes->find(frame.mission_id, "world") == nullptr ||
             render_targets == nullptr ||
             render_targets->find(frame.mission_id,
                                  render_passes->find(frame.mission_id, "world")->color_target) == nullptr)) {
          return false;
        }
        if (geometries != nullptr &&
            (render_passes == nullptr || render_passes->find(frame.mission_id, "world") == nullptr)) {
          return false;
        }
        if (geometries != nullptr &&
            (render_resolves == nullptr || render_resolves->find(frame.mission_id, "world") == nullptr)) {
          return false;
        }
      }
    }
  }
  return true;
}

std::optional<RenderScene> VulkanRenderer::build_scene(
    const SimulationSnapshot& snapshot, RenderAssets assets,
    const std::uint32_t width, const std::uint32_t height) const {
  if (!snapshot.valid() || !snapshot.mission_ready || snapshot.active_units == 0 ||
      snapshot.player_entity == 0) {
    return std::nullopt;
  }
  const WorldFrame frame = compatibility_frame(snapshot);
  if (!assets.ready_for(frame) || assets.definition == nullptr || assets.drawables == nullptr ||
      assets.buffers == nullptr || assets.geometries == nullptr || assets.transforms == nullptr ||
      assets.materials == nullptr ||
      assets.textures == nullptr || assets.shaders == nullptr || assets.render_targets == nullptr ||
      assets.render_passes == nullptr || assets.render_resolves == nullptr) {
    return std::nullopt;
  }

  const MissionRenderPass* world_pass = assets.render_passes->find(frame.mission_id, "world");
  const MissionRenderResolve* resolve = assets.render_resolves->find(frame.mission_id, "world");
  if (world_pass == nullptr || resolve == nullptr) return std::nullopt;
  const MissionRenderTargetDefinition* color_target =
      assets.render_targets->find(frame.mission_id, world_pass->color_target);
  const MissionRenderTargetDefinition* destination_target =
      assets.render_targets->find(frame.mission_id, resolve->destination_target);
  if (color_target == nullptr || destination_target == nullptr) return std::nullopt;

  RenderScene scene;
  scene.tick = snapshot.tick;
  scene.mission_id = snapshot.mission_id;
  scene.camera = snapshot.camera;
  scene.surface.width = width == 0 ? color_target->width : width;
  scene.surface.height = height == 0 ? color_target->height : height;
  scene.surface.sample_count = color_target->sample_count;
  scene.surface.color_format = color_target->color_format == "rgba8"
                                   ? "rgba8_unorm"
                                   : color_target->color_format;
  scene.surface.depth_format = color_target->depth_format == "none"
                                   ? "none"
                                   : "d32_sfloat";
  scene.passes.push_back({world_pass->pass_id, world_pass->order,
                          clear_color(world_pass->clear_color), world_pass->clear_depth,
                          true, world_pass->depth_target != "none"});

  std::uint32_t ordinal = 0;
  for (std::uint32_t asset_index = 0; asset_index < assets.definition->asset_ids.size();
       ++asset_index) {
    const AssetId asset = assets.definition->asset_ids[asset_index];
    const auto drawables = assets.drawables->find_by_asset(frame.mission_id, asset);
    if (drawables.empty()) return std::nullopt;
    for (const MissionDrawable* drawable : drawables) {
      if (drawable == nullptr) return std::nullopt;
      const NativeGeometryMetadata* geometry =
          assets.geometries->find(drawable->buffer_id);
      const DecodedGeometry* decoded = assets.geometries->decoded(drawable->buffer_id);
      const MissionDrawableTransform* transform =
          assets.transforms->find(frame.mission_id, drawable->stable_id);
      const MissionMaterial* material =
          assets.materials->find(frame.mission_id, drawable->stable_id);
      const MissionTextureBinding* texture =
          assets.textures->find(frame.mission_id, drawable->stable_id);
      const ShaderPermutation* shader =
          material == nullptr ? nullptr : assets.shaders->find(material->shader_permutation);
      if (geometry == nullptr || decoded == nullptr || transform == nullptr ||
          material == nullptr || texture == nullptr || shader == nullptr ||
          geometry->index_count == 0 || decoded->indices.empty()) {
        return std::nullopt;
      }

      const auto material_it = std::find_if(
          scene.materials.begin(), scene.materials.end(),
          [&material](const MaterialPipeline& candidate) {
            return candidate.stable_id == material->stable_id;
          });
      if (material_it == scene.materials.end()) {
        MaterialPipeline pipeline;
        pipeline.stable_id = material->stable_id;
        pipeline.vertex_shader_hash = stable_hash(material->shader_permutation + ":vertex");
        pipeline.fragment_shader_hash = stable_hash(material->shader_permutation + ":fragment");
        pipeline.texture_bindings.push_back({0, texture->texture_id});
        pipeline.sampler_bindings.push_back({0, sampler_id(*texture)});
        pipeline.constant_count = shader->constant_count;
        if (!pipeline.valid()) return std::nullopt;
        scene.materials.push_back(std::move(pipeline));
      }

      DrawPacket packet;
      packet.mesh_id = drawable->buffer_id;
      packet.material_id = material->stable_id;
      packet.texture_ids.push_back(texture->texture_id);
      packet.transform = transform_matrix(*transform);
      packet.index_count = geometry->index_count;
      packet.topology = topology_for(geometry->topology);
      packet.depth.test = material->depth_test;
      packet.depth.write = material->depth_write;
      packet.blend.enabled = material->blend_mode != "opaque";
      packet.blend.destination_factor = material->blend_mode == "additive" ? 1U : 7U;
      packet.raster.cull_back_faces = true;
      packet.sort_key = ordinal++;
      if (!packet.valid()) return std::nullopt;
      scene.draw_packets.push_back(std::move(packet));
    }
  }
  scene.refresh_digest();
  return scene.valid() ? std::optional<RenderScene>(std::move(scene)) : std::nullopt;
}

bool VulkanRenderer::render(const WorldFrame& frame, RenderAssets assets,
                            NativeRenderTarget* target) noexcept {
  if (!frame.mission_ready || frame.active_units == 0 || frame.player_entity == 0 ||
      !assets.ready_for(frame)) return false;
  if (frame.tick != 0 && assets.geometries != nullptr) {
    const SimulationSnapshot snapshot = make_simulation_snapshot(
        frame, ScenarioState::Gameplay, 0, 0, false);
    if (!build_scene(snapshot, assets, target == nullptr ? 0 : target->width(),
                     target == nullptr ? 0 : target->height())) {
      return false;
    }
  }
  if (target != nullptr) {
    for (std::uint32_t i = 0; i < assets.definition->asset_ids.size(); ++i) {
      const AssetId asset = assets.definition->asset_ids[i];
      if (assets.drawables != nullptr) {
        const auto drawables = assets.drawables->find_by_asset(frame.mission_id, asset);
        if (drawables.empty()) return false;
        for (std::uint32_t j = 0; j < drawables.size(); ++j) {
          if (assets.geometries != nullptr) {
            const NativeGeometryMetadata* geometry = assets.geometries->find(drawables[j]->buffer_id);
            const DecodedGeometry* decoded = assets.geometries->decoded(drawables[j]->buffer_id);
            const MissionDrawableTransform* transform =
                assets.transforms == nullptr ? nullptr :
                    assets.transforms->find(frame.mission_id, drawables[j]->stable_id);
            const MissionMaterial* material =
                assets.materials == nullptr ? nullptr :
                    assets.materials->find(frame.mission_id, drawables[j]->stable_id);
            const MissionTextureBinding* texture =
                assets.textures == nullptr ? nullptr :
                    assets.textures->find(frame.mission_id, drawables[j]->stable_id);
            const ShaderPermutation* shader =
                material == nullptr || assets.shaders == nullptr ? nullptr :
                    assets.shaders->find(material->shader_permutation);
            const MissionRenderPass* pass =
                assets.render_passes == nullptr ? nullptr :
                    assets.render_passes->find(frame.mission_id, "world");
            const MissionRenderResolve* resolve =
                assets.render_resolves == nullptr ? nullptr :
                    assets.render_resolves->find(frame.mission_id, "world");
            if (geometry == nullptr || decoded == nullptr || transform == nullptr ||
                material == nullptr || texture == nullptr || shader == nullptr ||
                pass == nullptr || resolve == nullptr) return false;
            const MissionRenderTargetDefinition* render_target =
                assets.render_targets == nullptr ? nullptr :
                    assets.render_targets->find(frame.mission_id, pass->color_target);
            const MissionRenderTargetDefinition* destination_target =
                assets.render_targets == nullptr ? nullptr :
                    assets.render_targets->find(frame.mission_id, resolve->destination_target);
            if (render_target == nullptr || destination_target == nullptr ||
                !target->draw_world_geometry(frame, *drawables[j], *geometry, *decoded,
                                             *transform, *material, *texture, *shader, *render_target,
                                             *destination_target, *pass, *resolve, assets.camera,
                                             assets.textures, i * 4096u + j)) return false;
          } else if (!target->draw_world_asset(frame, *drawables[j], i * 4096u + j)) {
            return false;
          }
        }
      } else if (!target->mark_world_asset(frame, asset, i)) {
        return false;
      }
    }
  }
  ++submitted_frames_;
  last_world_asset_count_ = static_cast<std::uint32_t>(assets.definition->asset_ids.size());
  world_asset_submissions_ += last_world_asset_count_;
  return true;
}

}  // namespace ac6
