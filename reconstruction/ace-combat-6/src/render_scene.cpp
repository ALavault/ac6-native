#include "ac6/render_scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_set>

namespace ac6 {
namespace {

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

void append_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
  append_u32(bytes, static_cast<std::uint32_t>(value));
  append_u32(bytes, static_cast<std::uint32_t>(value >> 32U));
}

void append_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
  append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_float(std::vector<std::uint8_t>& bytes, float value) {
  static_assert(sizeof(float) == sizeof(std::uint32_t));
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  append_u32(bytes, bits);
}

void append_bool(std::vector<std::uint8_t>& bytes, bool value) {
  bytes.push_back(value ? 1U : 0U);
}

void append_string(std::vector<std::uint8_t>& bytes, const std::string& value) {
  append_u64(bytes, value.size());
  bytes.insert(bytes.end(), value.begin(), value.end());
}

void append_camera(std::vector<std::uint8_t>& bytes, const RenderCamera& camera) {
  for (const float value : camera.position) append_float(bytes, value);
  for (const float value : camera.target) append_float(bytes, value);
  for (const float value : camera.up) append_float(bytes, value);
  append_float(bytes, camera.vertical_fov_radians);
  append_float(bytes, camera.near_plane);
  append_float(bytes, camera.far_plane);
}

bool nonzero_digest(const Sha256Digest& digest) noexcept {
  return std::any_of(digest.begin(), digest.end(), [](const std::uint8_t byte) {
    return byte != 0U;
  });
}

template <typename T>
bool finite_array(const T& values) noexcept {
  for (const float value : values) {
    if (!std::isfinite(value)) return false;
  }
  return true;
}

void append_objective(std::vector<std::uint8_t>& bytes, const ObjectiveRecord& objective) {
  append_u32(bytes, objective.id);
  append_string(bytes, objective.stable_id);
  append_bool(bytes, objective.required);
  append_u32(bytes, static_cast<std::uint32_t>(objective.state));
  append_u32(bytes, static_cast<std::uint32_t>(objective.condition));
  append_u32(bytes, objective.target_entity);
}

void append_binding(std::vector<std::uint8_t>& bytes, const RenderBinding& binding) {
  append_u32(bytes, binding.slot);
  append_string(bytes, binding.resource_id);
}

void append_packet(std::vector<std::uint8_t>& bytes, const DrawPacket& packet) {
  append_string(bytes, packet.mesh_id);
  append_string(bytes, packet.material_id);
  append_u64(bytes, packet.texture_ids.size());
  for (const std::string& texture : packet.texture_ids) append_string(bytes, texture);
  for (const float value : packet.transform) append_float(bytes, value);
  append_u32(bytes, packet.first_index);
  append_u32(bytes, packet.index_count);
  append_i32(bytes, packet.vertex_offset);
  append_u32(bytes, static_cast<std::uint32_t>(packet.topology));
  append_bool(bytes, packet.depth.test);
  append_bool(bytes, packet.depth.write);
  append_bool(bytes, packet.depth.reverse_z);
  append_bool(bytes, packet.blend.enabled);
  append_u32(bytes, packet.blend.source_factor);
  append_u32(bytes, packet.blend.destination_factor);
  append_bool(bytes, packet.raster.cull_back_faces);
  append_bool(bytes, packet.raster.front_counter_clockwise);
  append_bool(bytes, packet.raster.wireframe);
  append_u32(bytes, packet.sort_key);
}

}  // namespace

bool RenderCamera::valid() const noexcept {
  return finite_array(position) && finite_array(target) && finite_array(up) &&
         std::isfinite(vertical_fov_radians) && vertical_fov_radians > 0.0F &&
         std::isfinite(near_plane) && near_plane > 0.0F && std::isfinite(far_plane) &&
         far_plane > near_plane;
}

SimulationSnapshot make_simulation_snapshot(
    const WorldFrame& frame, ScenarioState mission_state, std::uint32_t sub_mission,
    std::uint32_t step, bool script_ended, std::span<const ObjectiveRecord> objectives) {
  SimulationSnapshot snapshot;
  snapshot.tick = frame.tick;
  snapshot.mission_id = frame.mission_id;
  snapshot.mission_ready = frame.mission_ready;
  snapshot.player_entity = frame.player_entity;
  snapshot.player_position = {frame.position_x, frame.position_y, frame.position_z};
  snapshot.player_attitude = {frame.pitch, frame.roll, frame.yaw};
  snapshot.player_speed = frame.speed;
  snapshot.active_units = frame.active_units;
  snapshot.camera.position = {frame.camera_x, frame.camera_y, frame.camera_z};
  snapshot.camera.target = {frame.camera_target_x, frame.camera_target_y,
                            frame.camera_target_z};
  snapshot.mission_state = mission_state;
  snapshot.sub_mission = sub_mission;
  snapshot.step = step;
  snapshot.script_ended = script_ended;
  snapshot.objectives.assign(objectives.begin(), objectives.end());
  snapshot.refresh_digest();
  return snapshot;
}

