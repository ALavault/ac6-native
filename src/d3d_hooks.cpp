#include "d3d_hooks.h"

#include <algorithm>
#include <shared_mutex>
#include <unordered_set>

#include <rex/cvar.h>
#include <rex/memory.h>
#include <rex/logging.h>
#include <rex/ppc.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(ac6_d3d_trace, false, "AC6/Render",
                    "Log every D3D device state change and draw call");
REXCVAR_DEFINE_BOOL(ac6_render_capture, false, "AC6/Render",
                    "Capture per-frame draw, clear, and resolve records for the native renderer");

namespace {
const rex::LogCategoryId kLogGPU = rex::log::GPU;
}  // namespace

namespace {

std::shared_mutex g_shadow_mutex;
ac6::d3d::ShadowState g_shadow{};

ac6::d3d::DrawStats g_live_stats{};

std::shared_mutex g_snapshot_mutex;
ac6::d3d::DrawStatsSnapshot g_snapshot{};

std::shared_mutex g_capture_mutex;
ac6::d3d::FrameCaptureSnapshot g_capture_snapshot{};
ac6::d3d::FrameCaptureSummary g_capture_summary{};
uint64_t g_capture_live_frame_index = 1;
uint32_t g_capture_live_sequence = 0;
std::vector<ac6::d3d::DrawCallRecord> g_live_draws;
std::vector<ac6::d3d::ClearRecord> g_live_clears;
std::vector<ac6::d3d::ResolveRecord> g_live_resolves;
ac6::d3d::MaterialDrawIdentityLatch g_pending_material_identity;

template <typename Container>
uint32_t CountNonZero(const Container& values) {
    uint32_t count = 0;
    for (const auto& value : values) {
        if (value) {
            ++count;
        }
    }
    return count;
}

uint32_t CountNonZeroStreams(const std::array<ac6::d3d::StreamBinding, ac6::d3d::kMaxStreams>& streams) {
    uint32_t count = 0;
    for (const auto& stream : streams) {
        if (stream.buffer) {
            ++count;
        }
    }
    return count;
}

uint32_t CountNonZeroSamplers(
    const std::array<ac6::d3d::SamplerBinding, ac6::d3d::kMaxSamplers>& samplers) {
    uint32_t count = 0;
    for (const auto& sampler : samplers) {
        if (sampler.mag_filter || sampler.min_filter || sampler.mip_filter ||
            sampler.border_color || sampler.aniso_bias_raw || sampler.lod_bias_raw ||
            sampler.mip_min_level || sampler.mip_max_level) {
            ++count;
        }
    }
    return count;
}

void HashU32(uint64_t& hash, uint32_t value) {
    constexpr uint64_t kFnvPrime = 1099511628211ull;
    hash ^= value;
    hash *= kFnvPrime;
}

void HashU64(uint64_t& hash, uint64_t value) {
    HashU32(hash, static_cast<uint32_t>(value & 0xFFFFFFFFull));
    HashU32(hash, static_cast<uint32_t>(value >> 32));
}

uint64_t ComputeVertexFetchLayoutSignature(const ac6::d3d::ShadowState& shadow) {
    constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
    uint64_t hash = kFnvOffsetBasis;
    HashU32(hash, shadow.vertex_declaration);
    for (const auto& stream : shadow.streams) {
        HashU32(hash, stream.buffer);
        HashU32(hash, stream.offset);
        HashU32(hash, stream.stride);
    }
    return hash;
}

uint64_t ComputeTextureFetchLayoutSignature(const ac6::d3d::ShadowState& shadow) {
    constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
    uint64_t hash = kFnvOffsetBasis;
    for (uint32_t texture : shadow.texture_fetch_ptrs) {
        HashU32(hash, texture);
    }
    return hash;
}

uint64_t ComputeResourceBindingSignature(const ac6::d3d::ShadowState& shadow) {
    constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
    uint64_t hash = kFnvOffsetBasis;
    for (uint32_t target : shadow.render_targets) {
        HashU32(hash, target);
    }
    HashU32(hash, shadow.depth_stencil);
    HashU32(hash, shadow.index_buffer);
    for (uint32_t texture : shadow.textures) {
        HashU32(hash, texture);
    }
    for (const auto& sampler : shadow.samplers) {
        HashU32(hash, sampler.mag_filter);
        HashU32(hash, sampler.min_filter);
        HashU32(hash, sampler.mip_filter);
        HashU32(hash, sampler.border_color);
        HashU32(hash, sampler.aniso_bias_raw);
        HashU32(hash, sampler.lod_bias_raw);
        HashU32(hash, sampler.mip_min_level);
        HashU32(hash, sampler.mip_max_level);
    }
    HashU64(hash, shadow.vertex_fetch_layout_signature);
    HashU64(hash, shadow.texture_fetch_layout_signature);
    return hash;
}

void HashDrawRecord(uint64_t& hash, const ac6::d3d::DrawCallRecord& draw) {
    HashU32(hash, static_cast<uint32_t>(draw.kind));
    HashU32(hash, draw.primitive_type);
    HashU32(hash, draw.start);
    HashU32(hash, draw.count);
    HashU32(hash, draw.flags);
    HashU32(hash, draw.material_identity.valid ? 1u : 0u);
    HashU32(hash, draw.material_identity.request);
    HashU32(hash, draw.material_identity.material);
    HashU32(hash, draw.material_identity.material_key);
    HashU32(hash, draw.material_identity.draw_context_key);
    HashU32(hash, draw.material_identity.device);
    HashU32(hash, draw.shadow_state.render_targets[0]);
    HashU32(hash, draw.shadow_state.depth_stencil);
    HashU32(hash, draw.shadow_state.viewport.width);
    HashU32(hash, draw.shadow_state.viewport.height);
}

// D3DPRIMITIVETYPE (D3D9); used by DrawIndexed* and DrawPrimitive.
void IncrementTopologyHistogram(ac6::d3d::FrameCaptureSummary& summary, uint32_t primitive_type) {
    switch (primitive_type) {
        case 1:
            ++summary.topology_pointlist;
            break;
        case 2:
            ++summary.topology_linelist;
            break;
        case 3:
            ++summary.topology_linestrip;
            break;
        case 4:
            ++summary.topology_trianglelist;
            break;
        case 5:
            ++summary.topology_trianglestrip;
            break;
        case 6:
            ++summary.topology_trianglefan;
            break;
        default:
            ++summary.topology_other;
            break;
    }
}

ac6::d3d::FrameCaptureSummary MakeFrameCaptureSummary(
    const ac6::d3d::FrameCaptureSnapshot& frame_capture) {
    ac6::d3d::FrameCaptureSummary summary;
    summary.capture_enabled = REXCVAR_GET(ac6_render_capture);
    summary.frame_index = frame_capture.frame_index;
    summary.frame_stats = frame_capture.stats;
    summary.draw_count = static_cast<uint32_t>(frame_capture.draws.size());
    summary.clear_count = static_cast<uint32_t>(frame_capture.clears.size());
    summary.resolve_count = static_cast<uint32_t>(frame_capture.resolves.size());
    summary.frame_end_render_target_count =
        CountNonZero(frame_capture.frame_end_shadow.render_targets);
    summary.frame_end_texture_count = CountNonZero(frame_capture.frame_end_shadow.textures);
    summary.frame_end_stream_count = CountNonZeroStreams(frame_capture.frame_end_shadow.streams);
    summary.frame_end_sampler_count = CountNonZeroSamplers(frame_capture.frame_end_shadow.samplers);
    summary.frame_end_texture_fetch_count =
        CountNonZero(frame_capture.frame_end_shadow.texture_fetch_ptrs);
    summary.frame_end_render_target_0 = frame_capture.frame_end_shadow.render_targets[0];
    summary.frame_end_depth_stencil = frame_capture.frame_end_shadow.depth_stencil;
    summary.frame_end_viewport_width = frame_capture.frame_end_shadow.viewport.width;
    summary.frame_end_viewport_height = frame_capture.frame_end_shadow.viewport.height;
    if (!frame_capture.draws.empty()) {
        summary.first_draw_render_target_0 =
            frame_capture.draws.front().shadow_state.render_targets[0];
        summary.last_draw_render_target_0 =
            frame_capture.draws.back().shadow_state.render_targets[0];
        const auto& last_draw = frame_capture.draws.back();
        summary.last_draw_primitive_type = last_draw.primitive_type;
        summary.last_draw_count = last_draw.count;
        summary.last_draw_flags = last_draw.flags;
    }
    if (summary.capture_enabled && !frame_capture.draws.empty()) {
        constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
        uint64_t signature = kFnvOffsetBasis;
        std::unordered_set<uint32_t> unique_rt0s;
        unique_rt0s.reserve(std::min<size_t>(frame_capture.draws.size(), 128));
        uint32_t previous_rt0 = frame_capture.draws.front().shadow_state.render_targets[0];
        for (const auto& draw : frame_capture.draws) {
            if (draw.material_identity.valid) {
                ++summary.material_identity_draw_count;
            }
            switch (draw.kind) {
                case ac6::d3d::DrawCallKind::kIndexed:
                    ++summary.indexed_draw_count;
                    break;
                case ac6::d3d::DrawCallKind::kIndexedShared:
                    ++summary.indexed_shared_draw_count;
                    break;
                case ac6::d3d::DrawCallKind::kPrimitive:
                    ++summary.primitive_draw_count;
                    break;
            }
            IncrementTopologyHistogram(summary, draw.primitive_type);
            uint32_t rt0 = draw.shadow_state.render_targets[0];
            unique_rt0s.insert(rt0);
            if (rt0 != previous_rt0) {
                ++summary.rt0_switch_count;
                previous_rt0 = rt0;
            }
            HashDrawRecord(signature, draw);
        }
        summary.unique_rt0_count = static_cast<uint32_t>(unique_rt0s.size());
        summary.record_signature = signature;
        summary.record_signature_valid = true;
    }
    return summary;
}

void RememberDevice(uint32_t device) {
    std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
    g_shadow.device = device;
}

ac6::d3d::ShadowState SnapshotShadowState(uint8_t* base, uint32_t device) {
    std::shared_lock<std::shared_mutex> lock(g_shadow_mutex);
    ac6::d3d::ShadowState shadow = g_shadow;
    if (device != 0) {
        shadow.device = device;
        // AC6 inlines many resource binds instead of routing all callers
        // through the physical setters. Refresh the committed state from
        // the retail device at capture time so every configured draw boundary
        // observes the actual committed bindings. The PPC contracts are:
        //   +0x2E24       vertex declaration (setter 0x821DE790)
        //   +0x308C       index buffer (setter 0x821DD188)
        //   +0x318C       compact/pixel shader object (setter 0x821DE2C8)
        //   +0x3190       vertex shader object (setter 0x821DE5C0)
        //   +0x30A4+4*i  vertex stream buffer (setter 0x821DD068)
        //   +0x077C-8*i  bytes remaining after the stream offset
        //   +0x30E8+i    stride divided by four
        shadow.vertex_declaration =
            PPC_LOAD_U32(device + ac6::d3d::kDeviceVertexDeclarationOffset);
        shadow.index_buffer = PPC_LOAD_U32(device + ac6::d3d::kDeviceIndexBufferOffset);
        shadow.guest_pixel_shader =
            PPC_LOAD_U32(device + ac6::d3d::kDevicePixelShaderObjectOffset);
        shadow.guest_vertex_shader =
            PPC_LOAD_U32(device + ac6::d3d::kDeviceVertexShaderObjectOffset);
        if (shadow.guest_pixel_shader != 0) {
            const uint32_t descriptor = shadow.guest_pixel_shader + PPC_LOAD_U32(
                shadow.guest_pixel_shader + ac6::d3d::kPixelShaderDescriptorOffsetField);
            shadow.pixel_shader_program = ac6::d3d::RecoverShaderProgramReference(
                PPC_LOAD_U32(shadow.guest_pixel_shader +
                             ac6::d3d::kPixelShaderProgramBaseField),
                PPC_LOAD_U32(descriptor + ac6::d3d::kPixelShaderProgramRelativeField),
                PPC_LOAD_U32(descriptor + ac6::d3d::kPixelShaderProgramSizeField));
        } else {
            shadow.pixel_shader_program = {};
        }
        for (uint32_t variant = 0;
             variant < ac6::d3d::kCapturedVertexShaderVariantCount; ++variant) {
            auto& program = shadow.vertex_shader_program_candidates[variant];
            if (shadow.guest_vertex_shader == 0) {
                program = {};
                continue;
            }
            const uint32_t descriptor = shadow.guest_vertex_shader + PPC_LOAD_U32(
                shadow.guest_vertex_shader + ac6::d3d::kVertexShaderVariantTableBase +
                variant * ac6::d3d::kVertexShaderVariantTableStride);
            program = ac6::d3d::RecoverShaderProgramReference(
                PPC_LOAD_U32(shadow.guest_vertex_shader +
                             ac6::d3d::kVertexShaderProgramBaseField),
                PPC_LOAD_U32(descriptor + ac6::d3d::kVertexShaderProgramRelativeField),
                PPC_LOAD_U32(descriptor + ac6::d3d::kVertexShaderProgramSizeField));
        }
        for (uint32_t stream = 0; stream < ac6::d3d::kMaxStreams; ++stream) {
            auto& binding = shadow.streams[stream];
            binding.buffer = PPC_LOAD_U32(
                device + ac6::d3d::kDeviceStreamBufferBase + stream * sizeof(uint32_t));
            if (binding.buffer == 0) {
                binding.offset = 0;
                binding.stride = 0;
                continue;
            }

            const uint32_t buffer_size = PPC_LOAD_U32(binding.buffer + 0x1Cu);
            const uint32_t remaining_size = PPC_LOAD_U32(
                device + ac6::d3d::kDeviceStreamRemainingSizeBase - stream * 8u);
            binding.offset = ac6::d3d::RecoverStreamOffset(buffer_size, remaining_size);
            binding.stride = ac6::d3d::RecoverStreamStride(PPC_LOAD_U8(
                device + ac6::d3d::kDeviceStreamStrideBase + stream));
        }
    }
    shadow.vertex_fetch_layout_signature = ComputeVertexFetchLayoutSignature(shadow);
    shadow.texture_fetch_layout_signature = ComputeTextureFetchLayoutSignature(shadow);
    shadow.resource_binding_signature = ComputeResourceBindingSignature(shadow);
    return shadow;
}

ac6::d3d::DrawStatsSnapshot SnapshotDrawStats() {
    return ac6::d3d::DrawStatsSnapshot{
        g_live_stats.draw_calls.load(std::memory_order_relaxed),
        g_live_stats.draw_calls_indexed.load(std::memory_order_relaxed),
        g_live_stats.draw_calls_indexed_shared.load(std::memory_order_relaxed),
        g_live_stats.draw_calls_primitive.load(std::memory_order_relaxed),
        g_live_stats.total_indices.load(std::memory_order_relaxed),
        g_live_stats.total_vertices.load(std::memory_order_relaxed),
        g_live_stats.set_texture_calls.load(std::memory_order_relaxed),
        g_live_stats.set_render_target_calls.load(std::memory_order_relaxed),
        g_live_stats.set_depth_stencil_calls.load(std::memory_order_relaxed),
        g_live_stats.set_vertex_decl_calls.load(std::memory_order_relaxed),
        g_live_stats.set_index_buffer_calls.load(std::memory_order_relaxed),
        g_live_stats.set_stream_source_calls.load(std::memory_order_relaxed),
        g_live_stats.set_viewport_calls.load(std::memory_order_relaxed),
        g_live_stats.set_sampler_state_calls.load(std::memory_order_relaxed),
        g_live_stats.set_texture_fetch_calls.load(std::memory_order_relaxed),
        g_live_stats.clear_calls.load(std::memory_order_relaxed),
        g_live_stats.resolve_calls.load(std::memory_order_relaxed),
    };
}

void CaptureDrawCall(uint8_t* base, ac6::d3d::DrawCallKind kind, uint32_t device,
                     uint32_t primitive_type, uint32_t start, uint32_t count,
                     uint32_t flags) {
    if (!REXCVAR_GET(ac6_render_capture)) {
        return;
    }

    ac6::d3d::DrawCallRecord record;
    record.kind = kind;
    record.primitive_type = primitive_type;
    record.start = start;
    record.count = count;
    record.flags = flags;
    record.shadow_state = SnapshotShadowState(base, device);

    std::unique_lock<std::shared_mutex> lock(g_capture_mutex);
    g_pending_material_identity.ConsumeForDevice(device, &record.material_identity);
    record.sequence = ++g_capture_live_sequence;
    g_live_draws.push_back(std::move(record));
}

void CaptureClear(uint8_t* base, uint32_t device, uint32_t rect_count, uint32_t rects_ptr,
                  uint32_t flags, uint32_t color, uint32_t stencil, float depth) {
    if (!REXCVAR_GET(ac6_render_capture)) {
        return;
    }

    ac6::d3d::ClearRecord record;
    record.rect_count = rect_count;
    record.flags = flags;
    record.color = color;
    record.stencil = stencil;
    record.depth = depth;
    record.shadow_state = SnapshotShadowState(base, device);
    if (rects_ptr && rect_count) {
        const uint32_t captured_rect_count =
            std::min<uint32_t>(rect_count, ac6::d3d::kMaxClearRectsPerRecord);
        record.captured_rect_count = captured_rect_count;
        for (uint32_t i = 0; i < captured_rect_count; ++i) {
            const uint32_t rect_ptr = rects_ptr + i * 16;
            record.rects[i].left = PPC_LOAD_U32(rect_ptr + 0);
            record.rects[i].top = PPC_LOAD_U32(rect_ptr + 4);
            record.rects[i].right = PPC_LOAD_U32(rect_ptr + 8);
            record.rects[i].bottom = PPC_LOAD_U32(rect_ptr + 12);
        }
    }

    std::unique_lock<std::shared_mutex> lock(g_capture_mutex);
    record.sequence = ++g_capture_live_sequence;
    g_live_clears.push_back(std::move(record));
}

void CaptureResolve(uint8_t* base, uint32_t device, const std::array<uint32_t, 7>& args,
                    float depth_or_scale) {
    if (!REXCVAR_GET(ac6_render_capture)) {
        return;
    }

    ac6::d3d::ResolveRecord record;
    record.args = args;
    record.depth_or_scale = depth_or_scale;
    record.shadow_state = SnapshotShadowState(base, device);

    std::unique_lock<std::shared_mutex> lock(g_capture_mutex);
    record.sequence = ++g_capture_live_sequence;
    g_live_resolves.push_back(std::move(record));
}

}  // namespace

