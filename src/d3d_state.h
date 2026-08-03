#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace ac6::d3d {

// The PAL XEX render-target setter at 0x821DD220 and its reset loop expose
// exactly four color slots (0..3). Depth-stencil is the separate 0x30A0 field.
inline constexpr uint32_t kMaxRenderTargets = 4;
inline constexpr uint32_t kMaxTextures = 16;
inline constexpr uint32_t kMaxStreams = 16;
inline constexpr uint32_t kDeviceVertexDeclarationOffset = 0x2E24;
inline constexpr uint32_t kDeviceIndexBufferOffset = 0x308C;
// Structural stage cross-match in the PAL XEX: the +0x3190 object owns the
// vertex-specific +0x368 metadata block, while +0x318C owns the compact pixel
// shader block. Both are consumed by the common pre-draw at 0x821ED1D0.
inline constexpr uint32_t kDevicePixelShaderObjectOffset = 0x318C;
inline constexpr uint32_t kDeviceVertexShaderObjectOffset = 0x3190;
inline constexpr uint32_t kPixelShaderDescriptorOffsetField = 0x40;
inline constexpr uint32_t kPixelShaderProgramBaseField = 0x18;
inline constexpr uint32_t kPixelShaderProgramRelativeField = 0x28;
inline constexpr uint32_t kPixelShaderProgramSizeField = 0x2C;
inline constexpr uint32_t kVertexShaderProgramBaseField = 0x20;
inline constexpr uint32_t kVertexShaderVariantTableBase = 0x380;
inline constexpr uint32_t kVertexShaderVariantTableStride = 8;
inline constexpr uint32_t kVertexShaderProgramRelativeField = 0x368;
inline constexpr uint32_t kVertexShaderProgramSizeField = 0x36C;
inline constexpr uint32_t kCapturedVertexShaderVariantCount = 2;
inline constexpr uint32_t kDeviceStreamBufferBase = 0x30A4;
inline constexpr uint32_t kDeviceStreamStrideBase = 0x30E8;
inline constexpr uint32_t kDeviceStreamRemainingSizeBase = 0x077C;
inline constexpr uint32_t kMaxSamplers = 16;
inline constexpr uint32_t kMaxFetchConstants = 32;
inline constexpr uint32_t kMaxClearRectsPerRecord = 8;

inline constexpr uint32_t RecoverStreamOffset(uint32_t buffer_size,
                                              uint32_t remaining_size) noexcept {
    return buffer_size - remaining_size;
}

inline constexpr uint32_t RecoverStreamStride(uint8_t encoded_stride) noexcept {
    return static_cast<uint32_t>(encoded_stride) << 2;
}

static_assert(RecoverStreamOffset(0x1000u, 0x0FC0u) == 0x40u);
static_assert(RecoverStreamStride(0x0Du) == 0x34u);
static_assert(RecoverStreamStride(0x05u) == 0x14u);

// The replaceable chunk at 0x821E10C8 is entered after the PAL XEX instruction
// at 0x821E10B8 has changed r4 from the texture stage to device + stage. r3 is
// still the original device pointer at that boundary. Unsigned subtraction
// therefore recovers the original 32-bit stage even if the addition wrapped.
inline constexpr uint32_t RecoverTextureStageAt821E10C8(
    uint32_t device, uint32_t device_plus_stage) noexcept {
    return device_plus_stage - device;
}

static_assert(RecoverTextureStageAt821E10C8(0x82000000u, 0x82000000u) == 0u);
static_assert(RecoverTextureStageAt821E10C8(0x82000000u, 0x8200001Fu) == 31u);
static_assert(RecoverTextureStageAt821E10C8(0xFFFFFFF0u, 0x0000000Fu) == 31u);

// The replaceable chunks at 0x821DC538 and 0x821DC6C8 are entered after the
// PAL XEX has computed r8 = device + sampler. At both boundaries r4 has
// already been repurposed as a lookup-table byte offset, so it is not a
// sampler index. Recover the original 32-bit sampler from r8 and r3.
inline constexpr uint32_t RecoverSamplerAt821DC538(
    uint32_t device, uint32_t device_plus_sampler) noexcept {
    return device_plus_sampler - device;
}

inline constexpr uint32_t RecoverSamplerAt821DC6C8(
    uint32_t device, uint32_t device_plus_sampler) noexcept {
    return device_plus_sampler - device;
}

static_assert(RecoverSamplerAt821DC538(0x82000000u, 0x82000000u) == 0u);
static_assert(RecoverSamplerAt821DC538(0x82000000u, 0x8200000Fu) == 15u);
static_assert(RecoverSamplerAt821DC538(0xFFFFFFF8u, 0x00000007u) == 15u);
static_assert(RecoverSamplerAt821DC6C8(0x82000000u, 0x82000000u) == 0u);
static_assert(RecoverSamplerAt821DC6C8(0x82000000u, 0x8200000Fu) == 15u);
static_assert(RecoverSamplerAt821DC6C8(0xFFFFFFF8u, 0x00000007u) == 15u);

