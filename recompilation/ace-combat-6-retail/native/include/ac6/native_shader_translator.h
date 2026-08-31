#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ac6::native {

enum class ShaderFormat : std::uint8_t { kXenosMicrocode, kSpirv };

struct ShaderTranslation final {
  std::vector<std::uint32_t> spirv;
  std::string error;

  [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

class ShaderTranslator final {
 public:
  // Only pretranslated SPIR-V capsules are accepted in the product boundary.
  // Xenos microcode remains an explicit fail-closed input until its AC6 fetch
  // signatures are qualified against the pinned offline golden oracle.
  [[nodiscard]] static ShaderTranslation translate(
      ShaderFormat format, std::span<const std::uint32_t> words);
};

}  // namespace ac6::native