void ac6MateDrawRequestHook(PPCRegister& r30, PPCRegister& r31) {
    if (!REXCVAR_GET(ac6_render_capture)) {
        return;
    }

    const uint32_t request = r30.u32;
    const uint32_t device = r31.u32;
    if (request == 0 || device == 0 || request > UINT32_MAX - 0x24u) {
        return;
    }

    auto* memory = REX_KERNEL_MEMORY();
    auto load_u32 = [memory](uint32_t address, uint32_t* value) {
        if (!memory || !value || address > UINT32_MAX - 3u ||
            !memory->LookupHeap(address) || !memory->LookupHeap(address + 3u)) {
            return false;
        }
        *value = rex::memory::load_and_swap<uint32_t>(memory->TranslateVirtual(address));
        return true;
    };

    uint32_t material = 0;
    uint32_t material_key = 0;
    uint32_t draw_context_key = 0;
    if (!load_u32(request + 0x24u, &material) ||
        !load_u32(request + 0x08u, &draw_context_key)) {
        return;
    }
    if (material == 0 || material > UINT32_MAX - sizeof(uint32_t)) {
        return;
    }
    if (!load_u32(material, &material_key)) {
        return;
    }

    ac6::d3d::MaterialDrawIdentity identity;
    identity.valid = true;
    identity.request = request;
    identity.material = material;
    identity.material_key = material_key;
    identity.draw_context_key = draw_context_key;
    identity.device = device;

    std::unique_lock<std::shared_mutex> lock(g_capture_mutex);
    g_pending_material_identity.Publish(identity);
}