struct DrawStats {
    std::atomic<uint32_t> draw_calls{0};
    std::atomic<uint32_t> draw_calls_indexed{0};
    std::atomic<uint32_t> draw_calls_indexed_shared{0};
    std::atomic<uint32_t> draw_calls_primitive{0};
    std::atomic<uint64_t> total_indices{0};
    std::atomic<uint64_t> total_vertices{0};
    std::atomic<uint32_t> set_texture_calls{0};
    std::atomic<uint32_t> set_render_target_calls{0};
    std::atomic<uint32_t> set_depth_stencil_calls{0};
    std::atomic<uint32_t> set_vertex_decl_calls{0};
    std::atomic<uint32_t> set_index_buffer_calls{0};
    std::atomic<uint32_t> set_stream_source_calls{0};
    std::atomic<uint32_t> set_viewport_calls{0};
    std::atomic<uint32_t> set_sampler_state_calls{0};
    std::atomic<uint32_t> set_texture_fetch_calls{0};
    std::atomic<uint32_t> clear_calls{0};
    std::atomic<uint32_t> resolve_calls{0};

    void Reset() {
        draw_calls.store(0, std::memory_order_relaxed);
        draw_calls_indexed.store(0, std::memory_order_relaxed);
        draw_calls_indexed_shared.store(0, std::memory_order_relaxed);
        draw_calls_primitive.store(0, std::memory_order_relaxed);
        total_indices.store(0, std::memory_order_relaxed);
        total_vertices.store(0, std::memory_order_relaxed);
        set_texture_calls.store(0, std::memory_order_relaxed);
        set_render_target_calls.store(0, std::memory_order_relaxed);
        set_depth_stencil_calls.store(0, std::memory_order_relaxed);
        set_vertex_decl_calls.store(0, std::memory_order_relaxed);
        set_index_buffer_calls.store(0, std::memory_order_relaxed);
        set_stream_source_calls.store(0, std::memory_order_relaxed);
        set_viewport_calls.store(0, std::memory_order_relaxed);
        set_sampler_state_calls.store(0, std::memory_order_relaxed);
        set_texture_fetch_calls.store(0, std::memory_order_relaxed);
        clear_calls.store(0, std::memory_order_relaxed);
        resolve_calls.store(0, std::memory_order_relaxed);
    }
};

struct DrawStatsSnapshot {
    uint32_t draw_calls;
    uint32_t draw_calls_indexed;
    uint32_t draw_calls_indexed_shared;
    uint32_t draw_calls_primitive;
    uint64_t total_indices;
    uint64_t total_vertices;
    uint32_t set_texture_calls;
    uint32_t set_render_target_calls;
    uint32_t set_depth_stencil_calls;
    uint32_t set_vertex_decl_calls;
    uint32_t set_index_buffer_calls;
    uint32_t set_stream_source_calls;
    uint32_t set_viewport_calls;
    uint32_t set_sampler_state_calls;
    uint32_t set_texture_fetch_calls;
    uint32_t clear_calls;
    uint32_t resolve_calls;
};

enum class DrawCallKind : uint8_t {
    kIndexed,
    kIndexedShared,
    kPrimitive,
};

struct MaterialDrawIdentity {
    bool valid{false};
    uint32_t request{0};
    uint32_t material{0};
    uint32_t material_key{0};
    uint32_t draw_context_key{0};
    uint32_t device{0};
};

class MaterialDrawIdentityLatch {
  public:
    void Publish(MaterialDrawIdentity identity) noexcept { pending_ = identity; }

    bool ConsumeForDevice(uint32_t device, MaterialDrawIdentity* identity_out) noexcept {
        if (!pending_.valid || pending_.device != device || identity_out == nullptr) {
            return false;
        }
        *identity_out = pending_;
        pending_ = {};
        return true;
    }

    void Clear() noexcept { pending_ = {}; }

  private:
    MaterialDrawIdentity pending_{};
};

struct StreamBinding {
    uint32_t buffer{0};       // Guest address of D3DVertexBuffer
    uint32_t offset{0};       // Offset in bytes
    uint32_t stride{0};       // Vertex stride in bytes
};

struct SamplerBinding {
    uint32_t mag_filter{0};       // Requested D3DTEXTUREFILTERTYPE.
    uint32_t min_filter{0};       // Requested D3DTEXTUREFILTERTYPE.
    uint32_t mip_filter{0};       // No qualified capture hook yet.
    uint32_t border_color{0};     // No qualified capture hook yet.
    uint32_t aniso_bias_raw{0};   // Raw requested float bits before Xenos encoding.
    uint32_t lod_bias_raw{0};     // Raw requested float bits before Xenos encoding.
    uint32_t mip_min_level{0};    // Effective clamped Xenos fetch value.
    uint32_t mip_max_level{0};    // Effective clamped Xenos fetch value.
};

// Guest microcode location recovered from the PAL XEX shader descriptors.
// Size is the unshifted byte count read by the state compiler before it emits
// size >> 2 into the Xenos command packet. This is an address/extent identity,
// not a content hash or proof that a vertex variant was selected for a draw.
struct ShaderProgramReference {
    uint32_t guest_address{0};
    uint32_t size_bytes{0};
};

