#include <array>
#include <cstddef>
#include <cstdint>

struct RecordPoolRuntime {
    std::array<std::byte, 0x20> reserved{};
    std::uint32_t pool_base_address{};
};

static_assert(offsetof(RecordPoolRuntime, pool_base_address) == 0x20);

std::uint64_t FUN_822c3150(const RecordPoolRuntime* runtime,
                           const std::uint64_t packed_handle) {
    return (packed_handle & 0x03ffffffULL) * 0x40ULL +
           static_cast<std::uint64_t>(runtime->pool_base_address);
}