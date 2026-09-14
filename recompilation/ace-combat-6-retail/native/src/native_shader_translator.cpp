#include "ac6/native_shader_translator.h"

#include <cstddef>
#include <cstdio>
#include <utility>

namespace ac6::native {

namespace {

struct PinnedEntry final {
  UcodeFetchSignature signature{};
  std::vector<std::uint32_t> microcode;
  std::vector<std::uint32_t> spirv;
  ConstantRegisterMapSnapshot constant_map{};
  bool has_constant_map{false};
  std::uint64_t modification{};
};

std::vector<PinnedEntry>& pinned_registry() {
  static std::vector<PinnedEntry> registry;
  return registry;
}

}  // namespace

std::mutex& ShaderTranslator::registry_mutex() noexcept {
  static std::mutex mutex;
  return mutex;
}

std::uint64_t ShaderTranslator::digest_words(
    std::span<const std::uint32_t> words) noexcept {
  std::uint64_t digest = 0xcbf29ce484222325ull;
  for (const std::uint32_t word : words) {
    digest ^= word;
    digest *= 0x100000001b3ull;
  }
  return digest;
}

std::size_t ShaderTranslator::pinned_count() noexcept {
  std::lock_guard lock(registry_mutex());
  return pinned_registry().size();
}

bool ShaderTranslator::register_pinned(
    const UcodeFetchSignature& signature, std::span<const std::uint32_t> microcode,
    std::span<const std::uint32_t> spirv) {
  return register_pinned(signature, microcode, spirv,
                         ConstantRegisterMapSnapshot{});
}

bool ShaderTranslator::register_pinned(
    const UcodeFetchSignature& signature, std::span<const std::uint32_t> microcode,
    std::span<const std::uint32_t> spirv,
    const ConstantRegisterMapSnapshot& constant_map, std::uint64_t modification) {
  if (spirv.empty() || signature.dword_count != microcode.size() ||
      signature.digest != digest_words(microcode)) {
    return false;
  }
  if (constant_map.float_dynamic > 1u || constant_map.float_count > 256u) {
    return false;
  }
  std::lock_guard lock(registry_mutex());
  PinnedEntry entry;
  entry.signature = signature;
  entry.microcode.assign(microcode.begin(), microcode.end());
  entry.spirv.assign(spirv.begin(), spirv.end());
  entry.constant_map = constant_map;
  entry.has_constant_map = true;
  entry.modification = modification;
  pinned_registry().push_back(std::move(entry));
  return true;
}

ShaderTranslation ShaderTranslator::translate_ucode(
    std::uint32_t shader_type, std::uint32_t start,
    std::span<const std::uint32_t> microcode) {
  const ShaderTranslation by_mod = translate_ucode(shader_type, start,
                                                   microcode, 0u);
  if (by_mod.ok()) {
    return by_mod;
  }
  // Legacy exact-digest resolution: first registered variant (the v2
  // behavior; used by tests that register one variant per digest).
  const std::uint64_t digest = digest_words(microcode);
  std::lock_guard lock(registry_mutex());
  for (const PinnedEntry& entry : pinned_registry()) {
    if (entry.signature.shader_type != shader_type ||
        entry.signature.start != start ||
        entry.signature.dword_count != microcode.size() ||
        entry.signature.digest != digest) {
      continue;
    }
    bool equal = true;
    for (std::size_t index = 0u; index < microcode.size(); ++index) {
      if (entry.microcode[index] != microcode[index]) {
        equal = false;
        break;
      }
    }
    if (equal) {
      ShaderTranslation result;
      result.spirv = entry.spirv;
      result.has_constant_map = entry.has_constant_map;
      result.constant_map = entry.constant_map;
      result.modification = entry.modification;
      return result;
    }
  }
  return by_mod;
}

ShaderTranslation ShaderTranslator::translate_ucode_variant(
    std::uint32_t shader_type, std::uint32_t start,
    std::span<const std::uint32_t> microcode,
    std::uint64_t modification_high_value,
    std::uint32_t required_interpolator_mask) {
  const std::uint64_t digest = digest_words(microcode);
  constexpr std::uint64_t kHighMask = 0xFFFFFFFF00000000ull;
  constexpr std::uint32_t kInterpolatorMask = 0xFFFFu;
  // The caller may pass the full modification (high dword + the paired PS's
  // interpolator mask); the high-dword filter below compares high dwords.
  const std::uint64_t required_high = modification_high_value & kHighMask;
  std::lock_guard lock(registry_mutex());
  const PinnedEntry* matched = nullptr;
  std::uint32_t matched_mask_popcount = 0u;
  for (const PinnedEntry& entry : pinned_registry()) {
    if (entry.signature.shader_type != shader_type ||
        entry.signature.start != start ||
        entry.signature.dword_count != microcode.size() ||
        entry.signature.digest != digest) {
      continue;
    }
    if ((entry.modification & kHighMask) != required_high) {
      continue;
    }
    const std::uint32_t declared_mask =
        static_cast<std::uint32_t>(entry.modification & kInterpolatorMask);
    if ((declared_mask & required_interpolator_mask) !=
        required_interpolator_mask) {
      continue;
    }
    const std::uint32_t mask_popcount =
        __builtin_popcount(declared_mask);
    if (matched != nullptr && mask_popcount >= matched_mask_popcount) {
      continue;
    }
    bool equal = true;
    for (std::size_t index = 0u; index < microcode.size(); ++index) {
      if (entry.microcode[index] != microcode[index]) {
        equal = false;
        break;
      }
    }
    if (equal) {
      matched = &entry;
      matched_mask_popcount = mask_popcount;
    }
  }
  if (matched == nullptr) {
    return {{}, {}, false, 0,
            "no pinned variant matches this draw state"};
  }
  ShaderTranslation result;
  result.spirv = matched->spirv;
  result.has_constant_map = matched->has_constant_map;
  result.constant_map = matched->constant_map;
  result.modification = matched->modification;
  return result;
}

ShaderTranslation ShaderTranslator::translate_ucode(
    std::uint32_t shader_type, std::uint32_t start,
    std::span<const std::uint32_t> microcode, std::uint64_t modification) {
  const std::uint64_t digest = digest_words(microcode);
  std::lock_guard lock(registry_mutex());
  for (const PinnedEntry& entry : pinned_registry()) {
    if (entry.signature.shader_type != shader_type ||
        entry.signature.start != start ||
        entry.signature.dword_count != microcode.size() ||
        entry.signature.digest != digest) {
      continue;
    }
    if (entry.microcode.size() != microcode.size()) {
      continue;
    }
    bool equal = true;
    for (std::size_t index = 0u; index < microcode.size(); ++index) {
      if (entry.microcode[index] != microcode[index]) {
        equal = false;
        break;
      }
    }
    if (equal && entry.modification == modification) {
      ShaderTranslation result;
      result.spirv = entry.spirv;
      result.has_constant_map = entry.has_constant_map;
      result.constant_map = entry.constant_map;
      result.modification = entry.modification;
      return result;
    }
  }
  char msg[128];
  std::snprintf(msg, sizeof(msg),
                "no pinned fetch signature matches this microcode "
                "(type=%u start=%u dwords=%zu digest=%016llx mod=%016llx)",
                shader_type, start, microcode.size(),
                static_cast<unsigned long long>(digest),
                static_cast<unsigned long long>(modification));
  return {{}, {}, false, 0, msg};
}

ShaderTranslation ShaderTranslator::translate(
    ShaderFormat format, std::span<const std::uint32_t> words) {
  if (format == ShaderFormat::kXenosMicrocode) {
    return {{}, {}, false, 0,
            "Xenos microcode translation is not qualified for AC6"};
  }
  if (words.size() < 5u || words[0] != 0x07230203u || words[1] > 0x00010600u ||
      words[3] == 0u) {
    return {{}, {}, false, 0,
            "SPIR-V module header or bound is invalid"};
  }
  ShaderTranslation result;
  result.spirv.assign(words.begin(), words.end());
  return result;
}

}  // namespace ac6::native
