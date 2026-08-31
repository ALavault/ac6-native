# AC6 retail NTSC-U/J — the stalled ring's own allocation returned NULL (r80)

Date: 2026-08-31.

## Method

r79 recommended stopping static call-graph tracing (76 callers of the
generic `sub_821E60A8` entry point) in favour of a live runtime read of the
actual stalled object's field values. This cycle did that: the existing
bounded, opt-in entry probe (`AC6_NATIVE_ALLOW_ENTRY_PROBE=1`,
`SDL_AUDIODRIVER=dummy`) was launched under GDB (`gdb --args ac6recomp
--probe-entry <assets>`), left running ~13s (matching r51/r75/r76's known
stall window), then interrupted with `SIGINT` sent directly to the traced
inferior (not to GDB) once it was reliably parked.

The live PPCContext is not memory-resident in this build
(`PPC_CONFIG_NON_VOLATILE_AS_LOCAL` is defined in `ppc_config.h`, confirmed
by reading `generated/ppc_context.h`) — non-volatile guest registers
(`r14`-`r31`) are compiler-local host registers, not struct fields. Reading
`__imp__sub_821E61A8`'s prologue (`mov (%rdi),%rbp` — copying `ctx.r3`,
offset 0, into `%rbp`) established that the PPC `r31`/"object" local is
carried in the host `%rbp` register for the lifetime of that function, and
the guest memory base pointer is carried in `%r14` (confirmed against
`PPC_IMAGE_BASE=0x82000000` guest addressing). This was verified twice,
independently, both giving the same values.

## Result

At the stall (`Thread 1` inside `sub_821E6AC8`, called from `sub_821E61A8`,
called from `sub_821E64A8`, called from `sub_821E65B0` — matching r78/r79's
static chain exactly):

- guest object address: **`0x1a0010`** (reproduced identically across two
  independent runs);
- guest memory base (host): `0x7ffef7000000`.

Reading the object's fields at the live host addresses (`base + object +
offset`), all big-endian dwords, byte order already accounted for by GDB's
raw byte dump matching zero either way:

| field | value |
|---|---|
| `+0x2abd` (flag byte) | `0x00` |
| `+0x540c` | `0x00000000` |
| `+0x34bc` (allocator pool ptr) | `0x00000000` |
| **`+0x2a90` (the pointer `sub_821E61A8` dereferences)** | **`0x00000000`** |
| `+0x2a9c` (limit, compared against) | `0x00000000` |
| `+0x30` / `+0x38` (write cursor / limit2) | `0x00000000` / `0x00000000` |
| `+0x2af8` (r77's counting fence) | `0x00000000` |
| `+0x2a94` | `0x00000000` |
| `+0x3a44` / `+0x3a48` (budget tracker) | `0x00000000` / `0x00000000` |

**Every relevant field is zero.** This is not a partially-progressed ring
stuck mid-flight — the object was never initialized at all.

## Diagnosis

`+0x2a90 == 0` is decisive: `sub_821E61A8` loads this as a pointer and
dereferences it (`lwz r10,0x0(r10)`); a null pointer, in this address space,
reads back as `0` (the reserved-but-unmapped guest range is zero-filled, per
`GuestAddressSpace`'s `MAP_NORESERVE` reservation — it does not fault). So
the wait's comparison (`C >= requested`) is `0 >= 4`, permanently false, by
construction — not because a completion signal never arrived, but because
the value being watched was never written in the first place.

r79 already found the only static writer of `+0x2a90`: `sub_821E65B0`
(`0x821e6740: stw r3,0x2a90(r31)`, directly storing the return value of an
allocator call `bl 0x821d74a8` with no null-check before the store — the
null-check happens *after*, as an early-exit branch `beq 0x821e69f8`). A
null store here means **the allocator returned null**, and the reset/init
routine's own recovery path (bail out early) is exactly what leaves every
downstream field at its zero-initialized default — matching the observation
above field-for-field.

Traced one level further (read-only, `sub_821D74A8`, its only caller from
this chain): it is a debug-tagged pool allocator wrapper — checks a category
byte, takes a lock (`bl 0x823d007c`), calls the real allocator
**`sub_82222d80`** with a heap handle loaded from a fixed global
(`lwz r3,-0x4690(r11)`), unlocks, and on a null result sets a fixed global
"allocation failed" flag byte. `sub_82222d80` itself (0x8222xxxx range, well
below the engine cluster this whole investigation has stayed in) is
**not yet examined** — not established whether it is guest-side heap logic,
a kernel import (`ExAllocatePool`-shaped), or something else.

## Decision

Do not implement anything yet. The chain is now: wait (r78) → fence
misdirection corrected, real gate is ring-space (r78) → gate function traced
to a suballocator (r79) → **that suballocator's own backing allocation
(`sub_82222d80`, via the global heap handle) returned null at the live
stall (r80)**. The next cycle's job is `sub_82222d80` and the global heap
handle it reads — static disassembly first (same discipline as this whole
thread), and if it bottoms out in a kernel import, cross-check which of the
229 stubs `materialize_native_import_stubs.py` currently generates for it
and whether that stub's generic `kOfflineStatus` fail-closed behavior (per
the Explore agent's finding from the first cycle of this thread) is the
actual root cause. Only implement a fix once that import (or guest-side
heap defect) is identified — writing a non-null placeholder into
`object+0x2a90` now would be exactly the synthetic-state shortcut this
thread has refused three times already (r53, r77, r79).

New tooling: none kept from this cycle (the GDB session was interactive,
not scripted into a reusable tool; `DumpUsAllocatorRoot.java`, a one-target
throwaway, was removed after use, matching r79's practice).
