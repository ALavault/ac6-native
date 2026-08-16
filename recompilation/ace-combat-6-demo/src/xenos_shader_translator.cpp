#include "ac6demo/xenos_shader_translator.hpp"

#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"

#include <array>
#include <cctype>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace ac6demo {
namespace {

// The public demo evidence records five loads and these four unique payloads.
// It doesn't expose the proprietary payload words themselves.
constexpr std::array<XenosReachedShaderIdentity, 4> kReachedShaders{{
    {XenosShaderStage::Vertex, 0U, 24U,
     "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3"},
    {XenosShaderStage::Vertex, 0U, 27U,
     "93488cb9a7bbbb2f0a8bc9cf9cc6b4111102ccaba9e76d0a16ef65184ea0402b"},
    {XenosShaderStage::Pixel, 0U, 9U,
     "4913603d899eb3d5c8f5b3e2fa918ffb461320222f4748b233983ad8a2c98e25"},
    {XenosShaderStage::Vertex, 0U, 15U,
     "586168ec589613862294dae90f866303312abb8756318fa8d8633c8562a83cc0"},
}};

constexpr std::size_t kMaximumReachedDwords = 27U;
constexpr std::size_t kMaximumFetchSnapshots = 32U;
constexpr std::size_t kMaximumFetchWords = 32U;
constexpr std::size_t kMaximumConstantSnapshots = 16U;
constexpr std::size_t kMaximumConstantVectors = 512U;

void append_u8(std::vector<std::byte> &bytes, std::uint8_t value) {
  bytes.push_back(static_cast<std::byte>(value));
}

void append_u16(std::vector<std::byte> &bytes, std::uint16_t value) {
  append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
  append_u8(bytes, static_cast<std::uint8_t>(value));
}

void append_u32(std::vector<std::byte> &bytes, std::uint32_t value) {
  append_u8(bytes, static_cast<std::uint8_t>(value >> 24U));
  append_u8(bytes, static_cast<std::uint8_t>(value >> 16U));
  append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
  append_u8(bytes, static_cast<std::uint8_t>(value));
}

void append_words(std::vector<std::byte> &bytes,
                  std::span<const std::uint32_t> words) {
  for (const std::uint32_t word : words) {
    append_u32(bytes, word);
  }
}

void append_size(std::vector<std::byte> &bytes, std::size_t size) {
  if (size > std::numeric_limits<std::uint32_t>::max()) {
    throw RuntimeTrap("Xenos raw shader snapshot exceeds cache-key bounds");
  }
  append_u32(bytes, static_cast<std::uint32_t>(size));
}

std::vector<std::byte>
guest_bytes(std::span<const std::uint32_t> guest_word_values) {
  std::vector<std::byte> result;
  result.reserve(guest_word_values.size() * sizeof(std::uint32_t));
  append_words(result, guest_word_values);
  return result;
}

bool is_sha256(std::string_view value) {
  if (value.size() != 64U) {
    return false;
  }
  for (const char character : value) {
    if (!std::isdigit(static_cast<unsigned char>(character)) &&
        !(character >= 'a' && character <= 'f')) {
      return false;
    }
  }
  return true;
}

bool valid_fetch_kind(XenosRawFetchKind kind) {
  switch (kind) {
  case XenosRawFetchKind::VertexFull:
  case XenosRawFetchKind::VertexMini:
  case XenosRawFetchKind::Texture:
    return true;
  }
  return false;
}

const XenosReachedShaderIdentity *
find_reached_identity(const XenosRawShaderInput &input,
                      std::string_view actual_sha256) {
  for (const auto &identity : kReachedShaders) {
    if (identity.stage == input.stage &&
        identity.start_dword == input.start_dword &&
        identity.dword_count == input.guest_word_values.size() &&
        identity.guest_big_endian_sha256 == actual_sha256) {
      return &identity;
    }
  }
  return nullptr;
}

std::string cache_key(const XenosRawShaderInput &input) {
  std::vector<std::byte> bytes;
  bytes.reserve(kXenosRawShaderCacheSchema.size() +
                input.guest_word_values.size() * sizeof(std::uint32_t) + 64U);
  for (const char character : kXenosRawShaderCacheSchema) {
    append_u8(bytes, static_cast<std::uint8_t>(character));
  }
  append_u8(bytes, 0U);
  append_u8(bytes, static_cast<std::uint8_t>(input.stage));
  append_u16(bytes, input.start_dword);
  append_size(bytes, input.guest_word_values.size());
  append_words(bytes, input.guest_word_values);

  append_size(bytes, input.fetches.size());
  for (const auto &fetch : input.fetches) {
    append_u8(bytes, static_cast<std::uint8_t>(fetch.kind));
    append_u8(bytes, fetch.slot);
    append_size(bytes, fetch.guest_word_values.size());
    append_words(bytes, fetch.guest_word_values);
  }

  append_size(bytes, input.constants.size());
  for (const auto &constant : input.constants) {
    append_u16(bytes, constant.first_vector);
    append_size(bytes, constant.guest_word_values.size());
    append_words(bytes, constant.guest_word_values);
  }
  return Sha256::bytes(bytes);
}

std::string_view status_name(XenosRawShaderStatus status) {
  switch (status) {
  case XenosRawShaderStatus::InvalidMicrocodeShape:
    return "invalid-microcode-shape";
  case XenosRawShaderStatus::InvalidDeclaredIdentity:
    return "invalid-declared-identity";
  case XenosRawShaderStatus::IdentityMismatch:
    return "identity-mismatch";
  case XenosRawShaderStatus::InvalidConstantSnapshot:
    return "invalid-constant-snapshot";
  case XenosRawShaderStatus::UnsupportedFetchKind:
    return "unsupported-fetch-kind";
  case XenosRawShaderStatus::InvalidFetchSnapshot:
    return "invalid-fetch-snapshot";
  case XenosRawShaderStatus::UnreachedShaderIdentity:
    return "unreached-shader-identity";
  case XenosRawShaderStatus::UnqualifiedFetchSnapshot:
    return "unqualified-fetch-snapshot";
  case XenosRawShaderStatus::MissingSemanticEvidence:
    return "missing-semantic-evidence";
  }
  return "invalid-status";
}

XenosRawShaderInspection failure(XenosRawShaderStatus status,
                                 XenosRawShaderIr ir,
                                 std::string detail) {
  return XenosRawShaderInspection{status, std::move(ir), std::move(detail)};
}

} // namespace

