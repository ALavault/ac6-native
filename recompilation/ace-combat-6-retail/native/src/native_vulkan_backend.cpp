#include "ac6/native_vulkan_backend.h"

#include "ac6/native_pinned_shaders.h"
#include "ac6/native_vulkan_device.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <unordered_map>

namespace ac6::native {

namespace {
bool valid_primitive(std::uint32_t primitive) noexcept {
  return (primitive >= 1u && primitive <= 8u) ||
         (primitive >= 0x0Cu && primitive <= 0x12u);
}
}  // namespace

bool VulkanBackend::submit(const XenosState& state,
                           std::span<const XenosCommand> commands) {
  error_.clear();
  std::uint64_t draws = 0u;
  std::uint64_t resolves = 0u;
  std::uint64_t presents = 0u;
  for (const XenosCommand& command : commands) {
    const bool accepted = std::visit(
        [&](const auto& packet) -> bool {
          using Packet = std::decay_t<decltype(packet)>;
          if constexpr (std::is_same_v<Packet, DrawPacket>) {
            if (packet.vertex_count == 0u || packet.index_count == 0u ||
                packet.vertex_stride == 0u || !valid_primitive(packet.primitive_type) ||
                packet.index_format > 1u) {
              error_ = "DRAW packet has no drawable vertices";
              return false;
            }
            if ((packet.texture_dimension == 1u && !capabilities_.texture_2d) ||
                (packet.texture_dimension == 2u && !capabilities_.texture_3d) ||
                (packet.texture_dimension == 3u && !capabilities_.texture_cube) ||
                packet.texture_dimension > 3u) {
              error_ = "Vulkan backend lacks the requested texture dimension";
              return false;
            }
            if (packet.uses_blend_scissor && !capabilities_.blend_scissor) {
              error_ = "Vulkan backend lacks blend/scissor capability";
              return false;
            }
            if (packet.uses_primitive_restart && !capabilities_.primitive_restart) {
              error_ = "Vulkan backend lacks primitive-restart capability";
              return false;
            }
            const std::uint64_t vertex_bytes =
                static_cast<std::uint64_t>(packet.vertex_count) *
                packet.vertex_stride;
            const std::uint64_t index_bytes =
                static_cast<std::uint64_t>(packet.index_count) *
                (packet.index_format == 0u ? 2u : 4u);
            if ((packet.vertex_address != 0u &&
                 (vertex_bytes == 0u ||
                  static_cast<std::uint64_t>(packet.vertex_address) +
                          vertex_bytes > (1ull << 32u))) ||
                (packet.index_address != 0u &&
                 (index_bytes == 0u ||
                  static_cast<std::uint64_t>(packet.index_address) +
                          index_bytes > (1ull << 32u)))) {
              error_ = "DRAW packet guest range overflows 32-bit address space";
              return false;
            }
            ++draws;
            return true;
          } else if constexpr (std::is_same_v<Packet, ResolvePacket>) {
            if (!capabilities_.resolves || !capabilities_.edram_aliasing) {
              error_ = "Vulkan backend lacks EDRAM resolve capability";
              return false;
            }
            const std::uint64_t source_bytes =
                static_cast<std::uint64_t>(packet.width) * packet.height *
                packet.sample_count * 4u;
            if (packet.source_edram >= state.edram_bytes() ||
                source_bytes == 0u ||
                static_cast<std::uint64_t>(packet.source_edram) + source_bytes >
                    state.edram_bytes() ||
                packet.destination_surface >= 16u ||
                packet.width == 0u || packet.width > 4096u ||
                packet.height == 0u || packet.height > 4096u ||
                (packet.sample_count != 1u && packet.sample_count != 2u &&
                 packet.sample_count != 4u && packet.sample_count != 8u) ||
                static_cast<std::uint32_t>(packet.color_endian) > 3u) {
              error_ = "RESOLVE packet references invalid EDRAM or dimensions";
              return false;
            }
            if (packet.sample_count > 1u && !capabilities_.msaa) {
              error_ = "Vulkan backend lacks MSAA capability";
              return false;
            }
            if (packet.depth && !capabilities_.depth_stencil) {
              error_ = "Vulkan backend lacks depth/stencil capability";
              return false;
            }
            ++resolves;
            return true;
          } else if constexpr (std::is_same_v<Packet, PresentPacket>) {
            if (packet.surface >= 16u || packet.width == 0u ||
                packet.height == 0u) {
              error_ = "PRESENT packet references invalid surface";
              return false;
            }
            ++presents;
            return true;
          } else if constexpr (std::is_same_v<Packet, WaitPacket>) {
            if (packet.selector > 7u) {
              error_ = "WAIT selector is not qualified";
              return false;
            }
            return true;
          } else if constexpr (std::is_same_v<Packet, IndirectBufferPacket>) {
            error_ = "INDIRECT_BUFFER requires a bound guest-memory resolver";
            return false;
          } else if constexpr (std::is_same_v<Packet, MicroEngineInitPacket>) {
            // The packet is a qualified Xenon bootstrap envelope. Its payload
            // is retained by the command stream but never run as host code.
            return true;
          } else if constexpr (std::is_same_v<Packet, EventWriteShdPacket>) {
            return (packet.initiator & ~0x8000003Fu) == 0u;
          } else if constexpr (std::is_same_v<Packet, ImmediateShaderPacket>) {
            return packet.shader_type <= 1u && packet.start == 0u &&
                   packet.dword_count != 0u;
          }
          return false;
        },
        command);
    if (!accepted) return false;
  }
  draw_count_ += draws;
  resolve_count_ += resolves;
  present_count_ += presents;
  return true;
}

bool VulkanBackend::present_to_offscreen(VulkanOffscreenTarget& target,
                                         const PresentPacket& packet,
                                         float red, float green, float blue,
                                         float alpha) {
  error_.clear();
  if (packet.surface >= 16u || packet.width == 0u || packet.height == 0u) {
    error_ = "PRESENT packet references invalid surface";
    return false;
  }
  if (!target.valid() || target.width() != packet.width ||
      target.height() != packet.height) {
    error_ = "PRESENT packet dimensions do not match the offscreen target";
    return false;
  }
  if (!target.clear(red, green, blue, alpha)) {
    error_ = "offscreen clear failed: " + target.error();
    return false;
  }
  ++present_count_;
  return true;
}

// ---------------------------------------------------------------------------
// PinnedShaderRuntime (r256): real-path execution of decoded streams with the
// pinned-registry shaders. Contract (see native_vulkan_backend.h): set 0
// binding 0 = XeSharedMemory SSBO of raw guest bytes; set 1 bindings 0..4 =
// XeSystemConstants, XeFloatConstants vertex, XeFloatConstants pixel,
// XeBoolLoopConstants, XeFetchConstants (raw guest register words). Draw
// state derives from XenosState registers the way the oracle's
// UpdateSystemConstants does for the fields the pinned shaders read.

namespace {

// Xenos register indices (qualified register_table.inc identity).
constexpr std::uint32_t kRegVgtMaxVtxIndx = 0x2100u;
constexpr std::uint32_t kRegVgtMinVtxIndx = 0x2101u;
constexpr std::uint32_t kRegVgtIndxOffset = 0x2102u;
constexpr std::uint32_t kRegRbColorControl = 0x2202u;
constexpr std::uint32_t kRegPaSuPointMinmax = 0x2281u;
constexpr std::uint32_t kRegPaSuPointSize = 0x2280u;
constexpr std::uint32_t kRegRbColorInfo = 0x2001u;
constexpr std::uint32_t kRegRbSurfaceInfo = 0x2000u;
constexpr std::uint32_t kRegRbDepthControl = 0x2200u;
constexpr std::uint32_t kRegRbColorMask = 0x2104u;
constexpr std::uint32_t kRegRbModeControl = 0x2208u;
constexpr std::uint32_t kRegPaScScreenScissorTL = 0x200Eu;
constexpr std::uint32_t kRegPaScScreenScissorBR = 0x200Fu;
constexpr std::uint32_t kRegPaScWindowOffset = 0x2080u;
constexpr std::uint32_t kRegPaClVportXScale = 0x210Fu;  // floats, X..Z pairs
constexpr std::uint32_t kRegPaClClipCntl = 0x2204u;
constexpr std::uint32_t kRegPaSuScModeCntl = 0x2205u;
constexpr std::uint32_t kRegPaSuVtxCntl = 0x2302u;
constexpr std::uint32_t kRegPaClVteCntl = 0x2206u;
constexpr std::uint32_t kRegShaderConstant000X = 0x4000u;   // vertex cN floats
constexpr std::uint32_t kRegShaderConstant256X = 0x4400u;  // pixel cN floats
constexpr std::uint32_t kRegShaderConstantFetch000 = 0x4800u;
constexpr std::uint32_t kFetchConstantDwords = 192u;  // 48 fetch x vec4
constexpr std::uint32_t kRegShaderConstantBool = 0x4900u;  // 4 dwords
constexpr std::uint32_t kRegShaderConstantLoop0 = 0x4908u;  // 32 dwords

// EDRAM geometry (qualified oracle constants: rex/graphics/xenos.h).
constexpr std::uint32_t kEdramTileWidthSamples = 80u;
constexpr std::uint32_t kEdramTileHeightSamples = 16u;
constexpr std::uint32_t kEdramTileCount = 2048u;
// Bounded EDRAM image dims this cycle (fail-closed outside).
constexpr std::uint32_t kMaxEdramImageWidth = 4096u;
constexpr std::uint32_t kMaxEdramImageHeight = 8192u;
constexpr std::uint32_t kMaxEdramRegionWidth = 4096u;
constexpr std::uint32_t kMaxEdramRegionHeight = 4096u;

// r459: sample_count is always 1, 2 or 4 by construction (derived from the
// two-bit RB_SURFACE_INFO MSAA field; 8x is refused before this is called).
VkSampleCountFlagBits vk_sample_count(std::uint32_t sample_count) noexcept {
  switch (sample_count) {
    case 2u:
      return VK_SAMPLE_COUNT_2_BIT;
    case 4u:
      return VK_SAMPLE_COUNT_4_BIT;
    default:
      return VK_SAMPLE_COUNT_1_BIT;
  }
}
// EdramMode::kColorDepth (rex/graphics/xenos.h).
constexpr std::uint32_t kEdramModeColorDepth = 4u;

// System-constant block offsets are the SPIR-V member offsets of the pinned
// shaders (offsetof() of the oracle's SystemConstants struct).
constexpr std::uint32_t kSysOffsetFlags = 0u;
constexpr std::uint32_t kSysOffsetVertexIndexLoadAddress = 4u;
constexpr std::uint32_t kSysOffsetVertexIndexEndian = 8u;
constexpr std::uint32_t kSysOffsetLineLoopClosingIndex = 12u;
constexpr std::uint32_t kSysOffsetVertexBaseIndex = 16u;
constexpr std::uint32_t kSysOffsetVertexIndexMin = 496u;
constexpr std::uint32_t kSysOffsetVertexIndexMax = 500u;
constexpr std::uint32_t kSysOffsetColorExpBias = 288u;
constexpr std::uint32_t kSysOffsetNdcScale = 124u;
constexpr std::uint32_t kSysOffsetNdcOffset = 140u;
constexpr std::uint32_t kSystemConstantsBytes = 512u;

constexpr std::uint32_t kFloatConstantsBytes = 512u * 16u;
constexpr std::uint32_t kBoolLoopBytes = 160u;  // 8 + 32 dwords
constexpr std::uint32_t kFetchBytes = kFetchConstantDwords * 4u;

// SysFlag bits (SpirvShaderTranslator::kSysFlag_*).
constexpr std::uint32_t kSysFlagVertexIndexLoad = 1u << 0u;
constexpr std::uint32_t kSysFlagComputeOrPrimitiveVertexIndexLoad = 1u << 1u;
constexpr std::uint32_t kSysFlagComputeOrPrimitiveVertexIndexLoad32Bit =
    1u << 2u;
constexpr std::uint32_t kSysFlagXyDividedByW = 1u << 8u;
constexpr std::uint32_t kSysFlagZDividedByW = 1u << 9u;
constexpr std::uint32_t kSysFlagWNotReciprocal = 1u << 10u;
constexpr std::uint32_t kSysFlagPrimitivePolygonal = 1u << 11u;
constexpr std::uint32_t kSysFlagPrimitiveLine = 1u << 12u;
constexpr std::uint32_t kAlphaPassIfLessShift = 16u;
constexpr std::uint32_t kCompareFunctionAlways = 7u;

[[nodiscard]] VkPrimitiveTopology topology_of(std::uint32_t primitive, bool& ok) noexcept {
  switch (primitive) {
    case 0x02u: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case 0x03u: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case 0x04u: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case 0x05u: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case 0x06u: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // Rectangle lists are expanded to two-triangle strips host-side
    // (r265; the oracle's kRectangleListAsTriangleStrip scheme).
    case 0x08u: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default:
      ok = false;
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}

[[nodiscard]] bool is_line_primitive(std::uint32_t primitive) noexcept {
  return primitive == 0x02u || primitive == 0x03u;
}

[[nodiscard]] bool is_polygonal_primitive(std::uint32_t primitive) noexcept {
  return primitive == 0x04u || primitive == 0x07u || primitive == 0x08u;
}

}  // namespace

PinnedShaderRuntime::PinnedShaderRuntime(const VulkanDevice& device) noexcept {
  error_.clear();
  device_ = &device;
  if (!device.valid()) {
    error_ = "pinned runtime: invalid Vulkan device";
    return;
  }
  VkDevice dev = device.device();

  // Shared memory SSBO: 512 MiB of raw guest bytes -- the full Xbox 360
  // physical RAM size, not a guess. r435 measured real retail fetch
  // constant addresses live (e.g. 0x1274027b, ~310 MiB) using guest
  // addresses directly as byte offsets into this buffer (the contract
  // draw_pinned/decode_pixel_texture already assume); the prior 64 MiB
  // window was sized for synthetic test addresses only and fails closed
  // on every real draw whose fetch address falls beyond it. Draws whose
  // fetches go beyond 512 MiB still fail closed.
  shared_memory_dwords_ = (512ull * 1024ull * 1024ull) / 4ull;
  VkBufferCreateInfo shared_info{};
  shared_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  shared_info.size = shared_memory_dwords_ * 4u;
  shared_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  if (vkCreateBuffer(dev, &shared_info, nullptr, &shared_memory_) != VK_SUCCESS) {
    error_ = "pinned runtime: shared memory buffer creation failed";
    return;
  }
  VkMemoryRequirements shared_reqs{};
  vkGetBufferMemoryRequirements(dev, shared_memory_, &shared_reqs);

  VkPhysicalDeviceMemoryProperties props{};
  vkGetPhysicalDeviceMemoryProperties(device.physical_device(), &props);
  auto find_host_coherent = [&props](std::uint32_t type_bits,
                                     std::uint32_t& index_out) {
    for (std::uint32_t type = 0u; type < props.memoryTypeCount; ++type) {
      const VkMemoryType& memory_type = props.memoryTypes[type];
      if ((memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u &&
          (memory_type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u &&
          (type_bits & (1u << type)) != 0u) {
        index_out = type;
        return true;
      }
    }
    return false;
  };
  std::uint32_t shared_memory_type = 0xFFFFFFFFu;
  if (!find_host_coherent(shared_reqs.memoryTypeBits, shared_memory_type)) {
    error_ = "pinned runtime: no host-coherent memory type for shared memory";
    return;
  }
  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = shared_reqs.size;
  alloc.memoryTypeIndex = shared_memory_type;
  if (vkAllocateMemory(dev, &alloc, nullptr, &shared_memory_device_) !=
          VK_SUCCESS ||
      vkBindBufferMemory(dev, shared_memory_, shared_memory_device_, 0) !=
          VK_SUCCESS) {
    error_ = "pinned runtime: shared memory allocation failed";
    return;
  }
  void* shared_mapping = nullptr;
  if (vkMapMemory(dev, shared_memory_device_, 0, shared_reqs.size, 0,
                  &shared_mapping) != VK_SUCCESS) {
    error_ = "pinned runtime: shared memory mapping failed";
    return;
  }
  shared_memory_mapped_ = static_cast<std::uint8_t*>(shared_mapping);
  std::memset(shared_memory_mapped_, 0, static_cast<std::size_t>(shared_reqs.size));

  // Five constant blocks in one host-visible allocation. Each block offset
  // is aligned to minUniformBufferOffsetAlignment (the uniform-buffer range
  // offset requirement; the spec guarantee is 256).
  VkPhysicalDeviceProperties device_properties{};
  vkGetPhysicalDeviceProperties(device.physical_device(), &device_properties);
  const VkDeviceSize range_alignment =
      device_properties.limits.minUniformBufferOffsetAlignment;
  const VkDeviceSize block_sizes[5] = {kSystemConstantsBytes,
                                       kFloatConstantsBytes,
                                       kFloatConstantsBytes, kBoolLoopBytes,
                                       kFetchBytes};
  VkMemoryRequirements total{};
  total.memoryTypeBits = 0xFFFFFFFFu;
  VkDeviceSize block_offsets[5]{};
  for (std::uint32_t index = 0u; index < 5u; ++index) {
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = block_sizes[index];
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (vkCreateBuffer(dev, &buffer_info, nullptr,
                       &constant_buffers_[index]) != VK_SUCCESS) {
      error_ = "pinned runtime: constant buffer creation failed";
      return;
    }
    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(dev, constant_buffers_[index], &reqs);
    total.size = (total.size + range_alignment - 1u) & ~(range_alignment - 1u);
    block_offsets[index] = total.size;
    constant_block_offsets_[index] = block_offsets[index];
    total.size += reqs.size;
    total.alignment = total.alignment < reqs.alignment ? reqs.alignment
                                                       : total.alignment;
    total.memoryTypeBits &= reqs.memoryTypeBits;
  }
  std::uint32_t constant_memory_type = 0xFFFFFFFFu;
  if (!find_host_coherent(total.memoryTypeBits, constant_memory_type)) {
    error_ = "pinned runtime: no host-coherent memory type for constants";
    return;
  }
  alloc.allocationSize = total.size;
  alloc.memoryTypeIndex = constant_memory_type;
  if (vkAllocateMemory(dev, &alloc, nullptr, &constant_memory_) != VK_SUCCESS) {
    error_ = "pinned runtime: constant memory allocation failed";
    return;
  }
  for (std::uint32_t index = 0u; index < 5u; ++index) {
    if (vkBindBufferMemory(dev, constant_buffers_[index], constant_memory_,
                           block_offsets[index]) != VK_SUCCESS) {
      error_ = "pinned runtime: constant buffer binding failed";
      return;
    }
  }
  void* constant_mapping = nullptr;
  if (vkMapMemory(dev, constant_memory_, 0, total.size, 0, &constant_mapping) !=
      VK_SUCCESS) {
    error_ = "pinned runtime: constant mapping failed";
    return;
  }
  constant_mapped_ = static_cast<std::uint8_t*>(constant_mapping);
  std::memset(constant_mapped_, 0, static_cast<std::size_t>(total.size));

  // Descriptor set layouts: the contract the oracle's translator emits.
  VkDescriptorSetLayoutBinding bindings[6]{};
  bindings[0].binding = 0u;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  bindings[0].descriptorCount = 1u;
  bindings[0].stageFlags = VK_SHADER_STAGE_ALL;
  for (std::uint32_t index = 0u; index < 5u; ++index) {
    bindings[index + 1u].binding = index;
    bindings[index + 1u].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[index + 1u].descriptorCount = 1u;
    bindings[index + 1u].stageFlags = VK_SHADER_STAGE_ALL;
  }
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 1u;
  layout_info.pBindings = &bindings[0];
  if (vkCreateDescriptorSetLayout(dev, &layout_info, nullptr,
                                  &set_layout_shared_) != VK_SUCCESS) {
    error_ = "pinned runtime: shared memory set layout failed";
    return;
  }
  layout_info.bindingCount = 5u;
  layout_info.pBindings = &bindings[1];
  if (vkCreateDescriptorSetLayout(dev, &layout_info, nullptr,
                                  &set_layout_constants_) != VK_SUCCESS) {
    error_ = "pinned runtime: constants set layout failed";
    return;
  }
  // Empty set layout: used at slot 2 (vertex textures are outside this
  // cycle's contract).
  VkDescriptorSetLayoutCreateInfo empty_layout_info{};
  empty_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  if (vkCreateDescriptorSetLayout(dev, &empty_layout_info, nullptr,
                                  &set_layout_empty_) != VK_SUCCESS) {
    error_ = "pinned runtime: empty set layout failed";
    return;
  }
  // Pixel-texture set layouts are created per shader signature in
  // ensure_pixel_texture_layout (r273): the pinned shaders place their
  // image and sampler bindings at different binding numbers per shader, so
  // one fixed superset layout collides descriptor types at shared binding
  // numbers (the hoisted-gradients payload: images 0..11 + samplers
  // 12..19; the earlier payloads: images 0..3 + samplers 4..5).
  pixel_texture_layout_image_count_ = 12u;
  pixel_texture_layout_sampler_count_ = 8u;
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = device.queue_family();
  if (vkCreateCommandPool(dev, &pool_info, nullptr, &pool_) != VK_SUCCESS) {
    error_ = "pinned runtime: command pool failed";
    return;
  }

  VkDescriptorPoolSize pool_sizes[4]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_sizes[0].descriptorCount = 1u;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[1].descriptorCount = 5u;
  pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  pool_sizes[2].descriptorCount = 64u;
  pool_sizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLER;
  pool_sizes[3].descriptorCount = 64u;
  VkDescriptorPoolCreateInfo descriptor_pool_info{};
  descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_info.flags =
      VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptor_pool_info.maxSets = 42u;
  descriptor_pool_info.poolSizeCount = 4u;
  descriptor_pool_info.pPoolSizes = pool_sizes;
  if (vkCreateDescriptorPool(dev, &descriptor_pool_info, nullptr,
                             &descriptor_pool_) != VK_SUCCESS) {
    error_ = "pinned runtime: descriptor pool failed";
    return;
  }
  VkDescriptorSetAllocateInfo set_alloc{};
  set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  set_alloc.descriptorPool = descriptor_pool_;
  set_alloc.descriptorSetCount = 1u;
  set_alloc.pSetLayouts = &set_layout_shared_;
  if (vkAllocateDescriptorSets(dev, &set_alloc, &descriptor_set_shared_) !=
      VK_SUCCESS) {
    error_ = "pinned runtime: shared descriptor set failed";
    return;
  }
  set_alloc.pSetLayouts = &set_layout_constants_;
  if (vkAllocateDescriptorSets(dev, &set_alloc, &descriptor_set_constants_) !=
      VK_SUCCESS) {
    error_ = "pinned runtime: constants descriptor set failed";
    return;
  }
  set_alloc.pSetLayouts = &set_layout_empty_;
  if (vkAllocateDescriptorSets(dev, &set_alloc, &descriptor_set_empty_) !=
      VK_SUCCESS) {
    error_ = "pinned runtime: empty descriptor set failed";
    return;
  }
  VkDescriptorBufferInfo shared_buffer_info{};
  shared_buffer_info.buffer = shared_memory_;
  shared_buffer_info.range = VK_WHOLE_SIZE;
  VkWriteDescriptorSet shared_write{};
  shared_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  shared_write.dstSet = descriptor_set_shared_;
  shared_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  shared_write.descriptorCount = 1u;
  shared_write.pBufferInfo = &shared_buffer_info;
  vkUpdateDescriptorSets(dev, 1u, &shared_write, 0u, nullptr);
  VkDescriptorBufferInfo constant_buffer_infos[5]{};
  VkWriteDescriptorSet constant_writes[5]{};
  for (std::uint32_t index = 0u; index < 5u; ++index) {
    constant_buffer_infos[index].buffer = constant_buffers_[index];
    constant_buffer_infos[index].range = block_sizes[index];
    constant_writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    constant_writes[index].dstSet = descriptor_set_constants_;
    constant_writes[index].dstBinding = index;
    constant_writes[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    constant_writes[index].descriptorCount = 1u;
    constant_writes[index].pBufferInfo = &constant_buffer_infos[index];
  }
  vkUpdateDescriptorSets(dev, 5u, constant_writes, 0u, nullptr);

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  if (vkCreateFence(dev, &fence_info, nullptr, &fence_) != VK_SUCCESS) {
    error_ = "pinned runtime: fence failed";
    return;
  }
}

PinnedShaderRuntime::~PinnedShaderRuntime() noexcept {
  if (device_ == nullptr) return;
  VkDevice dev = device_->device();
  // Drain in-flight queue work before releasing any GPU resources: the
  // frame's submit may still be executing, and destroying pipelines,
  // fences or descriptor sets while in use corrupts the next user of the
  // shared physical queue (r273: the test-order-dependent corruption).
  vkDeviceWaitIdle(dev);
  for (auto& entry : pipelines_) {
    if (entry.second != VK_NULL_HANDLE) {
      vkDestroyPipeline(dev, entry.second, nullptr);
    }
  }
  for (auto& bank : shader_modules_) {
    for (auto& entry : bank) {
      if (entry.second != VK_NULL_HANDLE) {
        vkDestroyShaderModule(dev, entry.second, nullptr);
      }
    }
  }
  if (fence_ != VK_NULL_HANDLE) vkDestroyFence(dev, fence_, nullptr);
  if (descriptor_pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(dev, descriptor_pool_, nullptr);
  }
  for (std::uint32_t layout_index = 0u; layout_index < pixel_texture_layout_count_;
       ++layout_index) {
    // The sets come from the pool (destroyed wholesale below).
    pixel_texture_layouts_[layout_index].set = VK_NULL_HANDLE;
    if (pixel_texture_layouts_[layout_index].layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(
          dev, pixel_texture_layouts_[layout_index].layout, nullptr);
      pixel_texture_layouts_[layout_index].layout = VK_NULL_HANDLE;
    }
  }
  pixel_texture_layout_count_ = 0u;
  for (std::uint32_t slot_index = 0u; slot_index < kMaxPixelTextures;
       ++slot_index) {
    PixelTextureSlot& slot = texture_slots_[slot_index];
    if (slot.sampler != VK_NULL_HANDLE) {
      vkDestroySampler(dev, slot.sampler, nullptr);
    }
    if (slot.view != VK_NULL_HANDLE) {
      vkDestroyImageView(dev, slot.view, nullptr);
    }
    if (slot.image != VK_NULL_HANDLE) {
      vkDestroyImage(dev, slot.image, nullptr);
    }
    if (slot.memory != VK_NULL_HANDLE) {
      vkFreeMemory(dev, slot.memory, nullptr);
    }
  }
  if (texture_staging_mapped_ != nullptr) {
    vkUnmapMemory(dev, texture_staging_memory_);
  }
  if (index_buffer_mapped_ != nullptr) {
    vkUnmapMemory(dev, index_buffer_memory_);
  }
  if (index_buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(dev, index_buffer_, nullptr);
  }
  if (index_buffer_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, index_buffer_memory_, nullptr);
  }
  if (texture_staging_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(dev, texture_staging_, nullptr);
  }
  if (texture_staging_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, texture_staging_memory_, nullptr);
  }
  if (pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(dev, pool_, nullptr);
  for (std::uint32_t index = 0u; index < edram_pass_count_; ++index) {
    if (edram_passes_[index].load != VK_NULL_HANDLE) {
      vkDestroyRenderPass(dev, edram_passes_[index].load, nullptr);
    }
    if (edram_passes_[index].clear != VK_NULL_HANDLE) {
      vkDestroyRenderPass(dev, edram_passes_[index].clear, nullptr);
    }
  }
  if (edram_framebuffer_ != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(dev, edram_framebuffer_, nullptr);
  }
  if (edram_view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(dev, edram_view_, nullptr);
  }
  if (edram_image_ != VK_NULL_HANDLE) {
    vkDestroyImage(dev, edram_image_, nullptr);
  }
  if (edram_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, edram_memory_, nullptr);
  }
  if (edram_resolve_view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(dev, edram_resolve_view_, nullptr);
  }
  if (edram_resolve_image_ != VK_NULL_HANDLE) {
    vkDestroyImage(dev, edram_resolve_image_, nullptr);
  }
  if (edram_resolve_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, edram_resolve_memory_, nullptr);
  }
  for (std::uint32_t layout_index = 0u; layout_index < pipeline_layout_count_;
       ++layout_index) {
    if (pipeline_layouts_[layout_index].layout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(dev, pipeline_layouts_[layout_index].layout,
                              nullptr);
      pipeline_layouts_[layout_index].layout = VK_NULL_HANDLE;
    }
  }
  pipeline_layout_count_ = 0u;
  if (set_layout_constants_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(dev, set_layout_constants_, nullptr);
  }
  if (set_layout_empty_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(dev, set_layout_empty_, nullptr);
  }
  if (set_layout_shared_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(dev, set_layout_shared_, nullptr);
  }
  for (std::uint32_t index = 0u; index < 5u; ++index) {
    if (constant_buffers_[index] != VK_NULL_HANDLE) {
      vkDestroyBuffer(dev, constant_buffers_[index], nullptr);
    }
  }
  if (constant_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, constant_memory_, nullptr);
  }
  if (shared_memory_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(dev, shared_memory_, nullptr);
  }
  if (shared_memory_device_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, shared_memory_device_, nullptr);
  }
}

