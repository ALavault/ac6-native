#include "ac6demo/vulkan_neutral_resolve.hpp"

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "vulkan_reached_resolve_core.inc"

#include "ac6demo/reached_copy_runtime_certificate.hpp"

#include <cstdio>
#include <cstdlib>

namespace ac6demo {

namespace {

VulkanNeutralResolveResult execute_certified_reached_resolve(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const VulkanNormalDrawResult &source,
    const XenosDrawCommand &copy, const XenosPresentCommand &present,
    ResolveSource resolve_source) {
  auto result = execute_vulkan_reached_resolve(
      physical, device, queue, queue_family, source, copy, present,
      resolve_source);

  const auto certificate = resolve_source == ResolveSource::Title1x
      ? certify_reached_title_copy_runtime(source.resolved_rgba8,
                                           result.tiled_bytes)
      : certify_reached_copy_runtime(source.resolved_rgba8,
                                     result.tiled_bytes);
  if (std::getenv("AC6_DEMO_WATCH_COPY_DIFFERENTIAL") != nullptr) {
    const std::string trace = certificate.trace_line();
    std::fprintf(stderr, "%s\n", trace.c_str());
  }
  require_reached_copy_runtime_writeback(certificate);
  return result;
}

} // namespace

VulkanNeutralResolveResult execute_vulkan_neutral_resolve(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const VulkanNormalDrawResult &normal,
    const XenosDrawCommand &copy, const XenosPresentCommand &present) {
  return execute_certified_reached_resolve(
      physical, device, queue, queue_family, normal, copy, present,
      ResolveSource::Normal4x);
}

VulkanNeutralResolveResult execute_vulkan_title_resolve(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const VulkanNormalDrawResult &title,
    const XenosDrawCommand &copy, const XenosPresentCommand &present) {
  return execute_certified_reached_resolve(
      physical, device, queue, queue_family, title, copy, present,
      ResolveSource::Title1x);
}

} // namespace ac6demo

#endif
