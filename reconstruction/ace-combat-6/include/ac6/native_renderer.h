#pragma once

#include "ac6/product_runtime.h"

namespace ac6 {

class VulkanRenderer final {
 public:
  struct RenderAssets {
    const MissionAssetDatabase* database{};
    const MissionRenderDefinition* definition{};
    const MissionDrawableDatabase* drawables{};
    const QualifiedBufferDatabase* buffers{};
    const NativeGeometryDatabase* geometries{};
    const MissionTransformDatabase* transforms{};
    const MissionMaterialDatabase* materials{};
    const MissionTextureDatabase* textures{};
    const ShaderPermutationDatabase* shaders{};
    const MissionRenderTargetDatabase* render_targets{};
    const MissionRenderPassDatabase* render_passes{};
    const MissionRenderResolveDatabase* render_resolves{};
    const MissionCameraDefinition* camera{};
    bool has(AssetId id) const noexcept;
    bool ready_for(const WorldFrame& frame) const noexcept;
  };

  bool render(const WorldFrame& frame, RenderAssets assets, NativeRenderTarget* target = nullptr) noexcept;
  std::uint64_t submitted_frames() const noexcept { return submitted_frames_; }
  std::uint32_t last_world_asset_count() const noexcept { return last_world_asset_count_; }
  std::uint64_t world_asset_submissions() const noexcept { return world_asset_submissions_; }

 private:
  std::uint64_t submitted_frames_{};
  std::uint32_t last_world_asset_count_{};
  std::uint64_t world_asset_submissions_{};
};

}  // namespace ac6
