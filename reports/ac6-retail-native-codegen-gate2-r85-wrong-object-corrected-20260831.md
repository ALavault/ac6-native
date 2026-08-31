# AC6 retail NTSC-U/J — r77 through r84 traced the wrong object; the real one is the already-known Vd ring (r85)

Date: 2026-08-31.

## Correction

r78 established the "stalled object" address as `0x1a0010` by selecting
`frame 1` (`sub_821E61A8`, while GDB was stopped at frame 0 inside
`sub_821E6AC8`) and reading `print/x $rbp`. Every report since (r78
through r84) built on that address, without re-deriving it by a method
that does not depend on GDB reconstructing a callee-saved register's value
for a non-innermost frame in a binary with no debug info.

This cycle re-derived the object a different way — breaking at the true,
unambiguous entry of `__imp__sub_821E64A8` (confirmed at `<+0>`, before any
instruction runs) and reading `*(unsigned long*)$rdi`, which is `ctx.r3`
(the incoming argument) at a point where no register reallocation has
happened yet — and got **`object = 0x10001a00`**, not `0x1a0010`. These
are different numbers, not a formatting artifact (5 hex digits vs. 8).

`0x10001a00` is independently corroborated: it is exactly the address
already named, months of cycles ago, in
`reports/ac6-retail-native-codegen-gate2-r11-20260831.md` (the r53 slice):
*"confirmé l'objet à `0x10001a00`, le ring `0x162d0000` et le readback"* —
the known, already-qualified Vd/PM4 ring object from the r56-r75 work,
established by a completely independent investigation before this
`0x1a0010` thread (r77) ever started. `0x1a0010` matches nothing previously
established anywhere in this project's history.

**Root cause of the error**: `frame 1` + register read, on a stripped
binary, is not a reliable way to recover a caller's register value in this
setup. `sub_821E6AC8` (frame 0) has its own independent `push rbp` /
internal use of `%rbp`; GDB's display of "`frame 1`'s `%rbp`" most likely
reflected frame 0's *current* `%rbp` (a `sub_821E6AC8`-local value that
happens to look like a plausible small guest address) rather than a
properly CFI-unwound value belonging to `sub_821E61A8`. This was never
cross-checked against an independent source until this cycle.

## What changes with the correct object

Re-running the single-thread experimental probe (r83/r84's temporary,
uncommitted `AC6_NATIVE_EXPERIMENT_SINGLE_THREAD` no-op of `ExCreateThread`,
reverted after use as before; CTest 9/9 confirmed clean afterward) with the
verified object:

- `object+0x2a9c` (the wait's compared "limit"): **`0x00000007`**
  (read byte-by-byte, `x/4xb`, no endianness ambiguity).
- `object+0x30` (write cursor): **`0x162e017c`** — in the same region as
  the already-known ring `0x162d0000`.
- `object+0x2a90` (the pointer the wait dereferences): **`0x164e0000`** —
  **exactly** the readback block address already named in the r53 report
  ("le bloc de readback `0x164e0000`").

All three values are **stable and unchanged across 200 consecutive
iterations** of `sub_821E61A8`'s backoff loop (single-threaded, deterministic,
no contention). None of r77-r84's "null object" / "fields read zero" /
"contradiction" findings apply to this object — they were built on a
different, spurious address the whole time. That entire storyline
(r77-r84) is superseded by this correction, not merely refined.

## What this actually is

With the real object, `object+0x2a90` dereferences to the **already-known
Vd readback address** and `object+0x30` is a cursor near the **already-known
ring address**. This reconnects the investigation to ground already
established in the r56-r75 work (`STATE.md`'s "r64-r75" slice): the native
Vd/PM4 service publishes indices and a readback at `state+60`, and accepts
`PM4_ME_INIT`/IB batches — but per that same slice, "le consommateur PM4/Vd
natif n'est pas encore relié" (r53) and the probe was already known to
expire after IB consumption with no further guest progress. `sub_821E61A8`'s
wait is a real, correctly-initialized wait for the readback pointer to
advance past the write cursor by the requested amount — it does not, over
200 iterations, because nothing drives it forward. This is exactly the
original r51/NEXT.md framing this whole `0x1a0010` detour (r77) departed
from.

## Decision

Retract the `0x1a0010` object identity and every claim built on it
(r77-r84's specific field values, the "null allocation"/"heap ordering"/
"gate contradiction" narratives). Keep what those cycles established that
does **not** depend on the wrong address: the function-level control-flow
facts (`sub_821E64A8`'s packet push and conditional call structure,
`sub_821E61A8`'s space-wait loop shape, `sub_821E6AC8`'s bounded backoff,
`sub_821E5D60`'s cursor-advance logic, `sub_821E6A08`'s telemetry-only
escalation) — those were read from the disassembly directly and do not
depend on which specific object's fields were sampled. Also keep the
endianness note from r84 (guest memory is big-endian; `x/xw` on this host
needs correction) — it remains true and was the discipline that helped
surface this deeper error.

Next cycle: use the verified object (`0x10001a00`) and its two known
fields (`+0x2a90` = readback `0x164e0000`, `+0x30` cursor `0x162e017c`,
requested space via `r30`/arg2 = 4) to determine exactly what would need
to advance for this wait to clear, and whether that ties directly into the
already-open r53 item ("consommateur PM4/Vd natif n'est pas encore relié")
rather than opening a new investigation thread. No implementation yet;
verify the connection to the existing Vd service code
(`native/src/native_guest_vd.cpp`) first.

No native code changed (temporary edit reverted, rebuilt, CTest 9/9
reconfirmed). Two throwaway dump scripts removed after use.
