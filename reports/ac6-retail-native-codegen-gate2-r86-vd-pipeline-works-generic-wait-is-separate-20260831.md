# AC6 retail NTSC-U/J — the PM4/Vd pipeline works; the stalled wait is a separate, adjacent sub-allocator (r86)

Date: 2026-08-31.

## Method

Following r85's concrete next step: cross-checked the verified object
(`0x10001a00`) against `native/src/native_guest_vd.cpp` directly, instead
of more GDB archaeology. `NativeGuestVdService::discover_write_index_locked`
already scans allocations for a field at **offset `+10896`** (decimal) to
auto-locate the producer object — `10896 == 0x2A90` in hex, the **same
offset** `sub_821E61A8`'s wait reads. Re-ran the bounded entry probe with
the service's own existing diagnostic (`AC6_NATIVE_VD_TRACE=1`, not a new
tool) to see the pipeline's real behavior directly.

## Result — the PM4/Vd pipeline is not stuck

```
vd discover object=0x10001a00 state=0x164e0000 write=19 cursor=0x162e0038 limit=0x162eff60
vd publish write=19 read=0
vd drain accepted consumed_dwords=19
vd publish write=31 read=19
vd drain accepted consumed_dwords=12
```

`object=0x10001a00` matches r85's corrected identity exactly.
`state=0x164e0000` matches the live value already read at `object+0x2a90`
(r85). Both known batches (`PM4_ME_INIT` 19 dwords, then the IB bootstrap
12 dwords — r56-r75) are found and **accepted**, matching what was already
established months of cycles ago in `reports/ac6-retail-native-codegen-gate2-r11-20260831.md`.
Nothing here is stuck; the native Vd/PM4 consumer is working exactly as
previously qualified.

## The stalled wait is not this ring

`enable_readback` resolved the readback address to **`0x164e003c`**
(`object+0x2a90`'s value `0x164e0000` **plus `0x3c`**) — the field
`drain_locked()` actually writes the consumed index into. But
`sub_821E61A8`'s wait dereferences **offset `+0x0`** of the block at
`object+0x2a90` (`lwz r10,0x0(r10)`, established r77-r85), not `+0x3c`.
These are two different fields of the same 0x60-byte block. Separately,
`object+0x2a9c` (the "limit" `sub_821E61A8` compares against) reads a
stable **`7`** (r84/r85) — nowhere near the ring's real write indices
(`19`, then `31`); it cannot be tracking ring-word progress.

Both facts point the same way: `sub_821E61A8`/`sub_821E64A8`/
`sub_821E65B0`'s generic push-and-wait machinery (r77-r83's function-level
findings, which remain valid — only the object identity was wrong, per r85)
is a **separate, small sub-allocator** — most plausibly a command-list
staging buffer that shares the same parent "graphics device" object as the
Vd ring (explaining why both groups of fields sit within ~12-100 bytes of
each other on the same `0x10001a00` object) but is drained by a *different*
mechanism than the one the native Vd service already implements. r79's
earlier observation that the pushed packet (`0x5c8`, `0x00020000`) never
fit a PM4 type-3 header is consistent with this: it was never PM4 packet
data at all.

## Decision

This reframes the whole `sub_821E6AC8`-parked-thread investigation
(r77-r85): it is very likely **not** blocking on graphics ring progress —
the graphics ring itself is already working. The real blocker is whatever
is supposed to drain this small, adjacent staging allocator (limit
`0x2a9c=7`, currently full or otherwise never freed) — a mechanism this
project has not yet identified, let alone implemented. Implementing
anything here (e.g., wiring the existing Vd drain to also touch
`object+0x2a90+0x0`) would be guessing at an unverified relationship
between two genuinely different fields; not done.

Next cycle: identify what real retail code (not `sub_821E61A8`'s wait
side) writes to `object+0x2a90+0x0` — i.e. offset `0x2a90` of the object
directly, not through the `0x164e0000` block's `+0x3c` readback — to learn
what this specific staging counter represents and whether it is meant to
be driven by the same interrupt/notification path as the Vd ring, or by
something entirely separate (audio, streaming, or a generic job system,
matching r79's "76 callers, generic entry point" finding for
`sub_821E60A8`).

No native code changed. No new scripts; reused the existing
`AC6_NATIVE_VD_TRACE` diagnostic already built into
`native/src/native_guest_vd.cpp`.
