#include "ac6/native_guest_memory.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
  ac6::native::GuestAddressSpace memory;
  assert(memory.valid());
  assert((reinterpret_cast<std::uintptr_t>(memory.base()) & 0x1fu) == 0u);
  constexpr std::uint32_t address = 0x82000000u;
  assert(memory.contains(address, 0x1000u));
  assert(!memory.contains(0xfffffff0u, 0x20u));
  assert(!memory.contains(0xffffffffu, std::numeric_limits<std::size_t>::max()));
  auto bytes = memory.view(address, 4u);
  assert(bytes.size() == 4u);
  bytes[0] = 0x12u;
  bytes[3] = 0x34u;
  assert(memory.base()[address] == 0x12u);
  assert(memory.base()[address + 3u] == 0x34u);
  assert(memory.view(0xfffffff0u, 0x20u).empty());
  return 0;
}