PPC_EXTERN_FUNC(__imp__rex_sub_821DEF18);  // DrawIndexedVertices
PPC_EXTERN_FUNC(__imp__rex_sub_821DF300);  // DrawIndexedVertices_Shared
PPC_EXTERN_FUNC(__imp__rex_sub_821DEA48);  // DrawPrimitive
PPC_EXTERN_FUNC(__imp__rex_sub_821DD0A8);  // Non-null-only stream chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821D95C8);  // Unqualified internal-state chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821D9D38);  // Command-dispatch chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DD1C8);  // Unqualified mid-function chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DD258);  // SetRenderTarget chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DD5C8);  // SetDepthStencilSurface chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DA698);  // Device flush/validation chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DCF28);  // SetViewport input chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DC538);  // SetSamplerState_MinFilter chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DC6C8);  // SetSamplerState_MagFilter
PPC_EXTERN_FUNC(__imp__rex_sub_821DC9C0);  // SetSamplerState_AnisoBias chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DCA68);  // SetSamplerState_LodBias chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DCB08);  // SetSamplerState_MipMinLevel chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DCB88);  // SetSamplerState_MipMaxLevel chunk
PPC_EXTERN_FUNC(__imp__rex_sub_821DBAF8);  // SetShaderGPRAlloc
PPC_EXTERN_FUNC(__imp__rex_sub_821E2380);  // Clear
PPC_EXTERN_FUNC(__imp__rex_sub_821E10C8);  // SetTextureFetchConstant
PPC_EXTERN_FUNC(__imp__rex_sub_821E2BB8);  // Resolve

