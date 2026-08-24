#include "ac6demo/rexglue_runtime_shader.hpp"

#include "ac6demo/runtime_error.hpp"

#include <rex/graphics/pipeline/shader/shader.h>
#include <rex/graphics/pipeline/shader/spirv_translator.h>
#include <rex/graphics/register_file.h>
#include <rex/graphics/util/draw.h>
#include <rex/string/buffer.h>
#include <spirv-tools/libspirv.hpp>

#include <openssl/evp.h>

#include <array>
#include <bit>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <string_view>
#include <variant>

namespace ac6demo {
namespace {

std::string sha256(const void *data, std::size_t size) {
  std::array<unsigned char, 32> digest{};
  unsigned int digest_size = 0;
  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (context == nullptr ||
      EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1 ||
      EVP_DigestUpdate(context, data, size) != 1 ||
      EVP_DigestFinal_ex(context, digest.data(), &digest_size) != 1 ||
      digest_size != digest.size()) {
    EVP_MD_CTX_free(context);
    throw RuntimeTrap("runtime shader SHA-256 failed");
  }
  EVP_MD_CTX_free(context);
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const auto byte : digest) {
    output << std::setw(2) << static_cast<unsigned>(byte);
  }
  return output.str();
}

struct Identity {
  XenosShaderStage stage;
  std::uint16_t dwords;
  std::uint32_t register_count;
  std::string_view sha256;
  std::size_t spirv_bytes;
  std::string_view spirv_sha256;
  std::uint32_t guest_source_address;
  std::uint16_t interpolator_mask;
};

const Identity &qualify(const XenosShaderLoadCommand &shader,
                        std::uint32_t register_count) {
  static constexpr std::array identities{
      Identity{XenosShaderStage::Vertex, 24U, 15U,
               "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
               7800U,
               "944fd75222b6de743b9ce1cd18440b8497230e3813bb105c655cd6cfba123ce6",
               0U, 0U},
      Identity{XenosShaderStage::Vertex, 27U, 2U,
               "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
               12496U,
               "ba9b97cceb816059cd21ff6abfda6c59160363155d5b270a3e315b215adb0576",
               0U, 0U},
      Identity{XenosShaderStage::Pixel, 9U, 1U,
               "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25",
               7008U,
               "f6422d60ff48b5ed43292db838655199322a6d439fea10d39302deda69ece9fe",
               0U, 0U},
      Identity{XenosShaderStage::Vertex, 15U, 3U,
               "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
               9288U,
               "4913cadb00aef0bba3f42c25e25919b6403e2de654e8165748337df331cdc920",
               0U, 0U},
      Identity{XenosShaderStage::Vertex, 66U, 4U,
               "84e2d87ca4c7e6b6463cd007778e3e76f2d33d5b3a2bdf71bdabedf5e2949e6b",
               20324U,
               "f41fa1c701b03f4b6b449ae8827c58ab59615ccf948500361ee6bd8f2d0bbcf3",
               0x155FAA40U, 0x3U},
      Identity{XenosShaderStage::Pixel, 15U, 2U,
               "8982431ab8c37106400e0cc23a09a9a08b6de1d952dcb7f7def0016bd5714825",
               14092U,
               "bda1448b0653250ecd21c99c91f557366edb5dc7af746c22285c66776abfacfe",
               0x155FAB80U, 0x3U},
  };
  const auto identity = std::ranges::find_if(
      identities, [&](const Identity &candidate) {
        return candidate.stage == shader.stage &&
               candidate.dwords == shader.size_dwords &&
               candidate.register_count == register_count &&
               candidate.sha256 == shader.guest_big_endian_sha256 &&
               candidate.guest_source_address == shader.guest_source_address;
      });
  if (identity == identities.end() || shader.start_dword != 0U ||
      shader.guest_big_endian_dwords.size() != shader.size_dwords ||
      register_count == 0U || register_count > 64U) {
    throw RuntimeTrap("unqualified runtime ReXGlue shader translation");
  }
  return *identity;
}

} // namespace

