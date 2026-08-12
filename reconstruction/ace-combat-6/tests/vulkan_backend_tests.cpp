#include "ac6/vulkan_backend.h"
#include "fixtures/vulkan_triangle_spirv.h"
#include "fixtures/vulkan_textured_triangle_spirv.h"
#include "fixtures/vulkan_world_textured_spirv.h"
#include "fixtures/vulkan_clip_mesh_spirv.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {

[[nodiscard]] bool near_byte(const std::uint8_t value,
                             const std::uint8_t expected) {
  const int difference = static_cast<int>(value) - expected;
  return difference >= -1 && difference <= 1;
}

[[nodiscard]] bool clear_pixel_matches(
    const std::vector<std::uint8_t>& pixels, const std::size_t offset) {
  return offset + 3U < pixels.size() && near_byte(pixels[offset], 32U) &&
         near_byte(pixels[offset + 1U], 64U) &&
         near_byte(pixels[offset + 2U], 128U) &&
         pixels[offset + 3U] == 255U;
}

int fail(const char* condition) {
  std::cerr << "vulkan_backend_test=fail condition=" << condition << '\n';
  return 1;
}

}  // namespace

int main() {
  auto created = ac6::VulkanBackend::create();
  if (!created) {
    std::cout << "vulkan_backend_skipped="
              << ac6::vulkan_backend_error_name(created.error) << '\n';
    return 77;
  }
  ac6::VulkanBackend& backend = *created.backend;
  if (backend.caps().device_name.empty() ||
      backend.caps().max_image_dimension_2d < 16U) {
    return fail("device_caps");
  }

  const std::array<ac6::VulkanVertex, 3> vertices{{
      {-0.8F, -0.8F},
      {0.8F, -0.8F},
      {0.0F, 0.8F},
  }};
  const std::array<std::uint16_t, 3> indices{{0U, 1U, 2U}};
  const std::array<std::uint16_t, 3> invalid_indices{{0U, 1U, 3U}};
  if (backend.create_mesh(vertices, invalid_indices) ||
      backend.create_render_target(0U, 16U, false)) {
    return fail("invalid_resource_accepted");
  }
  const ac6::VulkanMeshHandle mesh = backend.create_mesh(vertices, indices);
  const bool use_depth = backend.caps().depth_d32;
  const ac6::VulkanRenderTargetHandle target =
      backend.create_render_target(16U, 16U, use_depth);
  if (!mesh || !target || backend.live_mesh_count() != 1U ||
      backend.live_render_target_count() != 1U) {
    return fail("resource_creation");
  }
  if (!backend.clear_render_target(target, {0.125F, 0.25F, 0.5F, 1.0F})) {
    return fail("clear_submission");
  }
  const auto clear_pixels = backend.readback_rgba8(target);
  if (clear_pixels.size() != 16U * 16U * 4U ||
      !clear_pixel_matches(clear_pixels, 0U) ||
      !clear_pixel_matches(clear_pixels, (8U * 16U + 8U) * 4U)) {
    return fail("clear_readback");
  }

  ac6::VulkanPipelineState state;
  state.depth_test = use_depth;
  state.depth_write = use_depth;
  if (backend.create_pipeline(target, {}, ac6_test::kTriangleFragmentSpirv,
                              state)) {
    return fail("invalid_spirv_accepted");
  }
  const ac6::VulkanPipelineHandle pipeline = backend.create_pipeline(
      target, ac6_test::kTriangleVertexSpirv,
      ac6_test::kTriangleFragmentSpirv, state);
  if (!pipeline || !backend.has_pipeline(pipeline) ||
      backend.live_pipeline_count() != 1U) {
    return fail("pipeline_creation");
  }
  if (!backend.draw_indexed(target, pipeline, mesh) ||
      !backend.draw_indexed(target, pipeline, mesh)) {
    return fail("indexed_draw");
  }
  const auto draw_pixels = backend.readback_rgba8(target);
  const std::size_t center = (8U * 16U + 8U) * 4U;
  if (draw_pixels.size() != 16U * 16U * 4U ||
      draw_pixels[center] != 0U || draw_pixels[center + 1U] != 255U ||
      draw_pixels[center + 2U] != 0U ||
      draw_pixels[center + 3U] != 255U ||
      !clear_pixel_matches(draw_pixels, 0U)) {
    return fail("draw_readback");
  }

  const std::array<ac6::VulkanTexturedVertex, 3> textured_vertices{{
      {-0.8F, -0.8F, 0.0F, 0.0F},
      {0.8F, -0.8F, 1.0F, 0.0F},
      {0.0F, 0.8F, 0.5F, 1.0F},
  }};
  const ac6::VulkanTexturedMeshHandle textured_mesh =
      backend.create_textured_mesh(textured_vertices, indices);
  const std::array<std::uint8_t, 4U * 4U * 4U> texture_pixels = [] {
    std::array<std::uint8_t, 4U * 4U * 4U> pixels{};
    for (std::size_t index = 0U; index < pixels.size(); index += 4U) {
      pixels[index] = 255U;
      pixels[index + 3U] = 255U;
    }
    return pixels;
  }();
  const ac6::VulkanTextureHandle texture =
      backend.create_texture_rgba8(4U, 4U, texture_pixels);
  const ac6::VulkanPipelineHandle textured_pipeline = backend.create_textured_pipeline(
      target, ac6_test::kTexturedTriangleVertexSpirv,
      ac6_test::kTexturedTriangleFragmentSpirv, {});
  if (!textured_mesh || !texture || !textured_pipeline ||
      backend.live_textured_mesh_count() != 1U ||
      backend.live_texture_count() != 1U ||
      !backend.has_textured_mesh(textured_mesh) || !backend.has_texture(texture) ||
      !backend.draw_textured_indexed(target, textured_pipeline, textured_mesh,
                                     texture)) {
    return fail("textured_draw");
  }
  if (backend.draw_indexed(target, textured_pipeline, mesh)) {
    return fail("textured_pipeline_position_draw");
  }
  const auto textured_pixels = backend.readback_rgba8(target);
  if (textured_pixels.size() != 16U * 16U * 4U ||
      textured_pixels[center] != 255U || textured_pixels[center + 1U] != 0U ||
      textured_pixels[center + 2U] != 0U || textured_pixels[center + 3U] != 255U) {
    return fail("textured_readback");
  }
  backend.release_pipeline(textured_pipeline);
  backend.release_texture(texture);
  backend.release_textured_mesh(textured_mesh);
  if (backend.live_texture_count() != 0U ||
      backend.live_textured_mesh_count() != 0U ||
      backend.has_texture(texture) || backend.has_textured_mesh(textured_mesh)) {
    return fail("textured_release");
  }

  const std::array<ac6::VulkanClipTexturedVertex, 3> clip_vertices{{
      {-0.8F, -0.8F, 0.0F, 1.0F, 0.0F, 0.0F},
      {0.8F, -0.8F, 0.0F, 1.0F, 1.0F, 0.0F},
      {0.0F, 0.8F, 0.0F, 1.0F, 0.5F, 1.0F},
  }};
  const ac6::VulkanClipTexturedMeshHandle clip_mesh =
      backend.create_clip_textured_mesh(clip_vertices, indices);
  const ac6::VulkanTextureHandle clip_texture =
      backend.create_texture_rgba8(4U, 4U, texture_pixels);
  const ac6::VulkanPipelineHandle clip_pipeline = backend.create_clip_textured_pipeline(
      target, ac6_test::kClipMeshVertexSpirv,
      ac6_test::kTexturedTriangleFragmentSpirv, {});
  if (!clip_mesh || !clip_texture || !clip_pipeline ||
      !backend.draw_clip_textured_indexed(target, clip_pipeline, clip_mesh,
                                          clip_texture)) {
    return fail("clip_textured_draw");
  }
  backend.release_pipeline(clip_pipeline);
  backend.release_texture(clip_texture);
  backend.release_clip_textured_mesh(clip_mesh);
  if (backend.live_clip_textured_mesh_count() != 0U) {
    return fail("clip_textured_release");
  }

  if (use_depth) {
    const std::array<ac6::VulkanWorldTexturedVertex, 3> world_vertices{{
        {-0.8F, -0.8F, 0.0F, 0.0F, 0.0F},
        {0.8F, -0.8F, 0.0F, 1.0F, 0.0F},
        {0.0F, 0.8F, 0.0F, 0.5F, 1.0F},
    }};
    const ac6::VulkanWorldTexturedMeshHandle world_mesh =
        backend.create_world_textured_mesh(world_vertices, indices);
    const ac6::VulkanTextureHandle world_texture =
        backend.create_texture_rgba8(4U, 4U, texture_pixels);
    const ac6::VulkanPipelineState world_state{true, true, false};
    const auto no_depth_target = backend.create_render_target(16U, 16U, false);
    if (!world_mesh || !world_texture || !no_depth_target ||
        backend.render_target_has_d32(no_depth_target) ||
        backend.create_world_textured_pipeline(
            no_depth_target, ac6_test::kWorldTexturedVertexSpirv,
            ac6_test::kTexturedTriangleFragmentSpirv, world_state)) {
      return fail("world_depth_target_refusal");
    }
    backend.release_render_target(no_depth_target);
    const ac6::VulkanPipelineHandle world_pipeline =
        backend.create_world_textured_pipeline(
            target, ac6_test::kWorldTexturedVertexSpirv,
            ac6_test::kTexturedTriangleFragmentSpirv, world_state);
    std::array<float, 16> world_transform{1.0F,  0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                          0.0F,  0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                          0.25F, 0.0F, 0.2F, 1.0F};
    std::array<float, 16> nonfinite_transform = world_transform;
    nonfinite_transform[5] = std::numeric_limits<float>::infinity();
    if (!world_pipeline || !backend.render_target_has_d32(target) ||
        backend.live_world_textured_mesh_count() != 1U ||
        !backend.clear_render_target(target, {0.0F, 0.0F, 0.0F, 1.0F}, 1.0F) ||
        backend.draw_world_textured_indexed(target, world_pipeline, world_mesh,
                                            world_texture,
                                            nonfinite_transform) ||
        !backend.draw_world_textured_indexed(target, world_pipeline, world_mesh,
                                             world_texture, world_transform)) {
      return fail("world_textured_draw");
    }
    backend.release_pipeline(world_pipeline);
    backend.release_texture(world_texture);
    backend.release_world_textured_mesh(world_mesh);
    if (backend.live_world_textured_mesh_count() != 0U ||
        backend.has_world_textured_mesh(world_mesh)) {
      return fail("world_textured_release");
    }
  }

  backend.release_pipeline(pipeline);
  if (backend.has_pipeline(pipeline) || backend.live_pipeline_count() != 0U) {
    return fail("pipeline_release");
  }
  const ac6::VulkanPipelineHandle target_owned_pipeline = backend.create_pipeline(
      target, ac6_test::kTriangleVertexSpirv,
      ac6_test::kTriangleFragmentSpirv, state);
  if (!target_owned_pipeline) return fail("second_pipeline_creation");
  backend.release_render_target(target);
  if (backend.has_render_target(target) ||
      backend.has_pipeline(target_owned_pipeline) ||
      backend.live_render_target_count() != 0U ||
      backend.live_pipeline_count() != 0U ||
      !backend.readback_rgba8(target).empty()) {
    return fail("target_release");
  }
  backend.release_mesh(mesh);
  if (backend.has_mesh(mesh) || backend.live_mesh_count() != 0U) {
    return fail("mesh_release");
  }

  std::cout << "vulkan_backend=pass device=" << backend.caps().device_name
            << " api=" << (backend.caps().api_version >> 22U) << '.'
            << ((backend.caps().api_version >> 12U) & 0x3ffU)
            << " depth_d32=" << (backend.caps().depth_d32 ? 1 : 0) << '\n';
  return 0;
}
