#include "ac6/native_pinned_shaders.h"

#include <array>
#include <cstring>
#include <mutex>

namespace ac6::native {

namespace {

// Base64 decoder for the generated data payload (standard alphabet,
// padding required). Invalid input yields an empty vector: fail closed.
[[nodiscard]] std::vector<std::uint8_t> decode_base64(const char* text) noexcept {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static const std::array<std::int8_t, 256> table = [] {
    std::array<std::int8_t, 256> values{};
    values.fill(static_cast<std::int8_t>(-1));
    for (int index = 0; index < 64; ++index) {
      values[static_cast<std::size_t>(kAlphabet[index])] =
          static_cast<std::int8_t>(index);
    }
    return values;
  }();
  std::vector<std::uint8_t> out;
  const std::size_t length = std::strlen(text);
  if (length == 0u || length % 4u != 0u) {
    return out;
  }
  out.reserve(length / 4u * 3u);
  std::uint32_t accumulator = 0u;
  int bits = 0;
  bool padding_seen = false;
  for (std::size_t index = 0u; index < length; ++index) {
    const auto symbol = static_cast<std::uint8_t>(text[index]);
    if (symbol == '=') {
      // Padding only at the end, 1 or 2 symbols.
      const std::size_t remaining = length - index;
      if (remaining > 2u) {
        return {};
      }
      padding_seen = true;
      continue;
    }
    if (padding_seen) {
      return {};
    }
    const std::int8_t value = table[static_cast<std::size_t>(symbol)];
    if (value < 0) {
      return {};
    }
    accumulator = (accumulator << 6) | static_cast<std::uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 0xFFu));
    }
  }
  return out;
}

constexpr std::uint8_t kCapsuleMagic[8] = {'A', 'C', '6', 'P', 'S', 'R', '1', 0};
constexpr std::uint32_t kCapsuleVersion = 3u;
constexpr std::size_t kHeaderBytes = 24u;  // magic + version + count + seal
constexpr std::size_t kEntryHeaderBytes = 24u;
// v2: the per-entry constant-register map appended after the SPIR-V.
constexpr std::size_t kConstantMapBytes =
    32u + 12u + 32u + 12u;  // float_bitmap[4] + counts + bool[8] + fetch[3]
constexpr std::uint32_t kMaxMicrocodeDwords = 0xFFFFu;
constexpr std::uint32_t kMaxSpirvDwords = 1u << 20u;

std::mutex& bundled_once_mutex() noexcept {
  static std::mutex mutex;
  return mutex;
}

bool& bundled_once_state() noexcept {
  static bool state = false;
  return state;
}

[[nodiscard]] std::uint64_t digest_bytes_words(std::span<const std::uint8_t> bytes) noexcept {
  // FNV-1a64 over little-endian uint32 words, mirroring
  // ShaderTranslator::digest_words() so the seal and the entry digests use
  // one shared scheme over the logical guest words.
  std::uint64_t digest = 0xcbf29ce484222325ull;
  const std::size_t words = bytes.size() / 4u;
  for (std::size_t index = 0u; index < words; ++index) {
    std::uint32_t word = 0u;
    std::memcpy(&word, bytes.data() + index * 4u, sizeof(word));
    digest ^= word;
    digest *= 0x100000001b3ull;
  }
  return digest;
}

[[nodiscard]] std::uint32_t read_u32(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) noexcept {
  std::uint32_t value = 0u;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

[[nodiscard]] std::uint64_t read_u64(std::span<const std::uint8_t> bytes,
                                     std::size_t offset) noexcept {
  std::uint64_t value = 0u;
  std::memcpy(&value, bytes.data() + offset, sizeof(value));
  return value;
}

}  // namespace

