#pragma once

#include <iostream>

template <typename Renderer>
void print_renderer_frontier(const Renderer &renderer) {
  const auto stats = renderer.stats();
  std::cout << "renderer shader_loads=" << stats.shader_loads
            << " draws=" << stats.draws << " presents=" << stats.presents
            << " validated_modules=" << stats.translated_modules
            << " vulkan_modules=" << stats.vulkan_modules
            << " descriptor_set_layouts="
            << stats.vulkan_descriptor_set_layouts
            << " pipeline_layouts=" << stats.vulkan_pipeline_layouts
            << " graphics_pipelines=" << stats.vulkan_graphics_pipelines
            << " shared_memory_descriptors="
            << stats.vulkan_shared_memory_descriptors
            << " constant_buffer_descriptors="
            << stats.vulkan_constant_buffer_descriptors
            << " normal_draws=" << stats.vulkan_normal_draws
            << " neutral_resolves=" << stats.vulkan_neutral_resolves
            << " normal_readback_sha256=" << stats.normal_readback_sha256
            << " neutral_resolve_sha256=" << stats.neutral_resolve_sha256
            << " guest_writeback=" << (stats.guest_writeback ? 1 : 0)
            << " guest_linear_sha256=" << stats.guest_linear_sha256 << '\n';
}