std::span<const XenosReachedShaderIdentity>
XenosRawShaderTranslator::reached_shader_identities() noexcept {
  return kReachedShaders;
}

XenosRawShaderInspection
XenosRawShaderTranslator::inspect(const XenosRawShaderInput &input) const {
  XenosRawShaderIr ir;
  ir.stage = input.stage;
  ir.start_dword = input.start_dword;
  ir.fetch_snapshot_count = input.fetches.size();

  if (input.guest_word_values.empty() ||
      input.guest_word_values.size() % 3U != 0U ||
      input.guest_word_values.size() > kMaximumReachedDwords ||
      input.guest_word_values.size() >
          std::numeric_limits<std::uint16_t>::max() ||
      input.start_dword != 0U) {
    return failure(XenosRawShaderStatus::InvalidMicrocodeShape, std::move(ir),
                   "only reached start=0 payloads of 3-dword blocks up to 27 "
                   "dwords are admitted");
  }

  ir.dword_count =
      static_cast<std::uint16_t>(input.guest_word_values.size());
  ir.opaque_block_count = static_cast<std::uint16_t>(
      input.guest_word_values.size() / 3U);
  const auto raw_guest_bytes = guest_bytes(input.guest_word_values);
  ir.guest_big_endian_sha256 = Sha256::bytes(raw_guest_bytes);

  if (!is_sha256(input.declared_guest_big_endian_sha256)) {
    return failure(XenosRawShaderStatus::InvalidDeclaredIdentity,
                   std::move(ir),
                   "declared shader identity must be 64 lowercase hex digits");
  }
  if (input.declared_guest_big_endian_sha256 !=
      ir.guest_big_endian_sha256) {
    return failure(XenosRawShaderStatus::IdentityMismatch, std::move(ir),
                   "declared shader identity differs from guest bytes");
  }

  if (input.constants.size() > kMaximumConstantSnapshots) {
    return failure(XenosRawShaderStatus::InvalidConstantSnapshot,
                   std::move(ir), "too many constant snapshot ranges");
  }
  for (const auto &constant : input.constants) {
    if (constant.guest_word_values.empty() ||
        constant.guest_word_values.size() % 4U != 0U) {
      return failure(XenosRawShaderStatus::InvalidConstantSnapshot,
                     std::move(ir),
                     "constant snapshots must contain complete float4 "
                     "vectors");
    }
    const auto vectors = constant.guest_word_values.size() / 4U;
    if (vectors > kMaximumConstantVectors ||
        constant.first_vector > kMaximumConstantVectors - vectors ||
        ir.constant_vector_count > kMaximumConstantVectors - vectors) {
      return failure(XenosRawShaderStatus::InvalidConstantSnapshot,
                     std::move(ir),
                     "constant snapshot range exceeds the bounded file");
    }
    ir.constant_vector_count += vectors;
  }

  if (input.fetches.size() > kMaximumFetchSnapshots) {
    return failure(XenosRawShaderStatus::InvalidFetchSnapshot, std::move(ir),
                   "too many fetch snapshots");
  }
  for (const auto &fetch : input.fetches) {
    if (!valid_fetch_kind(fetch.kind)) {
      return failure(XenosRawShaderStatus::UnsupportedFetchKind,
                     std::move(ir),
                     "unknown Xenos fetch kind is fail-closed");
    }
    if (fetch.guest_word_values.empty() ||
        fetch.guest_word_values.size() > kMaximumFetchWords) {
      return failure(XenosRawShaderStatus::InvalidFetchSnapshot,
                     std::move(ir),
                     "fetch snapshot is empty or outside the word bound");
    }
  }

  ir.cache_key_sha256 = cache_key(input);

  ir.reached_identity =
      find_reached_identity(input, ir.guest_big_endian_sha256) != nullptr;
  if (!ir.reached_identity) {
    return failure(XenosRawShaderStatus::UnreachedShaderIdentity,
                   std::move(ir),
                   "shader stage/size/hash is outside the reached demo "
                   "catalogue");
  }

  // The evidence contains draw-level fetch addresses and register-state
  // hashes, but no descriptor words suitable for semantic translation.
  if (!input.fetches.empty()) {
    return failure(XenosRawShaderStatus::UnqualifiedFetchSnapshot,
                   std::move(ir),
                   "full/mini/texture fetch descriptor semantics are not "
                   "yet qualified for these demo shaders");
  }

  return failure(
      XenosRawShaderStatus::MissingSemanticEvidence, std::move(ir),
      "raw reached words and their opcode/fetch semantics are absent from "
      "the publishable evidence; SPIR-V emission is disabled");
}

std::vector<std::uint32_t> XenosRawShaderTranslator::translate_to_spirv(
    const XenosRawShaderInput &input) const {
  const auto inspection = inspect(input);
  throw RuntimeTrap("Xenos shader translation refused (" +
                    std::string(status_name(inspection.status)) + "): " +
                    inspection.detail);
}

} // namespace ac6demo