// D3DDevice_DrawIndexedVertices chunk (0x821DEF18).
// The containing function starts at 0x821DEED8 and has preserved the original
// device/primitive/start/count arguments in r31/r25/r21/r22. r4 and r6 have
// already been repurposed before this chunk.
PPC_FUNC_IMPL(rex_sub_821DEF18) {
    PPC_FUNC_PROLOGUE();

    const uint32_t device = ctx.r31.u32;
    const uint32_t prim_type = ctx.r25.u32;
    const uint32_t start = ctx.r21.u32;
    const uint32_t index_count = ctx.r22.u32;
    RememberDevice(device);

    g_live_stats.draw_calls.fetch_add(1, std::memory_order_relaxed);
    g_live_stats.draw_calls_indexed.fetch_add(1, std::memory_order_relaxed);
    g_live_stats.total_indices.fetch_add(index_count, std::memory_order_relaxed);
    CaptureDrawCall(base, ac6::d3d::DrawCallKind::kIndexed, device, prim_type, start,
                    index_count, 0);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "DrawIndexedVertices: prim={} start={} count={}",
            prim_type, start, index_count);
    }

    __imp__rex_sub_821DEF18(ctx, base);
}

// D3DDevice_DrawIndexedVertices_Shared chunk (0x821DF300).
// The containing function starts at 0x821DF2C0 and has preserved the original
// arguments in r31/r16/r15/r19/r17. r4 has already been repurposed.
PPC_FUNC_IMPL(rex_sub_821DF300) {
    PPC_FUNC_PROLOGUE();

    const uint32_t device = ctx.r31.u32;
    const uint32_t prim_type = ctx.r16.u32;
    const uint32_t flags = ctx.r15.u32;
    const uint32_t start = ctx.r19.u32;
    const uint32_t index_count = ctx.r17.u32;
    RememberDevice(device);

    g_live_stats.draw_calls.fetch_add(1, std::memory_order_relaxed);
    g_live_stats.draw_calls_indexed_shared.fetch_add(1, std::memory_order_relaxed);
    g_live_stats.total_indices.fetch_add(index_count, std::memory_order_relaxed);
    CaptureDrawCall(base, ac6::d3d::DrawCallKind::kIndexedShared, device, prim_type, start,
                    index_count, flags);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "DrawIndexedVertices_Shared: prim={} flags={} start={} count={}",
            prim_type, flags, start, index_count);
    }

    __imp__rex_sub_821DF300(ctx, base);
}

