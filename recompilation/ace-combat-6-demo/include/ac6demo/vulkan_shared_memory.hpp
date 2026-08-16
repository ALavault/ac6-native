#pragma once

#ifdef AC6_DEMO_HAVE_VULKAN_RENDERER_FRONTIER

#include "ac6demo/session.hpp"
#include "ac6demo/xenos_commands.hpp"
#include "ac6demo/rexglue_runtime_shader.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <vulkan/vulkan.h>

namespace ac6demo {

class VulkanSharedMemory final {
public:
  VulkanSharedMemory() = default;
  VulkanSharedMemory(const VulkanSharedMemory &) = delete;
  VulkanSharedMemory &operator=(const VulkanSharedMemory &) = delete;

  void populate(VkPhysicalDevice physical, VkDevice device,
                VkDescriptorSetLayout layout, DemoSession &session,
                std::span<const XenosCommand> commands,
                std::uint32_t shader_loads, std::uint32_t draws,
                std::uint32_t presents, std::uint32_t translated_modules,
                std::uint32_t graphics_pipelines);
  void cleanup(VkDevice device) noexcept;
  void populate_constants(VkPhysicalDevice physical, VkDevice device,
                          VkDescriptorSetLayout layout,
                          const XenosDrawCommand &draw,
                          const ReachedShaderSpirv &vertex,
                          const ReachedShaderSpirv &pixel,
                          std::uint32_t viewport_x_max,
                          std::uint32_t viewport_y_max);
  [[nodiscard]] bool populated() const noexcept {
    return descriptor_set_ != VK_NULL_HANDLE;
  }
  [[nodiscard]] std::uint32_t constant_descriptor_count() const noexcept;
  [[nodiscard]] VkDescriptorSet shared_descriptor_set() const noexcept {
    return descriptor_set_;
  }
  [[nodiscard]] VkDescriptorSet
  constant_descriptor_set(std::string_view vertex_shader) const noexcept;

private:
  struct HostBuffer final {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{};
  };

  static std::uint32_t host_memory_type(VkPhysicalDevice physical,
                                        std::uint32_t allowed);
  static HostBuffer create_buffer(VkPhysicalDevice physical, VkDevice device,
                                  VkDeviceSize size, VkBufferUsageFlags usage);
  static void destroy_buffer(VkDevice device, HostBuffer &buffer) noexcept;
  static void write_buffer(VkDevice device, const HostBuffer &buffer,
                           VkDeviceSize offset,
                           std::span<const std::byte> bytes);

  std::array<HostBuffer, 4> buffers_{};
  VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
  struct ConstantSet final {
    std::string vertex_shader;
    std::array<HostBuffer, 5> buffers{};
    VkDescriptorPool pool{VK_NULL_HANDLE};
    VkDescriptorSet set{VK_NULL_HANDLE};
  };
  std::array<ConstantSet, 2> constants_{};
};

} // namespace ac6demo

#endif
