# AC6 retail NTSC-U/J — r87's causal chain doesn't reach the unblock write; the nested callback is null by design (r88)

Date: 2026-08-31.

## Method

r87's open item: trace the nested sub-callback (`context+0x2a94+0x14`)
through to the already-known unblock write (`sub_821E60A8 →
sub_821E5D60`, writing into `object+0x2a90`'s pointee offset `+0x0`).
Re-read `AC6_NATIVE_VD_TRACE`'s allocation log correlation
(`object+0x2a94`'s block address, `0x164f0000`, immediately follows the
`object+0x2a90` block `0x164e0000` in allocation order) and confirmed it
live: broke at `__imp__VdSetGraphicsInterruptCallback`'s own entry (the
host C++ stub, `ctx&` in `$rdi`, `base` in `$rsi`), read the PPC arguments
directly from `ctx` (`ctx.r3` at `$rdi+0`, `ctx.r4` at `$rdi+16`, per the
struct layout established r84):

- `ctx.r3 = 0x821E63F0` (the callback) — matches r87 exactly.
- `ctx.r4 = 0x10001a00` (the context) — **independently confirms r85's
  object identity a third time**, from a completely different call site
  than the ones used in r80/r85/r86.
- `*(object+0x2a94)+0x0..0x7` (the nested block's first 8 bytes): all
  zero, at the moment of registration.

## Correction to r87's reading

Re-reading `sub_821E63F0`'s logic precisely: offset `+0x10` of the
`object+0x2a94` block is not a separate "sentinel" guarding a distinct
callback field — **it is the callback function pointer itself**, and the
`0xBADF00D` comparison is a poison-value guard (`bne cr6,+0x40` skips the
trap when the value is *anything but* that specific magic — including the
completely ordinary case of a null, never-registered pointer). Offset
`+0x14` is the callback's context argument. The subsequent check
(`cmplwi r31,0; beq +0x18`) means: **when the pointer is null (as it is
here), the handler correctly and intentionally skips the nested callback
invocation** and falls through directly to clearing a status bit at
`object+0x2a94`'s pointee offset `+0x0` (confirmed as the only static
writer through this pointer anywhere in the image,
`FindDerefWritesAtDisplacement.java 0x2a94`, single hit, inside
`sub_821E63F0` itself).

Nothing anywhere in the retail image writes a non-null value to offset
`+0x10` of this block. `sub_821E65B0`'s own reset code (which allocates
this exact 0x20-byte block) immediately zeroes it with a real `memset`
call (`bl 0x823830f0`, `r4=0`, `r5=0x20`, confirmed in the cached
disassembly) and never revisits it.

## What this means

Even a correct implementation of the native `VdSetGraphicsInterruptCallback`
stub — recording and actually invoking `sub_821E63F0` on genuine ring
progress, as r87 proposed — would, on this evidence, harmlessly skip the
nested callback (it is null by design at this point in execution) and do
nothing that reaches `sub_821E60A8`/`sub_821E5D60`. r87's causal chain
from "interrupt callback never fires" to "the unblock write never
happens" **does not hold as traced**. This is a genuine dead end for this
specific path, not a confirmation.

## Decision

Retract the specific mechanism r87 proposed (fixing
`VdSetGraphicsInterruptCallback` alone will not unblock the wait); keep
the underlying facts (the stub is genuinely a no-op, the registration is
real, the object identity is now confirmed three separate ways). The
question of what actually calls `sub_821E60A8` for this object — or
whether the wait is meant to clear some other way entirely — remains
open. Given four consecutive cycles (r83 GDB instrument limits, r85
wrong-object correction, r87 interrupt-callback lead, r88 this dead end)
have each spent significant effort narrowing and then closing off
specific mechanisms without landing the final answer, the next cycle
should reconsider scope rather than propose a fifth specific hypothesis:
either accept the current state of knowledge as sufficient to document a
bounded negative for this sub-thread and return to the broader NEXT.md
scheduler/kernel migration list, or commit to a substantially more
expensive static pass (e.g. a full cross-reference of every write to
`sub_821E60A8`'s 76 call sites' surrounding conditions) only if judged
worth the cost.

No native code changed. No new scripts (reused
`FindDerefWritesAtDisplacement.java` from an earlier cycle).
