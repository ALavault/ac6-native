#include <array>
#include <cstddef>
#include <cstdint>

struct Ace6GlobalRuntime {
    std::array<std::byte, 0x78> reserved_00{};
    std::uint32_t current_mode{};
};

static_assert(offsetof(Ace6GlobalRuntime, current_mode) == 0x78);

extern Ace6GlobalRuntime* PTR_DAT_826e4eb4;

void FUN_820f7d38([[maybe_unused]] const std::uint64_t caller_context,
                  std::uint32_t* const out_current_mode) {
    *out_current_mode = PTR_DAT_826e4eb4->current_mode;
}