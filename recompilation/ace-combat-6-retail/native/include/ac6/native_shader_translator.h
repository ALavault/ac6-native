#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace ac6::native {

enum class ShaderFormat : std::uint8_t { kXenosMicrocode, kSpirv };

// The analyzed constant-register map of one pinned shader (the oracle
// translator's Shader::ConstantRegisterMap, captured offline). The float
// bitmap defines the COMPACTED float-constant layout the pinned SPIR-V
// indexes; dynamic float addressing means the raw bank order.
struct ConstantRegisterMapSnapshot final {
  std::uint64_t float_bitmap[4]{};
  std::uint32_t float_count{};
  std::uint32_t float_dynamic{};
  std::uint32_t loop_bitmap{};
  std::uint32_t bool_bitmap[8]{};
  std::uint32_t vertex_fetch_bitmap[3]{};
};

struct ShaderTranslation final {
  std::vector<std::uint32_t> spirv;
  ConstantRegisterMapSnapshot constant_map{};
  bool has_constant_map{false};
  std::uint64_t modification{};
  std::string error;

  [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

// Identity key for one qualified fetch: the IM_LOAD_IMMEDIATE envelope
// (type, start, dword count) plus a 64-bit FNV-1a digest over the exact
// microcode words. The digest is a lookup key, not a security claim;
// activation additionally requires byte-exact word equality.
struct UcodeFetchSignature final {
  std::uint32_t shader_type{};
  std::uint32_t start{};
  std::uint32_t dword_count{};
  std::uint64_t digest{};
};

class ShaderTranslator final {
 public:
  // Only pretranslated SPIR-V capsules are accepted in the product boundary.
  // Xenos microcode remains an explicit fail-closed input until its AC6 fetch
  // signatures are qualified against the pinned offline golden oracle.
  [[nodiscard]] static ShaderTranslation translate(
      ShaderFormat format, std::span<const std::uint32_t> words);

  // Translates retained IM_LOAD_IMMEDIATE microcode if and only if its fetch
  // signature exactly matches a pinned registration (envelope, digest, and
  // full word equality). Anything else is refused with no output.
  [[nodiscard]] static ShaderTranslation translate_ucode(
      std::uint32_t shader_type, std::uint32_t start,
      std::span<const std::uint32_t> microcode);

  // Pins one qualified (signature, SPIR-V) pair. Empty SPIR-V, digest
  // mismatch against the words, or envelope mismatch is rejected.
  // Returns false without touching the registry.
  static bool register_pinned(const UcodeFetchSignature& signature,
                              std::span<const std::uint32_t> microcode,
                              std::span<const std::uint32_t> spirv);

  // Pins one qualified pair WITH its analyzed constant-register map (the
  // compacted float-constant layout). Same rejection rules.
  static bool register_pinned(const UcodeFetchSignature& signature,
                              std::span<const std::uint32_t> microcode,
                              std::span<const std::uint32_t> spirv,
                              const ConstantRegisterMapSnapshot& constant_map,
                              std::uint64_t modification = 0u);

  // Resolves the microcode against a pinned variant with the EXACT runtime
  // modification (the capsule is keyed by (digest, modification); multi-
  // modification shaders have one entry per variant).
  [[nodiscard]] static ShaderTranslation translate_ucode(
      std::uint32_t shader_type, std::uint32_t start,
      std::span<const std::uint32_t> microcode, std::uint64_t modification);

  // Selective-variant resolution (r260): picks the registered variant of
  // the microcode whose modification's HIGH dword (bits 32..63: the
  // state-driven fields - depth-stencil mode, param gen, dynamic count)
  // matches the given value exactly; the low dword (the interpolator mask,
  // a property of the ucode pair) is taken from the variant itself. The
  // returned translation carries the variant's full modification. Fails
  // closed when no variant matches.
  [[nodiscard]] static ShaderTranslation translate_ucode_variant(
      std::uint32_t shader_type, std::uint32_t start,
      std::span<const std::uint32_t> microcode,
      std::uint64_t modification_high_value);

  [[nodiscard]] static std::uint64_t digest_words(
      std::span<const std::uint32_t> words) noexcept;

  // Number of entries currently pinned. Diagnostics and tests only; the
  // lookup path never reads it.
  [[nodiscard]] static std::size_t pinned_count() noexcept;

 private:
  [[nodiscard]] static std::mutex& registry_mutex() noexcept;
};

}  // namespace ac6::native