bool PinnedShaderRuntime::valid() const noexcept {
  return device_ != nullptr && pool_ != VK_NULL_HANDLE &&
         fence_ != VK_NULL_HANDLE &&
         descriptor_set_constants_ != VK_NULL_HANDLE &&
         shared_memory_mapped_ != nullptr && constant_mapped_ != nullptr;
}

bool PinnedShaderRuntime::write_shared_memory(
    std::uint64_t dword_address, std::span<const std::uint8_t> bytes) noexcept {
  error_.clear();
  if (shared_memory_mapped_ == nullptr) {
    error_ = "pinned runtime: shared memory not mapped";
    return false;
  }
  const std::uint64_t byte_offset = dword_address * 4u;
  if (byte_offset + bytes.size() > shared_memory_dwords_ * 4u) {
    error_ = "pinned runtime: shared memory write out of bounds";
    return false;
  }
  std::memcpy(shared_memory_mapped_ + byte_offset, bytes.data(), bytes.size());
  return true;
}

bool PinnedShaderRuntime::spirv_uses_image_opcodes(
    std::span<const std::uint32_t> spirv) noexcept {
  // Bounded scan: any sampled-image/fetch/read/write operation in the
  // module's code section. OpImageSampleImplicitLod = 97, ExplicitLod =
  // 98, DrefImplicitLod = 99, DrefExplicitLod = 100, Gather = 101,
  // DrefGather = 102, Read = 103, Write = 104, Fetch = 105.
  std::size_t index = 5u;
  while (index < spirv.size()) {
    const std::uint32_t word = spirv[index];
    const std::uint32_t opcode = word & 0xFFFFu;
    const std::uint32_t word_count = (word >> 16u) & 0xFFFFu;
    if (word_count == 0u) return false;
    if ((opcode >= 97u && opcode <= 105u)) return true;
    index += word_count;
  }
  return false;
}

bool PinnedShaderRuntime::spirv_uses_only_supported_resources(
    std::span<const std::uint32_t> spirv) noexcept {
  // Set-2 (vertex-texture) decorations are tolerated in modules without
  // any image operation (declared-but-unused resources in pinned VS
  // variants, e.g. the r265 rectangle-expansion shader, are never
  // accessed); any module that actually samples refuses.
  const bool uses_images = spirv_uses_image_opcodes(spirv);
  std::size_t index = 5u;
  while (index < spirv.size()) {
    const std::uint32_t word = spirv[index];
    const std::uint32_t opcode = word & 0xFFFFu;
    const std::uint32_t word_count = (word >> 16u) & 0xFFFFu;
    if (word_count == 0u) return false;
    if (opcode == 71u && word_count >= 4u &&  // OpDecorate
        spirv[index + 2u] == 34u) {           // DescriptorSet
      const std::uint32_t set = spirv[index + 3u];
      if (set > 3u) return false;  // beyond the pixel-texture set: refused
      if (set == 2u && uses_images) {
        return false;  // vertex textures: refused this cycle
      }
      if (set == 1u && spirv[index + 3u] > 4u) return false;
    }
    if (opcode == 59u && word_count >= 4u) {  // OpVariable
      const std::uint32_t storage_class = spirv[index + 3u];
      if (storage_class == 6u || storage_class == 5u) {
        return false;  // push constants and opaque uniforms: refused
      }
    }
    index += word_count;
  }
  return true;
}

bool PinnedShaderRuntime::ensure_pixel_texture_layout(
    const std::vector<PixelTextureBinding>& bindings,
    std::uint64_t& signature_out) noexcept {
  // Signature: FNV-1a over the sorted (binding, is-sampler) pairs, so two
  // shaders with the same binding/type shape share one layout and set.
  std::uint64_t signature = 1469598103934665603ull;
  std::uint32_t pairs[64];
  std::uint32_t pair_count = 0u;
  for (const PixelTextureBinding& binding : bindings) {
    pairs[pair_count++] = binding.binding * 2u + (binding.is_sampler ? 1u : 0u);
  }
  for (std::uint32_t i = 0u; i + 1u < pair_count; ++i) {
    for (std::uint32_t j = i + 1u; j < pair_count; ++j) {
      if (pairs[j] < pairs[i]) {
        const std::uint32_t swapped = pairs[i];
        pairs[i] = pairs[j];
        pairs[j] = swapped;
      }
    }
  }
  for (std::uint32_t i = 0u; i < pair_count; ++i) {
    signature ^= pairs[i];
    signature *= 1099511628211ull;
  }
  signature ^= static_cast<std::uint64_t>(pair_count) << 24u;
  signature_out = signature;
  for (std::uint32_t layout_index = 0u; layout_index < pixel_texture_layout_count_;
       ++layout_index) {
    if (pixel_texture_layouts_[layout_index].signature == signature) {
      return true;
    }
  }
  if (pixel_texture_layout_count_ >= kMaxPixelTextureLayouts) {
    error_ = "pinned texture: too many distinct pixel-texture layouts";
    return false;
  }
  VkDescriptorSetLayoutBinding layout_bindings[64]{};
  std::uint32_t layout_binding_count = 0u;
  for (std::uint32_t i = 0u; i < pair_count; ++i) {
    layout_bindings[layout_binding_count].binding = pairs[i] / 2u;
    layout_bindings[layout_binding_count].descriptorType =
        (pairs[i] & 1u) ? VK_DESCRIPTOR_TYPE_SAMPLER
                        : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layout_bindings[layout_binding_count].descriptorCount = 1u;
    layout_bindings[layout_binding_count].stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    ++layout_binding_count;
  }
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = layout_binding_count;
  layout_info.pBindings = layout_bindings;
  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  if (vkCreateDescriptorSetLayout(device_->device(), &layout_info, nullptr,
                                  &layout) != VK_SUCCESS) {
    error_ = "pinned texture: pixel-texture set layout failed";
    return false;
  }
  pixel_texture_layouts_[pixel_texture_layout_count_].signature = signature;
  pixel_texture_layouts_[pixel_texture_layout_count_].layout = layout;
  ++pixel_texture_layout_count_;
  return true;
}

