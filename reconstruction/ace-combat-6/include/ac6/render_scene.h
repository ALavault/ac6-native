#pragma once

#include "ac6/product_runtime.h"
#include "ac6/vulkan_backend.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ac6 {

// These are owned, renderer-facing values.  They deliberately contain no
// guest pointers, views into a retail container, or generated-code types.
// WorldFrame remains the compatibility input while the native snapshot and
// scene contracts are qualified.
struct RenderCamera final {
  std::array<float, 3> position{};
  std::array<float, 3> target{};
  std::array<float, 3> up{0.0F, 1.0F, 0.0F};
  float vertical_fov_radians{1.0471975512F};
  float near_plane{0.1F};
  float far_plane{4096.0F};

  [[nodiscard]] bool valid() const noexcept;
  friend bool operator==(const RenderCamera&, const RenderCamera&) = default;
};

struct SimulationSnapshot final {
  std::uint64_t tick{};
  std::uint32_t mission_id{};
  bool mission_ready{};
  EntityId player_entity{};
  std::array<float, 3> player_position{};
  std::array<float, 3> player_attitude{};
  float player_speed{};
  std::uint32_t active_units{};
  RenderCamera camera{};
  ScenarioState mission_state{ScenarioState::Loading};
  std::uint32_t sub_mission{};
  std::uint32_t step{};
  bool script_ended{};
  std::vector<ObjectiveRecord> objectives;
  Sha256Digest digest{};

  [[nodiscard]] bool valid() const;
  [[nodiscard]] bool digest_matches() const;
  void refresh_digest();
};

// The adapter is intentionally explicit about the mission/script boundary.
// It is the only supported way for the compatibility WorldFrame to enter the
// renderer-facing snapshot until the native session owns all producers.
[[nodiscard]] SimulationSnapshot make_simulation_snapshot(
    const WorldFrame& frame, ScenarioState mission_state, std::uint32_t sub_mission,
    std::uint32_t step, bool script_ended,
    std::span<const ObjectiveRecord> objectives = {});

enum class RenderPrimitiveTopology : std::uint8_t {
  TriangleList,
  TriangleStripRestart,
  LineList,
  PointList,
};

struct RenderDepthState final {
  bool test{true};
  bool write{true};
  bool reverse_z{};
  friend bool operator==(const RenderDepthState&, const RenderDepthState&) = default;
};

struct RenderBlendState final {
  bool enabled{};
  std::uint32_t source_factor{1};
  std::uint32_t destination_factor{};
  friend bool operator==(const RenderBlendState&, const RenderBlendState&) = default;
};

struct RenderRasterState final {
  bool cull_back_faces{true};
  bool front_counter_clockwise{true};
  bool wireframe{};
  friend bool operator==(const RenderRasterState&, const RenderRasterState&) = default;
};

struct RenderBinding final {
  std::uint32_t slot{};
  std::string resource_id;
  [[nodiscard]] bool valid() const noexcept { return !resource_id.empty(); }
  friend bool operator==(const RenderBinding&, const RenderBinding&) = default;
};

struct DrawPacket final {
  // Stable cache IDs.  These are not guest addresses and remain valid after
  // the sealed retail cache has been unmapped.
  std::string mesh_id;
  std::string material_id;
  std::vector<std::string> texture_ids;
  std::array<float, 16> transform{
      1.0F, 0.0F, 0.0F, 0.0F,
      0.0F, 1.0F, 0.0F, 0.0F,
      0.0F, 0.0F, 1.0F, 0.0F,
      0.0F, 0.0F, 0.0F, 1.0F};
  std::uint32_t first_index{};
  std::uint32_t index_count{};
  std::int32_t vertex_offset{};
  RenderPrimitiveTopology topology{RenderPrimitiveTopology::TriangleList};
  RenderDepthState depth{};
  RenderBlendState blend{};
  RenderRasterState raster{};
  std::uint32_t sort_key{};

  [[nodiscard]] bool valid() const;
};

struct MaterialPipeline final {
  std::string stable_id;
  std::uint64_t vertex_shader_hash{};
  std::uint64_t fragment_shader_hash{};
  std::vector<RenderBinding> texture_bindings;
  std::vector<RenderBinding> sampler_bindings;
  std::uint32_t constant_offset{};
  std::uint32_t constant_count{};
  float sampler_anisotropy{1.0F};

  [[nodiscard]] bool valid() const;
};

struct RenderPass final {
  std::string stable_id;
  std::uint32_t order{};
  std::array<float, 4> clear_color{};
  float clear_depth{1.0F};
  bool clear_color_enabled{};
  bool clear_depth_enabled{true};

  [[nodiscard]] bool valid() const noexcept { return !stable_id.empty(); }
};

struct HudPacket final {
  std::string stable_id;
  std::array<float, 4> rect{};  // x, y, width, height in target pixels
  std::uint32_t color{0xFFFFFFFFU};
  bool visible{true};

  [[nodiscard]] bool valid() const noexcept { return !stable_id.empty(); }
};

struct RenderSurfaceRequirements final {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t sample_count{1};
  std::string color_format{"rgba8_unorm"};
  std::string depth_format{"d32_sfloat"};
  float sampler_anisotropy{1.0F};

  enum class PresentMode : std::uint8_t { Headless, Fifo, Mailbox };
  PresentMode present_mode{PresentMode::Headless};

  [[nodiscard]] bool valid() const noexcept {
    return width != 0 && height != 0 && sample_count != 0 &&
           !color_format.empty() && !depth_format.empty() && sampler_anisotropy >= 1.0F;
  }
};

struct RenderScene final {
  std::uint64_t tick{};
  std::uint32_t mission_id{};
  RenderCamera camera{};
  RenderSurfaceRequirements surface{};
  std::vector<RenderPass> passes;
  std::vector<MaterialPipeline> materials;
  std::vector<DrawPacket> draw_packets;
  std::vector<HudPacket> hud;
  Sha256Digest digest{};

  [[nodiscard]] bool valid() const;
  [[nodiscard]] bool digest_matches() const;
  void refresh_digest();
};

[[nodiscard]] Sha256Digest simulation_snapshot_digest(const SimulationSnapshot& snapshot);
[[nodiscard]] Sha256Digest render_scene_digest(const RenderScene& scene);

// The Vulkan transport exposes the actual device limits.  This gate is kept
// separate from RenderScene::valid so an offline/headless scene can be built
// and inspected without opening a device.
[[nodiscard]] bool render_scene_supported(const RenderScene& scene,
                                          const RenderDeviceCaps& caps) noexcept;

}  // namespace ac6
