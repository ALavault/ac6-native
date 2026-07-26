#include <cstddef>
#include <cstdint>

struct Ace6PackedSubrecordTable {
    std::int32_t subrecord_count{};
    std::uint32_t resource_blob_base_address{};
    std::uint32_t packed_descriptor_fields{};
    std::uint32_t subrecord_offset_table_address{};
};

static_assert(offsetof(Ace6PackedSubrecordTable, subrecord_count) == 0x00);
static_assert(offsetof(Ace6PackedSubrecordTable, resource_blob_base_address) == 0x04);
static_assert(offsetof(Ace6PackedSubrecordTable, packed_descriptor_fields) == 0x08);
static_assert(offsetof(Ace6PackedSubrecordTable, subrecord_offset_table_address) == 0x0c);

std::uint64_t FUN_82234dd0(const Ace6PackedSubrecordTable* const subrecord_table,
                           const std::int32_t subrecord_index) {
    std::uint32_t subrecord_offset;

    if ((subrecord_index < subrecord_table->subrecord_count) &&
        ((subrecord_offset = *reinterpret_cast<const std::uint32_t*>(
              static_cast<std::uintptr_t>(
                  static_cast<std::intptr_t>(subrecord_index) *
                      static_cast<std::intptr_t>(sizeof(std::uint32_t)) +
                  static_cast<std::intptr_t>(
                      subrecord_table->subrecord_offset_table_address)))),
         subrecord_offset != 0U)) {
        return static_cast<std::uint64_t>(subrecord_table->resource_blob_base_address) +
               static_cast<std::uint64_t>(subrecord_offset);
    }

    return 0;
}