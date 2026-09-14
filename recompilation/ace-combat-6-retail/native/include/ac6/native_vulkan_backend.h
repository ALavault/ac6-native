#pragma once

#include "ac6/native_vulkan_device.h"
#include "ac6/native_xenos.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac6::native {

class VulkanOffscreenTarget;

// Capability contract for the product Vulkan backend.  Hardware-facing Vulkan
// object creation lives behind this narrow boundary; unsupported Xenos state
// is rejected instead of silently mapped to an arbitrary host default.
struct VulkanCapabilities final {
  bool edram_aliasing{true};
  bool msaa{true};
  bool resolves{true};
  bool texture_2d{true};
  bool texture_3d{true};
  bool texture_cube{true};
  bool depth_stencil{true};
  bool blend_scissor{true};
  bool primitive_restart{true};
};

class VulkanBackend final {
 public:
  explicit VulkanBackend(VulkanCapabilities capabilities = {}) noexcept
      : capabilities_(capabilities) {}

  [[nodiscard]] bool submit(const XenosState& state,
                            std::span<const XenosCommand> commands);
  // Real present path: validates the packet with the same rules as submit(),
  // requires matching offscreen dimensions, then executes a GPU clear with
  // the caller-provided color. submit() behavior is unchanged.
  [[nodiscard]] bool present_to_offscreen(VulkanOffscreenTarget& target,
                                          const PresentPacket& packet,
                                          float red, float green, float blue,
                                          float alpha);
  [[nodiscard]] const std::string& error() const noexcept { return error_; }
  [[nodiscard]] std::uint64_t present_count() const noexcept {
    return present_count_;
  }
  [[nodiscard]] std::uint64_t draw_count() const noexcept { return draw_count_; }
  [[nodiscard]] std::uint64_t resolve_count() const noexcept {
    return resolve_count_;
  }

 private:
  VulkanCapabilities capabilities_;
  std::string error_;
  std::uint64_t present_count_{};
  std::uint64_t draw_count_{};
  std::uint64_t resolve_count_{};
  std::uint64_t last_pixel_modification_{};
};

// Execute decoded command streams with the pinned-registry shaders (r256):
// staged active VS/PS are resolved through ShaderTranslator::translate_ucode
// (byte-exact qualified fetches only), built into real Vulkan pipelines, and
// drawn into the offscreen target. The descriptor/pipeline layout contract is
// the one the oracle's own SpirvShaderTranslator emits (set 0 binding 0:
// XeSharedMemory SSBO of raw guest bytes; set 1: XeSystemConstants @0,
// XeFloatConstants vertex @1 / pixel @2, XeBoolLoopConstants @3,
// XeFetchConstants @4 - raw guest register words). Draw state (System
// constants) is derived from XenosState registers following the oracle's
// UpdateSystemConstants derivation for the fields the pinned shaders read;
// every constant block is raw guest register words elsewhere. Anything not
// covered (texture sets 2/3, push constants, memexport) fails closed.
// r523: untiles a 2D tiled DXT4_5 (BC3, 16-byte blocks) surface into
// linear block-row-major bytes, applying the fetch endianness per
// CopySwapBlock semantics (0 = none, 1 = 8-in-16 pairs, 2 = 8-in-32
// words). Tiled math per the cited Xenos 2D rules (32x32-block tiles,
// GetTiledOffset2D). Fail-closed on zero/oversize dims, pitch below
// width or off the 32-block grid, other endianness, and short source.
// Free function (no device) so unit tests cover it without a runtime.
bool untile_tiled_dxt45_2d(const std::uint8_t* tiled_bytes,
                           std::uint64_t tiled_size,
                           std::uint32_t width_blocks,
                           std::uint32_t height_blocks,
                           std::uint32_t pitch_blocks,
                           std::uint32_t endianness,
                           std::vector<std::uint8_t>& linear_out) noexcept;

// r524: block geometry + conservative tiled footprint for a 2D BC3
// surface (width/height texels, pitch pixels): blocks are 4x4 texels,
// footprint upper-bounds the tile grid covering pitch x height
// (16 KiB per 32x32-block tile, tile-row-major per the cited rules).
// Fail-closed on the same bounds untile enforces. No device needed.
bool tiled_dxt45_layout(std::uint32_t width_pixels, std::uint32_t height_pixels,
                        std::uint32_t pitch_pixels, std::uint32_t& width_blocks,
                        std::uint32_t& height_blocks,
                        std::uint32_t& pitch_blocks,
                        std::uint64_t& linear_bytes,
                        std::uint64_t& footprint_bytes) noexcept;

