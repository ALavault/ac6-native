#include "ac6/native_shader_translator.h"

namespace ac6::native {

ShaderTranslation ShaderTranslator::translate(
    ShaderFormat format, std::span<const std::uint32_t> words) {
  if (format == ShaderFormat::kXenosMicrocode) {
    return {{}, "Xenos microcode translation is not qualified for AC6"};
  }
  if (words.size() < 5u || words[0] != 0x07230203u || words[1] > 0x00010600u ||
      words[3] == 0u) {
    return {{}, "SPIR-V module header or bound is invalid"};
  }
  return {std::vector<std::uint32_t>(words.begin(), words.end()), {}};
}

}  // namespace ac6::native
