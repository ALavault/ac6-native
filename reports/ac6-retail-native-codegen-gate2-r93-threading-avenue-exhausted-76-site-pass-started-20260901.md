# AC6 retail NTSC-U/J — the host-threading avenue is exhausted; the 76-call-site pass is started, not closed (r93)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle (measurement and static/live
verification only).

## Part 1 — the host-threading avenue is exhausted

Per r92's recommendation, this cycle resumed the frequency-driven survey
over a longer window and checked whether the r90/r91 host-threading fixes
moved the actual Gate 2 milestone forward.

- **60-second bounded probe, `AC6_NATIVE_IMPORT_TRACE=1`**: identical 100
  generic-fallback hits, identical per-name counts, as every prior 20-25s
  probe this session (r90-r92). No new hot spot at 3x the window.
- **40-second bounded probe, `AC6_NATIVE_VD_TRACE=1`**: identical sequence
  to the one established in r11/r56-r75 months ago (`PM4_ME_INIT` 19
  dwords, then a 12-dword IB batch, then nothing).
- **30-second GDB hit-counted breakpoint on `__imp__VdSwap`**: zero hits —
  confirmed expected, not new: `native_guest_vd.cpp`'s own comment
  (lines 135-138) documents that the first command buffer is published by
  a dedicated 1ms-interval background poller (`poll_loop()` →
  `poll_once()`), independent of `VdSwap`, before the guest ever reaches
  its first `VdSwap` call.

**Settled**: three cycles of host-threading work (r90-r92) produced real,
verified host-side corrections but zero measured change to the actual
milestone. The render pipeline is, today, in exactly the state r11 left it
in. This avenue, as a path to moving this specific milestone, is
exhausted.

## Part 2 — the deferred 76-call-site pass: started, with a real new finding

r79 first identified, and r88/r89 twice more declined, a full
cross-reference of `sub_821E60A8`'s 76 static call sites as premature.
With the threading avenue now confirmed flat, this cycle started that
pass rather than deferring it a fourth time.

`FindDirectCallsTo.java 0x821e60a8` against `ac6-us` confirmed exactly 76
call sites (matching r79's original count), distributed: 54 in the
`0x821Exxxx` range, 11 in `0x821Fxxxx`, 11 in `0x821Dxxxx`. Ten sites fall
in the same tight `0x821E6xxx`/`0x821F1xxx` cluster as the
already-characterized VD/wait subsystem (`sub_821E60A8` itself,
`sub_821E61A8`, `sub_821E63F0`, `sub_821E64A8`, `sub_821E65B0`,
`sub_821E6AC8`, and the interrupt-registration site `0x821f1220`) — the
highest-priority candidates for a caller specific to our object, checked
first.

Disassembling the ~10 bytes preceding each of these 10 calls found the
**same pattern at 6 of them** (`0x821e64d0`, `0x821e6584`, `0x821e6db8`,
`0x821e6f54`, `0x821f1780`, and — critically — inside `sub_821E64A8`
itself, already in the traced stall chain):

```
r3 = *(object+0x30)   // write cursor (r85: 0x162e017c)
r11 = *(object+0x38)  // a limit field, not previously named
if (r3 > r11) sub_821E60A8(object)   // overflow safety net
```

**New finding, not previously established**: `sub_821E64A8` is not only a
link in the wait chain (`sub_821E6AC8 ← sub_821E61A8 ← sub_821E64A8 ←
sub_821E65B0`, r85-r89) — it is itself a **ring-packet writer**. Its full
body (`0x821e64a8`-`0x821e651c`) shows: run the cursor-vs-limit overflow
check above; then unconditionally write two dwords (`0x5c8`,
`0x00020000` — shaped like a PM4 packet header/payload pair) at the
cursor via `stwu` (store-with-update, advancing `r3` by 4 each time);
write the advanced cursor back to `object+0x30`; then, if
`object+0x2a9c` (r85: `7`) is nonzero, call `sub_821E61A8(object, 4)` —
the exact wait this whole investigation traces — and finally spin on
`object+0x2af8` until it reads zero before returning.

**Live verification** (single raw-entry GDB breakpoint capture of `base`,
same method as r85/r88/r91, no register-reallocation ambiguity), at the
confirmed stall point inside `__imp__sub_821E6AC8`:

- `object+0x30` (cursor) = **`0x162e017c`** — matches r85 exactly.
- `object+0x38` (limit) = **`0x162eff60`** — matches, independently, the
  `limit=0x162eff60` already printed by the existing `AC6_NATIVE_VD_TRACE`
  discover-object log line (r75/r11-era), now identified as the same
  field `sub_821E64A8` compares the cursor against.
- `object+0x2af8` (the trailing spin field) = **`0x00000000`** — already
  satisfied; not the blocker.
- `*(0x164e0000)+0x0` (the actual wait target, r86) = raw bytes
  `05 00 00 00`, unchanged from r91's read of the same address two cycles
  ago — stable, not advancing.

**What this settles and what it doesn't**: cursor (`0x162e017c`) is
`0x162eff60 - 0x162e017c` ≈ `0xFDE4` (about 65,000) bytes below the limit
— nowhere near overflow. The overflow safety net that would call
`sub_821E60A8` is **not currently triggered**, and won't be until roughly
65KB more has been written at this cursor's rate (two dwords, 8 bytes, per
`sub_821E64A8` call) — this specific mechanism is not, on current
evidence, an active path to the unblock write either. This does not
reopen r88/r89's refutation of the interrupt-callback mechanism; it adds
a second, independently-checked mechanism (the overflow guard) to the set
of paths that do not currently reach `sub_821E60A8`'s completion latch for
this object.

## Decision

**Not closed.** 10 of 76 call sites checked; the highest-priority cluster
(same subsystem as the traced object) does not currently trigger, for a
concrete, measured reason (cursor far below the write-cursor limit) rather
than an assumption. The remaining 66 sites (mostly in the `0x821Dxxxx`
range, r79's "generic allocator reused across the engine" territory) have
not been checked and are the honest remaining unknown — per r79, these are
very plausibly unrelated subsystems operating on their own, different
objects, but that has not been verified for any of them individually.

**Next cycle**: either (a) batch-dump the guard context for the remaining
66 sites in one headless pass (the same technique used here, scaled up)
and triage for any that could plausibly address `0x10001a00` specifically
rather than a subsystem-local object, or (b) given `sub_821E64A8`'s
writer role is now understood, consider whether `sub_821E65B0`'s calling
loop (1112 bytes, called `sub_821E64A8` at least twice per the r90 xref
list) is itself gated on something the native runtime could plausibly
drive forward — a new, narrower lead than the interrupt-callback one r88
refuted, worth one bounded check before the full 66-site sweep.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on the same pre-existing,
  unrelated N2 evidence mismatch as r90-r92, not touched.
- `ctest` (native profile): **9/9** passed (no code changed this cycle).
- No files changed; nothing to stage for gates ordering beyond this
  report and the handoff trackers.

No scripts left behind (`DumpR93PriorityCallers.java`,
`DumpR93Sub64A8Full.java` used read-only against `ghidra-projects/ac6-us`
and deleted; confirmed by `git status --porcelain=v1 -- scripts/` showing
empty).
