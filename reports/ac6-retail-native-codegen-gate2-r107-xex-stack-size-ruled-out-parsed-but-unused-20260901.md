# AC6 retail NTSC-U/J — the XEX's declared stack size is parsed but never consulted, and doesn't explain r106's value (r107)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No native code changed** — one temporary diagnostic line
added to the hand-maintained (tracked) `native/src/ac6recomp_main.cpp`,
one run, reverted from backup before this report. `ctest` (9/9)
reconfirmed clean afterward; `git status` on `native/` shows nothing
changed.

## Continuing r106's frontier

r106 pinpointed the exact guest address (`0x8feffd1c`) and value
(`0x400000`) behind r105's crash chain, and found it falls outside every
guest stack frame this project has traced, including `_xstart`'s own. It
named two directions: trace a fuller kernel/loader boot sequence, or
scope this as a probe-harness limitation.

## A concrete, checkable hypothesis: the XEX-declared default stack size

This project's own `native_xex.cpp` already parses a
`kHeaderDefaultStackSize` field (XEX optional-header tag `0x00020200`)
into `XexMetadata::stack_size` — a real, documented XEX format field
giving the title's requested default thread stack size. If real
hardware's loader uses this value when setting up a thread's initial
stack region (writing metadata near the top of the stack, or simply
bounding what memory is actually backed/meaningful there), it was a
plausible, checkable candidate for what should occupy the address r106
found — worth testing before assuming a much larger, harder-to-trace
kernel history is the only path forward.

## Checked directly: parsed, but never used, and doesn't explain the value

Added one diagnostic line printing `runtime->xex_metadata()->stack_size`
right after the existing "mapped XEX image" message in
`ac6recomp_main.cpp`, rebuilt, ran once (no probe entry needed — this
field is available immediately after XEX metadata parsing, before any
guest code runs):

```
[r107] xex stack_size=0x40000
```

**256 KiB.** Two things follow:

1. **This project's own harness never reads `stack_size` anywhere
   outside its own parser** (`grep -rn "stack_size"` across
   `ac6recomp_main.cpp`, `native_runtime.{h,cpp}`: no hits before this
   cycle's temporary addition). The probe hardcodes `r1 = 0x8ff00000`
   unconditionally, regardless of what the title's own XEX header
   requests. This is a real, independent gap from r105/r106's crash —
   noted, not fixed here, since it isn't established to be the actual
   cause of anything yet.
2. **256 KiB does not plausibly explain a 4 MiB / 8 MiB quantity.**
   Whatever real-hardware convention would place `0x400000` or
   `0x800000` at the specific stack depth r106 found (740 bytes below
   the harness's initial `r1`), it is not simply "the declared stack
   size" or an obvious arithmetic function of it. This hypothesis is
   ruled out by direct measurement, not by further reasoning about it.

## Decision

This is a documented negative result, in the same spirit as this
project's own standard for killing a plausible-but-uncontrolled
hypothesis (`CLAUDE.md` cites cycles 1111/1113 for exactly this pattern).
It does not by itself point at the real explanation, but it removes one
candidate cleanly rather than leaving it as an untested assumption for a
future cycle to redo. The stack-size-unused finding is worth keeping
in mind independently of r105/r106's crash — a probe that ignores a
title's declared stack size is a real fidelity gap in this harness — but
implementing anything from it now would be scope creep past what this
cycle actually established.

## Gates

No native code changed. `ctest` (native profile): 9/9. `git status` on
`native/`: clean.

## Next

1. r106's two options stand unchanged: trace a larger kernel/loader boot
   sequence (this project's static-disassembly toolchain has no clear
   path to that), or scope this as an explicit probe-harness limitation.
   This cycle's negative result narrows what "a fuller boot sequence"
   would need to explain — it is not simply about respecting the
   declared stack size.
2. If a future cycle does decide to make the probe honor
   `xex_metadata()->stack_size` (using it to size/place the initial
   guest stack region, rather than the current hardcoded
   `0x8ff00000`), that is a legitimate, independently-motivated harness
   fidelity improvement — but should not be sold as a fix for r105/r106's
   specific crash unless a future cycle actually verifies it changes the
   value at `0x8feffd1c`.
3. r100's older open thread (the `sub_821E6AC8`/`sub_821F03B0` wait chain,
   r94) stays probably moot for the same reason r101-r106 gave.
