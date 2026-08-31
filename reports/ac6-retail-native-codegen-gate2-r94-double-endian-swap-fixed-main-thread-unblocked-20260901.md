# AC6 retail NTSC-U/J — a double byte-swap in EVENT_WRITE_SHD delivery fixed; the r85-r93 stall is genuinely resolved (r94)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed: `native/src/native_guest_vd.cpp` (fixes
`EVENT_WRITE_SHD` delivery), all 8 `native/tests/*.cpp` files (adds the
project's own `#ifdef NDEBUG / #error` assert-liveness guard, previously
missing from the entire retail track; one new test added to
`native_xenos_tests.cpp`).

## How this cycle got here

Consulted the advisor before continuing r93's 66-remaining-sites sweep.
It connected three facts already on record but never linked: (1) the raw
bytes at `*(0x164e0000)+0x0` had been read identically — `05 00 00 00` —
across three independent cycles (r89, r91, r93) and never explained; (2)
`allocate_guest` memsets new allocations to zero, ruling out "leftover
allocator content"; (3) `drain_locked()`'s `EventWriteShdPacket` handler
(`native_guest_vd.cpp:261-272`) writes to *dynamically resolved* guest
addresses via `gpu_swap()` + `store_guest_word()` — a path r89's "nothing
writes this offset" claim never actually excluded, since that check only
covered static fixed-offset writes.

## The bug, verified twice

Added a permanent trace line to the `EventWriteShd` success path (same
`AC6_NATIVE_VD_TRACE` idiom used throughout this session) and ran the
existing bounded probe. It found real, live event writes targeting
`0x164e0000` — the exact address this investigation has stared at since
r86:

```
vd event write guest=0x164e0000 raw_value=0x00000003 endian=2 stored=0x03000000
vd event write guest=0x164e0000 raw_value=0x00000005 endian=2 stored=0x05000000
```

`stored=0x05000000` matches the raw bytes read in r89/r91/r93 exactly.
Reading `native_guest_vd.cpp`'s pipeline precisely: `gpu_swap(value, Endian::k8In32)`
calls `__builtin_bswap32(value)` — this **emulates the Xenos GPU's own
byte-lane swap unit**, so its output already *is* the final byte pattern
the hardware would place in memory. The code then passed that result
through `store_guest_word()`, which applies its *own* `bswap32` (correct
for a normal host-native logical value, wrong for a value that has
already been hardware-swapped) before a raw `memcpy`. Two swaps compound
instead of cancelling out only one intended transformation.

**Verified algebraically against both observed live values before writing
any code** (`python3`, exact arithmetic, not estimation):

| raw guest value | current (buggy) delivered bytes | fixed delivered bytes | fixed BE interpretation |
|---|---|---|---|
| `0x00000005` (a fence value) | `05 00 00 00` | `00 00 00 05` | `5` |
| `0x162e00d4` (a second event, guest=`0x164e0004`) | `d4 00 2e 16` (not a valid address) | `16 2e 00 d4` | `0x162e00d4` — a clean address in the exact `0x162Exxxx` ring range this investigation has tracked since r85 |

Both cases confirm the fix independently: the buggy path corrupts a
literal small fence value into a huge, meaningless number, and corrupts a
legitimate guest address into a byte-reversed non-address. The fixed path
recovers the plausible, small fence value and the plausible, in-range
address in both cases.

## Fix

Added `store_guest_bytes_raw()` next to `store_guest_word()`/`load_guest_word()`
in `native_guest_vd.cpp` — a raw `memcpy` with no further transformation,
documented as the correct sink for anything that has already passed
through `gpu_swap()`. The `EventWriteShd` handler now calls it instead of
`store_guest_word()`. No other call site in the file uses `gpu_swap()`, so
this is the only place the bug could occur.

## Verified live: the fix changes runtime behavior, isolated by A/B rebuild

Before drawing any conclusion, rebuilt and ran the bounded probe **with
the fix reverted** (`git stash` on just this file) to confirm the
established r11/r56-r93 behavior was unchanged by anything else this
cycle: identical, single "vd publish write=31 read=19 / vd drain accepted
consumed_dwords=12" sequence, no repeat, matching r93 exactly. Restored
the fix and rebuilt again.

