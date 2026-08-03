#include "ac6_pac_index.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

void PutBE32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    data[offset + 0] = static_cast<uint8_t>(value >> 24);
    data[offset + 1] = static_cast<uint8_t>(value >> 16);
    data[offset + 2] = static_cast<uint8_t>(value >> 8);
    data[offset + 3] = static_cast<uint8_t>(value);
}

std::vector<uint8_t> MakeTable(uint32_t first_offset, uint32_t first_size,
                               uint32_t second_offset, uint32_t second_size) {
    std::vector<uint8_t> table(8 + 2 * 16, 0);
    PutBE32(table, 0, 2);
    PutBE32(table, 8 + 0, 0);
    PutBE32(table, 8 + 4, first_offset);
    PutBE32(table, 8 + 8, first_size);
    PutBE32(table, 8 + 12, first_size);
    PutBE32(table, 24 + 0, 0x01000000u);
    PutBE32(table, 24 + 4, second_offset);
    PutBE32(table, 24 + 8, second_size);
    PutBE32(table, 24 + 12, second_size);
    return table;
}

}  // namespace

int main() {
    // The old packed key overlapped the archive selector with offset bit 31.
    // These two rows must remain independently addressable.
    const auto table = MakeTable(0x80000000u, 0x100u, 0x00000100u, 0x100u);
    assert(ac6_pac_index::LoadFromBuffer(table.data(), table.size()));

    const auto data00 = ac6_pac_index::Find(false, 0x80000000u, 0x100u);
    const auto data01 = ac6_pac_index::Find(true, 0x00000100u, 0x100u);
    assert(data00 && data00->index == 0 && !data00->is_data01);
    assert(data01 && data01->index == 1 && data01->is_data01);

    const auto overlap00 = ac6_pac_index::FindOverlapping(false, 0x80000010u, 0x80000020u);
    const auto overlap01 = ac6_pac_index::FindOverlapping(true, 0x00000110u, 0x00000120u);
    assert(overlap00.size() == 1 && overlap00.front() == 0);
    assert(overlap01.size() == 1 && overlap01.front() == 1);

    // A 32-bit offset plus size that crosses the addressable PAC range is
    // malformed and must not replace a previously valid index.
    const auto wrapped = MakeTable(0xFFFFFF00u, 0x200u, 0x100u, 0x10u);
    assert(!ac6_pac_index::LoadFromBuffer(wrapped.data(), wrapped.size()));
    assert(ac6_pac_index::Find(false, 0x80000000u, 0x100u));
    return 0;
}
