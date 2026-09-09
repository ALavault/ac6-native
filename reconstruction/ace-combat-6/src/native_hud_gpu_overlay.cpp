// Diagnostic-only. The panel/reticle geometry below is hand-picked pixel
// coordinates, not derived from a retail draw call; see
// reports/ac6-native-visual-shader-hud-20260829.md for what it validates
// and, explicitly, what it does not. It must not be used as a source of
// HUD pixel data in a gated delivery path (CLAUDE.md, HUD rule).

#include "ac6/native_hud_gpu_overlay.h"

#include "vulkan_retail_shaders.h"

#include <array>
#include <cstddef>

namespace ac6 {

NativeHudGpuOverlay::~NativeHudGpuOverlay() { reset(); }

bool NativeHudGpuOverlay::initialize(
    VulkanBackend& backend, const VulkanRenderTargetHandle target,
    const std::uint32_t width, const std::uint32_t height) noexcept {
  reset();
  backend_ = &backend;
  target_ = target;
  pipeline_ = backend_->create_clip_textured_pipeline(
      target_, retail_cli::detail::kRetailClipVertexSpirv,
      retail_cli::detail::kRetailTexturedFragmentSpirv,
      {false, false, true});
  if (!pipeline_) {
    reset();
    return false;
  }
  constexpr std::array<std::array<std::uint8_t, 4>, 4> colors{{
      {{24U, 255U, 80U, 220U}},
      {{255U, 190U, 32U, 220U}},
      {{32U, 180U, 255U, 210U}},
      {{255U, 64U, 64U, 230U}},
  }};
  for (const auto& color : colors) {
    const VulkanTextureHandle texture =
        backend_->create_texture_rgba8(1U, 1U, color);
    if (!texture) {
      reset();
      return false;
    }
    textures_.push_back(texture);
  }

  std::vector<std::vector<VulkanClipTexturedVertex>> vertices(4U);
  std::vector<std::vector<std::uint16_t>> indices(4U);
  const auto add_rect = [&](const std::size_t group, const float x0,
                            const float y0, const float x1,
                            const float y1) noexcept {
    if (group >= vertices.size() || vertices[group].size() > 65531U) {
      return false;
    }
    const auto base = static_cast<std::uint16_t>(vertices[group].size());
    vertices[group].push_back({x0, y0, 0.0F, 1.0F, 0.0F, 0.0F});
    vertices[group].push_back({x1, y0, 0.0F, 1.0F, 1.0F, 0.0F});
    vertices[group].push_back({x1, y1, 0.0F, 1.0F, 1.0F, 1.0F});
    vertices[group].push_back({x0, y1, 0.0F, 1.0F, 0.0F, 1.0F});
    indices[group].insert(indices[group].end(),
                          {base, static_cast<std::uint16_t>(base + 1U),
                           static_cast<std::uint16_t>(base + 2U), base,
                           static_cast<std::uint16_t>(base + 2U),
                           static_cast<std::uint16_t>(base + 3U)});
    return true;
  };
  const auto clip_x = [width](const float pixel) noexcept {
    return pixel / static_cast<float>(width) * 2.0F - 1.0F;
  };
  const auto clip_y = [height](const float pixel) noexcept {
    return 1.0F - pixel / static_cast<float>(height) * 2.0F;
  };
  const float scale_x = static_cast<float>(width) / 1280.0F;
  const float scale_y = static_cast<float>(height) / 720.0F;
  const auto add_rect_px = [&](const std::size_t group, const float x0,
                               const float y0, const float x1,
                               const float y1) noexcept {
    return add_rect(group, clip_x(x0 * scale_x), clip_y(y0 * scale_y),
                    clip_x(x1 * scale_x), clip_y(y1 * scale_y));
  };
  const auto add_outline_px = [&](const std::size_t group, const float x0,
                                  const float y0, const float x1,
                                  const float y1,
                                  const float thickness) noexcept {
    return add_rect_px(group, x0, y0, x1, y0 + thickness) &&
           add_rect_px(group, x0, y1 - thickness, x1, y1) &&
           add_rect_px(group, x0, y0, x0 + thickness, y1) &&
           add_rect_px(group, x1 - thickness, y0, x1, y1);
  };
  if (width == 0U || height == 0U ||
      !add_rect_px(0U, 616.0F, 359.0F, 630.0F, 361.0F) ||
      !add_rect_px(0U, 650.0F, 359.0F, 664.0F, 361.0F) ||
      !add_rect_px(0U, 639.0F, 336.0F, 641.0F, 350.0F) ||
      !add_rect_px(0U, 639.0F, 370.0F, 641.0F, 384.0F) ||
      !add_outline_px(0U, 16.0F, 608.0F, 316.0F, 704.0F, 2.0F) ||
      !add_rect_px(0U, 26.0F, 686.0F, 206.0F, 692.0F) ||
      !add_rect_px(0U, 180.0F, 638.0F, 286.0F, 644.0F) ||
      !add_outline_px(1U, 964.0F, 608.0F, 1264.0F, 704.0F, 2.0F) ||
      !add_rect_px(1U, 974.0F, 686.0F, 1154.0F, 692.0F) ||
      !add_outline_px(2U, 16.0F, 16.0F, 316.0F, 112.0F, 2.0F) ||
      !add_outline_px(2U, 1112.0F, 16.0F, 1264.0F, 168.0F, 2.0F) ||
      !add_rect_px(2U, 1187.0F, 89.0F, 1193.0F, 95.0F) ||
      !add_outline_px(2U, 410.0F, 628.0F, 870.0F, 676.0F, 2.0F) ||
      !add_rect_px(3U, 1208.0F, 58.0F, 1222.0F, 72.0F)) {
    reset();
    return false;
  }
  for (std::size_t group = 0U; group < vertices.size(); ++group) {
    meshes_.push_back(
        backend_->create_clip_textured_mesh(vertices[group], indices[group]));
    if (!meshes_.back()) {
      reset();
      return false;
    }
  }
  return true;
}

bool NativeHudGpuOverlay::render(
    const SimulationSnapshot& snapshot,
    const bool target_marker_visible) noexcept {
  if (backend_ == nullptr || !pipeline_ ||
      (snapshot.mission_state != ScenarioState::Gameplay &&
       snapshot.mission_state != ScenarioState::Paused &&
       snapshot.mission_state != ScenarioState::Complete &&
       snapshot.mission_state != ScenarioState::Aborted)) {
    return true;
  }
  for (std::size_t group = 0U; group < meshes_.size(); ++group) {
    if (group == 3U &&
        (!target_marker_visible || snapshot.active_units == 0U)) {
      continue;
    }
    if (!backend_->draw_clip_textured_indexed(target_, pipeline_,
                                              meshes_[group],
                                              textures_[group])) {
      return false;
    }
  }
  return true;
}

void NativeHudGpuOverlay::reset() noexcept {
  if (backend_ != nullptr) {
    for (const VulkanClipTexturedMeshHandle mesh : meshes_) {
      backend_->release_clip_textured_mesh(mesh);
    }
    for (const VulkanTextureHandle texture : textures_) {
      backend_->release_texture(texture);
    }
    if (pipeline_) backend_->release_pipeline(pipeline_);
  }
  meshes_.clear();
  textures_.clear();
  pipeline_ = {};
  target_ = {};
  backend_ = nullptr;
}

}  // namespace ac6
