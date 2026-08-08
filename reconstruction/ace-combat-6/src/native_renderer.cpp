#include "ac6/native_renderer.h"

namespace ac6 {

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

bool VulkanRenderer::render(const WorldFrame& frame, RenderAssets assets,
                            NativeRenderTarget* target) noexcept {
  if (!frame.mission_ready || frame.active_units == 0 || frame.player_entity == 0 ||
      !assets.ready_for(frame)) return false;
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