std::optional<ReachedShaderImageSource>
qualified_reached_shader_image_source(const XenosShaderLoadCommand &shader) noexcept {
  struct SourceIdentity {
    XenosShaderStage stage;
    std::uint16_t dwords;
    std::string_view sha256;
    std::uint32_t guest_source_address;
    ReachedShaderImageSource source;
  };
  static constexpr std::array sources{
      SourceIdentity{XenosShaderStage::Vertex, 24U,
                     "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
                     0U,
                     {0x82013E20U, 0x82013E80U}},
      SourceIdentity{XenosShaderStage::Vertex, 27U,
                     "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
                     0U,
                     {0x820140A0U, 0x8201410CU}},
      SourceIdentity{XenosShaderStage::Vertex, 15U,
                     "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
                     0U,
                     {0x82014140U, 0x8201417CU}},
      SourceIdentity{XenosShaderStage::Vertex, 66U,
                     "84e2d87ca4c7e6b6463cd007778e3e76f2d33d5b3a2bdf71bdabedf5e2949e6b",
                     0x155FAA40U, {0x155FAA40U, 0x155FAB48U}},
  };
  const auto found = std::ranges::find_if(sources, [&](const auto &candidate) {
    return candidate.stage == shader.stage &&
           candidate.dwords == shader.size_dwords &&
           candidate.sha256 == shader.guest_big_endian_sha256 &&
           candidate.guest_source_address == shader.guest_source_address;
  });
  if (found == sources.end() || shader.start_dword != 0U) {
    return std::nullopt;
  }
  return found->source;
}

ReachedShaderSpirv
translate_reached_shader_spirv(const XenosShaderLoadCommand &shader,
                               std::uint32_t register_count) {
  const Identity &identity = qualify(shader, register_count);
  std::vector<std::uint32_t> big_endian_storage;
  big_endian_storage.reserve(shader.guest_big_endian_dwords.size());
  for (const std::uint32_t word : shader.guest_big_endian_dwords) {
    big_endian_storage.push_back(std::byteswap(word));
  }
  const std::string input_sha =
      sha256(big_endian_storage.data(),
             big_endian_storage.size() * sizeof(std::uint32_t));
  if (input_sha != shader.guest_big_endian_sha256) {
    throw RuntimeTrap("runtime shader bytes do not match their identity");
  }
  const auto type = shader.stage == XenosShaderStage::Vertex
                        ? rex::graphics::xenos::ShaderType::kVertex
                        : rex::graphics::xenos::ShaderType::kPixel;
  rex::graphics::Shader source(type, 0, big_endian_storage.data(),
                               big_endian_storage.size(),
                               std::endian::big);
  rex::string::StringBuffer analysis;
  source.AnalyzeUcode(analysis);
  const auto &constant_map = source.constant_register_map();
  const rex::graphics::SpirvShaderTranslator::Features features(false);
  rex::graphics::SpirvShaderTranslator translator(features, false, false,
                                                   false, 1, 1);
  rex::graphics::SpirvShaderTranslator::Modification modification(
      type == rex::graphics::xenos::ShaderType::kVertex
          ? translator.GetDefaultVertexShaderModification(register_count)
          : translator.GetDefaultPixelShaderModification(register_count));
  if (type == rex::graphics::xenos::ShaderType::kVertex) {
    modification.vertex.interpolator_mask = identity.interpolator_mask;
  } else {
    modification.pixel.interpolator_mask = identity.interpolator_mask;
  }
  auto *translation = source.GetOrCreateTranslation(modification.value);
  if (!translator.TranslateAnalyzedShader(*translation) ||
      !translation->is_valid() || translation->translated_binary().empty() ||
      (translation->translated_binary().size() & 3U) != 0U) {
    throw RuntimeTrap("runtime ReXGlue shader translation failed");
  }
  const auto &binary = translation->translated_binary();
  ReachedShaderSpirv result;
  result.stage = shader.stage;
  result.microcode_sha256 = input_sha;
  result.words.resize(binary.size() / sizeof(std::uint32_t));
  std::memcpy(result.words.data(), binary.data(), binary.size());
  std::copy(std::begin(constant_map.float_bitmap),
            std::end(constant_map.float_bitmap), result.float_bitmap.begin());
  std::copy(std::begin(constant_map.bool_bitmap),
            std::end(constant_map.bool_bitmap), result.bool_bitmap.begin());
  result.loop_bitmap = constant_map.loop_bitmap;
  result.float_count = constant_map.float_count;
  result.float_dynamic_addressing = constant_map.float_dynamic_addressing;
  result.image_source = qualified_reached_shader_image_source(shader);
  if (shader.stage == XenosShaderStage::Vertex && !result.image_source) {
    throw RuntimeTrap("runtime vertex shader lacks qualified PAL image provenance");
  }
  spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
  std::string validation_error;
  validator.SetMessageConsumer(
      [&](spv_message_level_t, const char *, const spv_position_t &position,
          const char *message) {
        validation_error = std::to_string(position.line) + ":" +
                           std::to_string(position.column) + " " + message;
      });
  spvtools::ValidatorOptions validation_options;
  validation_options.SetScalarBlockLayout(true);
  if (!validator.Validate(result.words.data(), result.words.size(),
                          validation_options)) {
    throw RuntimeTrap("runtime ReXGlue SPIR-V validation failed: " +
                      validation_error + " sha256=" +
                      sha256(binary.data(), binary.size()));
  }
  result.spirv_sha256 = sha256(binary.data(), binary.size());
  if (binary.size() != identity.spirv_bytes ||
      result.spirv_sha256 != identity.spirv_sha256) {
    throw RuntimeTrap("runtime ReXGlue SPIR-V golden mismatch: bytes=" +
                      std::to_string(binary.size()) + " sha256=" +
                      result.spirv_sha256);
  }
  return result;
}

