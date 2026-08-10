#pragma once

#include "ac6/retail_mission01_map_render_assets.h"
#include "ac6/retail_mode2_camera.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

namespace ac6::retail {

// Explicit choices at boundaries for which no retail sampler or alpha-state
// derivation exists yet. They are carried into the frame report and therefore
// cannot silently become JV evidence.
enum class Mission01CpuSamplerAddress : std::uint8_t {
  Clamp,
  Repeat,
};

struct Mission01CpuCameraPose final {
  std::array<float, 3> eye{};
  std::array<float, 3> target{};
  std::array<float, 3> up{0.0F, 1.0F, 0.0F};
};

struct Mission01CpuFrameRequest final {
  std::uint32_t width{};
  std::uint32_t height{};
  CampaignLoadout loadout{};
  std::uint32_t view_mode{};
  bool alternate_fov{};
  // When present, mode 2 uses the qualified retail base-locator transform.
  // The external pose is ignored.
  std::optional<RetailMode2CameraState> mode2_camera_state;
  // When present with mode2_camera_state, 0x8225D9F0 advances the supplied
  // shake state and applies its result before the locator transform. The next
  // state is returned in the frame report for deterministic 60 Hz carry-over.
  std::optional<RetailMode2DynamicInput> mode2_dynamic_input;
  Mission01CpuCameraPose pose{};
  bool texture_swap_16{};
  Mission01CpuSamplerAddress sampler_address{Mission01CpuSamplerAddress::Clamp};
  // Renderer-owned colours use 0xAARRGGBB. Both are explicit approximations:
  // the accepted sky and water material consumers remain open.
  std::uint32_t clear_color{0xFF000000u};
  std::uint32_t water_color{0xFF2C4A74u};
};

struct Mission01CpuFrameReport final {
  Sha256Digest content_index_sha256{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t camera_group{};
  std::uint32_t view_mode{};
  bool alternate_fov{};
  float fov_radians{};
  float near_plane{};
  float far_plane{};
  Mission01CpuCameraPose camera_pose{};
  bool uses_external_camera_pose{};
  bool texture_swap_16{};
  Mission01CpuSamplerAddress sampler_address{Mission01CpuSamplerAddress::Clamp};
  std::uint32_t clear_color{};
  std::uint32_t water_color{};
  std::size_t terrain_instances_considered{};
  std::size_t terrain_instances_visible{};
  std::size_t terrain_instances_rasterized{};
  std::size_t terrain_candidate_triangles{};
  std::size_t terrain_rasterized_triangles{};
  std::size_t city_instances_considered{};
  std::size_t city_instances_visible{};
  std::size_t city_instances_rasterized{};
  std::size_t city_candidate_triangles{};
  std::size_t city_rasterized_triangles{};
  std::size_t terrain_fragment_writes{};
  std::size_t water_fragment_writes{};
  std::size_t city_fragment_writes{};
  std::size_t water_queries{};
  std::vector<std::uint8_t> decoded_atlas_pages;
  std::vector<std::uint32_t> decoded_map_texture_ids;
  std::uint32_t color_coverage{};
  std::uint32_t depth_coverage{};
  std::uint64_t color_hash{};
  std::uint64_t depth_hash{};
  std::uint64_t marker_writes{};
  bool store_backed{};
  bool uses_manifest_tsv{};
  bool terrain_geometry_retail{};
  bool terrain_uv_retail{};
  bool water_mask_retail{};
  bool city_geometry_retail{};
  bool city_binding_retail{};
  bool city_transform_retail{};
  bool camera_group_retail{};
  bool camera_fov_retail{};
  bool camera_mode_selection_retail{};
  bool camera_mode2_base_transform_retail{};
  bool camera_dynamic_offset_retail{};
  RetailMode2DynamicBranch camera_dynamic_branch{
      RetailMode2DynamicBranch::GuardedOut};
  std::uint8_t camera_random_draws_consumed{};
  std::optional<RetailMode2ShakeState> next_mode2_shake_state;
  bool camera_runtime_state_retail{};
  bool camera_pose_retail{};
  bool clip_pipeline_retail{};
  bool map_distance_policy_retail{};
  bool texture_byte_swap_retail{};
  bool mip_policy_retail{};
  bool sampler_state_retail{};
  bool alpha_state_retail{};
  bool water_material_retail{};
  bool sky_retail{};
  bool vegetation_retail{};
  bool active_units_retail{};

  bool marker_free() const noexcept {
    return marker_writes == 0 && !uses_manifest_tsv;
  }
  bool jv_eligible() const noexcept;
};

// One deterministic CPU reference frame. Pixels are 0xAARRGGBB and depth is
// positive camera-space distance; infinity denotes untouched background.
class Mission01CpuFrame final {
public:
  const std::vector<std::uint32_t> &pixels() const noexcept { return pixels_; }
  const std::vector<float> &depth() const noexcept { return depth_; }
  const Mission01CpuFrameReport &report() const noexcept { return report_; }
  bool write_ppm(const std::filesystem::path &path) const noexcept;
  bool write_report_json(const std::filesystem::path &path) const noexcept;

private:
  friend class RetailMission01CpuCompositor;
  std::vector<std::uint32_t> pixels_;
  std::vector<float> depth_;
  Mission01CpuFrameReport report_;
};

// Store-backed CPU composition lane for the map domains that are already
// closed. It consumes no extracted filename and has no marker API. Geometry,
// bindings and textures remain persistent across frames; texture surfaces are
// decoded lazily once per byte-swap choice.
//
// This class deliberately does not claim JV: opening camera mode, live camera
// state, complete pose, sampler/alpha state, water material, sky, vegetation
// and active-unit geometry are reported false until their retail consumers are
// joined.
class RetailMission01CpuCompositor final {
public:
  RetailMission01CpuCompositor(const RetailMission01CpuCompositor &) = delete;
  RetailMission01CpuCompositor &
  operator=(const RetailMission01CpuCompositor &) = delete;
  RetailMission01CpuCompositor(RetailMission01CpuCompositor &&) noexcept =
      default;
  RetailMission01CpuCompositor &
  operator=(RetailMission01CpuCompositor &&) noexcept = default;

  static std::optional<RetailMission01CpuCompositor>
  open(const RetailContentStore &store);
  static std::optional<RetailMission01CpuCompositor>
  assemble(RetailMission01MapRenderAssets assets,
           RetailCameraTable camera_table,
           const Sha256Digest &camera_content_index_sha256);

  std::optional<Mission01CpuFrame>
  render(const Mission01CpuFrameRequest &request);
  const RetailMission01MapRenderAssets &assets() const noexcept {
    return assets_;
  }

private:
  RetailMission01CpuCompositor(RetailMission01MapRenderAssets assets,
                               RetailCameraTable camera_table);

  const DecodedTexture *map_texture(std::uint32_t identifier, bool swap_16);
  const DecodedTexture *atlas_texture(std::uint8_t page, bool swap_16);

  RetailMission01MapRenderAssets assets_;
  RetailCameraTable camera_table_;
  std::map<std::uint64_t, DecodedTexture> map_textures_;
  std::map<std::uint64_t, DecodedTexture> atlas_textures_;
};

} // namespace ac6::retail
