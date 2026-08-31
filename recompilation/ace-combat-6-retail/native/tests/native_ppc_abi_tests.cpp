#ifdef NDEBUG
#error "Every check in this suite is an assert(); NDEBUG erases them and the \
suite then passes vacuously. Build this target with -UNDEBUG."
#endif

#include "ac6/native_ppc_abi.h"

#include <cassert>

int main() {
  ac6::native::GuestMemory memory(64u);
  assert(memory.store_u16(1u, 0x1234u));
  assert(memory.store_u32(4u, 0x12345678u));
  assert(memory.store_u64(8u, 0x0123456789ABCDEFu));
  std::uint16_t word16{};
  std::uint32_t word32{};
  std::uint64_t word64{};
  assert(memory.load_u16(1u, word16) && word16 == 0x1234u);
  assert(memory.load_u32(4u, word32) && word32 == 0x12345678u);
  assert(memory.load_u64(8u, word64) && word64 == 0x0123456789ABCDEFu);
  assert(!memory.load_u64(60u, word64));

  assert(memory.lwarx(4u, word32));
  assert(memory.stwcx(4u, 0xAABBCCDDu));
  assert((memory.condition_register() & 0x20000000u) != 0u);
  assert(memory.load_u32(4u, word32) && word32 == 0xAABBCCDDu);
  assert(memory.lwarx(4u, word32));
  assert(!memory.stwcx(8u, 1u));
  assert((memory.condition_register() & 0x20000000u) == 0u);
  assert(!memory.stwcx(4u, 2u));
  assert(!memory.lwarx(3u, word32));
  assert(memory.ldarx(8u, word64));
  assert(memory.stdcx(8u, 0xFEDCBA9876543210u));
  assert((memory.condition_register() & 0x20000000u) != 0u);

  ac6::native::PpcDispatcher dispatcher;
  ac6::native::PpcContext context;
  assert(context.vmx.size() == 128u);
  assert(dispatcher.bind(0x1000u, [](auto& ctx, auto& mem) {
    ctx.gpr[3] = 0x42u;
    (void)mem.store_u32(0u, 0xCAFEu);
  }));
  assert(!dispatcher.bind(0x1000u, [](auto&, auto&) {}));
  assert(dispatcher.call(0x1000u, context, memory));
  assert(context.gpr[3] == 0x42u);
  assert(!dispatcher.call(0x2000u, context, memory));
  return 0;
}