ReachedConstantPayloads build_reached_constant_payloads(
    const XenosDrawCommand &draw, const ReachedShaderSpirv &vertex,
    const ReachedShaderSpirv &pixel, const std::uint32_t viewport_x_max,
    const std::uint32_t viewport_y_max) {
  namespace graphics = rex::graphics;
  static constexpr std::string_view kTitleVertex =
      "84e2d87ca4c7e6b6463cd007778e3e76f2d33d5b3a2bdf71bdabedf5e2949e6b";
  static constexpr std::string_view kTitlePixel =
      "8982431ab8c37106400e0cc23a09a9a08b6de1d952dcb7f7def0016bd5714825";
  const bool rectangle = draw.primitive == XenosPrimitive::RectangleList &&
                         draw.index_count == 3U;
  const bool title = draw.primitive == XenosPrimitive::QuadList &&
                     draw.index_count == 4U && draw.predicated &&
                     draw.vertex_shader_sha256 == kTitleVertex &&
                     draw.pixel_shader_sha256 == kTitlePixel;
  if (!draw.registers || (!rectangle && !title) ||
      draw.source != XenosIndexSource::AutoIndex ||
      viewport_x_max == 0U || viewport_y_max == 0U ||
      vertex.stage != XenosShaderStage::Vertex ||
      pixel.stage != XenosShaderStage::Pixel ||
      vertex.microcode_sha256 != draw.vertex_shader_sha256 ||
      pixel.microcode_sha256 != draw.pixel_shader_sha256) {
    throw RuntimeTrap("unqualified reached constant payload request");
  }
  graphics::RegisterFile regs;
  static_assert(graphics::RegisterFile::kRegisterCount <= kXenosRegisterCount);
  for (std::uint32_t index = 0; index < graphics::RegisterFile::kRegisterCount;
       ++index) {
    regs[index] = draw.registers->value(static_cast<std::uint16_t>(index));
  }
  using Translator = graphics::SpirvShaderTranslator;
  Translator::SystemConstants system{};
  const auto vte = regs.Get<graphics::reg::PA_CL_VTE_CNTL>();
  const auto surface = regs.Get<graphics::reg::RB_SURFACE_INFO>();
  const auto color_control = regs.Get<graphics::reg::RB_COLORCONTROL>();
  const auto depth_info = regs.Get<graphics::reg::RB_DEPTH_INFO>();
  std::uint32_t flags = Translator::kSysFlag_PrimitivePolygonal;
  if (vte.vtx_xy_fmt) {
    flags |= Translator::kSysFlag_XYDividedByW;
  }
  if (vte.vtx_z_fmt) {
    flags |= Translator::kSysFlag_ZDividedByW;
  }
  if (vte.vtx_w0_fmt) {
    flags |= Translator::kSysFlag_WNotReciprocal;
  }
  flags |= static_cast<std::uint32_t>(surface.msaa_samples)
           << Translator::kSysFlag_MsaaSamples_Shift;
  if (depth_info.depth_format ==
      graphics::xenos::DepthRenderTargetFormat::kD24FS8) {
    flags |= Translator::kSysFlag_DepthFloat24;
  }
  const auto alpha = color_control.alpha_test_enable
                         ? color_control.alpha_func
                         : graphics::xenos::CompareFunction::kAlways;
  flags |= static_cast<std::uint32_t>(alpha)
           << Translator::kSysFlag_AlphaPassIfLess_Shift;
  system.flags = flags;
  system.vertex_index_load_address = 0U;
  system.vertex_index_endian = graphics::xenos::Endian::kNone;
  system.line_loop_closing_index = 0U;
  system.vertex_base_index =
      regs.Get<std::int32_t>(graphics::XE_GPU_REG_VGT_INDX_OFFSET);
  system.vertex_index_min =
      regs.Get<std::uint32_t>(graphics::XE_GPU_REG_VGT_MIN_VTX_INDX);
  system.vertex_index_max =
      regs.Get<std::uint32_t>(graphics::XE_GPU_REG_VGT_MAX_VTX_INDX);
  const auto normalized_depth = graphics::draw_util::GetNormalizedDepthControl(regs);
  graphics::draw_util::ViewportInfo viewport{};
  graphics::draw_util::GetHostViewportInfo(
      regs, 1U, 1U, false, viewport_x_max, viewport_y_max, true,
      normalized_depth, false, false, false, viewport);
  std::copy_n(viewport.ndc_scale, 3, system.ndc_scale);
  std::copy_n(viewport.ndc_offset, 3, system.ndc_offset);
  system.alpha_test_reference =
      regs.Get<float>(graphics::XE_GPU_REG_RB_ALPHA_REF);
  // The reached title shader is qualified against a host Vulkan view with
  // identity component order (BC3_UNORM exposes RGBA).  Xenos' fetch swizzle
  // is still consumed by the translated shader through the system constant
  // when image-view format swizzle is unavailable.  Keep that shared
  // renderer contract explicit instead of leaving the zero-initialized
  // 0000 swizzle (which aliases every output component to red).
  if (title) {
    const auto fetch_base = graphics::XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0;
    system.texture_swizzles[0] =
        (regs[fetch_base + 3U] >> 1U) & 0x0FFFU;
  }
  if (title && std::getenv("AC6_DEMO_TRACE_TITLE_STATE") != nullptr) {
    const auto fetch_base = graphics::XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0;
    std::fprintf(
        stderr,
        "AC6_TITLE_STATE flags=0x%08X alpha_enable=%u alpha_func=%u "
        "alpha_ref=%g vte=0x%08X surface=0x%08X depth=0x%08X "
        "fetch=%08X,%08X,%08X,%08X,%08X,%08X\n",
        flags, color_control.alpha_test_enable ? 1U : 0U,
        static_cast<unsigned>(alpha), system.alpha_test_reference,
        regs[graphics::XE_GPU_REG_PA_CL_VTE_CNTL],
        regs[graphics::XE_GPU_REG_RB_SURFACE_INFO],
        regs[graphics::XE_GPU_REG_RB_DEPTH_INFO], regs[fetch_base + 0U],
        regs[fetch_base + 1U], regs[fetch_base + 2U], regs[fetch_base + 3U],
        regs[fetch_base + 4U], regs[fetch_base + 5U]);
  }
  const std::uint32_t color_info = regs[graphics::XE_GPU_REG_RB_COLOR_INFO];
  const std::int32_t exp_bias =
      static_cast<std::int32_t>(color_info << 6U) >> 26U;
  system.color_exp_bias[0] =
      std::bit_cast<float>(0x3F800000U +
                           (static_cast<std::uint32_t>(exp_bias) << 23U));

  ReachedConstantPayloads result;
  result.system.resize(sizeof(system));
  std::memcpy(result.system.data(), &system, sizeof(system));
  const auto pack_float = [&](const ReachedShaderSpirv &shader,
                              std::uint32_t base) {
    if (shader.float_dynamic_addressing && shader.float_count != 256U) {
      throw RuntimeTrap("invalid dynamic reached float constant map");
    }
    std::vector<std::byte> packed(
        16U * std::max(shader.float_count, std::uint32_t{1}), std::byte{});
    std::size_t output = 0;
    for (std::uint32_t block = 0; block < shader.float_bitmap.size(); ++block) {
      std::uint64_t bits = shader.float_bitmap[block];
      for (std::uint32_t bit = 0; bit < 64U; ++bit) {
        if ((bits & (std::uint64_t{1} << bit)) == 0U) {
          continue;
        }
        const std::uint32_t constant = block * 64U + bit;
        for (std::uint32_t component = 0; component < 4U; ++component) {
          const auto value = regs[base + constant * 4U + component];
          std::memcpy(packed.data() + output, &value, sizeof(value));
          output += sizeof(value);
        }
      }
    }
    if (output != 16U * shader.float_count) {
      throw RuntimeTrap("reached float constant map count mismatch");
    }
    return packed;
  };
  result.float_vertex =
      pack_float(vertex, graphics::XE_GPU_REG_SHADER_CONSTANT_000_X);
  result.float_pixel =
      pack_float(pixel, graphics::XE_GPU_REG_SHADER_CONSTANT_256_X);
  if (title) {
    for (std::uint32_t constant = 40U; constant <= 43U; ++constant) {
      const auto base = graphics::XE_GPU_REG_SHADER_CONSTANT_000_X +
                        constant * 4U;
      std::fprintf(stderr,
                   "AC6_TITLE_CONSTANT index=%u x=%g y=%g z=%g w=%g\n",
                   constant, std::bit_cast<float>(regs[base]),
                   std::bit_cast<float>(regs[base + 1U]),
                   std::bit_cast<float>(regs[base + 2U]),
                   std::bit_cast<float>(regs[base + 3U]));
    }
  }
  std::memcpy(result.bool_loop.data(),
              &regs[graphics::XE_GPU_REG_SHADER_CONSTANT_BOOL_000_031],
              result.bool_loop.size());
  std::memcpy(result.fetch.data(),
              &regs[graphics::XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0],
              result.fetch.size());
  return result;
}