// Untiles a 2D tiled DXT1 (BC1, 8-byte blocks) surface into linear
// block-row-major bytes.  Same tiling rules as DXT4_5 but with
// bytes_per_block_log2 = 3 (8 KiB per 32x32-block macro tile).
bool untile_tiled_dxt1_2d(const std::uint8_t* tiled_bytes,
                          std::uint64_t tiled_size,
                          std::uint32_t width_blocks,
                          std::uint32_t height_blocks,
                          std::uint32_t pitch_blocks,
                          std::uint32_t endianness,
                          std::vector<std::uint8_t>& linear_out) noexcept;

bool tiled_dxt1_layout(std::uint32_t width_pixels, std::uint32_t height_pixels,
                       std::uint32_t pitch_pixels, std::uint32_t& width_blocks,
                       std::uint32_t& height_blocks,
                       std::uint32_t& pitch_blocks,
                       std::uint64_t& linear_bytes,
                       std::uint64_t& footprint_bytes) noexcept;

// r526: quad-list index expansion ([a,b,c,d] -> [a,b,c,a,c,d] per
// quad) as a single run: triangle lists have no bridging primitives,
// so no restart splitting. The words form takes host-order guest
// index values and refuses primitive restarts (fail-closed: a restart
// has no meaning inside one list draw); the auto form numbers verts
// sequentially. Both fail closed on zero/remainder/over-bound counts.
// Free functions (no device) so unit tests cover them directly.
bool expand_quad_list_auto(std::uint32_t index_count,
                           std::vector<std::uint32_t>& out) noexcept;
bool expand_quad_list_words(std::span<const std::uint32_t> guest_indices,
                            std::vector<std::uint32_t>& out) noexcept;