inline constexpr ShaderProgramReference RecoverShaderProgramReference(
    uint32_t program_base, uint32_t program_relative, uint32_t size_bytes) noexcept {
    return ShaderProgramReference{program_base + program_relative, size_bytes};
}

static_assert(RecoverShaderProgramReference(0x82000000u, 0x1234u, 0x80u).guest_address ==
              0x82001234u);
static_assert(RecoverShaderProgramReference(0xFFFFFFF0u, 0x20u, 0x40u).guest_address ==
              0x10u);

// All values are guest addresses into PPC address space unless noted.
struct ShadowState {
    uint32_t device{0};
    std::array<uint32_t, kMaxRenderTargets> render_targets{};
    uint32_t depth_stencil{0};
    std::array<uint32_t, kMaxTextures> textures{};
    uint32_t vertex_declaration{0};
    uint32_t index_buffer{0};
    uint32_t guest_vertex_shader{0};
    uint32_t guest_pixel_shader{0};
    ShaderProgramReference pixel_shader_program{};
    std::array<ShaderProgramReference, kCapturedVertexShaderVariantCount>
        vertex_shader_program_candidates{};
    std::array<StreamBinding, kMaxStreams> streams{};
    std::array<SamplerBinding, kMaxSamplers> samplers{};
    std::array<uint32_t, kMaxFetchConstants> texture_fetch_ptrs{};
    uint32_t shader_gpr_alloc{0};
    uint64_t vertex_fetch_layout_signature{0};
    uint64_t texture_fetch_layout_signature{0};
    uint64_t resource_binding_signature{0};

    struct {
        uint32_t x{0};
        uint32_t y{0};
        uint32_t width{0};
        uint32_t height{0};
    } viewport;
};

struct DrawCallRecord {
    uint32_t sequence{0};
    DrawCallKind kind{DrawCallKind::kIndexed};
    uint32_t primitive_type{0};
    uint32_t start{0};
    uint32_t count{0};
    uint32_t flags{0};
    MaterialDrawIdentity material_identity{};
    ShadowState shadow_state{};
};

struct ClearRect {
    uint32_t left{0};
    uint32_t top{0};
    uint32_t right{0};
    uint32_t bottom{0};
};

struct ClearRecord {
    uint32_t sequence{0};
    uint32_t rect_count{0};
    uint32_t captured_rect_count{0};
    uint32_t flags{0};
    uint32_t color{0};
    uint32_t stencil{0};
    float depth{1.0f};
    std::array<ClearRect, kMaxClearRectsPerRecord> rects{};
    ShadowState shadow_state{};
};

struct ResolveRecord {
    uint32_t sequence{0};
    std::array<uint32_t, 7> args{};
    float depth_or_scale{0.0f};
    ShadowState shadow_state{};
};

struct FrameCaptureSnapshot {
    uint64_t frame_index{0};
    DrawStatsSnapshot stats{};
    ShadowState frame_end_shadow{};
    std::vector<DrawCallRecord> draws;
    std::vector<ClearRecord> clears;
    std::vector<ResolveRecord> resolves;
};

struct FrameCaptureSummary {
    bool capture_enabled{false};
    bool record_signature_valid{false};
    uint64_t frame_index{0};
    uint64_t record_signature{0};
    uint32_t draw_count{0};
    uint32_t clear_count{0};
    uint32_t resolve_count{0};
    uint32_t indexed_draw_count{0};
    uint32_t indexed_shared_draw_count{0};
    uint32_t primitive_draw_count{0};
    uint32_t material_identity_draw_count{0};
    /// Per-frame guest D3D draw counters (same window as this capture; cleared each frame boundary).
    DrawStatsSnapshot frame_stats{};
    /// Histogram of `primitive_type` on all captured draws (D3DPRIMITIVETYPE: 1=point list … 6=fan).
    uint32_t topology_pointlist{0};
    uint32_t topology_linelist{0};
    uint32_t topology_linestrip{0};
    uint32_t topology_trianglelist{0};
    uint32_t topology_trianglestrip{0};
    uint32_t topology_trianglefan{0};
    uint32_t topology_other{0};
    uint32_t unique_rt0_count{0};
    uint32_t rt0_switch_count{0};
    uint32_t frame_end_render_target_count{0};
    uint32_t frame_end_texture_count{0};
    uint32_t frame_end_stream_count{0};
    uint32_t frame_end_sampler_count{0};
    uint32_t frame_end_texture_fetch_count{0};
    uint32_t frame_end_render_target_0{0};
    uint32_t frame_end_depth_stencil{0};
    uint32_t first_draw_render_target_0{0};
    uint32_t last_draw_render_target_0{0};
    uint32_t frame_end_viewport_width{0};
    uint32_t frame_end_viewport_height{0};
    uint32_t last_draw_primitive_type{0};
    uint32_t last_draw_count{0};
    uint32_t last_draw_flags{0};
};

}  // namespace ac6::d3d
