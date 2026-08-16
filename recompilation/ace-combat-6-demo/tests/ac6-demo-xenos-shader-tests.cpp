#include "ac6demo/xenos_shader_translator.hpp"

#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>

namespace {

std::string digest(std::span<const std::uint32_t> words) {
  std::array<std::byte, 27U * sizeof(std::uint32_t)> bytes{};
  assert(words.size() <= 27U);
  for (std::size_t index = 0; index < words.size(); ++index) {
    const std::uint32_t word = words[index];
    bytes[index * 4U] = static_cast<std::byte>(word >> 24U);
    bytes[index * 4U + 1U] = static_cast<std::byte>(word >> 16U);
    bytes[index * 4U + 2U] = static_cast<std::byte>(word >> 8U);
    bytes[index * 4U + 3U] = static_cast<std::byte>(word);
  }
  return ac6demo::Sha256::bytes(
      std::span<const std::byte>(bytes).first(words.size() * 4U));
}

void test_reached_catalogue_is_exact_and_payload_free() {
  const auto identities =
      ac6demo::XenosRawShaderTranslator::reached_shader_identities();
  assert(identities.size() == 4U);
  assert(identities[0].stage == ac6demo::XenosShaderStage::Vertex);
  assert(identities[0].start_dword == 0U &&
         identities[0].dword_count == 24U);
  assert(identities[0].guest_big_endian_sha256 ==
         "099625f3ea15a92e74e525503b3e41302fc268bc8845da6100c991f67321e4e3");
  assert(identities[1].dword_count == 27U);
  assert(identities[2].stage == ac6demo::XenosShaderStage::Pixel &&
         identities[2].dword_count == 9U);
  assert(identities[3].stage == ac6demo::XenosShaderStage::Vertex &&
         identities[3].dword_count == 15U);
  for (std::size_t left = 0; left < identities.size(); ++left) {
    assert(identities[left].guest_big_endian_sha256.size() == 64U);
    for (std::size_t right = left + 1U; right < identities.size(); ++right) {
      assert(identities[left].guest_big_endian_sha256 !=
             identities[right].guest_big_endian_sha256);
    }
  }
}

void test_big_endian_identity_and_fail_closed_envelope() {
  const std::array<std::uint32_t, 3> words{
      0x01020304U, 0xAABBCCDDU, 0x10203040U};
  const std::string identity = digest(words);
  assert(identity ==
         "8fc7e5e86a71b18c902bf3c4ff03c6f96e962680bad6b45e8b0b136f37d042be");

  const ac6demo::XenosRawShaderTranslator translator;
  const ac6demo::XenosRawShaderInput input{
      ac6demo::XenosShaderStage::Vertex, 0U, words, identity, {}, {}};
  const auto inspection = translator.inspect(input);
  assert(inspection.status ==
         ac6demo::XenosRawShaderStatus::UnreachedShaderIdentity);
  assert(inspection.ir.dword_count == 3U &&
         inspection.ir.opaque_block_count == 1U);
  assert(inspection.ir.guest_big_endian_sha256 == identity);
  assert(!inspection.ir.reached_identity);

  const std::array<std::uint32_t, 4> malformed{1U, 2U, 3U, 4U};
  const std::string malformed_identity = digest(malformed);
  const auto malformed_result = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, malformed,
       malformed_identity, {}, {}});
  assert(malformed_result.status ==
         ac6demo::XenosRawShaderStatus::InvalidMicrocodeShape);

  const auto mismatch = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words,
       "0000000000000000000000000000000000000000000000000000000000000000",
       {}, {}});
  assert(mismatch.status == ac6demo::XenosRawShaderStatus::IdentityMismatch);
}

void test_cache_key_covers_stage_fetches_and_constants() {
  const std::array<std::uint32_t, 3> words{1U, 2U, 3U};
  const std::string identity = digest(words);
  const ac6demo::XenosRawShaderTranslator translator;
  const auto base = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words, identity, {}, {}});
  const auto repeated = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words, identity, {}, {}});
  const auto pixel = translator.inspect(
      {ac6demo::XenosShaderStage::Pixel, 0U, words, identity, {}, {}});
  assert(base.ir.cache_key_sha256.size() == 64U);
  assert(base.ir.cache_key_sha256 == repeated.ir.cache_key_sha256);
  assert(base.ir.cache_key_sha256 != pixel.ir.cache_key_sha256);

  const std::array<std::uint32_t, 4> constant_words{1U, 2U, 3U, 4U};
  const std::array<ac6demo::XenosRawConstantSnapshot, 1> constants{{
      {7U, constant_words},
  }};
  const auto with_constants = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words, identity, {}, constants});
  assert(with_constants.ir.constant_vector_count == 1U);
  assert(base.ir.cache_key_sha256 != with_constants.ir.cache_key_sha256);

  const std::array<std::uint32_t, 2> fetch_words{0x11111111U, 0x22222222U};
  const std::array<ac6demo::XenosRawFetchSnapshot, 1> fetches{{
      {ac6demo::XenosRawFetchKind::VertexMini, 3U, fetch_words},
  }};
  const auto with_fetch = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words, identity, fetches, {}});
  assert(base.ir.cache_key_sha256 != with_fetch.ir.cache_key_sha256);
}

void test_unknown_snapshots_and_spirv_emission_trap() {
  const std::array<std::uint32_t, 3> words{1U, 2U, 3U};
  const std::string identity = digest(words);
  const ac6demo::XenosRawShaderTranslator translator;

  const std::array<std::uint32_t, 3> incomplete_constants{1U, 2U, 3U};
  const std::array<ac6demo::XenosRawConstantSnapshot, 1> bad_constants{{
      {0U, incomplete_constants},
  }};
  const auto bad_constant = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words, identity, {},
       bad_constants});
  assert(bad_constant.status ==
         ac6demo::XenosRawShaderStatus::InvalidConstantSnapshot);

  const std::array<std::uint32_t, 1> fetch_words{1U};
  const std::array<ac6demo::XenosRawFetchSnapshot, 1> unknown_fetch{{
      {static_cast<ac6demo::XenosRawFetchKind>(0xFFU), 0U, fetch_words},
  }};
  const auto bad_fetch = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words, identity,
       unknown_fetch, {}});
  assert(bad_fetch.status ==
         ac6demo::XenosRawShaderStatus::UnsupportedFetchKind);

  const std::array<ac6demo::XenosRawFetchSnapshot, 1> empty_fetch{{
      {ac6demo::XenosRawFetchKind::VertexFull, 0U, {}},
  }};
  const auto invalid_fetch = translator.inspect(
      {ac6demo::XenosShaderStage::Vertex, 0U, words, identity,
       empty_fetch, {}});
  assert(invalid_fetch.status ==
         ac6demo::XenosRawShaderStatus::InvalidFetchSnapshot);

  bool trapped = false;
  try {
    static_cast<void>(translator.translate_to_spirv(
        {ac6demo::XenosShaderStage::Vertex, 0U, words, identity, {}, {}}));
  } catch (const ac6demo::RuntimeTrap &error) {
    trapped = true;
    assert(std::string(error.what()).find("unreached-shader-identity") !=
           std::string::npos);
  }
  assert(trapped);
}

} // namespace

int main() {
  test_reached_catalogue_is_exact_and_payload_free();
  test_big_endian_identity_and_fail_closed_envelope();
  test_cache_key_covers_stage_fetches_and_constants();
  test_unknown_snapshots_and_spirv_emission_trap();
  std::cout << "AC6 demo Xenos raw shader tests passed\n";
  return 0;
}