**With the fix applied**, the same probe instead shows the same publish
step repeating continuously (tens of thousands of times over 30s), now
failing to decode: `vd drain rejected decode_ok=0 code=5 ... TYPE0
register range exceeds Xenos state (IB 0x308019200, header 0x346112)`.
This is a **new, separate, currently-unexplained decoder rejection** —
not investigated further this cycle, named below as the next frontier.

## The r85-r93 stall is genuinely resolved

Attached GDB to the fixed binary and interrupted the main thread (thread
1) after ~20 seconds. It is **not** the same stall: it is inside
`sub_821E61A8`/`sub_821E6AC8` again (the same generic wait function this
whole investigation has traced), but reached through an entirely
different, never-before-seen call chain:

```
#0  __imp__sub_821E6AC8
#1  __imp__sub_821E61A8
#2  __imp__sub_821F03B0
#3  __imp__sub_8234F558
#4  __imp__sub_8233E0A8
#5  __imp__sub_8233B5A0
```

Every prior cycle (r85-r93) observed exactly `sub_821E6AC8 ←
sub_821E61A8 ← sub_821E64A8 ← sub_821E65B0 ← sub_8234F2C8`. None of
`sub_821F03B0`, `sub_8234F558`, `sub_8233E0A8`, `sub_8233B5A0` have
appeared in this investigation before. The main thread returned from the
old chain, executed real, previously-unreached guest code through at
least four new functions, and is now blocked on a **new** instance of the
same wait mechanism — plausibly a different object, not yet identified.

This is the milestone the r85-r89/r93 static and live investigation was
aimed at: the specific, exhaustively-characterized stall this session
inherited is resolved, not worked around, and not asserted from static
reasoning alone — confirmed by a live before/after GDB comparison against
an isolated, single-file, algebraically-verified fix.

## What is not established, stated plainly

- **What the new stall's object is**, and whether it is a legitimately
  different subsystem or the same producer/consumer pattern recurring
  further along. Not read this cycle.
- **What the new decoder rejection** (`TYPE0 register range exceeds
  Xenos state`, `IB 0x308019200`) actually is, or whether it is itself
  now the binding constraint on further Vd progress. The `IB` address
  (`0x308019200`) exceeds 32 bits, which is suspicious and worth checking
  first — but not characterized here.
- No claim of boot, title, menus, cinematic, or gameplay. The probe still
  times out; this is confirmed forward progress past a specific,
  long-investigated block, not a new milestone reached.

## A separate, incidentally-discovered gap: fixed

While verifying this fix's test would actually execute, found the entire
retail native test suite (`native/tests/*.cpp`, 8 files) uses bare
`assert()` with no `#ifdef NDEBUG / #error` guard — the exact defect
class `CLAUDE.md` documents as already fixed elsewhere in the project
(the demo track) but never applied to retail. CMake already forces
`-UNDEBUG` on these test targets (`AC6_TEST_NO_NDEBUG`, confirmed in
`native/CMakeLists.txt`), so this was not an active silent-pass bug — but
it was one CMake-flag removal away from becoming one, with no source-level
defense. `tools/audit_test_assert_liveness.py` now passes clean (`suites=8
vacuous=0`) after adding the guard to all 8 files.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on the same pre-existing,
  unrelated N2 evidence mismatch as r90-r93
  (`reconstruction/ace-combat-6/src/retail_session.cpp`, not touched).
- `ctest` (native profile): **9/9** passed, including the new
  `guest_vd_service_event_write_applies_single_endian_swap` test.
- `pytest` (full retail suite): **130/130** passed (unchanged from r93 —
  no Python-side changes this cycle).
- `audit_test_assert_liveness.py`: **pass, suites=8 vacuous=0** (was
  8 failures before this cycle).
- `git status` after `ctest`: only the 9 intentionally-edited files
  changed.

No scripts left behind this cycle.

## Next

The new decoder rejection (`TYPE0 register range exceeds Xenos state`,
`IB 0x308019200`) is now the concrete, measured frontier — a real
repeating busy-loop of its own (the poller retries every 1ms with no
backoff), separate from and downstream of everything this session closed.
Characterizing it — starting with why an indirect-buffer address would
exceed 32 bits — is the natural next cycle, not undertaken here given
this cycle's scope. Identifying the new stalled object (via the same
raw-entry-capture method used throughout r85-r94) is the other open
thread.