bool SimulationSnapshot::valid() const {
  if (tick == 0 || mission_id == 0 || player_entity == 0 || !camera.valid() ||
      !finite_array(player_position) || !finite_array(player_attitude) ||
      !std::isfinite(player_speed) || !nonzero_digest(digest)) {
    return false;
  }
  for (const ObjectiveRecord& objective : objectives) {
    if (!objective.valid()) return false;
  }
  return digest_matches();
}

bool SimulationSnapshot::digest_matches() const {
  return digest == simulation_snapshot_digest(*this);
}

void SimulationSnapshot::refresh_digest() {
  digest = simulation_snapshot_digest(*this);
}

Sha256Digest simulation_snapshot_digest(const SimulationSnapshot& snapshot) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(256U + snapshot.objectives.size() * 32U);
  append_u64(bytes, snapshot.tick);
  append_u32(bytes, snapshot.mission_id);
  append_bool(bytes, snapshot.mission_ready);
  append_u32(bytes, snapshot.player_entity);
  for (const float value : snapshot.player_position) append_float(bytes, value);
  for (const float value : snapshot.player_attitude) append_float(bytes, value);
  append_float(bytes, snapshot.player_speed);
  append_u32(bytes, snapshot.active_units);
  append_camera(bytes, snapshot.camera);
  append_u32(bytes, static_cast<std::uint32_t>(snapshot.mission_state));
  append_u32(bytes, snapshot.sub_mission);
  append_u32(bytes, snapshot.step);
  append_bool(bytes, snapshot.script_ended);

  std::vector<const ObjectiveRecord*> objectives;
  objectives.reserve(snapshot.objectives.size());
  for (const ObjectiveRecord& objective : snapshot.objectives) objectives.push_back(&objective);
  std::stable_sort(objectives.begin(), objectives.end(), [](const auto* left, const auto* right) {
    return left->id < right->id;
  });
  append_u64(bytes, objectives.size());
  for (const ObjectiveRecord* objective : objectives) append_objective(bytes, *objective);
  return sha256_bytes(bytes);
}

bool DrawPacket::valid() const {
  if (mesh_id.empty() || material_id.empty() || index_count == 0 || !finite_array(transform)) {
    return false;
  }
  for (const std::string& texture : texture_ids) {
    if (texture.empty()) return false;
  }
  return true;
}

bool MaterialPipeline::valid() const {
  if (stable_id.empty() || vertex_shader_hash == 0 || fragment_shader_hash == 0 ||
      !std::isfinite(sampler_anisotropy) || sampler_anisotropy < 1.0F) {
    return false;
  }
  for (const RenderBinding& binding : texture_bindings) {
    if (!binding.valid()) return false;
  }
  for (const RenderBinding& binding : sampler_bindings) {
    if (!binding.valid()) return false;
  }
  return true;
}

bool RenderScene::valid() const {
  if (tick == 0 || mission_id == 0 || !camera.valid() || !surface.valid() ||
      passes.empty() || materials.empty() || !nonzero_digest(digest)) {
    return false;
  }
  std::unordered_set<std::string> pass_ids;
  std::uint32_t previous_order = 0;
  bool first_pass = true;
  for (const RenderPass& pass : passes) {
    if (!pass.valid() || !pass_ids.insert(pass.stable_id).second ||
        (!first_pass && pass.order <= previous_order)) {
      return false;
    }
    previous_order = pass.order;
    first_pass = false;
  }
  std::unordered_set<std::string> material_ids;
  for (const MaterialPipeline& material : materials) {
    if (!material.valid() || !material_ids.insert(material.stable_id).second) return false;
  }
  std::uint32_t previous_sort = 0;
  bool first_packet = true;
  for (const DrawPacket& packet : draw_packets) {
    if (!packet.valid() || material_ids.find(packet.material_id) == material_ids.end() ||
        (!first_packet && packet.sort_key < previous_sort)) {
      return false;
    }
    previous_sort = packet.sort_key;
    first_packet = false;
  }
  std::unordered_set<std::string> hud_ids;
  for (const HudPacket& packet : hud) {
    if (!packet.valid() || !hud_ids.insert(packet.stable_id).second ||
        !finite_array(packet.rect)) {
      return false;
    }
  }
  return digest_matches();
}

