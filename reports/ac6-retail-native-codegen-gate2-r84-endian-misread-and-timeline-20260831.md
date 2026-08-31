# AC6 retail NTSC-U/J — a GDB endianness bug in this thread's own method, and the real init timeline (r84)

Date: 2026-08-31.

## Executed the r83 recommendation

Built the temporary, uncommitted single-thread probe variant r83 proposed:
`ExCreateThread`'s worker spawn in
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`
gated behind `AC6_NATIVE_EXPERIMENT_SINGLE_THREAD` (unset in normal runs).
Regenerated `native-import-stubs.cpp` from the existing
`codegen-20260831-mapfix-96838` mapping, rebuilt `ac6recomp`, ran the
experiment, then reverted the source edit and regenerated/rebuilt back to
the original state (`git status` on the source file confirms clean; CTest
**9/9** after revert).

Note: the experiment did not eliminate every extra thread — one host thread
still appears (`NativeGuestVdService::poll_loop`, a native C++ service
thread the runtime spawns unconditionally, not a guest-code
`ExCreateThread` worker). It was still enough to get clean, single-shot
breakpoint hits without the ~16-18-thread contention that made r82's
watchpoints unreliable.

## What this found

Breaking at `__imp__sub_821E65B0`'s raw entry (confirmed at `<+0>`, so
`$rdi`/`$rsi` are the untouched SysV `ctx&`/`base` arguments) and continuing
through successive hits:

- **Call 1**: `object = 0x10001a00`, `object+0x2a9c` reads `0x00000000`.
  Disassembling the compiled gate at its exact host addresses (
  `cmpl $0x0,(%rsi,%rax,1)` / `je +86`, then the same for `+0x30`) confirms,
  byte for byte, that a zero value here **skips** the call into
  `sub_821E64A8` — my original static reading (r78-r83) was correct; this
  call took the "not ready yet" branch.
- **Call 2** (same object, moments later, still within the single-thread
  run): `object+0x2a9c` reads `0x05000000` — and the call **is** entered
  (`Breakpoint 2` at `sub_821E64A8`'s entry hit next, backtrace identical
  to every prior capture). This looked, at first, like a contradiction of
  r80/r81/r82's "all fields zero at the stall" finding.

**The `0x05000000` reading was a bug in this thread's own method, not a
fact about the guest.** Guest memory in this runtime is big-endian (every
generated load/store goes through `bswap` explicitly, confirmed in the
disassembly at `sub_821E65B0+20..+31`); GDB's `x/1xw` on an x86_64 host
prints raw bytes as little-endian. `0x05000000` read as bytes and
re-interpreted big-endian is `0x00000005` — i.e. the true guest value is
**5**. This matches exactly: `sub_821E65B0`'s own reset path
(`0x821e684c/0x821e6850`, newly read this cycle) initializes
`object+0x2a9c = 3` the first time it is zero, and `sub_821E5D60`'s known
`+= 2` (r79) after one push gives `3 + 2 = 5`. Every zero value in
r80-r83's prior reads is unaffected by this bug (zero is endian-invariant),
so **no earlier conclusion needs correction** — only this cycle's own new
`0x05000000` reading needed the fix, and it now resolves to a small,
sensible counter value instead of a confusing large one.

## The real remaining mystery, now sharper

`object+0x2a9c`'s only two static writers are `sub_821E5D60` (`+= 2`,
r79) and `sub_821E65B0`'s reset (`= 3`, only if the old value was zero,
this cycle). **Neither ever writes zero.** Yet r80 found it at zero, deep
in the same stalled call that started with it at 5 (Call 2 above). The
byte-order correction removes the false contradiction between "gate
requires nonzero" and "call 2 passed with a real nonzero value" — but it
sharpens, rather than resolves, the original question: something turns a
genuinely-nonzero counter back to zero later in the same call's execution,
and no static writer for that has been found yet.

## Decision

No implementation. Record the endianness correction as a standing note for
all future GDB reads in this investigation (`x/Nxb` and manual byte
reversal, or reading with an explicit big-endian expression, from here on —
plain `x/xw`/`x/xg` on guest memory in this runtime is misleading whenever
the true value is nonzero). Next cycle: with the endian bug fixed, redo a
GDB session that continues past Call 2 into the stall itself (not just to
the gate) and reads `object+0x2a9c` repeatedly through the loop, ideally at
several points, to catch the moment it changes — using `x/1xb` byte-by-byte
or a corrected big-endian read, and keeping the single-thread experimental
build for a clean signal.

No native code left changed (temporary edit reverted, build regenerated
and re-verified). Two throwaway dump scripts removed after use.