std::vector<PinnedShaderEntry> parse_pinned_shader_capsule(
    std::span<const std::uint8_t> capsule) {
  if (capsule.size() < kHeaderBytes ||
      std::memcmp(capsule.data(), kCapsuleMagic, sizeof(kCapsuleMagic)) != 0) {
    return {};
  }
  if (read_u32(capsule, 8u) != kCapsuleVersion) {
    return {};
  }
  const std::uint32_t entry_count = read_u32(capsule, 12u);
  if (entry_count == 0u || entry_count > 0x10000u ||
      read_u64(capsule, 16u) != digest_bytes_words(capsule.subspan(kHeaderBytes))) {
    return {};
  }
  std::vector<PinnedShaderEntry> entries;
  entries.reserve(entry_count);
  std::size_t offset = kHeaderBytes;
  for (std::uint32_t index = 0u; index < entry_count; ++index) {
    if (capsule.size() - offset < kEntryHeaderBytes) {
      return {};
    }
    PinnedShaderEntry entry;
    entry.signature.shader_type = read_u32(capsule, offset);
    entry.signature.start = read_u32(capsule, offset + 4u);
    entry.signature.dword_count = read_u32(capsule, offset + 8u);
    const std::uint32_t spirv_dwords = read_u32(capsule, offset + 12u);
    entry.signature.digest = read_u64(capsule, offset + 16u);
    offset += kEntryHeaderBytes;
    if (entry.signature.shader_type > 1u || entry.signature.start != 0u ||
        entry.signature.dword_count == 0u ||
        entry.signature.dword_count > kMaxMicrocodeDwords ||
        spirv_dwords == 0u || spirv_dwords > kMaxSpirvDwords ||
        capsule.size() - offset <
            (static_cast<std::size_t>(entry.signature.dword_count) + spirv_dwords) * 4u) {
      return {};
    }
    const auto microcode_bytes =
        capsule.subspan(offset, static_cast<std::size_t>(entry.signature.dword_count) * 4u);
    offset += microcode_bytes.size();
    const auto spirv_bytes = capsule.subspan(offset, static_cast<std::size_t>(spirv_dwords) * 4u);
    offset += spirv_bytes.size();
    // The seal covers the whole payload, but each entry also re-verifies its
    // own digest and SPIR-V header: a corrupted capsule must fail closed.
    if (digest_bytes_words(microcode_bytes) != entry.signature.digest) {
      return {};
    }
    entry.microcode.resize(entry.signature.dword_count);
    std::memcpy(entry.microcode.data(), microcode_bytes.data(), microcode_bytes.size());
    entry.spirv.resize(spirv_dwords);
    std::memcpy(entry.spirv.data(), spirv_bytes.data(), spirv_bytes.size());
    // v2: the analyzed constant-register map. Fail closed on truncated
    // maps, out-of-range counts or inconsistent flags.
    if (capsule.size() - offset < kConstantMapBytes) {
      return {};
    }
    for (std::uint32_t word = 0u; word < 4u; ++word) {
      entry.constant_map.float_bitmap[word] = read_u64(capsule, offset + word * 8u);
    }
    offset += 32u;
    entry.constant_map.float_count = read_u32(capsule, offset);
    entry.constant_map.float_dynamic = read_u32(capsule, offset + 4u);
    entry.constant_map.loop_bitmap = read_u32(capsule, offset + 8u);
    offset += 12u;
    for (std::uint32_t word = 0u; word < 8u; ++word) {
      entry.constant_map.bool_bitmap[word] = read_u32(capsule, offset + word * 4u);
    }
    offset += 32u;
    for (std::uint32_t word = 0u; word < 3u; ++word) {
      entry.constant_map.vertex_fetch_bitmap[word] =
          read_u32(capsule, offset + word * 4u);
    }
    offset += 12u;
    if (entry.constant_map.float_dynamic > 1u ||
        entry.constant_map.float_count > 256u) {
      return {};
    }
    if (entry.constant_map.float_dynamic == 0u) {
      std::uint32_t bits = 0u;
      for (const std::uint64_t block : entry.constant_map.float_bitmap) {
        bits += static_cast<std::uint32_t>(
            __builtin_popcountll(block));
      }
      if (bits != entry.constant_map.float_count) {
        return {};
      }
    }
    // v3: the runtime modification the variant was translated with.
    if (capsule.size() - offset < 8u) {
      return {};
    }
    entry.modification = read_u64(capsule, offset);
    offset += 8u;
    if (entry.spirv[0] != 0x07230203u || entry.spirv[1] > 0x00010600u ||
        entry.spirv[3] == 0u) {
      return {};
    }
    entries.push_back(std::move(entry));
  }
  if (offset != capsule.size()) {
    return {};
  }
  return entries;
}

bool register_pinned_shader_capsule(std::span<const std::uint8_t> capsule) {
  const std::vector<PinnedShaderEntry> entries = parse_pinned_shader_capsule(capsule);
  if (entries.empty()) {
    return false;
  }
  for (const PinnedShaderEntry& entry : entries) {
    if (!ShaderTranslator::register_pinned(entry.signature, entry.microcode,
                                           entry.spirv, entry.constant_map,
                                           entry.modification)) {
      return false;
    }
  }
  return true;
}

std::span<const std::uint8_t> bundled_pinned_shader_registry() {
  // Single shared decode: the base64 payload is large and immutable.
  static const std::vector<std::uint8_t> decoded = decode_base64(pinned_shader_registry_base64());
  return decoded;
}

bool register_bundled_pinned_shader_registry() {
  std::lock_guard lock(bundled_once_mutex());
  bool& done = bundled_once_state();
  if (done) {
    return true;
  }
  const auto capsule = bundled_pinned_shader_registry();
  if (!register_pinned_shader_capsule(capsule)) {
    return false;
  }
  done = true;
  return true;
}

}  // namespace ac6::native
