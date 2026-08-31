# AC6 retail NTSC-U/J — the retail Vd interrupt callback is registered and never invoked (r87)

Date: 2026-08-31.

## Method

r86's next step: find what retail code writes `object+0x2a90`'s pointee
offset `+0x0` directly (the field `sub_821E61A8`'s wait actually
dereferences). The writer was already found in an earlier cycle
(`FindDerefWritesAtDisplacement.java 0x2a90`, three hits: `sub_821E5D60`,
`sub_821E65B0`, `sub_821EFAF0` — all confirmed `stw ...,0x0(ptr)`), but
it was mis-connected at the time (before r85's object-identity correction).
`sub_821E5D60` is the relevant one: reached via
`sub_821E60A8 → sub_821E5E48 → sub_821E5D60` (r79), it writes the current
`object+0x2a9c` value into offset `0x0` of the `object+0x2a90` block —
exactly what would satisfy the stalled wait.

The open question was who calls `sub_821E60A8` for this object, since
nothing does within the 200-iteration single-thread observation window
(r84). Checked the Vd graphics interrupt registration path instead of
continuing to guess: `VdSetGraphicsInterruptCallback`'s only two static
callers (`0x821f1220`, `0x821f15fc`) were found and read.

## Result

`0x821f1220` calls `VdSetGraphicsInterruptCallback(callback=0x821E63F0,
context=r31)` — a real registration, not the teardown call (`0x821f15fc`
passes `0,0` — unregister, followed immediately by `bl sub_821E65B0`, part
of a shutdown path, not relevant here).

`sub_821E63F0` (`0x821e63f0..0x821e64a7`) is the actual interrupt handler:
- guards `source == 1` (the primary/CP interrupt path);
- validates a corruption sentinel (`0xBADF00D`) at a nested structure,
  traps (`twi r0,0x16`) if it doesn't match;
- loads a **second, registered sub-callback** from `context+0x2a94`,
  offset `+0x14`, and invokes it indirectly (`mtspr CTR,r31; bctrl`) if
  non-null;
- takes a lock, clears a bit in a status word at `context+0x2a98`, releases
  the lock.

This is the real Vd/CP interrupt dispatch retail code installs, expecting
the platform to call it on every graphics interrupt (vblank / command-
processor completion).

**The native runtime's `VdSetGraphicsInterruptCallback` stub
(`materialize_native_import_stubs.py`) is a no-op**: `ctx.r3.u64 = 0u; //
native renderer owns the Vd lifecycle`. It records nothing and never
invokes the callback it was handed. `sub_821E63F0` is therefore **never
called** by this runtime, under any circumstance — not once, not
periodically, not on ring progress.

## Not yet fully closed

The chain from `sub_821E63F0`'s nested sub-callback (`context+0x2a94+0x14`)
through to `sub_821E60A8`/`sub_821E5D60` (the actual unblock write) has
not been traced instruction-by-instruction — only the shape is
established: a registered interrupt handler that this runtime drops on
the floor, and a separate, already-identified unblock write that nothing
currently triggers. Whether the sub-callback reaches that unblock write
directly, or through further indirection, is the one remaining static
step before implementing anything.

## Decision

Still no implementation this cycle — the nested sub-callback link needs
tracing first, and per this thread's repeated discipline (r53, r77, r79,
r80, r85), driving the unblock write directly without verifying the real
trigger would be exactly the synthetic-state shortcut already refused
several times. Next cycle: read `context+0x2a94+0x14`'s registered
function (find it the same way — trace what gets stored there, likely
near the same `sub_821D5F48`-style subsystem-init code, or read it live
via the already-working single-thread GDB technique) and confirm it
reaches `sub_821E60A8` for the same object. If confirmed, the fix is to
make the native `VdSetGraphicsInterruptCallback` stub record the callback
and context, and have the Vd service invoke it (via `PPC_LOOKUP_FUNC`,
matching the pattern `ExCreateThread`'s stub already uses to invoke guest
code) once genuine ring/IB progress is observed in `drain_locked()` —
never on a fixed timer or unconditionally.

No native code changed. Three throwaway scripts removed after use
(`FindUsImportCallers.java`, `DumpUsInterruptCallbackSites.java`,
`DumpUsInterruptCallback.java`).
