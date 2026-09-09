#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "ac6/native_shader_translator.h"

namespace ac6::native {

// One decoded capsule record, ready for ShaderTranslator::register_pinned.
struct PinnedShaderEntry final {
  UcodeFetchSignature signature{};
  std::vector<std::uint32_t> microcode;
  std::vector<std::uint32_t> spirv;
  ConstantRegisterMapSnapshot constant_map{};
  std::uint64_t modification{};
};

// Fail-closed capsule parse: any structural error (header, version, seal,
// envelope, digest, SPIR-V header, trailing bytes) yields an empty vector
// and leaves the pinned registry untouched. The layout is defined by
// tools/materialize_pinned_shader_capsule.py, which also mirrors this parse.
[[nodiscard]] std::vector<PinnedShaderEntry> parse_pinned_shader_capsule(
    std::span<const std::uint8_t> capsule);

// Parses and registers every capsule entry. Returns false (registry
// untouched) on any parse or registration failure.
[[nodiscard]] bool register_pinned_shader_capsule(
    std::span<const std::uint8_t> capsule);

// The statically linked capsule generated from
// native/fixtures/pinned-shader-registry.v1.bin at configure time.
[[nodiscard]] std::span<const std::uint8_t> bundled_pinned_shader_registry();

// DEFINED by the configure-time generated source
// (native_pinned_shaders_data.cpp) from the fixture bytes. Never define it
// in the product tree; the fixture is the single source of truth.
[[nodiscard]] const char* pinned_shader_registry_base64() noexcept;

// Registers the bundled capsule exactly once; later calls are no-ops that
// return the first result. Returns false if the bundled capsule is absent
// or malformed (fail closed, registry untouched).
[[nodiscard]] bool register_bundled_pinned_shader_registry();

}  // namespace ac6::native