// Non-null-only SetStreamSource chunk (0x821DD0A8).
// The containing function starts at 0x821DD068, but a null buffer branches
// directly to 0x821DD0D8 and bypasses this configured address. Capturing here
// would leave stale shadow state after reset, so keep the chunk transparent.
// The universal store is 0x821DD154 and requires reproducible regeneration.
PPC_FUNC_IMPL(rex_sub_821DD0A8) {
    PPC_FUNC_PROLOGUE();
    __imp__rex_sub_821DD0A8(ctx, base);
}

// Internal-state chunk (0x821D95C8).
// The containing 0x821D9588 function receives an internal owner in r3 and
// reaches this boundary after loading state through r30. There is no live
// render-target index/surface ABI here.
PPC_FUNC_IMPL(rex_sub_821D95C8) {
    PPC_FUNC_PROLOGUE();
    __imp__rex_sub_821D95C8(ctx, base);
}

// Command-dispatch chunk (0x821D9D38).
// The containing 0x821D9CF8 function uses r3 as a small command selector
// (compared with 0x22 immediately before this boundary), not as a device.
PPC_FUNC_IMPL(rex_sub_821D9D38) {
    PPC_FUNC_PROLOGUE();
    __imp__rex_sub_821D9D38(ctx, base);
}

// Non-universal index-buffer chunk (0x821DD1C8).
// This boundary is entered after the original r4 argument has been saved in
// r29 and r4 has been repurposed. Keep it as a transparent continuation until
// the containing 0x821DD188 index-buffer function reaches its universal store.
PPC_FUNC_IMPL(rex_sub_821DD1C8) {
    PPC_FUNC_PROLOGUE();
    __imp__rex_sub_821DD1C8(ctx, base);
}

