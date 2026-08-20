#include "ac6demo/vulkan_neutral_resolve.hpp"

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

// Preserve the previously qualified Vulkan implementation byte-for-byte under
// an internal symbol. The public wrapper below adds an independent CPU
// differential certificate before any result can reach guest writeback.
#define execute_vulkan_neutral_resolve \
  execute_vulkan_neutral_resolve_uncertified
#include "vulkan_neutral_resolve_original.cpp"
#undef execute_vulkan_neutral_resolve

#include "ac6demo/reached_copy_runtime_certificate.hpp"

#include <cstdio>
#include <cstdlib>

namespace ac6demo {

VulkanNeutralResolveResult execute_vulkan_neutral_resolve(
    VkPhysicalDevice physical, VkDevice device, VkQueue queue,
    std::uint32_t queue_family, const VulkanNormalDrawResult &normal,
    const XenosDrawCommand &copy, const XenosPresentCommand &present) {
  auto result = execute_vulkan_neutral_resolve_uncertified(
      physical, device, queue, queue_family, normal, copy, present);

  const auto certificate = certify_reached_copy_runtime(
      normal.resolved_rgba8, result.tiled_bytes);
  if (std::getenv("AC6_DEMO_WATCH_COPY_DIFFERENTIAL") != nullptr) {
    const std::string trace = certificate.trace_line();
    std::fprintf(stderr, "%s\n", trace.c_str());
  }
  require_reached_copy_runtime_writeback(certificate);
  return result;
}

} // namespace ac6demo

#endif
