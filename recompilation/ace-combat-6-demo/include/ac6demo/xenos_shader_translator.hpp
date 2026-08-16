#pragma once

#include "ac6demo/xenos_commands.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac6demo {

inline constexpr std::string_view kXenosRawShaderCacheSchema =
    "ac6-demo-xenos-raw-shader-cache/v1";

// These names describe the guest snapshot supplied at a draw. They don't
// imply that the corresponding fetch semantics have been qualified.
enum class XenosRawFetchKind : std::uint8_t {
  VertexFull = 0,
  VertexMini = 1,
  Texture = 2,
};

struct XenosRawFetchSnapshot final {
  XenosRawFetchKind kind{};
  std::uint8_t slot{};
  // Host integer values decoded from guest big-endian dwords. Cache-key
  // serialization writes them back in guest byte order.
  std::span<const std::uint32_t> guest_word_values;
};

struct XenosRawConstantSnapshot final {
  std::uint16_t first_vector{};
  // Consecutive float-constant vectors, four dwords per vector. Values are
  // opaque until their consumers are qualified.
  std::span<const std::uint32_t> guest_word_values;
};

struct XenosRawShaderInput final {
  XenosShaderStage stage{};
  std::uint16_t start_dword{};
  // Raw IM_LOAD_IMMEDIATE payload values after the PM4 reader's big-endian
  // decode. No shader container or reflection metadata is accepted here.
  std::span<const std::uint32_t> guest_word_values;
  std::string_view declared_guest_big_endian_sha256;
  std::span<const XenosRawFetchSnapshot> fetches;
  std::span<const XenosRawConstantSnapshot> constants;
};

struct XenosReachedShaderIdentity final {
  XenosShaderStage stage{};
  std::uint16_t start_dword{};
  std::uint16_t dword_count{};
  std::string_view guest_big_endian_sha256;
};

enum class XenosRawShaderStatus : std::uint8_t {
  InvalidMicrocodeShape,
  InvalidDeclaredIdentity,
  IdentityMismatch,
  InvalidConstantSnapshot,
  UnsupportedFetchKind,
  InvalidFetchSnapshot,
  UnreachedShaderIdentity,
  UnqualifiedFetchSnapshot,
  MissingSemanticEvidence,
};

struct XenosRawShaderIr final {
  XenosShaderStage stage{};
  std::uint16_t start_dword{};
  std::uint16_t dword_count{};
  // Xenos control-flow instructions are paired in opaque 3-dword blocks.
  // This is only an envelope count, not an opcode decode.
  std::uint16_t opaque_block_count{};
  std::string guest_big_endian_sha256;
  std::string cache_key_sha256;
  std::size_t fetch_snapshot_count{};
  std::size_t constant_vector_count{};
  bool reached_identity{};
};

struct XenosRawShaderInspection final {
  XenosRawShaderStatus status{XenosRawShaderStatus::InvalidMicrocodeShape};
  XenosRawShaderIr ir;
  std::string detail;
};

class XenosRawShaderTranslator final {
public:
  [[nodiscard]] static std::span<const XenosReachedShaderIdentity>
  reached_shader_identities() noexcept;

  // Inspection computes identities and the complete environment cache key,
  // but never claims opcode semantics or produces pixels.
  [[nodiscard]] XenosRawShaderInspection
  inspect(const XenosRawShaderInput &input) const;

  // Deliberately fail-closed until the reached raw words, opcode set and
  // fetch/constant interpretations are recorded in qualified evidence.
  [[nodiscard]] std::vector<std::uint32_t>
  translate_to_spirv(const XenosRawShaderInput &input) const;
};

} // namespace ac6demo