// D3DDevice_SetRenderTarget chunk (0x821DD258).
// The containing function begins at 0x821DD220 with r3=device, r4=target slot
// and r5=surface. It writes r5 to device+0x3090+4*slot and handles exactly
// four color slots. Those arguments are still live at this boundary.
PPC_FUNC_IMPL(rex_sub_821DD258) {
    PPC_FUNC_PROLOGUE();

    const uint32_t target = ctx.r4.u32;
    const uint32_t surface = ctx.r5.u32;
    RememberDevice(ctx.r3.u32);
    if (target < ac6::d3d::kMaxRenderTargets) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.render_targets[target] = surface;
    }
    g_live_stats.set_render_target_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetRenderTarget: target={} surface=0x{:08X}", target, surface);
    }

    __imp__rex_sub_821DD258(ctx, base);
}

// D3DDevice_SetDepthStencilSurface chunk (0x821DD5C8).
// The containing function begins at 0x821DD588 and has preserved device in
// r31 and the original depth-stencil surface in r30 at this boundary. The
// function writes that surface to the separate device+0x30A0 binding.
PPC_FUNC_IMPL(rex_sub_821DD5C8) {
    PPC_FUNC_PROLOGUE();

    const uint32_t device = ctx.r31.u32;
    const uint32_t surface = ctx.r30.u32;
    RememberDevice(device);
    {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.depth_stencil = surface;
    }
    g_live_stats.set_depth_stencil_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetDepthStencilSurface: surface=0x{:08X}", surface);
    }

    __imp__rex_sub_821DD5C8(ctx, base);
}

// Device flush/validation chunk (0x821DA698).
// 0x821DCEE8 publishes viewport fields and then calls the containing function
// at 0x821DA658. This chunk validates device state and stream bindings; it is
// not entered with a stable x/y/width/height ABI.
PPC_FUNC_IMPL(rex_sub_821DA698) {
    PPC_FUNC_PROLOGUE();
    __imp__rex_sub_821DA698(ctx, base);
}

// D3DDevice_SetViewport input chunk (0x821DCF28).
// The containing function starts at 0x821DCEE8. At this boundary r3 is still
// the device and r4 still points at the four guest uint32 viewport fields;
// the function later publishes them to +0x317C..+0x3188 and calls the
// 0x821DA658 device-state validator.
PPC_FUNC_IMPL(rex_sub_821DCF28) {
    PPC_FUNC_PROLOGUE();

    const uint32_t device = ctx.r3.u32;
    const uint32_t viewport = ctx.r4.u32;
    RememberDevice(device);
    {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.viewport.x = PPC_LOAD_U32(viewport + 0);
        g_shadow.viewport.y = PPC_LOAD_U32(viewport + 4);
        g_shadow.viewport.width = PPC_LOAD_U32(viewport + 8);
        g_shadow.viewport.height = PPC_LOAD_U32(viewport + 12);
    }
    g_live_stats.set_viewport_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetViewport: {}x{} at ({},{})",
            g_shadow.viewport.width, g_shadow.viewport.height,
            g_shadow.viewport.x, g_shadow.viewport.y);
    }

    __imp__rex_sub_821DCF28(ctx, base);
}

// D3DDevice_Resolve chunk (0x821E2BB8).
// The containing function starts at 0x821E2B78 and has already preserved the
// original arguments in r31/r25/r26/r20/r27/r5/r22/r21 and f30. In
// particular, r5 has been repurposed to hold the original r8.
PPC_FUNC_IMPL(rex_sub_821E2BB8) {
    PPC_FUNC_PROLOGUE();

    const uint32_t device = ctx.r31.u32;
    const std::array<uint32_t, 7> args{
        ctx.r25.u32,
        ctx.r26.u32,
        ctx.r20.u32,
        ctx.r27.u32,
        ctx.r5.u32,
        ctx.r22.u32,
        ctx.r21.u32,
    };
    RememberDevice(device);
    g_live_stats.resolve_calls.fetch_add(1, std::memory_order_relaxed);
    CaptureResolve(base, device, args, static_cast<float>(ctx.f30.f64));

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU, "Resolve");
    }

    __imp__rex_sub_821E2BB8(ctx, base);
}

// D3DDevice_DrawPrimitive (0x821DEA48)
// r3=pDevice, r4=PrimitiveType, r5=VertexCount
PPC_FUNC_IMPL(rex_sub_821DEA48) {
    PPC_FUNC_PROLOGUE();

    uint32_t prim_type = ctx.r4.u32;
    uint32_t vertex_count = ctx.r5.u32;
    RememberDevice(ctx.r3.u32);

    g_live_stats.draw_calls.fetch_add(1, std::memory_order_relaxed);
    g_live_stats.draw_calls_primitive.fetch_add(1, std::memory_order_relaxed);
    g_live_stats.total_vertices.fetch_add(vertex_count, std::memory_order_relaxed);
    CaptureDrawCall(base, ac6::d3d::DrawCallKind::kPrimitive, ctx.r3.u32, prim_type, 0,
                    vertex_count, 0);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "DrawPrimitive: prim={} count={}",
            prim_type, vertex_count);
    }

    __imp__rex_sub_821DEA48(ctx, base);
}

