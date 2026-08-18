#pragma once

#include <cstdint>

// XEX kernel *data* imports.
//
// The XEX import descriptor (optional header key 0x000103FF, file offset
// 0x2844) declares 292 import addresses for xboxkrnl.exe: 141 sixteen-byte
// thunk records at 0x82375984, and 151 four-byte slots in
// 0x82000560..0x820007BC. A slot holds (library index << 16) | ordinal until a
// loader writes the resolved address over it. The port patches neither, which
// is harmless for the 141 CODE imports -- XenonRecomp replaces those calls --
// and wrong for the ten CONST ones, whose *value* the guest reads. Their names
// and types come from the XDK's own import library,
// sdk/xdk-xenon-6132.6/XDK/lib/xbox/xboxkrnl.lib.
//
// Only the one with measured wrong behaviour is patched here. KeTimeStampBundle
// (ordinal 173, slot 0x82000700) is read by sub_821A5040, which is nothing but
//
//     return *(uint32_t *)(KeTimeStampBundle + 16);
//
// the guest's millisecond tick count. That function runs 23,644 times across a
// 12,000-tick run, and unpatched it dereferences 0x000100AD and returns zero
// every time. The other nine stay unpatched and are listed in
// reports/AC6_DEMO_TEN_UNPATCHED_KERNEL_DATA_IMPORTS.md rather than guessed at.
namespace ac6demo::guest_bridge_detail {

// Slot, and the unpatched encoding it must still hold. Patching a slot that
// holds anything else would overwrite a value this bridge did not put there,
// so the caller refuses instead.
constexpr std::uint32_t kKeTimeStampBundleSlot = 0x82000700U;
constexpr std::uint32_t kKeTimeStampBundleUnpatched = 0x000100ADU;

// A dedicated page, deliberately NOT taken from allocate_address. The bridge's
// allocator also serves the guest's own allocations, so consuming one page from
// it moved the guest heap and the Xenos scratch writeback buffer with it, which
// the contract pins to 0x16A5B000/0x16AE2000 -- the first attempt tripped
// "unqualified Xenos scratch writeback target" at tick 0. This address sits
// below the allocator's 0x10000000 origin and is mapped only here.
constexpr std::uint32_t kKernelDataImportsBase = 0x0F000000U;
constexpr std::uint32_t kKernelDataImportsSize = 0x1000U;

// InterruptTime (u64), SystemTime (u64), TickCount (u32). Only TickCount has a
// reader in this image; the rest stay zero rather than being invented.
constexpr std::uint32_t kKeTimeStampBundleSize = 24U;
constexpr std::uint32_t kKeTimeStampBundleTickCount = 16U;

// The qualified profile is 60 Hz, the same one ppc.cpp uses for its 50 MHz
// timebase, so a tick is 1000/60 ms.
constexpr std::uint32_t tick_count_milliseconds(std::uint64_t tick) noexcept {
  return static_cast<std::uint32_t>((tick * 1000ULL) / 60ULL);
}

// VdGlobalDevice, ordinal 446: the slot the guest writes its device object
// into (sub_821C64E8) and reads it back from (sub_821C5190). Unpatched, that
// round trip goes through 0x000101BE and works by accident.
constexpr std::uint32_t kVdGlobalDeviceSlot = 0x82000608U;

// Byte 10941 of the device object is a flag word; bit 1 gates the ring
// publication path in sub_821B9BC8, which runs 47,238 times across a
// 12,000-tick run while the ring write pointer never leaves 25.
constexpr std::uint32_t kDeviceFlagsByte = 10941U;

} // namespace ac6demo::guest_bridge_detail