bool PinnedShaderRuntime::parse_pixel_texture_bindings(
    std::span<const std::uint32_t> spirv,
    std::vector<PixelTextureBinding>& bindings_out) noexcept {
  bindings_out.clear();
  // Two passes: SPIR-V names and decorations reference ids before the
  // declarations, so collect everything first, then interpret the
  // UniformConstant variables.
  enum { kMaxParsedEntries = 1024u };
  std::uint32_t type_ids[kMaxParsedEntries];
  struct Type final {
    std::uint32_t opcode{};
    std::uint32_t dim{};
    std::uint32_t arrayed{};
    std::uint32_t ms{};
    std::uint32_t sampled{};
    std::uint32_t sampled_type{};
  };
  Type types[kMaxParsedEntries];
  std::uint32_t type_count = 0u;
  std::uint32_t var_ids[kMaxParsedEntries];
  std::uint32_t var_types[kMaxParsedEntries];
  std::uint32_t var_storage[kMaxParsedEntries];
  std::uint32_t var_count = 0u;
  // Decoration/name maps keyed by id.
  std::uint32_t dec_ids[kMaxParsedEntries];
  std::uint32_t dec_sets[kMaxParsedEntries];
  std::int32_t dec_bindings[kMaxParsedEntries];
  std::uint32_t name_ids[kMaxParsedEntries];
  std::uint32_t name_offsets[kMaxParsedEntries];
  std::uint32_t dec_count = 0u;
  std::uint32_t name_count = 0u;
  std::size_t index = 5u;
  while (index < spirv.size()) {
    const std::uint32_t word = spirv[index];
    const std::uint32_t opcode = word & 0xFFFFu;
    const std::uint32_t word_count = (word >> 16u) & 0xFFFFu;
    if (word_count == 0u) return false;
    if (opcode == 71u && word_count >= 4u) {  // OpDecorate target kind ...
      if (dec_count < kMaxParsedEntries) {
        dec_ids[dec_count] = spirv[index + 1u];
        dec_sets[dec_count] = 0xFFFFFFFFu;
        dec_bindings[dec_count] = -1;
        const std::uint32_t kind = spirv[index + 2u];
        if (kind == 34u) dec_sets[dec_count] = spirv[index + 3u];
        if (kind == 33u) {
          dec_bindings[dec_count] = static_cast<std::int32_t>(spirv[index + 3u]);
        }
        // Only set/binding decorations occupy a slot; skip others.
        if (kind != 34u && kind != 33u) --dec_count;
        ++dec_count;
      }
    } else if (opcode == 5u && word_count >= 4u) {  // OpName target "..."
      if (name_count < kMaxParsedEntries) {
        name_ids[name_count] = spirv[index + 1u];
        name_offsets[name_count] = static_cast<std::uint32_t>(index + 2u);
        ++name_count;
      }
    } else if (opcode == 25u && word_count >= 9u) {  // OpTypeImage
      if (type_count < kMaxParsedEntries) {
        type_ids[type_count] = spirv[index + 1u];
        Type& t = types[type_count];
        t.opcode = 25u;
        t.sampled_type = spirv[index + 2u];
        t.dim = spirv[index + 3u];        // Dim
        // index + 4 is Depth.
        t.arrayed = spirv[index + 5u];
        t.ms = spirv[index + 6u];
        t.sampled = spirv[index + 7u];
        ++type_count;
      }
    } else if (opcode == 26u && word_count >= 2u) {  // OpTypeSampler
      if (type_count < kMaxParsedEntries) {
        type_ids[type_count] = spirv[index + 1u];
        types[type_count].opcode = 26u;
        ++type_count;
      }
    } else if (opcode == 22u && word_count >= 3u) {  // OpTypeFloat width
      if (type_count < kMaxParsedEntries) {
        type_ids[type_count] = spirv[index + 1u];
        types[type_count].opcode = 22u;
        ++type_count;
      }
    } else if (opcode == 32u && word_count >= 4u) {  // OpTypePointer
      if (type_count < kMaxParsedEntries) {
        type_ids[type_count] = spirv[index + 1u];
        types[type_count].opcode = 32u;
        types[type_count].sampled_type = spirv[index + 3u];  // pointee
        ++type_count;
      }
    } else if (opcode == 59u && word_count >= 4u) {  // OpVariable
      if (var_count < kMaxParsedEntries) {
        var_ids[var_count] = spirv[index + 2u];
        var_types[var_count] = spirv[index + 1u];
        var_storage[var_count] = spirv[index + 3u];
        ++var_count;
      }
    }
    index += word_count;
  }
  const auto find_type = [&](std::uint32_t id, const Type** out) {
    for (std::uint32_t i = 0u; i < type_count; ++i) {
      if (type_ids[i] == id) {
        *out = &types[i];
        return true;
      }
    }
    return false;
  };
  const auto find_dec = [&](std::uint32_t id, std::uint32_t& set_out,
                            std::int32_t& binding_out) {
    bool found = false;
    for (std::uint32_t i = 0u; i < dec_count; ++i) {
      if (dec_ids[i] != id) continue;
      if (dec_sets[i] != 0xFFFFFFFFu) set_out = dec_sets[i];
      if (dec_bindings[i] >= 0) binding_out = dec_bindings[i];
      found = true;
    }
    return found;
  };
  const auto find_name = [&](std::uint32_t id, std::uint32_t& offset_out) {
    for (std::uint32_t i = 0u; i < name_count; ++i) {
      if (name_ids[i] == id) {
        offset_out = name_offsets[i];
        return true;
      }
    }
    return false;
  };
  struct Slot final {
    std::uint32_t binding{};
    bool is_sampler{};
    std::uint32_t fetch_constant{};
    bool cube_image{};
  };
  Slot slots[64];
  std::uint32_t slot_count = 0u;
  for (std::uint32_t i = 0u; i < var_count; ++i) {
    if (var_storage[i] != 0u) continue;  // UniformConstant only
    std::uint32_t set = 0xFFFFFFFFu;
    std::int32_t binding = -1;
    if (!find_dec(var_ids[i], set, binding)) return false;
    if (set == 0xFFFFFFFFu) return false;  // unbound uniform constants: refuse
    if (set != 3u) continue;
    const Type* type = nullptr;
    if (!find_type(var_types[i], &type)) return false;
    if (type->opcode == 32u && !find_type(type->sampled_type, &type)) {
      return false;  // follow the pointer to the pointee type
    }
    bool is_sampler = false;
    bool cube_image = false;
    std::uint32_t fetch_constant = 0xFFFFFFFFu;
    if (type->opcode == 26u) {
      is_sampler = true;
    } else if (type->opcode == 25u) {
      // Bounded image pattern: float, not depth, not multisample, sampled,
      // unknown format; either the translator's normalized 2D-array form
      // (dim 1, arrayed 1) or a real cube image (dim 3, arrayed 0 - the
      // r273 payload's cube fetches). Other dimensions refuse.
      const Type* sampled_type = nullptr;
      if (!find_type(type->sampled_type, &sampled_type) ||
          sampled_type->opcode != 22u) {
        return false;
      }
      if (type->dim == 1u && type->arrayed == 1u) {
        cube_image = false;
      } else if (type->dim == 3u && type->arrayed == 0u) {
        cube_image = true;
      } else {
        return false;
      }
      if (type->ms != 0u || type->sampled != 1u) {
        return false;
      }
    } else {
      return false;
    }
    if (binding < 0 || binding >= 64) return false;
    std::uint32_t name_offset = 0u;
    if (find_name(var_ids[i], name_offset)) {
      const std::uint32_t word = name_offset;
      const auto string_byte = [&](std::uint32_t c) -> char {
        if (word + c / 4u >= spirv.size()) return '\0';
        return static_cast<char>((spirv[word + c / 4u] >> (8u * (c % 4u))) &
                                 0xFFu);
      };
      const auto matches = [&](const char* prefix) {
        for (std::size_t c = 0u; prefix[c] != '\0'; ++c) {
          if (string_byte(static_cast<std::uint32_t>(c)) != prefix[c]) {
            return false;
          }
        }
        return true;
      };
      const auto digits_after = [&](std::uint32_t start) -> std::int32_t {
        std::uint32_t value = 0u;
        bool any = false;
        for (std::uint32_t c = start; c < start + 8u; ++c) {
          const char byte = string_byte(c);
          if (byte < '0' || byte > '9') break;
          value = value * 10u + static_cast<std::uint32_t>(byte - '0');
          any = true;
        }
        return any ? static_cast<std::int32_t>(value) : -1;
      };
      if (matches("xe_texture")) {
        const std::int32_t value = digits_after(10u);
        if (value >= 0) fetch_constant = static_cast<std::uint32_t>(value);
      } else if (matches("xe_sampler")) {
        const std::int32_t value = digits_after(10u);
        if (value >= 0) fetch_constant = static_cast<std::uint32_t>(value);
      }
    }
    if (fetch_constant == 0xFFFFFFFFu || fetch_constant >= 32u) return false;
    slots[slot_count++] = {static_cast<std::uint32_t>(binding), is_sampler,
                           fetch_constant, cube_image};
    if (slot_count == 64u) return false;
  }
  // Validate the oracle binding layout: images at 0..N-1, samplers after,
  // no duplicate bindings, no gaps.
  std::uint32_t image_count = 0u;
  std::uint32_t sampler_count = 0u;
  for (std::uint32_t i = 0u; i < slot_count; ++i) {
    (slots[i].is_sampler ? sampler_count : image_count) += 1u;
  }
  if (slot_count != image_count + sampler_count) return false;
  // The fixed superset layout (12 images at 0..11, 8 samplers at 12..19)
  // bounds the contract: more image or sampler bindings would write
  // descriptors whose type disagrees with the layout.
  if (image_count > 12u || sampler_count > 8u) return false;
  for (std::uint32_t i = 0u; i < slot_count; ++i) {
    for (std::uint32_t j = i + 1u; j < slot_count; ++j) {
      if (slots[i].binding == slots[j].binding) return false;
    }
  }
  for (std::uint32_t binding = 0u; binding < image_count; ++binding) {
    bool found = false;
    for (std::uint32_t i = 0u; i < slot_count; ++i) {
      if (slots[i].binding == binding) {
        if (slots[i].is_sampler) return false;
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  for (std::uint32_t k = 0u; k < sampler_count; ++k) {
    const std::uint32_t binding = image_count + k;
    bool found = false;
    for (std::uint32_t i = 0u; i < slot_count; ++i) {
      if (slots[i].binding == binding) {
        if (!slots[i].is_sampler) return false;
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  bindings_out.reserve(slot_count);
  for (std::uint32_t i = 0u; i < slot_count; ++i) {
    bindings_out.push_back({slots[i].binding, slots[i].fetch_constant,
                            slots[i].is_sampler, slots[i].cube_image});
  }
  return true;
}

bool PinnedShaderRuntime::decode_pixel_texture(
    const XenosState& state, std::uint32_t fetch_constant,
    PixelTexture& texture) noexcept {
  if (fetch_constant >= 32u) {
    error_ = "pinned texture: fetch constant index out of range";
    return false;
  }
  std::uint32_t words[6];
  for (std::uint32_t i = 0u; i < 6u; ++i) {
    words[i] = state.register_value(kRegShaderConstantFetch000 + 6u * fetch_constant + i);
  }
  // dword0: type 0:1, signs 2:9, clamp x/y 10:15, pitch 22:30 (pixels>>5),
  // tiled bit 31. Only kTexture (2), unsigned signs, linear.
  if ((words[0] & 0x3u) != 2u) {
    error_ = "pinned texture: fetch constant is not a texture";
    return false;
  }
  if (((words[0] >> 2u) & 0xFFu) != 0u) {
    error_ =
        "pinned texture: signed/gamma texture signs are not qualified this "
        "cycle";
    return false;
  }
  if ((words[0] >> 31u) != 0u) {
    error_ = "pinned texture: tiled textures are not qualified this cycle";
    return false;
  }
  const std::uint32_t clamp_x = (words[0] >> 10u) & 0x7u;
  const std::uint32_t clamp_y = (words[0] >> 13u) & 0x7u;
  const auto address_mode = [](std::uint32_t clamp, VkSamplerAddressMode& out) {
    switch (clamp) {
      case 0u: out = VK_SAMPLER_ADDRESS_MODE_REPEAT; return true;
      case 2u: out = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; return true;
      default: return false;
    }
  };
  if (!address_mode(clamp_x, texture.address_x) ||
      !address_mode(clamp_y, texture.address_y)) {
    error_ =
        "pinned texture: clamp mode is not qualified this cycle (repeat or "
        "clamp-to-edge only)";
    return false;
  }
  const std::uint32_t pitch_units = (words[0] >> 22u) & 0x1FFu;
  texture.pitch_pixels = pitch_units << 5u;
  // dword1: format 0:5, endianness 6:7, request_size 8:9, stacked 10,
  // nearest_clamp_policy 11, base_address 12:31 (page >> 12).
  const std::uint32_t format = words[1] & 0x3Fu;
  if (format != 6u) {  // TextureFormat::k_8_8_8_8
    error_ =
        "pinned texture: texture format is not k_8_8_8_8 (only format 6 is "
        "qualified this cycle)";
    return false;
  }
  texture.endianness = (words[1] >> 6u) & 0x3u;
  if (((words[1] >> 10u) & 0x1u) != 0u) {
    error_ = "pinned texture: stacked textures are not qualified this cycle";
    return false;
  }
  texture.byte_address = (words[1] >> 12u) << 12u;
  // dword2 (size_2d): width-1 0:12, height-1 13:25, stack_depth 26:31.
  texture.width = (words[2] & 0x1FFFu) + 1u;
  texture.height = ((words[2] >> 13u) & 0x1FFFu) + 1u;
  if (texture.width > 4096u || texture.height > 4096u) {
    error_ = "pinned texture: size exceeds the bounded 4096x4096 this cycle";
    return false;
  }
  if (texture.pitch_pixels < texture.width) {
    error_ = "pinned texture: fetch pitch is smaller than the width";
    return false;
  }
  // dword3: num_format 0, swizzle 1:12, exp_adjust 13:18, filters 19:25,
  // aniso 25:27, arbitrary 28:30, border_size 31.
  const auto filter_of = [](std::uint32_t value, VkFilter& out) {
    switch (value) {
      case 0u: out = VK_FILTER_NEAREST; return true;   // kPoint
      case 1u: out = VK_FILTER_LINEAR; return true;    // kLinear
      case 2u: out = VK_FILTER_LINEAR; return true;    // kBaseMap
      default: return false;                            // kUseFetchConst etc.
    }
  };
  VkFilter mag{};
  VkFilter min_filter{};
  const std::uint32_t mip_filter = (words[3] >> 23u) & 0x3u;
  if (!filter_of((words[3] >> 19u) & 0x3u, mag) ||
      !filter_of((words[3] >> 21u) & 0x3u, min_filter) || mip_filter == 3u) {
    error_ =
        "pinned texture: filters are not qualified this cycle (point/linear/"
        "basemap only)";
    return false;
  }
  if (mag != min_filter) {
    error_ = "pinned texture: mismatched mag/min filters are not qualified";
    return false;
  }
  texture.filter = mag;
  const std::uint32_t aniso = (words[3] >> 25u) & 0x7u;
  if (aniso != 0u && aniso != 7u) {
    error_ = "pinned texture: anisotropic filtering is not qualified";
    return false;
  }
  // dword4: mip_min_level 2:5, mip_max_level 6:9 must both be 0 (single
  // base mip; mip address trees are a later cycle).
  if (((words[4] >> 2u) & 0xFu) != 0u || ((words[4] >> 6u) & 0xFu) != 0u) {
    error_ = "pinned texture: mip levels are not qualified this cycle";
    return false;
  }
  // dword5 bits 9:10: the data dimension (DataDimension). Cube textures
  // sample as six 2D-array layers whose face images are stacked
  // sequentially in guest memory (r270); 1D and 3D/stacked dimensions
  // stay fail-closed.
  const std::uint32_t dimension = (words[5] >> 9u) & 0x3u;
  if (dimension == 1u) {
    texture.layer_count = 1u;
  } else if (dimension == 3u) {
    texture.layer_count = 6u;
  } else {
    error_ = "pinned texture: texture dimension is not 2D or cube this cycle";
    return false;
  }
  if (((words[5] >> 11u) & 0x1u) != 0u) {
    error_ = "pinned texture: packed mips are not qualified this cycle";
    return false;
  }
  texture.fetch_constant = fetch_constant;
  return true;
}

bool PinnedShaderRuntime::ensure_pixel_textures(
    const XenosState& state,
    const std::vector<PixelTextureBinding>& bindings,
    std::uint64_t texture_layout_signature) noexcept {
  VkDevice dev = device_->device();
  // Distinct fetch constants referenced by the parsed image bindings, in
  // first-use order (bounded: one slot per fetch constant; the fixed
  // set-3 superset exposes 8 image bindings, so at most 8 textures).
  std::uint32_t fetch_constants[kMaxPixelTextures];
  bool fetch_cubes[kMaxPixelTextures] = {};
  std::uint32_t fetch_count = 0u;
  std::uint32_t staging_total = 0u;
  PendingUploadInfo pending[kMaxPixelTextures];
  std::uint32_t pending_count = 0u;
  const auto slot_for = [&](std::uint32_t fetch_constant,
                            std::uint32_t& slot_out) -> bool {
    for (std::uint32_t i = 0u; i < fetch_count; ++i) {
      if (fetch_constants[i] == fetch_constant) {
        slot_out = i;
        return true;
      }
    }
    if (fetch_count == kMaxPixelTextures) {
      error_ =
          "pinned texture: more texture fetch constants than the qualified "
          "superset (8) this cycle";
      return false;
    }
    fetch_constants[fetch_count] = fetch_constant;
    slot_out = fetch_count++;
    return true;
  };
  for (const PixelTextureBinding& binding : bindings) {
    if (binding.is_sampler) continue;
    std::uint32_t slot = 0u;
    if (!slot_for(binding.fetch_constant, slot)) return false;
  }
  // The image view type follows the SPIR-V image type of the bindings that
  // share the fetch constant: a real cube image (dim 3) needs a
  // VK_IMAGE_VIEW_TYPE_CUBE; the normalized 2D-array form (dim 1) needs a
  // 2D-array view. A fetch constant used with both forms refuses
  // (fail-closed: one view cannot serve both).
  {
    bool cube_seen[kMaxPixelTextures] = {};
    for (const PixelTextureBinding& binding : bindings) {
      if (binding.is_sampler) continue;
      for (std::uint32_t i = 0u; i < fetch_count; ++i) {
        if (fetch_constants[i] != binding.fetch_constant) continue;
        if (!cube_seen[i]) {
          fetch_cubes[i] = binding.cube_image;
          cube_seen[i] = true;
        } else if (fetch_cubes[i] != binding.cube_image) {
          error_ =
              "pinned texture: fetch constant used as both a 2D-array and a "
              "cube image";
          return false;
        }
      }
    }
  }
  // Decode every texture first (fail-closed before any resource work).
  PixelTexture decoded[kMaxPixelTextures];
  for (std::uint32_t slot_index = 0u; slot_index < fetch_count;
       ++slot_index) {
    if (!decode_pixel_texture(state, fetch_constants[slot_index],
                              decoded[slot_index])) {
      return false;
    }
  }
  // Grow the shared staging buffer ONCE for all uploads: growing between
  // per-slot fills would destroy the earlier slots' staged bytes.
  {
    std::uint64_t total = 0u;
    for (std::uint32_t slot_index = 0u; slot_index < fetch_count;
         ++slot_index) {
      const PixelTexture& texture = decoded[slot_index];
      total += static_cast<std::uint64_t>(texture.pitch_pixels) * 4u *
               texture.height * texture.layer_count;
    }
    if (texture_staging_size_ < static_cast<VkDeviceSize>(total)) {
      const VkDeviceSize needed = static_cast<VkDeviceSize>(total);
      if (texture_staging_mapped_ != nullptr) {
        vkUnmapMemory(dev, texture_staging_memory_);
        texture_staging_mapped_ = nullptr;
      }
      if (texture_staging_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, texture_staging_, nullptr);
        texture_staging_ = VK_NULL_HANDLE;
      }
      if (texture_staging_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(dev, texture_staging_memory_, nullptr);
        texture_staging_memory_ = VK_NULL_HANDLE;
      }
      VkBufferCreateInfo buffer_info{};
      buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer_info.size = needed;
      buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      if (vkCreateBuffer(dev, &buffer_info, nullptr, &texture_staging_) !=
          VK_SUCCESS) {
        error_ = "pinned texture: staging buffer creation failed";
        return false;
      }
      VkMemoryRequirements reqs{};
      vkGetBufferMemoryRequirements(dev, texture_staging_, &reqs);
      VkPhysicalDeviceMemoryProperties props{};
      vkGetPhysicalDeviceMemoryProperties(device_->physical_device(), &props);
      std::uint32_t memory_type = 0xFFFFFFFFu;
      for (std::uint32_t type = 0u; type < props.memoryTypeCount; ++type) {
        const VkMemoryType& memory = props.memoryTypes[type];
        if ((reqs.memoryTypeBits & (1u << type)) != 0u &&
            (memory.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) !=
                0u &&
            (memory.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) !=
                0u) {
          memory_type = type;
          break;
        }
      }
      if (memory_type == 0xFFFFFFFFu) {
        error_ = "pinned texture: no host-coherent memory type for staging";
        return false;
      }
      VkMemoryAllocateInfo alloc{};
      alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alloc.allocationSize = reqs.size;
      alloc.memoryTypeIndex = memory_type;
      if (vkAllocateMemory(dev, &alloc, nullptr, &texture_staging_memory_) !=
              VK_SUCCESS ||
          vkBindBufferMemory(dev, texture_staging_, texture_staging_memory_,
                             0) != VK_SUCCESS ||
          vkMapMemory(dev, texture_staging_memory_, 0, needed, 0,
                      reinterpret_cast<void**>(&texture_staging_mapped_)) !=
              VK_SUCCESS) {
        error_ = "pinned texture: staging memory mapping failed";
        return false;
      }
      texture_staging_size_ = needed;
    }
  }
  // Create resources, stage bytes and queue the uploads per slot.
  for (std::uint32_t slot_index = 0u; slot_index < fetch_count;
       ++slot_index) {
    const PixelTexture& texture = decoded[slot_index];
    const std::uint64_t row_bytes =
        static_cast<std::uint64_t>(texture.pitch_pixels) * 4u;
    const std::uint64_t copy_bytes =
        static_cast<std::uint64_t>(texture.width) * 4u;
    const std::uint64_t total_bytes =
        row_bytes * texture.height * texture.layer_count;
    if (texture.byte_address + total_bytes >
        shared_memory_dwords_ * 4u) {
      error_ = "pinned texture: guest range exceeds the shared memory size";
      return false;
    }
    if (texture.endianness == 1u || texture.endianness == 3u) {
      error_ =
          "pinned texture: k8in16/k16in32 endianness is not qualified this "
          "cycle (kNone or k8in32 only)";
      return false;
    }
    PixelTextureSlot& slot = texture_slots_[slot_index];
    // Image: recreate only when the dimensions or the view type change.
    if (slot.image != VK_NULL_HANDLE &&
        (slot.width != texture.width || slot.height != texture.height ||
         slot.layers != texture.layer_count ||
         slot.cube_view != fetch_cubes[slot_index])) {
      vkDestroySampler(dev, slot.sampler, nullptr);
      slot.sampler = VK_NULL_HANDLE;
      vkDestroyImageView(dev, slot.view, nullptr);
      slot.view = VK_NULL_HANDLE;
      vkDestroyImage(dev, slot.image, nullptr);
      slot.image = VK_NULL_HANDLE;
      vkFreeMemory(dev, slot.memory, nullptr);
      slot.memory = VK_NULL_HANDLE;
      slot.valid = false;
    }
    if (slot.image == VK_NULL_HANDLE) {
      VkImageCreateInfo image_info{};
      image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      image_info.imageType = VK_IMAGE_TYPE_2D;
      image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
      image_info.extent = {texture.width, texture.height, 1u};
      image_info.mipLevels = 1u;
      image_info.arrayLayers = texture.layer_count;
      image_info.samples = VK_SAMPLE_COUNT_1_BIT;
      image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
      image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT;
      if (vkCreateImage(dev, &image_info, nullptr, &slot.image) !=
          VK_SUCCESS) {
        error_ = "pinned texture: image creation failed";
        return false;
      }
        VkMemoryRequirements reqs{};
      vkGetImageMemoryRequirements(dev, slot.image, &reqs);
      VkPhysicalDeviceMemoryProperties props{};
      vkGetPhysicalDeviceMemoryProperties(device_->physical_device(), &props);
      std::uint32_t memory_type = 0xFFFFFFFFu;
      for (std::uint32_t type = 0u; type < props.memoryTypeCount; ++type) {
        const VkMemoryType& memory = props.memoryTypes[type];
        if ((reqs.memoryTypeBits & (1u << type)) != 0u &&
            (memory.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) !=
                0u) {
          memory_type = type;
          break;
        }
      }
      if (memory_type == 0xFFFFFFFFu) {
        error_ = "pinned texture: no device-local memory type";
        return false;
      }
      VkMemoryAllocateInfo alloc{};
      alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alloc.allocationSize = reqs.size;
      alloc.memoryTypeIndex = memory_type;
      if (vkAllocateMemory(dev, &alloc, nullptr, &slot.memory) !=
              VK_SUCCESS ||
          vkBindImageMemory(dev, slot.image, slot.memory, 0) != VK_SUCCESS) {
        error_ = "pinned texture: memory allocation failed";
        return false;
      }
      VkImageViewCreateInfo view_info{};
      view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view_info.image = slot.image;
      // The view type follows the SPIR-V image type: the translator's
      // normalized 2D-array form for ordinary textures, a real cube image
      // for the r273 payload's cube fetches (6 faces, r270 layering).
      view_info.viewType = fetch_cubes[slot_index]
                               ? VK_IMAGE_VIEW_TYPE_CUBE
                               : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
      view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view_info.subresourceRange.levelCount = 1u;
      view_info.subresourceRange.layerCount = texture.layer_count;
        if (vkCreateImageView(dev, &view_info, nullptr, &slot.view) !=
          VK_SUCCESS) {
        error_ = "pinned texture: image view failed";
        return false;
      }
        slot.width = texture.width;
      slot.height = texture.height;
      slot.layers = texture.layer_count;
      slot.cube_view = fetch_cubes[slot_index];
      slot.layout = VK_IMAGE_LAYOUT_UNDEFINED;
      slot.valid = false;
    }
    if (slot.sampler == VK_NULL_HANDLE) {
      VkSamplerCreateInfo sampler_info{};
      sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler_info.magFilter = texture.filter;
      sampler_info.minFilter = texture.filter;
      sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
      sampler_info.addressModeU = texture.address_x;
      sampler_info.addressModeV = texture.address_y;
      sampler_info.maxLod = 0.0f;
        if (vkCreateSampler(dev, &sampler_info, nullptr, &slot.sampler) !=
          VK_SUCCESS) {
        error_ = "pinned texture: sampler creation failed";
        return false;
      }
    }
      // Stage the guest rows (shared staging buffer, one region per slot).
    const VkDeviceSize needed =
        static_cast<VkDeviceSize>(total_bytes);
    {
      const std::uint8_t* guest =
          shared_memory_mapped_ + texture.byte_address;
      for (std::uint32_t layer = 0u; layer < texture.layer_count; ++layer) {
        const std::uint8_t* face =
            guest + static_cast<std::uint64_t>(layer) * row_bytes *
                        texture.height;
        for (std::uint32_t row = 0u; row < texture.height; ++row) {
          const std::uint8_t* src =
              face + static_cast<std::uint64_t>(row) * row_bytes;
          std::uint8_t* dst = texture_staging_mapped_ + staging_total +
                              static_cast<std::uint64_t>(layer) *
                                  copy_bytes * texture.height +
                              static_cast<std::uint64_t>(row) * copy_bytes;
          if (texture.endianness == 0u) {
            std::memcpy(dst, src, copy_bytes);
          } else {
            for (std::uint32_t x = 0u; x < texture.width; ++x) {
              std::uint32_t word = 0u;
              std::memcpy(&word, src + static_cast<std::uint64_t>(x) * 4u,
                          4u);
              const std::uint32_t swapped =
                  ((word >> 24u) & 0xFFu) | ((word >> 8u) & 0xFF00u) |
                  ((word << 8u) & 0xFF0000u) |
                  ((word << 24u) & 0xFF000000u);
              std::memcpy(dst + static_cast<std::uint64_t>(x) * 4u, &swapped,
                          4u);
            }
          }
        }
      }
    }
      pending[pending_count++] = {slot_index, texture.width, texture.height,
                                  texture.layer_count,
                                  static_cast<VkDeviceSize>(staging_total),
                                  needed};
    staging_total += static_cast<std::uint32_t>(needed);
  }
  // Descriptor set 3: per-shader layout (allocated once per binding
  // signature); every parsed binding is written fresh for this draw,
  // resolving to its own fetch-constant slot (the _u/_s names of a texture
  // share its view).
  {
    VkWriteDescriptorSet writes[64]{};
    VkDescriptorImageInfo infos[64]{};
    VkDescriptorSet pixel_texture_set = VK_NULL_HANDLE;
    for (std::uint32_t layout_index = 0u;
         layout_index < pixel_texture_layout_count_; ++layout_index) {
      if (pixel_texture_layouts_[layout_index].signature ==
          texture_layout_signature) {
        pixel_texture_set = pixel_texture_layouts_[layout_index].set;
        break;
      }
    }
    if (pixel_texture_set == VK_NULL_HANDLE) {
      VkDescriptorSetAllocateInfo set_alloc{};
      set_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      set_alloc.descriptorPool = descriptor_pool_;
      set_alloc.descriptorSetCount = 1u;
      for (std::uint32_t layout_index = 0u;
           layout_index < pixel_texture_layout_count_; ++layout_index) {
        if (pixel_texture_layouts_[layout_index].signature ==
            texture_layout_signature) {
          set_alloc.pSetLayouts = &pixel_texture_layouts_[layout_index].layout;
          break;
        }
      }
      if (set_alloc.pSetLayouts == nullptr) {
        error_ = "pinned texture: pixel-texture layout missing";
        return false;
      }
      if (vkAllocateDescriptorSets(dev, &set_alloc, &pixel_texture_set) !=
          VK_SUCCESS) {
        error_ = "pinned texture: descriptor set allocation failed";
        return false;
      }
      for (std::uint32_t layout_index = 0u;
           layout_index < pixel_texture_layout_count_; ++layout_index) {
        if (pixel_texture_layouts_[layout_index].signature ==
            texture_layout_signature) {
          pixel_texture_layouts_[layout_index].set = pixel_texture_set;
          break;
        }
      }
    }
    std::uint32_t index2 = 0u;
    for (const PixelTextureBinding& binding : bindings) {
      std::uint32_t slot_index = 0u;
      if (!slot_for(binding.fetch_constant, slot_index)) return false;
      const PixelTextureSlot& slot = texture_slots_[slot_index];
      infos[index2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      writes[index2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[index2].dstSet = pixel_texture_set;
      writes[index2].dstBinding = binding.binding;
      if (binding.is_sampler) {
        writes[index2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        infos[index2].sampler = slot.sampler;
        infos[index2].imageView = VK_NULL_HANDLE;
        infos[index2].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      } else {
        writes[index2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        infos[index2].sampler = VK_NULL_HANDLE;
        infos[index2].imageView = slot.view;
      }
      writes[index2].descriptorCount = 1u;
      writes[index2].pImageInfo = &infos[index2];
      ++index2;
    }
      vkUpdateDescriptorSets(dev, index2, writes, 0u, nullptr);
  }
  // The uploads themselves (per slot: current layout -> TRANSFER_DST ->
  // copy from the staging region -> SHADER_READ_ONLY) are recorded at the
  // head of the draw's own command buffer (see draw_pinned) so the texture
  // data and the draw that samples it are one queue submission.
  pending_upload_count_ = pending_count;
  for (std::uint32_t i = 0u; i < pending_count; ++i) {
    pending_uploads_[i] = pending[i];
  }
  return true;
}

void PinnedShaderRuntime::record_texture_upload(
    VkCommandBuffer commands) noexcept {
  for (std::uint32_t i = 0u; i < pending_upload_count_; ++i) {
    const PendingUploadInfo& info = pending_uploads_[i];
    PixelTextureSlot& slot = texture_slots_[info.slot];
    if (slot.image == VK_NULL_HANDLE) continue;
    VkImageMemoryBarrier to_dst{};
    to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_dst.oldLayout = slot.layout;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.image = slot.image;
    to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_dst.subresourceRange.levelCount = 1u;
    to_dst.subresourceRange.layerCount = info.layers;
    to_dst.srcAccessMask = slot.layout == VK_IMAGE_LAYOUT_UNDEFINED
                               ? 0u
                               : VK_ACCESS_SHADER_READ_BIT;
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(
        commands,
        slot.layout == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u,
        &to_dst);
    slot.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    VkBufferImageCopy regions[6]{};
    const std::uint32_t region_count = info.layers;
    const VkDeviceSize face_bytes =
        static_cast<VkDeviceSize>(info.width) * 4u * info.height;
    for (std::uint32_t layer = 0u; layer < region_count; ++layer) {
      regions[layer].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u,
                                         1u};
      regions[layer].imageSubresource.baseArrayLayer = layer;
      regions[layer].imageExtent = {info.width, info.height, 1u};
      regions[layer].bufferOffset =
          info.offset + static_cast<VkDeviceSize>(layer) * face_bytes;
    }
    vkCmdCopyBufferToImage(commands, texture_staging_, slot.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, region_count,
                           regions);
    VkImageMemoryBarrier to_shader{};
    to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.image = slot.image;
    to_shader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_shader.subresourceRange.levelCount = 1u;
    to_shader.subresourceRange.layerCount = info.layers;
    to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0u, 0u,
                         nullptr, 0u, nullptr, 1u, &to_shader);
    slot.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    slot.valid = true;
  }
  pending_upload_count_ = 0u;
}


bool PinnedShaderRuntime::derive_edram_render_target(
    const XenosState& state, EdramRenderTarget& rt) noexcept {
  // RB_MODECONTROL bits 0:2: color draws require kColorDepth (4). Everything
  // else this cycle fails closed rather than guessing.
  const std::uint32_t mode = state.register_value(kRegRbModeControl) & 0x7u;
  if (mode != kEdramModeColorDepth) {
    error_ =
        "pinned draw: EDRAM mode is not color+depth (only kColorDepth is "
        "qualified this cycle)";
    return false;
  }
  // RB_SURFACE_INFO: pitch 0:13 (pixels), MSAA samples 16:17 (Xenos
  // MSAA_NumSamples: 0=1x, 1=2x, 2=4x, 3=8x). r459: 1x/2x/4x are qualified
  // for the Vulkan multisample-image-plus-resolve-attachment plumbing below;
  // 8x is not (unobserved this campaign, and a later cycle's problem if it
  // ever is). NOTE (r459, deliberately NOT resolved this cycle): the tile
  // origin/pitch math further down is UNCHANGED from the 1x-only version --
  // it treats pitch_pixels/base_tiles as plain pixel units regardless of
  // sample_count. Whether RB_SURFACE_INFO's pitch field already accounts for
  // the sample count (so no host-side adjustment is needed) or needs one is
  // NOT established from retail control or an oracle read; it is flagged
  // honestly in the r459 report rather than guessed. It is falsifiable once
  // real MSAA content renders (a systematically offset/torn image would be
  // the symptom), unlike the byte-accounting precedent already in this file
  // for the plain VulkanBackend::submit() ResolvePacket path (source_bytes =
  // width * height * sample_count * 4, i.e. linear in sample_count for a
  // fixed pixel width/height) that motivated qualifying MSAA at all here.
  const std::uint32_t surface_info = state.register_value(kRegRbSurfaceInfo);
  const std::uint32_t pitch_pixels = surface_info & 0x3FFFu;
  const std::uint32_t msaa_bits = (surface_info >> 16u) & 0x3u;
  std::uint32_t sample_count = 1u;
  if (msaa_bits == 1u) {
    sample_count = 2u;
  } else if (msaa_bits == 2u) {
    sample_count = 4u;
  } else if (msaa_bits == 3u) {
    error_ = "pinned draw: 8x MSAA render targets are not qualified this cycle";
    return false;
  }
  if (std::getenv("AC6_NATIVE_VD_TRACE") != nullptr && sample_count > 1u) {
    std::fprintf(stderr,
                  "r459: RB_SURFACE_INFO=0x%08x msaa_bits=%u sample_count=%u "
                  "pitch_pixels=%u\n",
                  surface_info, msaa_bits, sample_count, pitch_pixels);
  }
  if (pitch_pixels == 0u) {
    error_ = "pinned draw: RB_SURFACE_INFO pitch is zero";
    return false;
  }
  // RB_COLOR_INFO: base 0:11 (11-bit periodic tiles + bit 11), format 16:19.
  // Only k_8_8_8_8 (0) is qualified; other formats are a later cycle.
  const std::uint32_t color_info = state.register_value(kRegRbColorInfo);
  const std::uint32_t base_tiles = color_info & 0xFFFu;
  const std::uint32_t format = (color_info >> 16u) & 0xFu;
  // Qualified formats (r266, r268, r269): k_8_8_8_8 (0),
  // k_8_8_8_8_GAMMA (1), k_2_10_10_10 (2) and k_16_16_16_16_FLOAT (7).
  // The oracle's GetColorVulkanFormat maps 1 to VK_FORMAT_R8G8B8A8_UNORM
  // (byte-identical storage with format 0 - the copy resolve path
  // applies), 2 to VK_FORMAT_A8B8G8R8_UNORM_PACK32 (byte-identical with
  // R8G8B8A8_UNORM) and 7 to VK_FORMAT_R16G16B16A16_SFLOAT (float
  // formats are not render-pass compatible with the UNORM target:
  // per-format pass pairs, and the resolve blits with conversion instead
  // of copying). Everything else stays fail-closed (k_32_FLOAT's R32F
  // blit support is driver-dependent; a later cycle).
  if (format != 0u && format != 1u && format != 2u && format != 7u) {
    error_ =
        "pinned draw: color render target format is not qualified this "
        "cycle (k_8_8_8_8, k_8_8_8_8_GAMMA, k_2_10_10_10 and "
        "k_16_16_16_16_FLOAT only)";
    return false;
  }
  // RB_COLOR_MASK bits 0:3 (r460, was previously required to be all-on):
  // per-component RT0 write enables, R=bit0, G=bit1, B=bit2, A=bit3 (the
  // same bit order already used elsewhere in this file for register
  // sub-fields). A live probe against the real ISO showed the value 0x0 --
  // a draw that writes NO color channel this cycle (RB_DEPTHCONTROL is
  // already required to be 0 by the check below, so this is not a
  // depth-only prepass in this engine's model; whatever real purpose the
  // game has for it, translating the mask honestly to Vulkan's
  // colorWriteMask makes the draw a correct no-op for color output instead
  // of a full-batch rejection). All 16 combinations are mechanical
  // (component-order-preserving) and carry no unverified geometry
  // assumption, unlike the r459 EDRAM-tile-under-MSAA question.
  const std::uint32_t color_mask = state.register_value(kRegRbColorMask) & 0xFu;
  // RB_DEPTHCONTROL bits 0:2 (stencil/z enable, z write): no depth surface
  // is modeled this cycle; depth-enabled draws fail closed.
  if ((state.register_value(kRegRbDepthControl) & 0x7u) != 0u) {
    error_ = "pinned draw: depth/stencil tests are not qualified this cycle";
    return false;
  }
  if (state.register_value(kRegPaScWindowOffset) != 0u) {
    error_ =
        "pinned draw: nonzero PA_SC_WINDOW_OFFSET is not qualified this "
        "cycle";
    return false;
  }
  // Tile-pitched linear EDRAM surface: tile = 80x16 samples. The base is a
  // tile index; pixel (x,y) of the target maps to image pixel
  // ((base % pitch_tiles)*80 + x, (base / pitch_tiles)*16 + y). Computed
  // before the scissor region below (r461) so the scissor can be clamped
  // to the real surface instead of rejected outright.
  const std::uint32_t pitch_tiles =
      (pitch_pixels + kEdramTileWidthSamples - 1u) / kEdramTileWidthSamples;
  const std::uint32_t image_width = pitch_tiles * kEdramTileWidthSamples;
  const std::uint32_t tile_rows =
      (kEdramTileCount + pitch_tiles - 1u) / pitch_tiles;
  const std::uint32_t image_height = tile_rows * kEdramTileHeightSamples;
  if (image_width > kMaxEdramImageWidth ||
      image_height > kMaxEdramImageHeight) {
    error_ =
        "pinned draw: tile-pitched EDRAM surface exceeds the bounded image "
        "size this cycle";
    return false;
  }
  const std::uint32_t origin_x =
      (base_tiles % pitch_tiles) * kEdramTileWidthSamples;
  const std::uint32_t origin_y =
      (base_tiles / pitch_tiles) * kEdramTileHeightSamples;
  // Screen scissor defines the pixels the draw may write: signed 15-bit
  // fields. The bounded gate requires a positive region.
  const auto scissor_signed = [](std::uint32_t field) -> std::int32_t {
    const std::int32_t value = static_cast<std::int32_t>(field & 0x7FFFu);
    return (field & 0x4000u) != 0u ? value - 0x8000 : value;
  };
  const std::uint32_t scissor_tl = state.register_value(kRegPaScScreenScissorTL);
  const std::uint32_t scissor_br = state.register_value(kRegPaScScreenScissorBR);
  const std::int32_t region_x0 = scissor_signed(scissor_tl);
  const std::int32_t region_y0 = scissor_signed(scissor_tl >> 16u);
  std::int32_t region_x1 = scissor_signed(scissor_br);
  std::int32_t region_y1 = scissor_signed(scissor_br >> 16u);
  if (region_x0 < 0 || region_y0 < 0 || region_x1 <= region_x0 ||
      region_y1 <= region_y0) {
    error_ =
        "pinned draw: screen scissor does not describe a positive region";
    return false;
  }
  // r461: a live probe against the real ISO showed scissor_tl=0x00000000
  // scissor_br=0x20002000 -- a (0,0)-(8192,8192) region, far larger than
  // any real render target this title uses. This is the standard Xenos/
  // GPU scissor-test idiom for "no extra clipping": real hardware clamps
  // rasterization to the actual render target regardless of a nominally
  // larger scissor rect, so a scissor extending past the EDRAM surface is
  // harmless, not an error. The bounded gate here now clamps the scissor
  // to the tile-pitched surface's own bounds (image_width/image_height,
  // already computed above) instead of rejecting the draw outright --
  // this reuses the exact same tile-pitch geometry the surface's own
  // origin/dims already depend on, not a new assumption. A region that
  // clamps to empty (origin already past the surface edge) still fails
  // closed below rather than silently drawing nothing.
  if (origin_x < image_width) {
    region_x1 = std::min(
        region_x1, region_x0 + static_cast<std::int32_t>(image_width - origin_x));
  }
  if (origin_y < image_height) {
    region_y1 = std::min(
        region_y1,
        region_y0 + static_cast<std::int32_t>(image_height - origin_y));
  }
  if (region_x1 <= region_x0 || region_y1 <= region_y0) {
    error_ =
        "pinned draw: screen scissor clamps to an empty region on this "
        "EDRAM surface";
    return false;
  }
  const std::uint32_t region_width =
      static_cast<std::uint32_t>(region_x1 - region_x0);
  const std::uint32_t region_height =
      static_cast<std::uint32_t>(region_y1 - region_y0);
  if (region_width > kMaxEdramRegionWidth ||
      region_height > kMaxEdramRegionHeight) {
    if (std::getenv("AC6_NATIVE_VD_TRACE") != nullptr) {
      std::fprintf(stderr,
                    "r461 probe: scissor_tl=0x%08x scissor_br=0x%08x "
                    "region=(%d,%d)-(%d,%d) size=%ux%u (post-clamp)\n",
                    scissor_tl, scissor_br, region_x0, region_y0, region_x1,
                    region_y1, region_width, region_height);
    }
    error_ = "pinned draw: render target region exceeds the bounded size";
    return false;
  }
  if (origin_x + region_width > image_width ||
      origin_y + region_height > image_height) {
    error_ = "pinned draw: render target region exceeds the EDRAM surface";
    return false;
  }
  rt.pitch_pixels = pitch_pixels;
  rt.base_tiles = base_tiles;
  rt.format = format;
  rt.origin_x = origin_x;
  rt.origin_y = origin_y;
  rt.width = region_width;
  rt.height = region_height;
  rt.image_width = image_width;
  rt.image_height = image_height;
  rt.sample_count = sample_count;
  rt.color_mask = color_mask;
  return true;
}

VkFormat PinnedShaderRuntime::edram_image_format(
    std::uint32_t color_format) noexcept {
  // The oracle's GetColorVulkanFormat mapping (render_target_cache.cpp):
  // k_8_8_8_8 and k_8_8_8_8_GAMMA -> R8G8B8A8_UNORM (byte-identical
  // storage), k_2_10_10_10 -> A8B8G8R8_UNORM_PACK32 (byte-identical
  // storage), k_16_16_16_16_FLOAT -> R16G16B16A16_SFLOAT.
  switch (color_format) {
    case 1u:
    case 2u:
      return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case 7u:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    default:
      return VK_FORMAT_R8G8B8A8_UNORM;
  }
}

bool PinnedShaderRuntime::ensure_edram_passes(
    VkFormat format, std::uint32_t sample_count, VkRenderPass& clear_out,
    VkRenderPass& load_out) noexcept {
  for (std::uint32_t index = 0u; index < edram_pass_count_; ++index) {
    if (edram_passes_[index].format == format &&
        edram_passes_[index].samples == sample_count &&
        edram_passes_[index].clear != VK_NULL_HANDLE) {
      clear_out = edram_passes_[index].clear;
      load_out = edram_passes_[index].load;
      return true;
    }
  }
  if (edram_pass_count_ >= 4u) {
    error_ = "pinned runtime: EDRAM render pass cache exhausted";
    return false;
  }
  VkDevice dev = device_->device();
  // EDRAM render passes (r257): one color attachment. The first draw on a
  // (re)configured target clears; subsequent draws load (EDRAM persists
  // between draws on real hardware until reconfiguration). Both passes end
  // in TRANSFER_SRC_OPTIMAL so the XE_SWAP resolve can move the surface
  // into the readable image without extra layout churn. The format is the
  // surface's image format (float formats are not compatible with the
  // UNORM target).
  //
  // r459: at sample_count > 1 this attachment is simply multisampled
  // (vk_sample_count(sample_count)); it is still legal to end a multisample
  // color attachment in TRANSFER_SRC_OPTIMAL (vkCmdResolveImage accepts a
  // multisample source in that layout). No second attachment is added here
  // -- the EDRAM surface persists across draws exactly like the 1x case
  // (same clear/load semantics, same single image), and downsampling to a
  // 1x-readable image happens once, explicitly, in resolve_edram_to_target
  // at present time (via vkCmdResolveImage into edram_resolve_image_) --
  // not per-draw via an automatic subpass resolve, which would have thrown
  // away not-yet-resolved multisample content between accumulating draws on
  // the same target.
  VkAttachmentDescription attachment{};
  attachment.format = format;
  attachment.samples = vk_sample_count(sample_count);
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  VkAttachmentReference color_ref{};
  color_ref.attachment = 0u;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1u;
  subpass.pColorAttachments = &color_ref;
  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1u;
  render_pass_info.pAttachments = &attachment;
  render_pass_info.subpassCount = 1u;
  render_pass_info.pSubpasses = &subpass;
  VkRenderPass clear_pass = VK_NULL_HANDLE;
  if (vkCreateRenderPass(dev, &render_pass_info, nullptr, &clear_pass) !=
          VK_SUCCESS ||
      clear_pass == VK_NULL_HANDLE) {
    error_ = "pinned runtime: EDRAM clear render pass failed";
    return false;
  }
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachment.initialLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  VkRenderPass load_pass = VK_NULL_HANDLE;
  if (vkCreateRenderPass(dev, &render_pass_info, nullptr, &load_pass) !=
          VK_SUCCESS ||
      load_pass == VK_NULL_HANDLE) {
    vkDestroyRenderPass(dev, clear_pass, nullptr);
    error_ = "pinned runtime: EDRAM load render pass failed";
    return false;
  }
  EdramPasses& cached = edram_passes_[edram_pass_count_++];
  cached.format = format;
  cached.samples = sample_count;
  cached.clear = clear_pass;
  cached.load = load_pass;
  clear_out = clear_pass;
  load_out = load_pass;
  return true;
}

bool PinnedShaderRuntime::ensure_edram_surface(
    const EdramRenderTarget& rt) noexcept {
  if (edram_image_ != VK_NULL_HANDLE &&
      edram_surface_dims_.image_width == rt.image_width &&
      edram_surface_dims_.image_height == rt.image_height &&
      edram_surface_dims_.format == rt.format &&
      edram_surface_dims_.sample_count == rt.sample_count) {
    return true;
  }
  VkDevice dev = device_->device();
  if (edram_framebuffer_ != VK_NULL_HANDLE) {
    vkDestroyFramebuffer(dev, edram_framebuffer_, nullptr);
    edram_framebuffer_ = VK_NULL_HANDLE;
  }
  if (edram_view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(dev, edram_view_, nullptr);
    edram_view_ = VK_NULL_HANDLE;
  }
  if (edram_image_ != VK_NULL_HANDLE) {
    vkDestroyImage(dev, edram_image_, nullptr);
    edram_image_ = VK_NULL_HANDLE;
  }
  if (edram_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, edram_memory_, nullptr);
    edram_memory_ = VK_NULL_HANDLE;
  }
  if (edram_resolve_view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(dev, edram_resolve_view_, nullptr);
    edram_resolve_view_ = VK_NULL_HANDLE;
  }
  if (edram_resolve_image_ != VK_NULL_HANDLE) {
    vkDestroyImage(dev, edram_resolve_image_, nullptr);
    edram_resolve_image_ = VK_NULL_HANDLE;
  }
  if (edram_resolve_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(dev, edram_resolve_memory_, nullptr);
    edram_resolve_memory_ = VK_NULL_HANDLE;
  }
  edram_rt_valid_ = false;  // fresh surface: first draw clears
  const bool multisample = rt.sample_count > 1u;
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  const VkFormat image_format = edram_image_format(rt.format);
  image_info.format = image_format;
  image_info.extent = {rt.image_width, rt.image_height, 1u};
  image_info.mipLevels = 1u;
  image_info.arrayLayers = 1u;
  image_info.samples = vk_sample_count(rt.sample_count);
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  if (vkCreateImage(dev, &image_info, nullptr, &edram_image_) != VK_SUCCESS ||
      edram_image_ == VK_NULL_HANDLE) {
    error_ = "pinned runtime: EDRAM image creation failed";
    return false;
  }
  VkMemoryRequirements reqs{};
  vkGetImageMemoryRequirements(dev, edram_image_, &reqs);
  VkPhysicalDeviceMemoryProperties props{};
  vkGetPhysicalDeviceMemoryProperties(device_->physical_device(), &props);
  std::uint32_t memory_type = 0xFFFFFFFFu;
  for (std::uint32_t type = 0u; type < props.memoryTypeCount; ++type) {
    const VkMemoryType& memory = props.memoryTypes[type];
    if ((reqs.memoryTypeBits & (1u << type)) != 0u &&
        (memory.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0u) {
      memory_type = type;
      break;
    }
  }
  if (memory_type == 0xFFFFFFFFu) {
    error_ = "pinned runtime: no device-local memory type for EDRAM";
    return false;
  }
  VkMemoryAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc.allocationSize = reqs.size;
  alloc.memoryTypeIndex = memory_type;
  if (vkAllocateMemory(dev, &alloc, nullptr, &edram_memory_) != VK_SUCCESS ||
      vkBindImageMemory(dev, edram_image_, edram_memory_, 0) != VK_SUCCESS) {
    error_ = "pinned runtime: EDRAM memory allocation failed";
    return false;
  }
  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = edram_image_;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = image_format;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.levelCount = 1u;
  view_info.subresourceRange.layerCount = 1u;
  if (vkCreateImageView(dev, &view_info, nullptr, &edram_view_) !=
      VK_SUCCESS) {
    error_ = "pinned runtime: EDRAM image view failed";
    return false;
  }
  // r459: at sample_count > 1, a companion 1x image is created purely as
  // the vkCmdResolveImage destination in resolve_edram_to_target -- it is
  // never a render-pass attachment, so it needs no framebuffer/view role
  // beyond being a valid resolve target and TRANSFER_SRC for the existing
  // copy/blit-to-present-target step that follows resolve, unchanged from
  // the 1x path.
  if (multisample) {
    VkImageCreateInfo resolve_info = image_info;
    resolve_info.samples = VK_SAMPLE_COUNT_1_BIT;
    resolve_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateImage(dev, &resolve_info, nullptr, &edram_resolve_image_) !=
            VK_SUCCESS ||
        edram_resolve_image_ == VK_NULL_HANDLE) {
      error_ = "pinned runtime: EDRAM resolve image creation failed";
      return false;
    }
    VkMemoryRequirements resolve_reqs{};
    vkGetImageMemoryRequirements(dev, edram_resolve_image_, &resolve_reqs);
    std::uint32_t resolve_memory_type = 0xFFFFFFFFu;
    for (std::uint32_t type = 0u; type < props.memoryTypeCount; ++type) {
      const VkMemoryType& memory = props.memoryTypes[type];
      if ((resolve_reqs.memoryTypeBits & (1u << type)) != 0u &&
          (memory.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) !=
              0u) {
        resolve_memory_type = type;
        break;
      }
    }
    if (resolve_memory_type == 0xFFFFFFFFu) {
      error_ = "pinned runtime: no device-local memory type for EDRAM resolve";
      return false;
    }
    VkMemoryAllocateInfo resolve_alloc{};
    resolve_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    resolve_alloc.allocationSize = resolve_reqs.size;
    resolve_alloc.memoryTypeIndex = resolve_memory_type;
    if (vkAllocateMemory(dev, &resolve_alloc, nullptr,
                         &edram_resolve_memory_) != VK_SUCCESS ||
        vkBindImageMemory(dev, edram_resolve_image_, edram_resolve_memory_,
                          0) != VK_SUCCESS) {
      error_ = "pinned runtime: EDRAM resolve memory allocation failed";
      return false;
    }
    VkImageViewCreateInfo resolve_view_info = view_info;
    resolve_view_info.image = edram_resolve_image_;
    if (vkCreateImageView(dev, &resolve_view_info, nullptr,
                          &edram_resolve_view_) != VK_SUCCESS) {
      error_ = "pinned runtime: EDRAM resolve image view failed";
      return false;
    }
  }
  VkRenderPass surface_clear_pass = VK_NULL_HANDLE;
  VkRenderPass surface_load_pass = VK_NULL_HANDLE;
  if (!ensure_edram_passes(image_format, rt.sample_count, surface_clear_pass,
                           surface_load_pass)) {
    return false;
  }
  VkFramebufferCreateInfo framebuffer_info{};
  framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_info.renderPass = surface_clear_pass;
  framebuffer_info.attachmentCount = 1u;
  framebuffer_info.pAttachments = &edram_view_;
  framebuffer_info.width = rt.image_width;
  framebuffer_info.height = rt.image_height;
  framebuffer_info.layers = 1u;
  if (vkCreateFramebuffer(dev, &framebuffer_info, nullptr,
                          &edram_framebuffer_) != VK_SUCCESS) {
    error_ = "pinned runtime: EDRAM framebuffer failed";
    return false;
  }
  edram_surface_dims_ = rt;
  return true;
}

bool PinnedShaderRuntime::resolve_edram_to_target(
    VulkanOffscreenTarget& target, const EdramRenderTarget& rt) noexcept {
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = pool_;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = 1u;
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_->device(), &alloc_info, &commands) !=
          VK_SUCCESS ||
      commands == VK_NULL_HANDLE) {
    error_ = "pinned resolve: command buffer allocation failed";
    return false;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  bool ok = vkBeginCommandBuffer(commands, &begin) == VK_SUCCESS;
  const bool multisample = rt.sample_count > 1u;
  // r459: at sample_count > 1, edram_image_ is the multisample surface
  // draws accumulate into (persisted across draws exactly like the 1x
  // case). Before the existing copy/blit-to-present-target step (which
  // reads a 1x TRANSFER_SRC image), it is downsampled once here via
  // vkCmdResolveImage into edram_resolve_image_ -- the one point in the
  // whole draw/present cycle where multisample content is actually
  // resolved, so in-flight accumulation across separate draw_pinned()
  // render passes on the same target is never discarded early.
  const VkImage read_image = multisample ? edram_resolve_image_ : edram_image_;
  if (ok && multisample) {
    VkImageMemoryBarrier to_dst{};
    to_dst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.image = edram_resolve_image_;
    to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_dst.subresourceRange.levelCount = 1u;
    to_dst.subresourceRange.layerCount = 1u;
    to_dst.srcAccessMask = 0u;
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
                         nullptr, 1u, &to_dst);
    VkImageResolve resolve_region{};
    resolve_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    resolve_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    resolve_region.extent = {rt.image_width, rt.image_height, 1u};
    vkCmdResolveImage(commands, edram_image_,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      edram_resolve_image_,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u,
                      &resolve_region);
    VkImageMemoryBarrier to_src{};
    to_src.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_src.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.image = edram_resolve_image_;
    to_src.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_src.subresourceRange.levelCount = 1u;
    to_src.subresourceRange.layerCount = 1u;
    to_src.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u,
                         nullptr, 1u, &to_src);
  }
  if (ok) {
    // The EDRAM surface ends every draw pass in TRANSFER_SRC_OPTIMAL.
    ok = target.begin_transfer_dst(commands);
  }
  if (ok) {
    if (edram_image_format(rt.format) == VK_FORMAT_R8G8B8A8_UNORM) {
      VkImageCopy region{};
      region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
      region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
      region.srcOffset = {static_cast<std::int32_t>(rt.origin_x),
                          static_cast<std::int32_t>(rt.origin_y), 0};
      region.dstOffset = {0, 0, 0};
      region.extent = {rt.width, rt.height, 1u};
      vkCmdCopyImage(commands, read_image,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target.image(),
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
    } else {
      // Float formats convert at resolve: blit into the UNORM present
      // target (the EDRAM image ends each pass in TRANSFER_SRC_OPTIMAL,
      // which is also BLIT_SRC-capable; the target is TRANSFER_DST).
      VkImageBlit region{};
      region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
      region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
      region.srcOffsets[1] = {
          static_cast<std::int32_t>(rt.origin_x + rt.width),
          static_cast<std::int32_t>(rt.origin_y + rt.height), 1};
      region.dstOffsets[1] = {static_cast<std::int32_t>(rt.width),
                              static_cast<std::int32_t>(rt.height), 1};
      vkCmdBlitImage(commands, read_image,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target.image(),
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region,
                     VK_FILTER_NEAREST);
    }
  }
  if (ok) {
    ok = target.end_transfer_dst(commands);
  }
  ok = ok && vkEndCommandBuffer(commands) == VK_SUCCESS;
  if (ok) {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &commands;
    ok = vkQueueSubmit(device_->queue(), 1u, &submit, fence_) == VK_SUCCESS;
    if (ok) {
      ok = vkWaitForFences(device_->device(), 1u, &fence_, VK_TRUE,
                           10u * 1000u * 1000u * 1000u) == VK_SUCCESS;
      vkResetFences(device_->device(), 1u, &fence_);
    }
  }
  vkFreeCommandBuffers(device_->device(), pool_, 1u, &commands);
  if (!ok && error_.empty()) {
    error_ = "pinned resolve: submission failed";
  }
  return ok;
}

bool PinnedShaderRuntime::ensure_shader_module(
    std::uint32_t shader_type, std::span<const std::uint32_t> microcode,
    const std::vector<std::uint32_t>& spirv,
    VkShaderModule& module_out) noexcept {
  const std::uint64_t digest = ShaderTranslator::digest_words(microcode);
  auto& bank = shader_modules_[shader_type];
  auto found = bank.find(digest);
  if (found != bank.end()) {
    module_out = found->second;
    return true;
  }
  VkShaderModuleCreateInfo module_info{};
  module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module_info.codeSize = spirv.size() * sizeof(std::uint32_t);
  module_info.pCode = spirv.data();
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device_->device(), &module_info, nullptr, &module) !=
      VK_SUCCESS) {
    error_ = "pinned runtime: shader module creation failed";
    return false;
  }
  bank.emplace(digest, module);
  module_out = module;
  return true;
}

bool PinnedShaderRuntime::ensure_pipeline(
    std::uint32_t primitive_type, VkFormat color_format, VkShaderModule vertex,
    VkShaderModule pixel, std::uint64_t vs_digest, std::uint64_t ps_digest,
    std::uint64_t texture_layout_signature, VkDescriptorSetLayout texture_layout,
    std::uint32_t sample_count, std::uint32_t color_mask,
    VkPipeline& pipeline_out) noexcept {
  const PipelineKey key{primitive_type,
                        static_cast<std::uint32_t>(color_format), vs_digest,
                        ps_digest, texture_layout_signature, sample_count,
                        color_mask};
  auto found = pipelines_.find(key);
  if (found != pipelines_.end()) {
    pipeline_out = found->second;
    return true;
  }
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  {
    VkPipelineLayout existing = VK_NULL_HANDLE;
    for (std::uint32_t layout_index = 0u; layout_index < pipeline_layout_count_;
         ++layout_index) {
      if (pipeline_layouts_[layout_index].signature ==
          texture_layout_signature) {
        existing = pipeline_layouts_[layout_index].layout;
        break;
      }
    }
    if (existing == VK_NULL_HANDLE) {
      if (pipeline_layout_count_ >= kMaxPipelineLayouts) {
        error_ = "pinned runtime: too many pipeline layouts";
        return false;
      }
      VkPipelineLayoutCreateInfo pipeline_layout_info{};
      pipeline_layout_info.sType =
          VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      VkDescriptorSetLayout layouts[4] = {set_layout_shared_,
                                          set_layout_constants_,
                                          set_layout_empty_, texture_layout};
      pipeline_layout_info.setLayoutCount = 4u;
      pipeline_layout_info.pSetLayouts = layouts;
      if (vkCreatePipelineLayout(device_->device(), &pipeline_layout_info,
                                 nullptr, &existing) != VK_SUCCESS) {
        error_ = "pinned runtime: pipeline layout failed";
        return false;
      }
      pipeline_layouts_[pipeline_layout_count_].signature =
          texture_layout_signature;
      pipeline_layouts_[pipeline_layout_count_].layout = existing;
      ++pipeline_layout_count_;
    }
    pipeline_layout = existing;
  }
  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertex;
  stages[0].pName = "main";
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = pixel;
  stages[1].pName = "main";
  VkPipelineInputAssemblyStateCreateInfo ia{};
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  bool ok = true;
  ia.topology = topology_of(primitive_type, ok);
  if (!ok) {
    error_ = "pinned draw: primitive topology not qualified this cycle";
    return false;
  }
  // No dynamic index buffer: auto-indexed draws only this cycle; the vertex
  // fetch reads shared memory inside the pinned shader.
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  // Guest cull mode (PA_SU_SC_MODE_CNTL) is a later qualification step; the
  // contract here renders without host culling rather than guessing it.
  raster.cullMode = VK_CULL_MODE_NONE;
  raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  raster.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = vk_sample_count(sample_count);
  VkPipelineColorBlendAttachmentState blend_attachment{};
  // r460: RB_COLOR_MASK bits 0:3 translated directly, R=bit0..A=bit3.
  VkColorComponentFlags write_mask = 0u;
  if ((color_mask & 0x1u) != 0u) write_mask |= VK_COLOR_COMPONENT_R_BIT;
  if ((color_mask & 0x2u) != 0u) write_mask |= VK_COLOR_COMPONENT_G_BIT;
  if ((color_mask & 0x4u) != 0u) write_mask |= VK_COLOR_COMPONENT_B_BIT;
  if ((color_mask & 0x8u) != 0u) write_mask |= VK_COLOR_COMPONENT_A_BIT;
  blend_attachment.colorWriteMask = write_mask;
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = 1u;
  blend.pAttachments = &blend_attachment;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1u;
  viewport.scissorCount = 1u;
  // Dynamic viewport/scissor: the target dimensions come from the offscreen
  // target at draw time (the pipeline is dimension-independent).
  const VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                            VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_info{};
  dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_info.dynamicStateCount = 2u;
  dynamic_info.pDynamicStates = dynamic_states;
  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2u;
  pipeline_info.pStages = stages;
  pipeline_info.pInputAssemblyState = &ia;
  pipeline_info.pRasterizationState = &raster;
  pipeline_info.pMultisampleState = &multisample;
  pipeline_info.pColorBlendState = &blend;
  pipeline_info.pViewportState = &viewport;
  pipeline_info.pDynamicState = &dynamic_info;
  VkRenderPass pipeline_clear_pass = VK_NULL_HANDLE;
  VkRenderPass pipeline_load_pass = VK_NULL_HANDLE;
  if (!ensure_edram_passes(color_format, sample_count, pipeline_clear_pass,
                           pipeline_load_pass)) {
    return false;
  }
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = pipeline_clear_pass;
  pipeline_info.subpass = 0u;
  VkPipeline pipeline = VK_NULL_HANDLE;
  if (vkCreateGraphicsPipelines(device_->device(), VK_NULL_HANDLE, 1u,
                                &pipeline_info, nullptr, &pipeline) !=
      VK_SUCCESS) {
    error_ = "pinned runtime: pipeline creation failed";
    return false;
  }
  pipelines_.emplace(key, pipeline);
  pipeline_out = pipeline;
  return true;
}

std::uint64_t PinnedShaderRuntime::derive_pixel_modification_high(
    const XenosState& state) const noexcept {
  // Bounded subset of GetCurrentPixelShaderModification's dword1: dynamic
  // count 0 (shaders with dynamic addressing refuse at lookup), param_gen
  // 0 (non-point draws), depth_stencil_mode = kEarlyHint when coverage
  // does NOT depend on alpha (alpha test and alpha-to-coverage disabled;
  // the qualified shaders write color 0). The interpolator mask lives in
  // dword0 and is taken from the matched variant.
  const std::uint32_t color_control = state.register_value(kRegRbColorControl);
  const bool alpha_test = (color_control >> 3u) & 0x1u;
  const bool alpha_to_coverage = (color_control >> 4u) & 0x1u;
  const std::uint64_t depth_stencil_mode =
      (alpha_test || alpha_to_coverage) ? 0u : 1u;  // kEarlyHint = 1
  return depth_stencil_mode << 46u;  // dword1 bit 14 -> value bit 46
}

bool PinnedShaderRuntime::draw_pinned(VulkanOffscreenTarget& target,
                                      const XenosState& state,
                                      const DrawPacket& draw) noexcept {
  error_.clear();
  // Bounded contract: auto-indexed draws, plus REAL indexed draws from the
  // guest index buffer (DRAW_INDX source 0): 32-bit guest indices use the
  // oracle's in-shader manual load path (kSysFlag_VertexIndexLoad: the
  // pinned VS loads the index dword from shared memory itself), 16-bit
  // guest indices are expanded to a host 32-bit index buffer (the oracle
  // converts them into a Vulkan index buffer too). Primitive-restart
  // values fail closed.
  const bool indexed_draw = draw.index_address != 0u;
  // Rectangle lists (guest primitive 0x08) are expanded host-side to
  // two-triangle strips (r265; the oracle's kRectangleListAsTriangleStrip
  // scheme): one 4-vertex strip per 3 guest vertices, with primitive
  // restarts between strips; the pinned VS (host type 9 variant) maps
  // host index (i<<2)|j to guest vertex (i*3 + min(j, 2)) and optionally
  // loads the real guest index from the guest index buffer.
  const bool rect_strip_expand = draw.primitive_type == 0x08u;
  if (rect_strip_expand &&
      (draw.index_count == 0u || draw.index_count % 3u != 0u)) {
    error_ =
        "pinned draw: rectangle-list vertex count is not a multiple of 3";
    return false;
  }
  if (rect_strip_expand &&
      draw.index_count / 3u > (1u << 20u)) {
    error_ = "pinned draw: rectangle-list draw exceeds the bounded "
             "primitive count";
    return false;
  }
  if (indexed_draw && draw.index_format > 1u) {
    error_ = "pinned draw: unsupported index format";
    return false;
  }
  if (indexed_draw &&
      draw.index_count > (draw.index_format == 1u ? 0x1000000u : 0x2000000u)) {
    error_ = "pinned draw: indexed draw exceeds the bounded index count";
    return false;
  }
  // r263: the index stream is split into runs at primitive-restart
  // values (0xFFFF for 16-bit, 0xFFFFFFFF for 32-bit - the Xenos restart
  // encoding). A restart closes the current run; the runs after it are
  // drawn as separate indexed draws, so no bridging primitive is
  // rendered (the oracle expands the index buffer the same way). Without
  // a restart, the r262 paths are used unchanged: 32-bit indices stay on
  // the in-shader kSysFlag_VertexIndexLoad path, 16-bit indices use the
  // whole-buffer host index buffer.
  std::vector<std::uint32_t> host_indices;
  std::vector<std::uint32_t> run_firsts;
  std::vector<std::uint32_t> run_counts;
  bool restart_found = false;
  if (rect_strip_expand) {
    // The host index buffer is the oracle's two-triangle-strip builtin:
    // per primitive i, the strip indices (i<<2)+0..3, with a primitive
    // restart between strips (each strip then draws as its own run via
    // the r263 machinery). The guest index buffer (DMA draws) is read by
    // the pinned VS itself; guest restarts are not qualified here.
    // r457: this check is only meaningful for `indexed_draw` (a real
    // guest index buffer needs scanning, below, and this codepath only
    // supports 16-bit guest indices for that scan) -- it was previously
    // unconditional, rejecting every auto-indexed rectangle-list draw
    // (index_address==0, so indexed_draw is false) regardless of
    // index_format, since native_xenos.cpp's DRAW_INDX_2 decode always
    // sets index_format=0 for the auto-indexed case. Live-verified
    // (r435) that every real rectangle-list draw this session observed
    // is auto-indexed (index_address=0x00000000) -- unconditionally
    // rejecting on index_format alone meant no real rectangle-list draw
    // could ever be pinned, by construction, regardless of shader-registry
    // coverage. The host index generation just below already runs
    // unconditionally inside `if (rect_strip_expand)`, independent of
    // indexed_draw -- only the indexed_draw branch immediately after this
    // check reads the real guest index buffer, so index_format is simply
    // irrelevant when indexed_draw is false.
    if (indexed_draw && draw.index_format != 1u) {
      error_ =
          "pinned draw: rectangle-list expansion is not qualified with "
          "16-bit guest indices this cycle";
      return false;
    }
    if (indexed_draw) {
      const std::uint64_t byte_offset = draw.index_address;
      const std::uint64_t needed = draw.index_count * 4u;
      if (shared_memory_mapped_ == nullptr ||
          byte_offset + needed > shared_memory_dwords_ * 4u) {
        error_ = "pinned draw: index buffer outside shared memory";
        return false;
      }
      for (std::uint32_t index = 0u; index < draw.index_count; ++index) {
        const std::uint8_t* word =
            shared_memory_mapped_ + byte_offset + index * 4u;
        if (word[0] == 0xFFu && word[1] == 0xFFu && word[2] == 0xFFu &&
            word[3] == 0xFFu) {
          error_ =
              "pinned draw: primitive-restart index (0xFFFFFFFF) in the "
              "guest rectangle-list index buffer is not qualified this "
              "cycle";
          return false;
        }
      }
    }
    const std::uint32_t primitive_count = draw.index_count / 3u;
    host_indices.reserve(static_cast<std::size_t>(primitive_count) * 5u);
    for (std::uint32_t primitive = 0u; primitive < primitive_count;
         ++primitive) {
      if (primitive != 0u) {
        host_indices.push_back(0xFFFFFFFFu);
      }
      for (std::uint32_t corner = 0u; corner < 4u; ++corner) {
        host_indices.push_back((primitive << 2u) | corner);
      }
    }
    restart_found = true;
    run_firsts.push_back(0u);
    for (std::size_t entry = 0u; entry < host_indices.size(); ++entry) {
      if (host_indices[entry] == 0xFFFFFFFFu) {
        run_firsts.push_back(static_cast<std::uint32_t>(entry));
      }
    }
    run_firsts.push_back(static_cast<std::uint32_t>(host_indices.size()));
  } else if (indexed_draw && draw.index_format == 0u) {
    // 16-bit guest indices: big-endian words, 2 bytes each, at the guest
    // byte address. Expanded to 32-bit for the host index buffer.
    if (draw.index_address % 4u != 0u) {
      error_ = "pinned draw: unaligned 16-bit index buffer (bounded: "
               "4-byte aligned)";
      return false;
    }
    const std::uint64_t byte_offset = draw.index_address;
    const std::uint64_t needed = draw.index_count * 2u;
    if (shared_memory_mapped_ == nullptr ||
        byte_offset + needed > shared_memory_dwords_ * 4u) {
      error_ = "pinned draw: index buffer outside shared memory";
      return false;
    }
    host_indices.reserve(draw.index_count);
    run_firsts.push_back(0u);
    for (std::uint32_t index = 0u; index < draw.index_count; ++index) {
      const std::uint32_t high =
          shared_memory_mapped_[byte_offset + index * 2u];
      const std::uint32_t low =
          shared_memory_mapped_[byte_offset + index * 2u + 1u];
      if (high == 0xFFu && low == 0xFFu) {
        restart_found = true;
        run_firsts.push_back(static_cast<std::uint32_t>(host_indices.size()));
        continue;
      }
      host_indices.push_back((high << 8u) | low);
    }
    run_firsts.push_back(static_cast<std::uint32_t>(host_indices.size()));
  } else if (indexed_draw && draw.index_format == 1u) {
    const std::uint64_t byte_offset = draw.index_address;
    const std::uint64_t needed = draw.index_count * 4u;
    if (shared_memory_mapped_ == nullptr ||
        byte_offset + needed > shared_memory_dwords_ * 4u) {
      error_ = "pinned draw: index buffer outside shared memory";
      return false;
    }
    // The 32-bit values are read big-endian; without a restart the
    // r262 in-shader load path is used (host_indices stays empty). With
    // a restart, the values are expanded to the host index buffer and
    // split into runs.
    bool any_restart = false;
    for (std::uint32_t index = 0u; index < draw.index_count; ++index) {
      const std::uint8_t* word =
          shared_memory_mapped_ + byte_offset + index * 4u;
      if (word[0] == 0xFFu && word[1] == 0xFFu && word[2] == 0xFFu &&
          word[3] == 0xFFu) {
        any_restart = true;
        break;
      }
    }
    if (any_restart) {
      restart_found = true;
      host_indices.reserve(draw.index_count);
      run_firsts.push_back(0u);
      for (std::uint32_t index = 0u; index < draw.index_count; ++index) {
        const std::uint8_t* word =
            shared_memory_mapped_ + byte_offset + index * 4u;
        if (word[0] == 0xFFu && word[1] == 0xFFu && word[2] == 0xFFu &&
            word[3] == 0xFFu) {
          run_firsts.push_back(
              static_cast<std::uint32_t>(host_indices.size()));
          continue;
        }
        host_indices.push_back((static_cast<std::uint32_t>(word[0]) << 24u) |
                               (static_cast<std::uint32_t>(word[1]) << 16u) |
                               (static_cast<std::uint32_t>(word[2]) << 8u) |
                               static_cast<std::uint32_t>(word[3]));
      }
      run_firsts.push_back(static_cast<std::uint32_t>(host_indices.size()));
    }
  }
  // The run start offsets are cumulative position markers; convert them
  // to (first, count) pairs with the next marker as the run end.
  if (!run_firsts.empty()) {
    std::vector<std::uint32_t> firsts;
    std::vector<std::uint32_t> counts;
    for (std::size_t run = 0u; run + 1u < run_firsts.size(); ++run) {
      const std::uint32_t first = run_firsts[run];
      const std::uint32_t end = run_firsts[run + 1u];
      if (end > first) {
        firsts.push_back(first);
        counts.push_back(end - first);
      }
    }
    run_firsts = std::move(firsts);
    run_counts = std::move(counts);
  }
  const bool use_host_index_buffer =
      (indexed_draw &&
       (draw.index_format == 0u || restart_found)) ||
      rect_strip_expand;
  const auto vertex = state.active_shader(0u);
  const auto pixel = state.active_shader(1u);
  if (vertex.empty() || pixel.empty()) {
    error_ = "pinned draw: no staged active vertex/pixel shader";
    return false;
  }
  // r260 selective variants: match by the state-derived high dword. The
  // pixel early-Z hint falls back to kNoModifiers when no variant carries
  // the hint (e.g. implicit early-Z write is not permitted for this ucode).
  // r271: the PsParamGen bits (dword1 bit 8 enable + bits 9:12
  // interpolator) are shader-static (the oracle derives them from the
  // ucode analysis), so the state cannot decide them: the lookup also
  // tries the param-gen-enabled candidates (enable, interpolator 0, not a
  // point draw) for both depth modes. The exact hash+type+modification
  // match keeps this unambiguous.
  // r472: the two state-exact candidates (with the real depth-hint bits
  // from register state, ParamGen tried both off/on since it is
  // shader-static and unknown here) are tried BEFORE the two
  // depth-hint-dropped fallback candidates -- dropping the depth hint is
  // meant to be a last resort ("falls back ... when no variant carries the
  // hint", per the comment above), not preferred over an available
  // ParamGen variant that still carries the correct hint. The prior
  // ordering tried the ParamGen+no-hint candidate before the
  // ParamGen+hint candidate, so once a real ParamGen+no-hint pinned
  // variant existed for a digest (r471's 320-entry merge), it won by loop
  // order even when a ParamGen+hint variant was also pinned for the same
  // digest and correctly matched the draw's real register state.
  const std::uint64_t pixel_high = derive_pixel_modification_high(state);
  const std::uint64_t pixel_no_depth = pixel_high & ~(0x7ull << 46u);
  const std::uint64_t kParamGenEnable = 0x1ull << 40u;  // dword1 bit 8
  const std::uint64_t pixel_candidates[4] = {
      pixel_high,                          // exact depth hint, no ParamGen
      pixel_high | kParamGenEnable,        // exact depth hint, ParamGen
      pixel_no_depth,                      // fallback: depth hint dropped
      pixel_no_depth | kParamGenEnable,    // fallback: depth hint dropped, ParamGen
  };
  ShaderTranslation pixel_translation;
  for (const std::uint64_t candidate : pixel_candidates) {
    pixel_translation = ShaderTranslator::translate_ucode_variant(
        1u, 0u, pixel, candidate);
    if (pixel_translation.ok()) break;
  }
  const std::uint64_t vertex_high =
      rect_strip_expand ? (9ull << 32u) : 0ull;  // kRectangleListAsTriangleStrip
  const ShaderTranslation vertex_translation =
      ShaderTranslator::translate_ucode_variant(0u, 0u, vertex, vertex_high);
  if (!vertex_translation.ok() || !pixel_translation.ok()) {
    if (std::getenv("AC6_NATIVE_VD_TRACE") != nullptr) {
      const std::uint64_t vdig = ShaderTranslator::digest_words(vertex);
      const std::uint64_t pdig = ShaderTranslator::digest_words(pixel);
      std::fprintf(stderr,
                    "r478 probe: primitive_type=0x%02x rect_strip_expand=%d "
                    "vertex_digest=%016llx vertex_mod=%016llx "
                    "vertex_ok=%d pixel_digest=%016llx pixel_mod_tried="
                    "{%016llx,%016llx,%016llx,%016llx} pixel_ok=%d\n",
                    draw.primitive_type, rect_strip_expand ? 1 : 0,
                    static_cast<unsigned long long>(vdig),
                    static_cast<unsigned long long>(vertex_high),
                    vertex_translation.ok() ? 1 : 0,
                    static_cast<unsigned long long>(pdig),
                    static_cast<unsigned long long>(pixel_candidates[0]),
                    static_cast<unsigned long long>(pixel_candidates[1]),
                    static_cast<unsigned long long>(pixel_candidates[2]),
                    static_cast<unsigned long long>(pixel_candidates[3]),
                    pixel_translation.ok() ? 1 : 0);
    }
    error_ = "pinned draw: active shader is not pinned: " +
             (vertex_translation.ok() ? pixel_translation.error
                                      : vertex_translation.error);
    return false;
  }
  last_pixel_modification_ = pixel_translation.modification;
  if (!spirv_uses_only_supported_resources(vertex_translation.spirv) ||
      !spirv_uses_only_supported_resources(pixel_translation.spirv)) {
    error_ = "pinned draw: shader uses resources outside this cycle's contract";
    return false;
  }
  // Pixel-texture bindings (r258): parsed from the pinned SPIR-V, then made
  // resident from the guest fetch constant + guest bytes.
  std::vector<PixelTextureBinding> texture_bindings;
  if (!parse_pixel_texture_bindings(pixel_translation.spirv,
                                    texture_bindings)) {
    error_ =
        "pinned draw: pixel-texture bindings are outside the bounded "
        "set-3 contract";
    return false;
  }
  if (!vertex_translation.spirv.empty() && vertex_translation.spirv.size() > 0u) {
    std::vector<PixelTextureBinding> vertex_texture_bindings;
    if (!parse_pixel_texture_bindings(vertex_translation.spirv,
                                      vertex_texture_bindings) ||
        (!vertex_texture_bindings.empty() &&
         spirv_uses_image_opcodes(vertex_translation.spirv))) {
      error_ = "pinned draw: vertex textures are not qualified this cycle";
      return false;
    }
  }
  std::uint64_t texture_layout_signature = 0u;
  if (!texture_bindings.empty()) {
    if (!ensure_pixel_texture_layout(texture_bindings,
                                     texture_layout_signature)) {
      return false;
    }
    if (!ensure_pixel_textures(state, texture_bindings,
                               texture_layout_signature)) {
      return false;
    }
  }
  bool topology_ok = true;
  (void)topology_of(draw.primitive_type, topology_ok);
  if (!topology_ok) {
    error_ = "pinned draw: primitive topology not qualified this cycle";
    return false;
  }
  // Register-driven EDRAM render target: derive, then make sure the backing
  // surface covers the tile-pitched grid the target addresses.
  EdramRenderTarget rt{};
  if (!derive_edram_render_target(state, rt)) {
    return false;
  }
  if (!ensure_edram_surface(rt)) {
    return false;
  }

  // Constant blocks: the system block is derived from XenosState registers
  // (bounded to what the pinned shaders read); everything else is raw guest
  // register words, exactly the oracle runtime's fill contract.
  std::uint32_t* system = reinterpret_cast<std::uint32_t*>(constant_mapped_);
  // Raw guest register words; the pinned shaders byte-swap on read exactly
  // like the oracle runtime does. No value interpretation happens here.
  std::uint32_t* float_vertex = reinterpret_cast<std::uint32_t*>(
      constant_mapped_ + kSystemConstantsBytes);
  std::uint32_t* float_pixel = reinterpret_cast<std::uint32_t*>(
      constant_mapped_ + kSystemConstantsBytes + kFloatConstantsBytes);
  std::uint32_t* bool_loop =
      reinterpret_cast<std::uint32_t*>(constant_mapped_ + kSystemConstantsBytes +
                                       2u * kFloatConstantsBytes);
  // The mapped fill pointers must use the SAME aligned offsets the
  // descriptor blocks were bound at (block_offsets, not plain arithmetic).
  std::uint32_t* fetch = reinterpret_cast<std::uint32_t*>(
      constant_mapped_ + constant_block_offsets_[4]);
  std::memset(system, 0, kSystemConstantsBytes);
  std::memset(float_vertex, 0, kFloatConstantsBytes);
  std::memset(float_pixel, 0, kFloatConstantsBytes);
  std::memset(bool_loop, 0, kBoolLoopBytes);
  std::memset(fetch, 0, kFetchBytes);

  std::uint32_t flags = 0u;
  const std::uint32_t vte_cntl = state.register_value(kRegPaClVteCntl);

  // NDC scale/offset (system members 10 @124 and 12 @140): the oracle's
  // GetHostViewportInfo derivation (xenia-project/xenia@master,
  // src/xenia/gpu/draw_util.cc:170-410), draw_resolution_scale is 1 and
  // half_pixel_offset defaults to true in this engine.
  float ndc_scale[3] = {0.0f, 0.0f, 0.0f};
  float ndc_offset[3] = {0.0f, 0.0f, 0.0f};
  {
    const std::uint32_t clip_cntl = state.register_value(kRegPaClClipCntl);
    // The viewport registers are float bit patterns; bit-cast, never
    // integer-convert.
    auto reg_to_float = [](std::uint32_t bits) {
      float value = 0.0f;
      std::memcpy(&value, &bits, sizeof(value));
      return value;
    };
    auto scale_or = [&](std::uint32_t ena, std::uint32_t reg) {
      return ena ? reg_to_float(state.register_value(reg)) : 1.0f;
    };
    auto offset_or = [&](std::uint32_t ena, std::uint32_t reg) {
      return ena ? reg_to_float(state.register_value(reg)) : 0.0f;
    };
    float scale_xy[2] = {
        static_cast<float>(scale_or(vte_cntl & 1u, kRegPaClVportXScale)),
        static_cast<float>(
            scale_or((vte_cntl >> 2u) & 1u, kRegPaClVportXScale + 2u))};
    float offset_base_xy[2] = {
        static_cast<float>(
            offset_or((vte_cntl >> 1u) & 1u, kRegPaClVportXScale + 1u)),
        static_cast<float>(
            offset_or((vte_cntl >> 3u) & 1u, kRegPaClVportXScale + 3u))};
    float offset_add_xy[2] = {0.0f, 0.0f};
    const std::uint32_t sc_mode = state.register_value(kRegPaSuScModeCntl);
    if ((sc_mode >> 16u) & 1u) {  // vtx_window_offset_enable
      std::uint32_t window = state.register_value(kRegPaScWindowOffset);
      std::int32_t window_x =
          static_cast<std::int16_t>(window & 0xFFFFu);
      std::int32_t window_y =
          static_cast<std::int16_t>((window >> 16u) & 0xFFFFu);
      offset_add_xy[0] += static_cast<float>(window_x);
      offset_add_xy[1] += static_cast<float>(window_y);
    }
    // half_pixel_offset (oracle default true) for D3D-style pixel centers.
    const std::uint32_t vtx_cntl = state.register_value(kRegPaSuVtxCntl);
    if ((vtx_cntl & 1u) == 0u) {  // pix_center == kD3DZero
      offset_add_xy[0] += 0.5f;
      offset_add_xy[1] += 0.5f;
    }
    if ((clip_cntl >> 16u) & 1u) {
      // r462: clip_disable branch (draw_util.cc:362-386). Screen-space
      // draws (most commonly clears) skip hardware clipping and the
      // vertex shader outputs pixel coordinates directly; a huge fixed
      // extent remaps those pixel coordinates to NDC instead of the
      // render-target-sized extent the clip-enabled branch below uses.
      // Xenia's Vulkan caller (vulkan_command_processor.cc:2440-2444)
      // passes the HOST DEVICE's VkPhysicalDeviceLimits::
      // maxViewportDimensions as x_max/y_max (draw_resolution_scale is 1,
      // so no further scaling) -- NOT the render target's own pixel size
      // -- capped at xenos::kTexture2DCubeMaxWidthHeight = 1u<<13 = 8192
      // (xenos.h:1139-1141). The actual viewport/scissor
      // (vkCmdSetViewport/Scissor below) are already set to the
      // scissor-derived render target region regardless of this branch,
      // matching Xenia's "use a viewport at least as large as the
      // scissor region" requirement without any change there.
      VkPhysicalDeviceProperties device_props{};
      vkGetPhysicalDeviceProperties(device_->physical_device(), &device_props);
      const float extent_xy[2] = {
          static_cast<float>(std::min<std::uint32_t>(
              8192u, device_props.limits.maxViewportDimensions[0])),
          static_cast<float>(std::min<std::uint32_t>(
              8192u, device_props.limits.maxViewportDimensions[1]))};
      for (std::uint32_t axis = 0u; axis < 2u; ++axis) {
        const float pixels_to_ndc = 2.0f / extent_xy[axis];
        ndc_scale[axis] = scale_xy[axis] * pixels_to_ndc;
        ndc_offset[axis] = (offset_base_xy[axis] - extent_xy[axis] * 0.5f +
                            offset_add_xy[axis]) *
                           pixels_to_ndc;
      }
      ndc_scale[2] = static_cast<float>(
          scale_or((vte_cntl >> 4u) & 1u, kRegPaClVportXScale + 4u));
      ndc_offset[2] = static_cast<float>(
          offset_or((vte_cntl >> 5u) & 1u, kRegPaClVportXScale + 5u));
    } else {
      const float xy_max[2] = {static_cast<float>(target.width()),
                               static_cast<float>(target.height())};
      for (std::uint32_t axis = 0u; axis < 2u; ++axis) {
        const float offset_axis = offset_base_xy[axis] + offset_add_xy[axis];
        const float scale_axis = scale_xy[axis];
        const float scale_axis_abs =
            scale_axis < 0.0f ? -scale_axis : scale_axis;
        const auto clampf = [](float value, float low, float high) {
          return value < low ? low : (value > high ? high : value);
        };
        const std::uint32_t axis_0_int = static_cast<std::uint32_t>(
            clampf(offset_axis - scale_axis_abs, 0.0f, xy_max[axis]));
        const std::uint32_t axis_1_int = static_cast<std::uint32_t>(
            clampf(offset_axis + scale_axis_abs, 0.0f, xy_max[axis]));
        const std::uint32_t axis_extent_int = axis_1_int - axis_0_int;
        if (axis_extent_int != 0u) {
          const float axis_extent = static_cast<float>(axis_extent_int);
          ndc_scale[axis] = scale_axis * 2.0f / axis_extent;
          ndc_offset[axis] =
              (offset_axis -
               (static_cast<float>(axis_0_int) + axis_extent * 0.5f)) *
              2.0f / axis_extent;
        } else {
          ndc_scale[axis] = 1.0f;
          ndc_offset[axis] = 0.0f;
        }
      }
      if (((clip_cntl >> 19u) & 1u) == 0u) {  // not dx_clip_space_def
        ndc_scale[2] = 0.5f;
        ndc_offset[2] = 0.5f;
      } else {
        ndc_scale[2] = static_cast<float>(
            scale_or((vte_cntl >> 4u) & 1u, kRegPaClVportXScale + 4u));
        ndc_offset[2] = static_cast<float>(
            offset_or((vte_cntl >> 5u) & 1u, kRegPaClVportXScale + 5u));
      }
    }
  }
  if ((vte_cntl >> 8u) & 1u) flags |= kSysFlagXyDividedByW;
  if ((vte_cntl >> 9u) & 1u) flags |= kSysFlagZDividedByW;
  if ((vte_cntl >> 10u) & 1u) flags |= kSysFlagWNotReciprocal;
  if (is_polygonal_primitive(draw.primitive_type)) {
    flags |= kSysFlagPrimitivePolygonal;
  }
  if (is_line_primitive(draw.primitive_type)) {
    flags |= kSysFlagPrimitiveLine;
  }
  // Alpha compare: enabled pass-if bits from RB_COLORCONTROL, otherwise the
  // kAlways encoding the pinned shaders branch on (bits 16:18 == 7).
  const std::uint32_t color_control = state.register_value(kRegRbColorControl);
  const std::uint32_t alpha_function =
      ((color_control >> 3u) & 1u) ? (color_control & 0x7u)
                                   : kCompareFunctionAlways;
  flags |= alpha_function << kAlphaPassIfLessShift;
  std::memcpy(system + (kSysOffsetNdcScale / 4u), ndc_scale,
              sizeof(ndc_scale));
  std::memcpy(system + (kSysOffsetNdcOffset / 4u), ndc_offset,
              sizeof(ndc_offset));
  if (indexed_draw && draw.index_format == 1u && !restart_found) {
    // 32-bit guest indices without a restart: the pinned VS loads the
    // index dword itself from shared memory (one big-endian dword per
    // index, k8in32 - the same endianness convention as the vertex data,
    // r256-qualified). With a restart the runs are host-drawn instead.
    flags |= kSysFlagVertexIndexLoad;
    system[kSysOffsetVertexIndexLoadAddress / 4u] = draw.index_address;
    system[kSysOffsetVertexIndexEndian / 4u] = 2u;  // k8in32
  }
  if (rect_strip_expand) {
    // The pinned VS derives guest_index = (host>>2)*3 + min(host&3, 2)
    // and, for DMA draws, loads the real guest index from the guest index
    // buffer at member 1 + mapped_index * size (k8in32 big-endian words).
    if (indexed_draw) {
      flags |= kSysFlagComputeOrPrimitiveVertexIndexLoad |
               kSysFlagComputeOrPrimitiveVertexIndexLoad32Bit;
      system[kSysOffsetVertexIndexLoadAddress / 4u] = draw.index_address;
      system[kSysOffsetVertexIndexEndian / 4u] = 2u;  // k8in32
    }
  }
  // Line-loop closing index: never matches a real index (the pinned VS
  // forces the index to 0 when equal).
  system[kSysOffsetLineLoopClosingIndex / 4u] = 0xFFFFFFFFu;
  system[kSysOffsetFlags / 4u] = flags;
  system[kSysOffsetVertexBaseIndex / 4u] = state.register_value(kRegVgtIndxOffset);
  system[kSysOffsetVertexIndexMin / 4u] = state.register_value(kRegVgtMinVtxIndx);
  system[kSysOffsetVertexIndexMax / 4u] = state.register_value(kRegVgtMaxVtxIndx);

  // Raw guest register words (the decoded logical dwords the guest wrote;
  // the shaders byte-swap exactly like the oracle runtime does).
  // Vertex float bank c0..c255 at 0x4000 (1024 dwords), pixel bank c0..c255
  // at 0x4400 (XE_GPU_REG_SHADER_CONSTANT_256_X) - the oracle uploads each
  // stage's own bank into its own constant buffer.
  // Float constants: the oracle COMPACTS each stage's used constants (per
  // the analyzed constant-register map carried by the pinned entry) into
  // its own buffer; the pinned SPIR-V indexes that compact layout. Dynamic
  // float addressing keeps the raw bank order.
  constexpr std::uint32_t kFloatBankDwords = 256u * 4u;
  const auto fill_float_constants = [&](std::uint32_t* target,
                                        std::uint32_t bank_base,
                                        const ShaderTranslation& stage) {
    const auto& map = stage.constant_map;
    if (!stage.has_constant_map || map.float_dynamic != 0u ||
        map.float_count == 0u) {
      for (std::uint32_t dword = 0u; dword < kFloatBankDwords; ++dword) {
        target[dword] = state.register_value(bank_base + dword);
      }
      return;
    }
    std::uint32_t packed = 0u;
    for (std::uint32_t block = 0u; block < 4u; ++block) {
      std::uint64_t bits = map.float_bitmap[block];
      while (bits != 0u) {
        const std::uint32_t bit =
            static_cast<std::uint32_t>(__builtin_ctzll(bits));
        bits &= bits - 1u;
        const std::uint32_t constant = block * 64u + bit;
        for (std::uint32_t component = 0u; component < 4u; ++component) {
          target[packed * 4u + component] =
              state.register_value(bank_base + constant * 4u + component);
        }
        ++packed;
      }
    }
    for (std::uint32_t dword = packed * 4u; dword < kFloatBankDwords; ++dword) {
      target[dword] = 0u;
    }
  };
  fill_float_constants(float_vertex, kRegShaderConstant000X, vertex_translation);
  fill_float_constants(float_pixel, kRegShaderConstant256X, pixel_translation);
  for (std::uint32_t dword = 0u; dword < 8u; ++dword) {
    bool_loop[dword] = state.register_value(kRegShaderConstantBool + dword);
  }
  for (std::uint32_t dword = 0u; dword < 32u; ++dword) {
    bool_loop[8u + dword] =
        state.register_value(kRegShaderConstantLoop0 + dword);
  }
  for (std::uint32_t dword = 0u; dword < kFetchConstantDwords; ++dword) {
    fetch[dword] = state.register_value(kRegShaderConstantFetch000 + dword);
  }

  // Point/rectangle-expansion diameters (system members 11/13/14/15 at
  // offsets 136/152/156/164; the SPIR-V member offsets of the pinned
  // variants): the oracle derives point_vertex_diameter_min/max from
  // PA_SU_POINT_MINMAX and point_constant_diameter from PA_SU_POINT_SIZE
  // (raw 12.4 fixed-point sizes * 2/16 px), and
  // point_screen_diameter_to_ndc_radius from the viewport extent
  // (1 / width, 1 / height at draw resolution 1).
  {
    const std::uint32_t point_minmax =
        state.register_value(kRegPaSuPointMinmax);
    const std::uint32_t point_size =
        state.register_value(kRegPaSuPointSize);
    const auto to_float_bits = [](float value) {
      std::uint32_t bits = 0u;
      std::memcpy(&bits, &value, sizeof(bits));
      return bits;
    };
    system[136u / 4u] = to_float_bits(
        static_cast<float>(point_minmax & 0xFFFFu) * (2.0f / 16.0f));
    system[152u / 4u] = to_float_bits(
        static_cast<float>((point_minmax >> 16u) & 0xFFFFu) *
        (2.0f / 16.0f));
    system[156u / 4u] = to_float_bits(
        static_cast<float>(point_size & 0xFFFFu) * (2.0f / 16.0f));
    system[160u / 4u] = to_float_bits(
        static_cast<float>((point_size >> 16u) & 0xFFFFu) * (2.0f / 16.0f));
    const float ndc_radius_x =
        1.0f / static_cast<float>(rt.width > 0u ? rt.width : 1u);
    const float ndc_radius_y =
        1.0f / static_cast<float>(rt.height > 0u ? rt.height : 1u);
    system[164u / 4u] = to_float_bits(ndc_radius_x);
    system[168u / 4u] = to_float_bits(ndc_radius_y);
  }

  // Color expansion bias (system member 23 @288): exponent from
  // RB_COLOR_INFO bits 20:25, scale = 2^exp (the oracle derives the same
  // float from 127+exp bits; zero register = unbiased target = 1.0f).
  {
    const std::uint32_t color_info = state.register_value(kRegRbColorInfo);
    std::uint32_t exp_field = (color_info >> 20u) & 0x3Fu;
    if (exp_field & 0x20u) exp_field |= 0xFFFFFFC0u;  // sign-extend 6 bits
    const std::uint32_t bits = (127u + exp_field) << 23u;
    std::memcpy(system + (kSysOffsetColorExpBias / 4u), &bits,
                sizeof(bits));
  }

    VkShaderModule vertex_module = VK_NULL_HANDLE;
  VkShaderModule pixel_module = VK_NULL_HANDLE;
  if (!ensure_shader_module(0u, vertex, vertex_translation.spirv,
                            vertex_module) ||
      !ensure_shader_module(1u, pixel, pixel_translation.spirv, pixel_module)) {
    return false;
  }
  const std::uint64_t vertex_digest = ShaderTranslator::digest_words(vertex);
  const std::uint64_t pixel_digest = ShaderTranslator::digest_words(pixel);
  VkDescriptorSetLayout texture_layout = set_layout_empty_;
  for (std::uint32_t layout_index = 0u;
       layout_index < pixel_texture_layout_count_; ++layout_index) {
    if (pixel_texture_layouts_[layout_index].signature ==
        texture_layout_signature) {
      texture_layout = pixel_texture_layouts_[layout_index].layout;
      break;
    }
  }
  VkPipeline pipeline = VK_NULL_HANDLE;
  if (!ensure_pipeline(draw.primitive_type, edram_image_format(rt.format),
                       vertex_module, pixel_module, vertex_digest,
                       pixel_digest, texture_layout_signature, texture_layout,
                       rt.sample_count, rt.color_mask, pipeline)) {
    return false;
  }

    VkCommandBufferAllocateInfo alloc{};
  alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc.commandPool = pool_;
  alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc.commandBufferCount = 1u;
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device_->device(), &alloc, &commands) !=
      VK_SUCCESS) {
    error_ = "pinned draw: command buffer allocation failed";
    return false;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if (vkBeginCommandBuffer(commands, &begin) != VK_SUCCESS) {
    error_ = "pinned draw: command buffer begin failed";
    return false;
  }
  if (use_host_index_buffer && !host_indices.empty()) {
    // Upload the expanded host index buffer (host-visible coherent
    // memory; the host writes before submission are visible to the draw
    // without a barrier).
    const VkDeviceSize needed =
        static_cast<VkDeviceSize>(host_indices.size()) * 4u;
    if (index_buffer_size_ < needed) {
      VkDevice dev = device_->device();
      if (index_buffer_mapped_ != nullptr) {
        vkUnmapMemory(dev, index_buffer_memory_);
        index_buffer_mapped_ = nullptr;
      }
      if (index_buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, index_buffer_, nullptr);
        index_buffer_ = VK_NULL_HANDLE;
      }
      if (index_buffer_memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(dev, index_buffer_memory_, nullptr);
        index_buffer_memory_ = VK_NULL_HANDLE;
      }
      VkBufferCreateInfo buffer_info{};
      buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer_info.size = needed;
      buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      if (vkCreateBuffer(dev, &buffer_info, nullptr, &index_buffer_) !=
          VK_SUCCESS) {
        error_ = "pinned draw: index buffer creation failed";
        return false;
      }
      VkMemoryRequirements requirements{};
      vkGetBufferMemoryRequirements(dev, index_buffer_, &requirements);
      VkPhysicalDeviceMemoryProperties props{};
      vkGetPhysicalDeviceMemoryProperties(device_->physical_device(), &props);
      std::uint32_t memory_type = 0xFFFFFFFFu;
      for (std::uint32_t type = 0u; type < props.memoryTypeCount; ++type) {
        const VkMemoryType& memory = props.memoryTypes[type];
        if ((requirements.memoryTypeBits & (1u << type)) != 0u &&
            (memory.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) !=
                0u &&
            (memory.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) !=
                0u) {
          memory_type = type;
          break;
        }
      }
      if (memory_type == 0xFFFFFFFFu) {
        error_ = "pinned draw: no host-visible index memory type";
        return false;
      }
      VkMemoryAllocateInfo alloc_info{};
      alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alloc_info.allocationSize = requirements.size;
      alloc_info.memoryTypeIndex = memory_type;
      if (vkAllocateMemory(dev, &alloc_info, nullptr,
                           &index_buffer_memory_) != VK_SUCCESS ||
          vkBindBufferMemory(dev, index_buffer_, index_buffer_memory_, 0) !=
              VK_SUCCESS ||
          vkMapMemory(dev, index_buffer_memory_, 0, needed, 0,
                      reinterpret_cast<void**>(&index_buffer_mapped_)) !=
              VK_SUCCESS) {
        error_ = "pinned draw: index buffer allocation failed";
        return false;
      }
      index_buffer_size_ = needed;
    }
    std::memcpy(index_buffer_mapped_, host_indices.data(),
                static_cast<std::size_t>(needed));
  }
    record_texture_upload(commands);
  // First draw on a (re)configured target clears the EDRAM surface;
  // subsequent draws on the same target load (EDRAM persists between draws
  // until the registers reconfigure it).
    const bool clear_surface =
      !edram_rt_valid_ || !(rt == edram_active_rt_);
  VkClearValue clear_value{};
  clear_value.color = {{0.0f, 0.0f, 0.0f, 0.0f}};
  const VkFormat surface_format = edram_image_format(rt.format);
  VkRenderPass record_clear_pass = VK_NULL_HANDLE;
  VkRenderPass record_load_pass = VK_NULL_HANDLE;
  if (!ensure_edram_passes(surface_format, rt.sample_count, record_clear_pass,
                           record_load_pass)) {
    return false;
  }
  VkRenderPassBeginInfo pass_begin{};
  pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  pass_begin.renderPass =
      clear_surface ? record_clear_pass : record_load_pass;
  pass_begin.framebuffer = edram_framebuffer_;
  pass_begin.renderArea.offset.x =
      static_cast<std::int32_t>(rt.origin_x);
  pass_begin.renderArea.offset.y =
      static_cast<std::int32_t>(rt.origin_y);
  pass_begin.renderArea.extent.width = rt.width;
  pass_begin.renderArea.extent.height = rt.height;
  pass_begin.clearValueCount = clear_surface ? 1u : 0u;
  pass_begin.pClearValues = &clear_value;
    vkCmdBeginRenderPass(commands, &pass_begin, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  VkDescriptorSet pixel_texture_bound_set = descriptor_set_empty_;
  if (!texture_bindings.empty()) {
    for (std::uint32_t layout_index = 0u;
         layout_index < pixel_texture_layout_count_; ++layout_index) {
      if (pixel_texture_layouts_[layout_index].signature ==
          texture_layout_signature) {
        pixel_texture_bound_set = pixel_texture_layouts_[layout_index].set;
        break;
      }
    }
  }
  VkDescriptorSet sets[4] = {descriptor_set_shared_, descriptor_set_constants_,
                             descriptor_set_empty_, pixel_texture_bound_set};
  VkPipelineLayout bound_layout = VK_NULL_HANDLE;
  for (std::uint32_t layout_index = 0u; layout_index < pipeline_layout_count_;
       ++layout_index) {
    if (pipeline_layouts_[layout_index].signature ==
        texture_layout_signature) {
      bound_layout = pipeline_layouts_[layout_index].layout;
      break;
    }
  }
  vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bound_layout, 0u, 4u, sets, 0u, nullptr);
  const VkViewport viewport{
      static_cast<float>(rt.origin_x), static_cast<float>(rt.origin_y),
      static_cast<float>(rt.width), static_cast<float>(rt.height), 0.0f, 1.0f};
  const VkRect2D scissor{
      {static_cast<std::int32_t>(rt.origin_x),
       static_cast<std::int32_t>(rt.origin_y)},
      {rt.width, rt.height}};
  vkCmdSetViewport(commands, 0u, 1u, &viewport);
  vkCmdSetScissor(commands, 0u, 1u, &scissor);
  if (use_host_index_buffer && !host_indices.empty()) {
    // Each run after a restart is its own indexed draw: no bridging
    // primitive is rendered. Without restarts the single run covers the
    // whole stream (r262 behavior).
    vkCmdBindIndexBuffer(commands, index_buffer_, 0,
                         VK_INDEX_TYPE_UINT32);
    for (std::size_t run = 0u; run < run_firsts.size(); ++run) {
      vkCmdDrawIndexed(commands, run_counts[run], 1u, run_firsts[run], 0u,
                       0u);
    }
  } else {
    vkCmdDraw(commands, draw.vertex_count, 1u, 0u, 0u);
  }
  vkCmdEndRenderPass(commands);
  edram_rt_valid_ = true;
  edram_active_rt_ = rt;
  if (vkEndCommandBuffer(commands) != VK_SUCCESS) {
    error_ = "pinned draw: command buffer end failed";
    return false;
  }
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1u;
  submit_info.pCommandBuffers = &commands;
  vkResetFences(device_->device(), 1u, &fence_);
  if (vkQueueSubmit(device_->queue(), 1u, &submit_info, fence_) != VK_SUCCESS) {
    error_ = "pinned draw: queue submit failed";
    return false;
  }
  if (vkWaitForFences(device_->device(), 1u, &fence_, VK_TRUE,
                      UINT64_MAX) != VK_SUCCESS) {
    error_ = "pinned draw: fence wait failed";
    return false;
  }
  ++draw_count_;
  return true;
}