// D3DDevice_SetSamplerState_MinFilter chunk (0x821DC538)
// At this boundary r8=pDevice+Sampler, r5=Value, and r4 is a table offset.
PPC_FUNC_IMPL(rex_sub_821DC538) {
    PPC_FUNC_PROLOGUE();

    uint32_t sampler = ac6::d3d::RecoverSamplerAt821DC538(ctx.r3.u32, ctx.r8.u32);
    uint32_t value = ctx.r5.u32;
    RememberDevice(ctx.r3.u32);

    if (sampler < ac6::d3d::kMaxSamplers) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.samplers[sampler].min_filter = value;
    }
    g_live_stats.set_sampler_state_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetSamplerState_MinFilter: sampler={} value={}",
            sampler, value);
    }

    __imp__rex_sub_821DC538(ctx, base);
}

// D3DDevice_SetSamplerState_MagFilter (0x821DC6C8)
// At this boundary r8=pDevice+Sampler, r5=Value, and r4 is a table offset.
PPC_FUNC_IMPL(rex_sub_821DC6C8) {
    PPC_FUNC_PROLOGUE();

    uint32_t sampler = ac6::d3d::RecoverSamplerAt821DC6C8(ctx.r3.u32, ctx.r8.u32);
    uint32_t value = ctx.r5.u32;
    RememberDevice(ctx.r3.u32);

    if (sampler < ac6::d3d::kMaxSamplers) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.samplers[sampler].mag_filter = value;
    }
    g_live_stats.set_sampler_state_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetSamplerState_MagFilter: sampler={} value={}",
            sampler, value);
    }

    __imp__rex_sub_821DC6C8(ctx, base);
}

// D3DDevice_SetSamplerState_MipMaxLevel chunk (0x821DCB88).
// r4=Sampler, r5=requested value, r11=effective clamped value.
PPC_FUNC_IMPL(rex_sub_821DCB88) {
    PPC_FUNC_PROLOGUE();

    uint32_t sampler = ctx.r4.u32;
    uint32_t value = ctx.r11.u32;
    RememberDevice(ctx.r3.u32);

    if (sampler < ac6::d3d::kMaxSamplers) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.samplers[sampler].mip_max_level = value;
    }
    g_live_stats.set_sampler_state_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetSamplerState_MipMaxLevel: sampler={} effective_value={}",
            sampler, value);
    }

    __imp__rex_sub_821DCB88(ctx, base);
}

// D3DDevice_SetSamplerState_LodBias chunk (0x821DCA68).
// r4=Sampler, r5=raw requested float bits.
PPC_FUNC_IMPL(rex_sub_821DCA68) {
    PPC_FUNC_PROLOGUE();

    uint32_t sampler = ctx.r4.u32;
    uint32_t value = ctx.r5.u32;
    RememberDevice(ctx.r3.u32);

    if (sampler < ac6::d3d::kMaxSamplers) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.samplers[sampler].lod_bias_raw = value;
    }
    g_live_stats.set_sampler_state_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetSamplerState_LodBias: sampler={} raw=0x{:08X}",
            sampler, value);
    }

    __imp__rex_sub_821DCA68(ctx, base);
}

// D3DDevice_SetSamplerState_AnisoBias chunk (0x821DC9C0).
// r4=Sampler, r5=raw requested float bits.
PPC_FUNC_IMPL(rex_sub_821DC9C0) {
    PPC_FUNC_PROLOGUE();

    uint32_t sampler = ctx.r4.u32;
    uint32_t value = ctx.r5.u32;
    RememberDevice(ctx.r3.u32);

    if (sampler < ac6::d3d::kMaxSamplers) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.samplers[sampler].aniso_bias_raw = value;
    }
    g_live_stats.set_sampler_state_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetSamplerState_AnisoBias: sampler={} raw=0x{:08X}",
            sampler, value);
    }

    __imp__rex_sub_821DC9C0(ctx, base);
}

// D3DDevice_SetSamplerState_MipMinLevel chunk (0x821DCB08).
// r4=Sampler, r5=requested value, r11=effective clamped value.
PPC_FUNC_IMPL(rex_sub_821DCB08) {
    PPC_FUNC_PROLOGUE();

    uint32_t sampler = ctx.r4.u32;
    uint32_t value = ctx.r11.u32;
    RememberDevice(ctx.r3.u32);

    if (sampler < ac6::d3d::kMaxSamplers) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.samplers[sampler].mip_min_level = value;
    }
    g_live_stats.set_sampler_state_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetSamplerState_MipMinLevel: sampler={} effective_value={}",
            sampler, value);
    }

    __imp__rex_sub_821DCB08(ctx, base);
}

