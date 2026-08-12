#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ac6 {

enum class VulkanBackendError : std::uint8_t {
  none,
  instance_creation_failed,
  physical_device_unavailable,
  device_creation_failed,
  command_pool_creation_failed,
};

struct RenderDeviceCaps {
  std::uint32_t api_version{};
  std::uint32_t vendor_id{};
  std::uint32_t device_id{};
  std::uint32_t max_image_dimension_2d{};
  std::uint32_t max_color_sample_count{1};
  float max_sampler_anisotropy{1.0F};
  bool discrete_gpu{};
  bool depth_d32{};
  bool sampler_anisotropy{};
  bool color_rgba8_unorm{};
  bool color_bgra8_unorm{};
  bool sampled_rgba8_unorm{};
  // Surface-specific modes remain false until a swapchain query qualifies
  // them; the headless transport never invents presentation support.
  bool presentation_fifo{};
  bool presentation_mailbox{};
  std::string device_name;
};

struct VulkanVertex {
  float x{};
  float y{};
};

// Explicitly separate the UV-bearing mesh ABI from the position-only scene
// transport.  Retail textures are admitted only through the texture upload
// contract below; no guest-memory pointer crosses this boundary.
struct VulkanTexturedVertex {
  float x{};
  float y{};
  float u{};
  float v{};
};

// World-space position/UV transport. Unlike VulkanClipTexturedVertex this ABI
// preserves authored XYZ. The renderer composes DrawPacket's object-to-world
// transform with an explicit renderer camera and pushes the resulting 64-byte
// object-to-clip matrix to the vertex shader. Retail TCAM qualification is a
// separate boundary.
struct VulkanWorldTexturedVertex {
  float x{};
  float y{};
  float z{};
  float u{};
  float v{};
};

// Clip-space layout recovered as a generic transport mechanism.  It is kept
// separate from the 2D position/UV layout so a caller cannot accidentally
// submit world-space coordinates without an explicitly qualified projection.
struct VulkanClipTexturedVertex {
  float x{};
  float y{};
  float z{};
  float w{1.0F};
  float u{};
  float v{};
};

struct VulkanMeshHandle {
  std::uint64_t value{};
  [[nodiscard]] explicit operator bool() const noexcept { return value != 0U; }
  friend bool operator==(VulkanMeshHandle, VulkanMeshHandle) = default;
};

struct VulkanTexturedMeshHandle {
  std::uint64_t value{};
  [[nodiscard]] explicit operator bool() const noexcept { return value != 0U; }
  friend bool operator==(VulkanTexturedMeshHandle,
                         VulkanTexturedMeshHandle) = default;
};

struct VulkanWorldTexturedMeshHandle {
  std::uint64_t value{};
  [[nodiscard]] explicit operator bool() const noexcept { return value != 0U; }
  friend bool operator==(VulkanWorldTexturedMeshHandle,
                         VulkanWorldTexturedMeshHandle) = default;
};

struct VulkanClipTexturedMeshHandle {
  std::uint64_t value{};
  [[nodiscard]] explicit operator bool() const noexcept { return value != 0U; }
  friend bool operator==(VulkanClipTexturedMeshHandle,
                         VulkanClipTexturedMeshHandle) = default;
};

struct VulkanTextureHandle {
  std::uint64_t value{};
  [[nodiscard]] explicit operator bool() const noexcept { return value != 0U; }
  friend bool operator==(VulkanTextureHandle, VulkanTextureHandle) = default;
};

struct VulkanRenderTargetHandle {
  std::uint64_t value{};
  [[nodiscard]] explicit operator bool() const noexcept { return value != 0U; }
  friend bool operator==(VulkanRenderTargetHandle, VulkanRenderTargetHandle) = default;
};

struct VulkanPipelineHandle {
  std::uint64_t value{};
  [[nodiscard]] explicit operator bool() const noexcept { return value != 0U; }
  friend bool operator==(VulkanPipelineHandle, VulkanPipelineHandle) = default;
};

struct VulkanPipelineState {
  bool depth_test{};
  bool depth_write{};
  bool alpha_blend{};
};

struct VulkanBackendState;
struct VulkanBackendCreateResult;

// AC6-owned, renderer-neutral Vulkan transport. It owns no guest memory and
// accepts only explicit native resources and caller-supplied SPIR-V.
class VulkanBackend final {
 public:
  VulkanBackend(const VulkanBackend&) = delete;
  VulkanBackend& operator=(const VulkanBackend&) = delete;
  VulkanBackend(VulkanBackend&&) = delete;
  VulkanBackend& operator=(VulkanBackend&&) = delete;
  ~VulkanBackend();

  [[nodiscard]] static VulkanBackendCreateResult create();
  [[nodiscard]] const RenderDeviceCaps& caps() const noexcept;

  [[nodiscard]] VulkanMeshHandle create_mesh(
      std::span<const VulkanVertex> vertices,
      std::span<const std::uint16_t> indices) noexcept;
  void release_mesh(VulkanMeshHandle mesh) noexcept;
  [[nodiscard]] bool has_mesh(VulkanMeshHandle mesh) const noexcept;

  [[nodiscard]] VulkanTexturedMeshHandle create_textured_mesh(
      std::span<const VulkanTexturedVertex> vertices,
      std::span<const std::uint16_t> indices) noexcept;
  void release_textured_mesh(VulkanTexturedMeshHandle mesh) noexcept;
  [[nodiscard]] bool has_textured_mesh(VulkanTexturedMeshHandle mesh) const noexcept;

