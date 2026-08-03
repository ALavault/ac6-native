#include "ac6_backend_capture_bridge.h"

#include <cassert>

int main() {
  ac6::d3d::MaterialDrawIdentityLatch identity_latch;
  const ac6::d3d::MaterialDrawIdentity identity{
      true, 0x83001000u, 0x83002000u, 0x12345678u, 0u, 0x83003000u};
  identity_latch.Publish(identity);
  ac6::d3d::MaterialDrawIdentity consumed_identity;
  assert(!identity_latch.ConsumeForDevice(0x83003004u, &consumed_identity));
  assert(identity_latch.ConsumeForDevice(0x83003000u, &consumed_identity));
  assert(consumed_identity.valid);
  assert(consumed_identity.material_key == 0x12345678u);
  assert(consumed_identity.draw_context_key == 0u);
  assert(!identity_latch.ConsumeForDevice(0x83003000u, &consumed_identity));
  identity_latch.Publish(identity);
  identity_latch.Clear();
  assert(!identity_latch.ConsumeForDevice(0x83003000u, &consumed_identity));

  ac6::d3d::FrameCaptureSnapshot capture;
  ac6::d3d::DrawCallRecord earlier_draw;
  earlier_draw.shadow_state.vertex_declaration = 0x01010000u;
  earlier_draw.shadow_state.index_buffer = 0;
  earlier_draw.shadow_state.streams[0].buffer = 0x02020000u;
  capture.draws.push_back(earlier_draw);

  ac6::d3d::DrawCallRecord draw;
  draw.shadow_state.render_targets[0] = 0x11110000u;
  draw.shadow_state.depth_stencil = 0x22220000u;
  draw.shadow_state.vertex_declaration = 0x33330000u;
  draw.shadow_state.index_buffer = 0x44440000u;
  draw.shadow_state.guest_vertex_shader = 0x45450000u;
  draw.shadow_state.guest_pixel_shader = 0x46460000u;
  draw.shadow_state.pixel_shader_program = {0x82011000u, 0x80u};
  draw.shadow_state.vertex_shader_program_candidates[0] = {0x82022000u, 0x100u};
  draw.shadow_state.vertex_shader_program_candidates[1] = {0x82033000u, 0x140u};
  draw.shadow_state.streams[0].buffer = 0x55550000u;
  draw.shadow_state.streams[0].offset = 0x40u;
  draw.shadow_state.streams[0].stride = 0x34u;
  draw.shadow_state.streams[1].buffer = 0x56560000u;
  draw.shadow_state.streams[1].offset = 0;
  draw.shadow_state.streams[1].stride = 0x14u;
  draw.shadow_state.vertex_fetch_layout_signature = 0x66660000u;
  draw.shadow_state.resource_binding_signature = 0x77770000u;
  capture.draws.push_back(draw);

  ac6::d3d::FrameCaptureSummary summary;
  summary.draw_count = 2;

  ac6::d3d::ShadowState fallback;
  fallback.render_targets[0] = 0xAAAA0000u;
  fallback.vertex_declaration = 0xBBBB0000u;
  fallback.index_buffer = 0;
  fallback.guest_vertex_shader = 0xCCCC0000u;
  fallback.guest_pixel_shader = 0xDDDD0000u;
  fallback.pixel_shader_program = {0x82044000u, 0x60u};
  fallback.vertex_shader_program_candidates[0] = {0x82055000u, 0xC0u};

  const auto from_draw = ac6::backend::BuildRenderEventSignature(
      capture, summary, fallback, nullptr, 0, 0);
  assert(from_draw.render_target_0 == 0x11110000u);
  assert(from_draw.depth_stencil == 0x22220000u);
  assert(from_draw.vertex_declaration == 0x33330000u);
  assert(from_draw.index_buffer == 0x44440000u);
  assert(from_draw.guest_vertex_shader == 0x45450000u);
  assert(from_draw.guest_pixel_shader == 0x46460000u);
  assert(from_draw.pixel_shader_program.guest_address == 0x82011000u);
  assert(from_draw.pixel_shader_program.size_bytes == 0x80u);
  assert(from_draw.vertex_shader_program_candidates[0].guest_address == 0x82022000u);
  assert(from_draw.vertex_shader_program_candidates[1].size_bytes == 0x140u);
  assert(from_draw.stream_count == 2u);
  assert(ac6::d3d::RecoverStreamOffset(0x1000u, 0x0FC0u) == 0x40u);
  assert(ac6::d3d::RecoverStreamStride(0x0Du) == 0x34u);

  auto changed_shader_capture = capture;
  changed_shader_capture.draws.back().shadow_state.guest_pixel_shader ^= 4u;
  const auto changed_shader = ac6::backend::BuildRenderEventSignature(
      changed_shader_capture, summary, fallback, nullptr, 0, 0);
  assert(changed_shader.stable_id != from_draw.stable_id);

  auto changed_program_capture = capture;
  changed_program_capture.draws.back().shadow_state
      .vertex_shader_program_candidates[1].guest_address ^= 4u;
  const auto changed_program = ac6::backend::BuildRenderEventSignature(
      changed_program_capture, summary, fallback, nullptr, 0, 0);
  assert(changed_program.stable_id != from_draw.stable_id);

  capture.draws.clear();
  const auto from_fallback = ac6::backend::BuildRenderEventSignature(
      capture, summary, fallback, nullptr, 0, 0);
  assert(from_fallback.render_target_0 == 0xAAAA0000u);
  assert(from_fallback.vertex_declaration == 0xBBBB0000u);
  assert(from_fallback.index_buffer == 0u);
  assert(from_fallback.guest_vertex_shader == 0xCCCC0000u);
  assert(from_fallback.guest_pixel_shader == 0xDDDD0000u);
  assert(from_fallback.pixel_shader_program.guest_address == 0x82044000u);
  assert(from_fallback.vertex_shader_program_candidates[0].size_bytes == 0xC0u);
  assert(from_fallback.stream_count == 0u);
}