void ReachedShaderRuntimeCache::consume(
    const std::span<const XenosCommand> commands) {
  auto next_loads = shader_loads_;
  auto next_modules = modules_;
  auto next_stats = stats_;
  constexpr std::uint16_t kSqProgramCntl = 0x2180U;
  for (const auto &command : commands) {
    if (const auto *load = std::get_if<XenosShaderLoadCommand>(&command)) {
      if (next_loads.size() >= 6U &&
          next_loads.find(load->guest_big_endian_sha256) == next_loads.end()) {
        throw RuntimeTrap("runtime shader load cache limit reached");
      }
      next_loads[load->guest_big_endian_sha256] = *load;
      ++next_stats.shader_loads;
      continue;
    }
    if (const auto *draw = std::get_if<XenosDrawCommand>(&command)) {
      if (!draw->registers) {
        throw RuntimeTrap("runtime shader draw has no register snapshot");
      }
      const std::uint32_t program = draw->registers->value(kSqProgramCntl);
      const std::array identities{
          std::pair{draw->vertex_shader_sha256, (program & 0x3FU) + 1U},
          std::pair{draw->pixel_shader_sha256,
                    ((program >> 8U) & 0x3FU) + 1U},
      };
      for (const auto &[identity, register_count] : identities) {
        if (next_modules.find(identity) != next_modules.end()) {
          continue;
        }
        const auto load = next_loads.find(identity);
        if (load == next_loads.end()) {
          throw RuntimeTrap("runtime draw references unavailable shader bytes");
        }
        next_modules.emplace(
            identity,
            translate_reached_shader_spirv(load->second, register_count));
      }
      ++next_stats.draws;
      continue;
    }
    const auto &present = std::get<XenosPresentCommand>(command);
    if (present.format != 6U || !present.tiled || present.width != 1280U ||
        present.height != 720U || next_stats.draws == 0U) {
      throw RuntimeTrap("unqualified runtime shader present");
    }
    ++next_stats.presents;
  }
  next_stats.translated_modules =
      static_cast<std::uint32_t>(next_modules.size());
  shader_loads_ = std::move(next_loads);
  modules_ = std::move(next_modules);
  stats_ = next_stats;
}

const ReachedShaderSpirv *ReachedShaderRuntimeCache::module(
    const std::string_view microcode_sha256) const noexcept {
  const auto found = modules_.find(std::string(microcode_sha256));
  return found == modules_.end() ? nullptr : &found->second;
}

} // namespace ac6demo