  [[nodiscard]] VulkanWorldTexturedMeshHandle create_world_textured_mesh(
      std::span<const VulkanWorldTexturedVertex> vertices,
      std::span<const std::uint16_t> indices) noexcept;
  void release_world_textured_mesh(VulkanWorldTexturedMeshHandle mesh) noexcept;
  [[nodiscard]] bool has_world_textured_mesh(
      VulkanWorldTexturedMeshHandle mesh) const noexcept;

  [[nodiscard]] VulkanClipTexturedMeshHandle create_clip_textured_mesh(
      std::span<const VulkanClipTexturedVertex> vertices,
      std::span<const std::uint16_t> indices) noexcept;
  void release_clip_textured_mesh(VulkanClipTexturedMeshHandle mesh) noexcept;
  [[nodiscard]] bool has_clip_textured_mesh(
      VulkanClipTexturedMeshHandle mesh) const noexcept;

  // Uploads a persistent RGBA8 texture.  The caller owns the source bytes;
  // the backend copies them into a device-local image before returning.
  [[nodiscard]] VulkanTextureHandle create_texture_rgba8(
      std::uint32_t width, std::uint32_t height,
      std::span<const std::uint8_t> rgba8) noexcept;
  void release_texture(VulkanTextureHandle texture) noexcept;
  [[nodiscard]] bool has_texture(VulkanTextureHandle texture) const noexcept;

  [[nodiscard]] VulkanRenderTargetHandle create_render_target(
      std::uint32_t width, std::uint32_t height, bool with_depth) noexcept;
  void release_render_target(VulkanRenderTargetHandle target) noexcept;
  [[nodiscard]] bool has_render_target(VulkanRenderTargetHandle target) const noexcept;
  [[nodiscard]] bool render_target_has_d32(
      VulkanRenderTargetHandle target) const noexcept;

  [[nodiscard]] VulkanPipelineHandle create_pipeline(
      VulkanRenderTargetHandle target,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      VulkanPipelineState state = {}) noexcept;
  [[nodiscard]] VulkanPipelineHandle create_textured_pipeline(
      VulkanRenderTargetHandle target,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      VulkanPipelineState state = {}) noexcept;
  [[nodiscard]] VulkanPipelineHandle create_clip_textured_pipeline(
      VulkanRenderTargetHandle target,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      VulkanPipelineState state = {}) noexcept;
  [[nodiscard]] VulkanPipelineHandle create_world_textured_pipeline(
      VulkanRenderTargetHandle target,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv,
      VulkanPipelineState state = {true, true, false}) noexcept;
  void release_pipeline(VulkanPipelineHandle pipeline) noexcept;
  [[nodiscard]] bool has_pipeline(VulkanPipelineHandle pipeline) const noexcept;

  [[nodiscard]] bool clear_render_target(
      VulkanRenderTargetHandle target, std::array<float, 4> color,
      float depth = 1.0F) noexcept;
  [[nodiscard]] bool draw_indexed(VulkanRenderTargetHandle target,
                                  VulkanPipelineHandle pipeline,
                                  VulkanMeshHandle mesh) noexcept;
  [[nodiscard]] bool draw_textured_indexed(
      VulkanRenderTargetHandle target, VulkanPipelineHandle pipeline,
      VulkanTexturedMeshHandle mesh, VulkanTextureHandle texture) noexcept;
  [[nodiscard]] bool draw_clip_textured_indexed(
      VulkanRenderTargetHandle target, VulkanPipelineHandle pipeline,
      VulkanClipTexturedMeshHandle mesh, VulkanTextureHandle texture) noexcept;
  [[nodiscard]] bool draw_world_textured_indexed(
      VulkanRenderTargetHandle target, VulkanPipelineHandle pipeline,
      VulkanWorldTexturedMeshHandle mesh, VulkanTextureHandle texture,
      const std::array<float, 16>& object_to_clip) noexcept;
  [[nodiscard]] std::vector<std::uint8_t> readback_rgba8(
      VulkanRenderTargetHandle target) noexcept;

  [[nodiscard]] std::size_t live_mesh_count() const noexcept;
  [[nodiscard]] std::size_t live_textured_mesh_count() const noexcept;
  [[nodiscard]] std::size_t live_world_textured_mesh_count() const noexcept;
  [[nodiscard]] std::size_t live_clip_textured_mesh_count() const noexcept;
  [[nodiscard]] std::size_t live_texture_count() const noexcept;
  [[nodiscard]] std::size_t live_render_target_count() const noexcept;
  [[nodiscard]] std::size_t live_pipeline_count() const noexcept;

 private:
  [[nodiscard]] VulkanPipelineHandle create_pipeline_impl(
      VulkanRenderTargetHandle target,
      std::span<const std::uint32_t> vertex_spirv,
      std::span<const std::uint32_t> fragment_spirv, VulkanPipelineState state,
      bool textured, bool clip_space, bool world_space) noexcept;
  explicit VulkanBackend(std::unique_ptr<VulkanBackendState> state) noexcept;
  std::unique_ptr<VulkanBackendState> state_;
};

struct VulkanBackendCreateResult {
  std::unique_ptr<VulkanBackend> backend;
  VulkanBackendError error{VulkanBackendError::none};
  [[nodiscard]] explicit operator bool() const noexcept {
    return backend != nullptr && error == VulkanBackendError::none;
  }
};

[[nodiscard]] const char* vulkan_backend_error_name(VulkanBackendError error) noexcept;

}  // namespace ac6
