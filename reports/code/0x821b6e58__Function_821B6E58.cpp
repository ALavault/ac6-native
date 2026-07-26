#include <array>
#include <cstddef>
#include <cstdint>

struct Ace6CurrentLevelRuntime {
    std::array<std::byte, 0x08> reserved_00{};
    std::uint32_t current_mode{};
};

static_assert(offsetof(Ace6CurrentLevelRuntime, current_mode) == 0x08);

struct Ace6GlobalRuntime {
    std::array<std::byte, 0x70> reserved_00{};
    Ace6CurrentLevelRuntime current_level_runtime{};
};

static_assert(offsetof(Ace6GlobalRuntime, current_level_runtime) == 0x70);
static_assert(offsetof(Ace6GlobalRuntime, current_level_runtime) +
                  offsetof(Ace6CurrentLevelRuntime, current_mode) ==
              0x78);

extern Ace6GlobalRuntime* PTR_DAT_826e4eb4;

extern const std::uint32_t DAT_82065840[];
extern const std::uint32_t DAT_82065880[];
extern const std::uint32_t DAT_820658b8[];

std::uint32_t FUN_820f61b0(const Ace6CurrentLevelRuntime* level_runtime);

std::uint64_t Function_821B6E58(std::uint64_t level_selector) {
    std::int32_t current_mode =
        static_cast<std::int32_t>(PTR_DAT_826e4eb4->current_level_runtime.current_mode);
    const std::uint32_t* resource_id_table = nullptr;
    std::uint32_t table_byte_offset = 0;

    if (current_mode == 4) {
        current_mode = static_cast<std::int32_t>(level_selector);
        if ((0x0d < current_mode) && (current_mode < 0x2b)) {
            if (((current_mode < 0x0e) && (current_mode < 0x15)) &&
                ((current_mode < 0x1c) && (current_mode < 0x23))) {
                return 0x76dULL;
            }
            return level_selector + 0x75fULL;
        }
        if (0x0dULL < (level_selector & 0xffffffffULL)) {
            level_selector = 0;
        }
        table_byte_offset =
            static_cast<std::uint32_t>((level_selector & 0x3fffffffULL) << 2);
        resource_id_table = DAT_82065880;
    } else if (current_mode == 3) {
        if (0x07ULL < (level_selector & 0xffffffffULL)) {
            level_selector = 0;
        }
        table_byte_offset =
            static_cast<std::uint32_t>((level_selector & 0x3fffffffULL) << 2);
        resource_id_table = DAT_820658b8;
    } else {
        if (((current_mode == 2) || (current_mode == 5)) &&
            (current_mode = static_cast<std::int32_t>(
                 FUN_820f61b0(&PTR_DAT_826e4eb4->current_level_runtime)),
             current_mode == 6)) {
            std::uint64_t mapped_resource_id = level_selector + 0x3c0ULL;
            if (0x0eULL < ((level_selector - 1ULL) & 0xffffffffULL)) {
                mapped_resource_id = 0x3c1ULL;
            }
            return mapped_resource_id + 0x39dULL;
        }
        if (0x0fULL < (level_selector & 0xffffffffULL)) {
            level_selector = 0;
        }
        table_byte_offset =
            static_cast<std::uint32_t>((level_selector & 0x3fffffffULL) << 2);
        resource_id_table = DAT_82065840;
    }

    return static_cast<std::uint64_t>(*reinterpret_cast<const std::uint32_t*>(
        reinterpret_cast<const std::byte*>(resource_id_table) + table_byte_offset));
}