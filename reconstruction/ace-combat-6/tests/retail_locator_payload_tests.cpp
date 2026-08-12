#include "ac6/retail_locator_payload.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <utility>

namespace {

using ac6::retail::RetailLocatorPayload;

static_assert(sizeof(RetailLocatorPayload) == 0x40);
static_assert(alignof(RetailLocatorPayload) == 16);
static_assert(std::is_trivially_copyable_v<RetailLocatorPayload>);
static_assert(noexcept(ac6::retail::copy_locator_payload(
    std::declval<RetailLocatorPayload&>(),
    std::declval<const RetailLocatorPayload&>())));

// Every value remains an integer bit pattern: no test setup converts through
// float and accidentally quiets a signalling NaN or normalizes signed zero.
constexpr std::array<std::uint32_t, 16> kSourceWords{{
    0x00000000U,  // +0.0
    0x80000000U,  // -0.0
    0x7F800000U,  // +infinity
    0xFF800000U,  // -infinity
    0x7FC12345U,  // positive qNaN, non-default payload
    0xFFC54321U,  // negative qNaN, non-default payload
    0x7FA12345U,  // positive sNaN payload
    0xFFA54321U,  // negative sNaN payload
    0x00000001U,
    0x007FFFFFU,
    0x00800000U,
    0x3F800000U,
    0xDEADBEEFU,
    0x01234567U,
    0x89ABCDEFU,
    0xFFFFFFFFU,
}};
static_assert(sizeof(kSourceWords) == ac6::retail::kRetailLocatorPayloadBytes);

struct alignas(16) GuardedPayload {
  std::uint32_t vptr_like;
  std::array<std::byte, 12> leading_canary;
  RetailLocatorPayload payload;
  std::array<std::byte, 16> trailing_canary;
};

static_assert(offsetof(GuardedPayload, payload) == 0x10);

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

void load_source_words(RetailLocatorPayload& payload) {
  std::memcpy(payload.bytes.data(), kSourceWords.data(),
              ac6::retail::kRetailLocatorPayloadBytes);
}

void test_copy_is_exact_and_bounded() {
  GuardedPayload source{};
  source.vptr_like = 0x82007A10U;
  source.leading_canary.fill(std::byte{0xA5});
  source.trailing_canary.fill(std::byte{0x5A});
  load_source_words(source.payload);

  GuardedPayload destination{};
  destination.vptr_like = 0x822A6710U;
  destination.leading_canary.fill(std::byte{0x3C});
  destination.payload.bytes.fill(std::byte{0xCC});
  destination.trailing_canary.fill(std::byte{0xC3});

  const auto source_leading = source.leading_canary;
  const auto source_trailing = source.trailing_canary;
  const auto destination_leading = destination.leading_canary;
  const auto destination_trailing = destination.trailing_canary;

  ac6::retail::copy_locator_payload(destination.payload, source.payload);

  check(std::memcmp(destination.payload.bytes.data(),
                    source.payload.bytes.data(), 0x40) == 0,
        "all 0x40 payload bytes are copied exactly");
  check(source.vptr_like == 0x82007A10U,
        "the source vptr-like word is unchanged");
  check(destination.vptr_like == 0x822A6710U,
        "the destination vptr-like word is unchanged");
  check(source.leading_canary == source_leading &&
            source.trailing_canary == source_trailing,
        "the source canaries are unchanged");
  check(destination.leading_canary == destination_leading &&
            destination.trailing_canary == destination_trailing,
        "the destination canaries are unchanged");

  const auto destination_snapshot = destination.payload.bytes;
  source.payload.bytes.front() = std::byte{0x12};
  source.payload.bytes.back() = std::byte{0x34};
  check(destination.payload.bytes == destination_snapshot,
        "the destination is independent after source mutation");
  check(std::memcmp(destination.payload.bytes.data(),
                    source.payload.bytes.data(), 0x40) != 0,
        "mutating the source does not alias the destination");
}

void test_self_copy_is_a_noop() {
  RetailLocatorPayload payload;
  load_source_words(payload);
  const auto before = payload.bytes;

  ac6::retail::copy_locator_payload(payload, payload);

  check(payload.bytes == before, "self-copy preserves every payload bit");
}

}  // namespace

int main() {
  test_copy_is_exact_and_bounded();
  test_self_copy_is_a_noop();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "retail_locator_payload=pass\n";
  return 0;
}