class PinnedShaderRuntime final {
 public:
  explicit PinnedShaderRuntime(const VulkanDevice& device) noexcept;
  PinnedShaderRuntime(const PinnedShaderRuntime&) = delete;
  PinnedShaderRuntime& operator=(const PinnedShaderRuntime&) = delete;
  ~PinnedShaderRuntime() noexcept;

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] const std::string& error() const noexcept { return error_; }

  // Copies RAW GUEST BYTES (big-endian, exactly as the guest wrote them) at
  // the given dword address of the shared-memory SSBO. The pinned shaders
  // byte-swap the fetched data themselves.
  bool write_shared_memory(std::uint64_t dword_address,
                           std::span<const std::uint8_t> bytes) noexcept;
  // Byte-addressed form for registered allocation ranges, which need not
  // start at a dword boundary. Returns false with error() on an unmapped
  // buffer or a range outside the SSBO; no bytes are copied on failure.
  bool write_shared_memory_bytes(std::uint64_t byte_address,
                                 std::span<const std::uint8_t> bytes) noexcept;
  [[nodiscard]] std::uint64_t shared_memory_dwords() const noexcept {
    return shared_memory_dwords_;
  }
  [[nodiscard]] std::uint64_t draw_count() const noexcept {
    return draw_count_;
  }
  [[nodiscard]] std::uint64_t present_count() const noexcept {
    return present_count_;
  }
  // Resolves completed by PRESENT packets (XE_SWAP) since construction: the
  // EDRAM render-target surface copied into the readable image.
  [[nodiscard]] std::uint64_t edram_resolves() const noexcept {
    return resolve_count_;
  }
  // The 64-bit SpirvShaderTranslator::Modification of the pixel-shader
  // variant the LAST successful draw selected (r260: state-driven selective
  // variants; 0 when nothing has drawn).
  [[nodiscard]] std::uint64_t last_pixel_modification() const noexcept {
    return last_pixel_modification_;
  }

  // Runs a decoded stream: DrawPacket executes the pinned draw into the
  // register-driven EDRAM render target (both active shaders must resolve
  // through the pinned registry), PresentPacket resolves the EDRAM surface
  // into the readable image, everything else is validated and skipped
  // (submit() semantics unchanged).
  bool execute_frame(VulkanOffscreenTarget& target, const XenosState& state,
                     std::span<const XenosCommand> commands) noexcept;

 private:
  struct ShaderObject final {
    VkShaderModule module{VK_NULL_HANDLE};
  };
  struct PipelineKey final {
    std::uint32_t primitive_type;
    std::uint32_t color_format;
    std::uint64_t vertex_shader_digest;
    std::uint64_t pixel_shader_digest;
    std::uint64_t texture_layout_signature;
    std::uint32_t sample_count{1u};
    std::uint32_t color_mask{0xFu};
    // r490 depth/stencil pipeline state: the VkPipelineDepthStencilState-
    // bearing subset. depth_format 0 = no depth attachment bound (guest
    // DepthRenderTargetFormat 1 when bound). The packed stencil word is
    // ref | mask << 8 | write_mask << 16 from RB_STENCILREFMASK; op words
    // pack the guest compare/op values that map mechanically to Vulkan.
    std::uint32_t depth_format{0u};
    std::uint32_t depth_flags{0u};
    std::uint32_t depth_op_word{0u};
    std::uint32_t stencil_state{0u};
    bool operator==(const PipelineKey&) const noexcept = default;
  };
  struct PipelineKeyHash final {
    std::size_t operator()(const PipelineKey& key) const noexcept {
      std::size_t seed = std::hash<std::uint32_t>{}(key.primitive_type);
      seed ^= std::hash<std::uint32_t>{}(key.color_format) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint64_t>{}(key.vertex_shader_digest) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint64_t>{}(key.pixel_shader_digest) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint32_t>{}(key.sample_count) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint32_t>{}(key.color_mask) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint32_t>{}(key.depth_format) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint32_t>{}(key.depth_flags) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint32_t>{}(key.depth_op_word) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      seed ^= std::hash<std::uint32_t>{}(key.stencil_state) + 0x9e3779b97f4a7c15ull +
              (seed << 6) + (seed >> 2);
      return seed;
    }
  };

  bool ensure_shader_module(std::uint32_t shader_type,
                            std::span<const std::uint32_t> microcode,
                            const std::vector<std::uint32_t>& spirv,
                            VkShaderModule& module_out) noexcept;
  bool ensure_pipeline(std::uint32_t primitive_type, VkFormat color_format,
                       VkShaderModule vertex, VkShaderModule pixel,
                       std::uint64_t vs_digest, std::uint64_t ps_digest,
                       std::uint64_t texture_layout_signature,
                       VkDescriptorSetLayout texture_layout,
                       std::uint32_t sample_count, std::uint32_t color_mask,
                       VkFormat depth_format, std::uint32_t depth_flags,
                       std::uint32_t depth_op_word,
                       std::uint32_t stencil_state,
                       VkPipeline& pipeline_out) noexcept;
  // The register-driven EDRAM render target (r257): RB_MODECONTROL,
  // RB_SURFACE_INFO, RB_COLOR_INFO, RB_COLOR_MASK, RB_DEPTHCONTROL and the
  // screen scissor define the color surface the draw writes; everything
  // outside the qualified subset fails closed.
  struct EdramRenderTarget final {
    std::uint32_t pitch_pixels{};
    std::uint32_t base_tiles{};
    std::uint32_t format{};
    std::uint32_t origin_x{};
    std::uint32_t origin_y{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t image_width{};
    std::uint32_t image_height{};
    // RB_SURFACE_INFO bits 16:17 (r459): 1, 2 or 4. The tile pitch follows
    // the oracle's GetSurfacePitchTiles (r490): the guest pitch is in pixels
    // and shifts by one sample bit at 4x MSAA (2x unchanged), so no
    // host-side sample-count guess remains. Included in operator== so a
    // sample-count change alone invalidates the cached EDRAM surface and
    // forces a fresh clear.
    std::uint32_t sample_count{1u};
    // RB_COLOR_MASK bits 0:3 (r460): R=bit0, G=bit1, B=bit2, A=bit3.
    // Translated directly to VkPipelineColorBlendAttachmentState's
    // colorWriteMask -- a mechanical bit-order-preserving mapping, unlike
    // sample_count's EDRAM tile geometry question.
    std::uint32_t color_mask{0xFu};
    // r490 depth surface (RB_DEPTH_INFO 0x2002 + RB_DEPTHCONTROL): base
    // tiles share the color surface's tile-pitched mapping; the guest depth
    // format is 0 (D24S8) or 1 (D24FS8, fail-closed this cycle). Included
    // in operator== for the depth clear policy. The color clear policy
    // separately ignores depth and channel-mask changes (r572).
    std::uint32_t depth_base_tiles{};
    std::uint32_t depth_format{};
    std::uint32_t depth_origin_x{};
    std::uint32_t depth_origin_y{};
    // r491: the depth region clamps to the depth surface's own image
    // bounds (the same per-surface clamp real hardware applies to a
    // nominally larger scissor), which may be smaller than the color
    // region when the depth base sits low in the tile grid.
    std::uint32_t depth_width{};
    std::uint32_t depth_height{};
    bool depth_enable{false};
    bool operator==(const EdramRenderTarget&) const noexcept = default;
  };
  bool derive_edram_render_target(const XenosState& state,
                                  EdramRenderTarget& rt) noexcept;
  bool ensure_edram_surface(const EdramRenderTarget& rt) noexcept;
  // The EDRAM color formats map to distinct Vulkan formats (the oracle's
  // GetColorVulkanFormat mapping); float formats are not render-pass
  // compatible with the UNORM target, so the clear/load pass pair is
  // cached per format (r268).
  static VkFormat edram_image_format(std::uint32_t color_format) noexcept;
  // r490: the guest depth format (0 = D24S8, 1 = D24FS8) maps to a host
  // depth-stencil attachment format; VK_FORMAT_UNDEFINED when no depth
  // surface is bound. D24FS8 stores 20e4 [0,2) floats on the guest; the
  // host attachment keeps plain fp32 depth, which is unobservable while
  // every qualified draw's zfunc is ALWAYS (values are never read) and is
  // a recorded divergence for any later depth-reading configuration.
  static VkFormat edram_depth_image_format(std::uint32_t depth_format) noexcept;
  bool ensure_edram_passes(VkFormat format, std::uint32_t sample_count,
                           VkFormat depth_format, VkRenderPass& clear_out,
                           VkRenderPass& load_out) noexcept;
  // The depth-only pass pair (r491): color LOAD/store untouched, depth
  // CLEAR (first draw on the target) or LOAD. Used when the depth base
  // differs from the color base, so depth content lands at the depth
  // surface's own tile origin.
  bool ensure_edram_depth_passes(VkFormat format, std::uint32_t sample_count,
                                 VkFormat depth_format,
                                 VkRenderPass& clear_out,
                                 VkRenderPass& load_out) noexcept;
  // region_width/height clamp the resolved EDRAM window: draws use the
  // target's own region (r490), PRESENT uses the swap dimensions at the
  // surface origin.
  bool resolve_edram_to_target(VulkanOffscreenTarget& target,
                               const EdramRenderTarget& rt,
                               std::uint32_t region_width,
                               std::uint32_t region_height) noexcept;
  // Diagnostic-only (r490, env-gated): read back the resolved present
  // target and report the non-black pixel count and bounding box, so a
  // black capture distinguishes "no pixels drawn" from "pixels drawn
  // outside the resolved window".
  void probe_present_pixels(VulkanOffscreenTarget& target,
                            std::uint64_t present_index) noexcept;
  // Diagnostic-only (r492, env-gated): resolve the multisample EDRAM image
  // into its 1x companion (or read the 1x image directly) and report the
  // non-black pixel count and bounding box across the whole tile-pitched
  // image, so a black draw can be traced to its actual write location.
  void probe_edram_image(const EdramRenderTarget& rt) noexcept;
  // Diagnostic-only (r492, env-gated): read back the depth surface's
  // stencil (and depth) content after a depth-enabled draw. A non-zero
  // stencil byte (guest zpass REPLACE) or non-default depth value proves
  // fragments were actually rasterized, splitting "no fragments" from
  // "fragments with black pixel-shader output".
  void probe_depth_surface(const EdramRenderTarget& rt) noexcept;
  // Staging buffer for the EDRAM image probe; allocated on first use.
  VkBuffer probe_buffer_{VK_NULL_HANDLE};
  VkDeviceMemory probe_memory_{VK_NULL_HANDLE};
  std::uint8_t* probe_mapped_{nullptr};
  VkDeviceSize probe_size_{};
  // The EDRAM surface is created lazily per tile pitch (see
  // ensure_edram_surface); draws render into its register-driven region.
  bool draw_pinned(VulkanOffscreenTarget& target, const XenosState& state,
                   const DrawPacket& draw) noexcept;
  // r260: the state-driven high dword of the pixel modification (depth
  // stencil mode from the alpha state; the interpolator mask low dword is
  // taken from the matched variant).
  [[nodiscard]] std::uint64_t derive_pixel_modification_high(
      const XenosState& state) const noexcept;
  // r260: derives the runtime modification from the draw state (bounded
  // subset of the oracle's GetCurrentVertex/PixelShaderModification) and
  // resolves both pinned variants; the early-Z hint falls back to the
  // no-modifier variant when the shader does not permit it.
  [[nodiscard]] std::uint64_t derive_pixel_modification(
      const XenosState& state,
      std::span<const std::uint32_t> pixel_spirv) const noexcept;
  [[nodiscard]] std::uint64_t derive_vertex_modification(
      std::span<const std::uint32_t> vertex_spirv) const noexcept;
  static std::uint32_t spirv_interpolator_mask(
      std::span<const std::uint32_t> spirv, const char* prefix) noexcept;
  // SPIR-V structural gate: set 0/1 resources are the fixed contract; set 3
  // (pixel textures) is allowed and validated by
  // parse_pixel_texture_bindings; set 2 (vertex textures) and everything
  // else is refused rather than executed with a guessed layout.
  static bool spirv_uses_only_supported_resources(
      std::span<const std::uint32_t> spirv) noexcept;
  // Bounded pixel-texture contract (r258): parses set 3 variables out of a
  // pinned pixel shader's SPIR-V. Only 2D-array float sampled images and
  // samplers are accepted, with bindings images-first then samplers; the
  // fetch constant index comes from the translator's stable variable names
  // ("xe_textureN_*", "xe_samplerN_*").
  struct PixelTextureBinding final {
    std::uint32_t binding{};
    std::uint32_t fetch_constant{};
    bool is_sampler{};
    // The SPIR-V image type for image bindings: dim 1 (2D array, the
    // translator's normalized form) or dim 3 (a real cube image, the r273
    // payload's cube fetches). Samplers carry the value of their texture.
    bool cube_image{};
  };
  // True when the module contains any sampled-image / fetch / read /
  // write operation (bounded opcode scan; declared-but-unused texture
  // resources in pinned VS variants are ignored).
  static bool spirv_uses_image_opcodes(
      std::span<const std::uint32_t> spirv) noexcept;
  static bool parse_pixel_texture_bindings(
      std::span<const std::uint32_t> spirv,
      std::vector<PixelTextureBinding>& bindings_out) noexcept;
  // Decodes the guest texture fetch constant (registers 0x4800 + 6*index)
  // and validates it against the bounded subset (kTexture, linear,
  // k_8_8_8_8, unsigned signs, 2D, single mip, point/linear filters;
  // plus tiled DXT4_5/BC3 and tiled DXT1/BC1, unpacked,
  // identity-swizzled, 2D).
  struct PixelTexture final {
    std::uint32_t fetch_constant{};
    std::uint32_t width{};
    std::uint32_t height{};
    // 1 = 2D, 6 = cube faces stacked sequentially (r270).
    std::uint32_t layer_count{1u};
    std::uint32_t pitch_pixels{};
    std::uint32_t byte_address{};
    std::uint32_t endianness{};
    VkFilter filter{};
    VkSamplerAddressMode address_x{};
    VkSamplerAddressMode address_y{};
    // r523: tiled DXT4_5 (BC3) class. Decoded (dimensions, address,
    // sampler state) but not yet uploaded — the upload path still
    // fails closed on it with a named message.
    bool tiled_dxt45{false};
    bool tiled_dxt1{false};
  };
  bool decode_pixel_texture(const XenosState& state,
                            std::uint32_t fetch_constant,
                            PixelTexture& texture) noexcept;
  // Makes the texture resources ready for the parsed bindings: one shared
  // sampled-image view per fetch constant (bound to both the _u and _s
  // image slots; the shader only reads _s when the sign constants say so),
  // samplers from the fetch, descriptor set 3 uploaded from guest bytes.
  bool ensure_pixel_textures(
      const XenosState& state,
      const std::vector<PixelTextureBinding>& bindings,
      std::uint64_t texture_layout_signature) noexcept;
  // Records the pending texture upload (barriers + staging copy) at the
  // head of the given command buffer; the draw that samples the texture is
  // submitted in the same batch.
  void record_texture_upload(VkCommandBuffer commands) noexcept;

  const VulkanDevice* device_{nullptr};
  std::string error_;
  VkCommandPool pool_{VK_NULL_HANDLE};
  VkBuffer shared_memory_{VK_NULL_HANDLE};
  VkDeviceMemory shared_memory_device_{VK_NULL_HANDLE};
  std::uint8_t* shared_memory_mapped_{nullptr};
  std::uint64_t shared_memory_dwords_{};
  VkBuffer constant_buffers_[5]{};
  VkDeviceSize constant_block_offsets_[5]{};
  VkDeviceMemory constant_memory_{VK_NULL_HANDLE};
  std::uint8_t* constant_mapped_{nullptr};
  VkDescriptorSetLayout set_layout_shared_{VK_NULL_HANDLE};
  VkDescriptorSetLayout set_layout_constants_{VK_NULL_HANDLE};
  // Per-signature pipeline layouts (set 3 = the shader's pixel-texture
  // layout; the other sets are fixed).
  struct PipelineLayoutEntry final {
    std::uint64_t signature{};
    VkPipelineLayout layout{VK_NULL_HANDLE};
  };
  static constexpr std::uint32_t kMaxPipelineLayouts = 8u;
  PipelineLayoutEntry pipeline_layouts_[kMaxPipelineLayouts]{};
  std::uint32_t pipeline_layout_count_{};
  VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_shared_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_constants_{VK_NULL_HANDLE};
  VkFence fence_{VK_NULL_HANDLE};
  // Pixel-texture resources (r258): one VkImage + view + sampler + staging
  // buffer reused per draw; descriptor set 3 laid out from the parsed
  // bindings, set 2 kept as an empty layout (no vertex textures this cycle).
  VkDescriptorSetLayout set_layout_empty_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_empty_{VK_NULL_HANDLE};
  // Per-pixel-shader set-3 layouts (r273): the pinned shaders place their
  // image and sampler bindings at DIFFERENT binding numbers per shader
  // (the hoisted-gradients payload: images 0..11 + samplers 12..19; the
  // earlier payloads: images 0..3 + samplers 4..5), so one fixed superset
  // layout cannot serve both without a descriptor-type collision. Each
  // distinct (binding, type) signature gets its own layout and set.
  struct PixelTextureLayoutEntry final {
    std::uint64_t signature{};
    VkDescriptorSetLayout layout{VK_NULL_HANDLE};
    VkDescriptorSet set{VK_NULL_HANDLE};
  };
  static constexpr std::uint32_t kMaxPixelTextureLayouts = 8u;
  PixelTextureLayoutEntry pixel_texture_layouts_[kMaxPixelTextureLayouts]{};
  std::uint32_t pixel_texture_layout_count_{};
  bool ensure_pixel_texture_layout(
      const std::vector<PixelTextureBinding>& bindings,
      std::uint64_t& signature_out) noexcept;
  std::uint32_t pixel_texture_layout_image_count_{};
  std::uint32_t pixel_texture_layout_sampler_count_{};
  VkBuffer texture_staging_{VK_NULL_HANDLE};
  VkDeviceMemory texture_staging_memory_{VK_NULL_HANDLE};
  std::uint8_t* texture_staging_mapped_{nullptr};
  VkDeviceSize texture_staging_size_{};
  // Host-visible index buffer for 16-bit guest indices (expanded to 32-bit
  // little-endian values; 32-bit guest indices use the in-shader manual
  // load path instead).
  VkBuffer index_buffer_{VK_NULL_HANDLE};
  VkDeviceMemory index_buffer_memory_{VK_NULL_HANDLE};
  std::uint8_t* index_buffer_mapped_{nullptr};
  VkDeviceSize index_buffer_size_{};
  // Pixel-texture slots (r264): one per qualified fetch constant (the
  // fixed set-3 superset exposes 8 image bindings, so at most 8 distinct
  // textures per draw).
  struct PixelTextureSlot final {
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t layers{1u};
    bool cube_view{false};
    // r524: BC3/BC1 vs RGBA8 image/view format; a slot is recreated when
    // the format class changes.
    bool bc3{false};
    bool bc1{false};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    bool valid{false};
    bool upload_needed{false};
    std::uint32_t upload_width{};
    std::uint32_t upload_height{};
    VkDeviceSize staging_offset{};
  };
  static constexpr std::uint32_t kMaxPixelTextures = 8u;
  struct PendingUploadInfo final {
    std::uint32_t slot{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t layers{1u};
    VkDeviceSize offset{};
    VkDeviceSize size{};
  };
  PixelTextureSlot texture_slots_[kMaxPixelTextures];
  PendingUploadInfo pending_uploads_[kMaxPixelTextures];
  std::uint32_t pending_upload_count_{};
  // EDRAM backing surface (r257): one color image whose linear layout
  // represents the 2048 EDRAM tiles at the current render-target pitch; the
  // tile-pitched mapping is (base_tile % pitch_tiles) * 80 samples wide,
  // (base_tile / pitch_tiles) * 16 samples tall.
  VkImage edram_image_{VK_NULL_HANDLE};
  VkDeviceMemory edram_memory_{VK_NULL_HANDLE};
  VkImageView edram_view_{VK_NULL_HANDLE};
  // MSAA resolve target (r459): only created when the active surface's
  // sample_count > 1. edram_image_ becomes the multisample color attachment
  // in that case, and the render pass's automatic subpass resolve writes the
  // single-sample result here; resolve_edram_to_target reads from this image
  // instead of edram_image_ whenever it is valid. At sample_count == 1 this
  // stays VK_NULL_HANDLE and the surface is exactly the pre-r459 single-image
  // path (no behavior change for the already-verified 1x case).
  VkImage edram_resolve_image_{VK_NULL_HANDLE};
  VkDeviceMemory edram_resolve_memory_{VK_NULL_HANDLE};
  VkImageView edram_resolve_view_{VK_NULL_HANDLE};
  // r490 depth surface: separate image/view in the same tile-pitched EDRAM
  // geometry, attached to the render pass only when the draw's
  // RB_DEPTHCONTROL enables depth or stencil.
  VkImage edram_depth_image_{VK_NULL_HANDLE};
  VkDeviceMemory edram_depth_memory_{VK_NULL_HANDLE};
  VkImageView edram_depth_view_{VK_NULL_HANDLE};
  VkFramebuffer edram_framebuffer_{VK_NULL_HANDLE};
  // r491: color-only framebuffer for the color pass of a two-pass draw
  // (depth base differs from the color base); the combined framebuffer is
  // used by the depth-only pass.
  VkFramebuffer edram_color_framebuffer_{VK_NULL_HANDLE};
  struct EdramPasses final {
    VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
    std::uint32_t samples{1u};
    VkFormat depth_format{VK_FORMAT_UNDEFINED};
    VkRenderPass clear{VK_NULL_HANDLE};
    VkRenderPass load{VK_NULL_HANDLE};
    VkRenderPass depth_clear{VK_NULL_HANDLE};
    VkRenderPass depth_load{VK_NULL_HANDLE};
  };
  EdramPasses edram_passes_[8]{};
  std::uint32_t edram_pass_count_{0u};
  EdramRenderTarget edram_surface_dims_{};
  EdramRenderTarget edram_active_rt_{};
  bool edram_rt_valid_{false};
  std::uint64_t resolve_count_{};
  std::uint64_t last_pixel_modification_{};
  // r570: correlate at most four sampled textured draws with their next swap.
  std::uint64_t r570_pending_present_draw_{};
  // r571/r573: only the first sampled texture-to-swap interval, at most 128 draws.
  std::uint64_t r571_chain_start_draw_{};
  bool r571_chain_closed_{};
  std::unordered_map<std::uint64_t, VkShaderModule> shader_modules_[2];
  std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> pipelines_;
  std::uint64_t present_count_{};
  std::uint64_t draw_count_{};
  std::vector<std::uint32_t> scratch_;
  // Draw state derived per draw (documented bounded subset of the oracle's
  // UpdateSystemConstants; fields the pinned shaders read).
  std::vector<std::uint32_t> system_constants_;
};

}  // namespace ac6::native
