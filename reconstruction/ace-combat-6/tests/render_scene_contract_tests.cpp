#include "ac6/render_scene.h"

#include <iostream>

namespace {

int check(bool condition, const char* message) {
  if (!condition) std::cerr << "FAIL " << message << '\n';
  return condition ? 0 : 1;
}

}  // namespace

int main() {
  int failures = 0;

  ac6::WorldFrame frame;
  frame.tick = 1;
  frame.mission_id = 1;
  frame.mission_ready = true;
  frame.player_entity = 4097;
  frame.position_x = 10.0F;
  frame.position_y = 20.0F;
  frame.position_z = 30.0F;
  frame.camera_target_z = 1.0F;
  frame.active_units = 230;
  const ac6::ObjectiveRecord objective{1, "first", true, ac6::ObjectiveState::Active,
                                       ac6::ObjectiveCondition::Manual, 0};
  const ac6::SimulationSnapshot snapshot = ac6::make_simulation_snapshot(
      frame, ac6::ScenarioState::Gameplay, 1, 0, false, {&objective, 1});
  failures += check(snapshot.valid(), "simulation snapshot is sealed");
  failures += check(snapshot.digest_matches(), "simulation digest matches");

  ac6::RenderScene scene;
  scene.tick = snapshot.tick;
  scene.mission_id = snapshot.mission_id;
  scene.camera = snapshot.camera;
  scene.surface.width = 1280;
  scene.surface.height = 720;
  scene.passes.push_back({"world", 0, {0.0F, 0.0F, 0.0F, 1.0F}, 1.0F, true, true});
  scene.materials.push_back({"terrain", 0x11U, 0x22U, {{0, "terrain-albedo"}},
                             {{1, "linear-clamp"}}, 0, 16, 1.0F});
  ac6::DrawPacket packet;
  packet.mesh_id = "terrain-mesh";
  packet.material_id = "terrain";
  packet.texture_ids = {"terrain-albedo"};
  packet.index_count = 3;
  scene.draw_packets.push_back(packet);
  scene.hud.push_back({"reticle", {640.0F, 360.0F, 16.0F, 16.0F}, 0xFFFFFFFFU, true});
  scene.refresh_digest();
  failures += check(scene.valid(), "render scene is sealed");
  failures += check(scene.digest_matches(), "render scene digest matches");

  const auto before = scene.digest;
  scene.draw_packets[0].sort_key = 1;
  failures += check(ac6::render_scene_digest(scene) != before,
                     "scene digest changes with draw order metadata");
  scene.refresh_digest();
  failures += check(scene.valid(), "scene remains valid after reseal");

  ac6::RenderDeviceCaps caps;
  caps.max_image_dimension_2d = 4096;
  caps.max_color_sample_count = 1;
  caps.max_sampler_anisotropy = 16.0F;
  caps.depth_d32 = true;
  caps.sampler_anisotropy = true;
  caps.color_rgba8_unorm = true;
  failures += check(ac6::render_scene_supported(scene, caps), "scene fits device caps");
  caps.depth_d32 = false;
  failures += check(!ac6::render_scene_supported(scene, caps), "depth capability is explicit");
  return failures == 0 ? 0 : 1;
}