bool RenderScene::digest_matches() const {
  return digest == render_scene_digest(*this);
}

void RenderScene::refresh_digest() {
  digest = render_scene_digest(*this);
}

Sha256Digest render_scene_digest(const RenderScene& scene) {
  std::vector<std::uint8_t> bytes;
  bytes.reserve(512U + scene.draw_packets.size() * 128U);
  append_u64(bytes, scene.tick);
  append_u32(bytes, scene.mission_id);
  append_camera(bytes, scene.camera);
  append_u32(bytes, scene.surface.width);
  append_u32(bytes, scene.surface.height);
  append_u32(bytes, scene.surface.sample_count);
  append_string(bytes, scene.surface.color_format);
  append_string(bytes, scene.surface.depth_format);
  append_float(bytes, scene.surface.sampler_anisotropy);
  append_u32(bytes, static_cast<std::uint32_t>(scene.surface.present_mode));
  append_u64(bytes, scene.passes.size());
  for (const RenderPass& pass : scene.passes) {
    append_string(bytes, pass.stable_id);
    append_u32(bytes, pass.order);
    for (const float value : pass.clear_color) append_float(bytes, value);
    append_float(bytes, pass.clear_depth);
    append_bool(bytes, pass.clear_color_enabled);
    append_bool(bytes, pass.clear_depth_enabled);
  }
  append_u64(bytes, scene.materials.size());
  for (const MaterialPipeline& material : scene.materials) {
    append_string(bytes, material.stable_id);
    append_u64(bytes, material.vertex_shader_hash);
    append_u64(bytes, material.fragment_shader_hash);
    append_u64(bytes, material.texture_bindings.size());
    for (const RenderBinding& binding : material.texture_bindings) append_binding(bytes, binding);
    append_u64(bytes, material.sampler_bindings.size());
    for (const RenderBinding& binding : material.sampler_bindings) append_binding(bytes, binding);
    append_u32(bytes, material.constant_offset);
    append_u32(bytes, material.constant_count);
    append_float(bytes, material.sampler_anisotropy);
  }
  append_u64(bytes, scene.draw_packets.size());
  for (const DrawPacket& packet : scene.draw_packets) append_packet(bytes, packet);
  append_u64(bytes, scene.hud.size());
  for (const HudPacket& packet : scene.hud) {
    append_string(bytes, packet.stable_id);
    for (const float value : packet.rect) append_float(bytes, value);
    append_u32(bytes, packet.color);
    append_bool(bytes, packet.visible);
  }
  return sha256_bytes(bytes);
}

bool render_scene_supported(const RenderScene& scene, const RenderDeviceCaps& caps) noexcept {
  if (!scene.surface.valid() || scene.surface.width > caps.max_image_dimension_2d ||
      scene.surface.height > caps.max_image_dimension_2d ||
      scene.surface.sample_count > caps.max_color_sample_count) {
    return false;
  }
  if ((scene.surface.color_format == "rgba8_unorm" && !caps.color_rgba8_unorm) ||
      (scene.surface.color_format == "bgra8_unorm" && !caps.color_bgra8_unorm) ||
      (scene.surface.color_format != "rgba8_unorm" &&
       scene.surface.color_format != "bgra8_unorm")) {
    return false;
  }
  if (scene.surface.depth_format == "d32_sfloat" && !caps.depth_d32) return false;
  if (scene.surface.depth_format != "d32_sfloat") return false;
  switch (scene.surface.present_mode) {
    case RenderSurfaceRequirements::PresentMode::Headless:
      break;
    case RenderSurfaceRequirements::PresentMode::Fifo:
      if (!caps.presentation_fifo) return false;
      break;
    case RenderSurfaceRequirements::PresentMode::Mailbox:
      if (!caps.presentation_mailbox) return false;
      break;
  }
  if (!caps.sampler_anisotropy && scene.surface.sampler_anisotropy > 1.0F) return false;
  if (scene.surface.sampler_anisotropy > caps.max_sampler_anisotropy) return false;
  for (const MaterialPipeline& material : scene.materials) {
    if (!caps.sampler_anisotropy && material.sampler_anisotropy > 1.0F) return false;
    if (material.sampler_anisotropy > caps.max_sampler_anisotropy) return false;
  }
  return true;
}

}  // namespace ac6
