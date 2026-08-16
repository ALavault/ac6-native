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
};

const Identity &qualify(const XenosShaderLoadCommand &shader,
                        std::uint32_t register_count) {
  static constexpr std::array identities{
      Identity{XenosShaderStage::Vertex, 24U, 15U,
               "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
               7800U,
               "944fd75222b6de743b9ce1cd18440b8497230e3813bb105c655cd6cfba123ce6"},
      Identity{XenosShaderStage::Vertex, 27U, 2U,
               "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
               12496U,
               "ba9b97cceb816059cd21ff6abfda6c59160363155d5b270a3e315b215adb0576"},
      Identity{XenosShaderStage::Pixel, 9U, 1U,
               "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25",
               7008U,
               "f6422d60ff48b5ed43292db838655199322a6d439fea10d39302deda69ece9fe"},
      Identity{XenosShaderStage::Vertex, 15U, 3U,
               "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
               9288U,
               "4913cadb00aef0bba3f42c25e25919b6403e2de654e8165748337df331cdc920"},
  };
  const auto identity = std::ranges::find_if(
      identities, [&](const Identity &candidate) {
        return candidate.stage == shader.stage &&
               candidate.dwords == shader.size_dwords &&
               candidate.register_count == register_count &&
               candidate.sha256 == shader.guest_big_endian_sha256;
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
    ReachedShaderImageSource source;
  };
  static constexpr std::array sources{
      SourceIdentity{XenosShaderStage::Vertex, 24U,
                     "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3",
                     {0x82013E20U, 0x82013E80U}},
      SourceIdentity{XenosShaderStage::Vertex, 27U,
                     "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b",
                     {0x820140A0U, 0x8201410CU}},
      SourceIdentity{XenosShaderStage::Vertex, 15U,
                     "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0",
                     {0x82014140U, 0x8201417CU}},
  };
  const auto found = std::ranges::find_if(sources, [&](const auto &candidate) {
    return candidate.stage == shader.stage &&
           candidate.dwords == shader.size_dwords &&
           candidate.sha256 == shader.guest_big_endian_sha256;
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
  const std::uint64_t modification =
      type == rex::graphics::xenos::ShaderType::kVertex
          ? translator.GetDefaultVertexShaderModification(register_count)
          : translator.GetDefaultPixelShaderModification(register_count);
  auto *translation = source.GetOrCreateTranslation(modification);
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
  if (!draw.registers || draw.primitive != XenosPrimitive::RectangleList ||
      draw.source != XenosIndexSource::AutoIndex || draw.index_count != 3U ||
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
      if (next_loads.size() >= 4U &&
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