// D3DDevice_SetShaderGPRAlloc (0x821DBAF8)
// r3=pDevice, r4=Flags
PPC_FUNC_IMPL(rex_sub_821DBAF8) {
    PPC_FUNC_PROLOGUE();

    uint32_t flags = ctx.r4.u32;
    RememberDevice(ctx.r3.u32);
    {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.shader_gpr_alloc = flags;
    }

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetShaderGPRAlloc: flags=0x{:08X}", flags);
    }

    __imp__rex_sub_821DBAF8(ctx, base);
}

// D3DDevice_Clear (0x821E2380)
// r3=pDevice, r4=Count, r5=pRects, r6=Flags, r7=Color, f1=Z, r8=Stencil, r9=EDRAMClear
PPC_FUNC_IMPL(rex_sub_821E2380) {
    PPC_FUNC_PROLOGUE();

    const uint32_t device = ctx.r29.u32;
    RememberDevice(device);
    g_live_stats.clear_calls.fetch_add(1, std::memory_order_relaxed);
    CaptureClear(base, device, ctx.r4.u32, ctx.r5.u32, ctx.r28.u32, ctx.r7.u32, ctx.r8.u32,
                 static_cast<float>(ctx.f31.f64));

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "Clear: count={} flags=0x{:X} color=0x{:08X} stencil={}",
            ctx.r4.u32, ctx.r28.u32, ctx.r7.u32, ctx.r8.u32);
    }

    __imp__rex_sub_821E2380(ctx, base);
}

// Mid-function texture-fetch continuation (0x821E10C8).
// At this boundary r3=pDevice, r4=pDevice+stage, r5=pTexture. The original
// stage was consumed by `add r4,r31,r4` at 0x821E10B8 and must be recovered
// before indexing the host shadow state.
PPC_FUNC_IMPL(rex_sub_821E10C8) {
    PPC_FUNC_PROLOGUE();

    const uint32_t device = ctx.r3.u32;
    const uint32_t stage = ac6::d3d::RecoverTextureStageAt821E10C8(device, ctx.r4.u32);
    const uint32_t texture = ctx.r5.u32;
    RememberDevice(device);

    if (stage < ac6::d3d::kMaxFetchConstants) {
        std::unique_lock<std::shared_mutex> lock(g_shadow_mutex);
        g_shadow.texture_fetch_ptrs[stage] = texture;
    }
    g_live_stats.set_texture_fetch_calls.fetch_add(1, std::memory_order_relaxed);

    if (REXCVAR_GET(ac6_d3d_trace)) {
        REXLOG_CAT_TRACE(kLogGPU,
            "SetTextureFetchConstant: stage={} texture=0x{:08X}",
            stage, texture);
    }

    __imp__rex_sub_821E10C8(ctx, base);
}

namespace ac6::d3d {

void OnFrameBoundary() {
    ac6::d3d::DrawStatsSnapshot draw_stats = SnapshotDrawStats();

    std::unique_lock<std::shared_mutex> lock(g_snapshot_mutex);
    g_snapshot = draw_stats;
    lock.unlock();

    if (!REXCVAR_GET(ac6_render_capture)) {
        std::unique_lock<std::shared_mutex> capture_lock(g_capture_mutex);
        g_capture_summary = {};
        g_capture_summary.capture_enabled = false;
        g_capture_summary.frame_index = g_capture_live_frame_index++;
        g_capture_summary.frame_stats = draw_stats;
        g_capture_snapshot = {};
        g_capture_snapshot.frame_index = g_capture_summary.frame_index;
        g_capture_snapshot.stats = draw_stats;
        g_capture_live_sequence = 0;
        g_live_draws.clear();
        g_live_clears.clear();
        g_live_resolves.clear();
        g_pending_material_identity.Clear();
        g_live_stats.Reset();
        return;
    }

    ac6::d3d::FrameCaptureSnapshot frame_capture;
    frame_capture.stats = draw_stats;
    frame_capture.frame_end_shadow = SnapshotShadowState(nullptr, 0);

    std::unique_lock<std::shared_mutex> capture_lock(g_capture_mutex);
    frame_capture.frame_index = g_capture_live_frame_index++;
    frame_capture.draws.swap(g_live_draws);
    frame_capture.clears.swap(g_live_clears);
    frame_capture.resolves.swap(g_live_resolves);
    g_pending_material_identity.Clear();
    g_capture_live_sequence = 0;
    g_capture_summary = MakeFrameCaptureSummary(frame_capture);
    g_capture_snapshot = std::move(frame_capture);

    g_live_stats.Reset();
}

DrawStatsSnapshot GetDrawStats() {
    std::shared_lock<std::shared_mutex> lock(g_snapshot_mutex);
    return g_snapshot;
}

FrameCaptureSnapshot TakeFrameCapture(FrameCaptureSummary* summary_out) {
    std::unique_lock<std::shared_mutex> lock(g_capture_mutex);
    if (summary_out) {
        *summary_out = g_capture_summary;
    }
    FrameCaptureSnapshot snapshot = std::move(g_capture_snapshot);
    g_capture_snapshot = {};
    return snapshot;
}

ShadowState GetShadowState() {
    std::shared_lock<std::shared_mutex> lock(g_shadow_mutex);
    return g_shadow;
}

}  // namespace ac6::d3d