bool PinnedShaderRuntime::execute_frame(
    VulkanOffscreenTarget& target, const XenosState& state,
    std::span<const XenosCommand> commands) noexcept {
  error_.clear();
  if (!valid()) {
    error_ = "pinned runtime: not initialized";
    return false;
  }
  if (!target.valid() || target.width() == 0u || target.height() == 0u) {
    error_ = "pinned runtime: invalid offscreen target";
    return false;
  }
  for (const XenosCommand& command : commands) {
    const bool handled = std::visit(
        [&](const auto& packet) -> bool {
          using Packet = std::decay_t<decltype(packet)>;
          if constexpr (std::is_same_v<Packet, DrawPacket>) {
            if (packet.vertex_count == 0u || packet.index_count == 0u ||
                !valid_primitive(packet.primitive_type)) {
              error_ = "pinned frame: DRAW packet has no drawable vertices";
              return false;
            }
            return draw_pinned(target, state, packet);
          } else if constexpr (std::is_same_v<Packet, PresentPacket>) {
            if (packet.surface >= 16u || packet.width == 0u ||
                packet.height == 0u) {
              error_ = "pinned frame: PRESENT packet references invalid surface";
              return false;
            }
            // XE_SWAP resolves the active EDRAM render-target surface into
            // the readable image (the Xenos present boundary). The bounded
            // gate requires the swap dimensions to match the surface region.
            if (edram_rt_valid_) {
              const EdramRenderTarget& rt = edram_active_rt_;
              if (rt.width != packet.width || rt.height != packet.height) {
                error_ =
                    "pinned frame: swap dimensions differ from the EDRAM "
                    "render target (not qualified this cycle)";
                return false;
              }
              if (!resolve_edram_to_target(target, rt)) {
                return false;
              }
              ++resolve_count_;
            }
            ++present_count_;
            return true;
          } else {
            // Staged by the decoder (IM_LOAD_IMMEDIATE) or carry no execution
            // this cycle (validated like submit()).
            return true;
          }
        },
        command);
    if (!handled) return false;
  }
  return true;
}

}  // namespace ac6::native
