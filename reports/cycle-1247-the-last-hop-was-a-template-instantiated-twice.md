# Cycle 1247 — the last hop was a template instantiated twice

Cycle 1244 named one open hop: what starts the Set leader's order FSM. It is
closed, and the reason nobody found it is that **the search was for the wrong
functions.**

## Why it was invisible

`CFsm::SetInitialState` is `0x8219AAE8` and `CFsm::SetState` is `0x8219AB58` —
cycle 1218 established that, correctly. Between them they have **five** call
sites in 100% of `.text`, and all five install `CModeTaskGame` states. None
belongs to the unit family.

Because `CFsm` is a **template, instantiated twice**, and the two copies differ
only in the owner offset baked into a single instruction:

```
8219ab38  subi r3,r10,0x268     CFsm<CModeTaskGame>   sub-object at owner+0x268
82295080  subi r3,r10,0xf0      CFsm<the unit class>  sub-object at owner+0xF0
```

Read here, both. **Instruction for instruction identical otherwise.** The unit's
pair is `0x82295030` / `0x822950A0`, and searching for callers of the first
instantiation can never find the second.

That is a new shape: not a missing caller, not an unlisted instruction, not an
addressing form — **the right search, run against a function that is a sibling of
the one that matters.**

## The chain

```
8219a140  CModeTaskGame ENTER
8219a1b0    vt[+0x2C] -> 0x8219F8C0        the mission load; 0x820A7070 x3 builds the units
8219a204    vt[+0x34] -> 0x822EE6E8        the HSM, phase 1
              -> 0x8226B618 -> 0x82269A40 -> 0x8226FE30   the unit reset pass
8226fe64          u->vt[+0x38] -> 0x822980C8
822980ec            lwz   r7,0xdc(r31)     the CHILD COUNT
822980f8            cmpwi cr6,r7,0x0
82298158            addi  r10,r10,0x7b20   children -> PMF {0x82297B20}
82298174            bl    0x82295030       SetInitialState, which sends -3 at once
```

**The initial state is `0x82297B20`, installed only when `[leader+0xDC] > 0`** —
the same child count cycle 1244's placement loop iterates.

And the tick comes back through `0x82267450`, **a four-instruction tail branch**
to `0x822707C8`, which is why the campaign path shows only one `bl` caller and
is invisible to a `bl`-only graph. The thirteenth shape of
`INSTRUMENT_DISCIPLINE.md`, live.

## What it does to cycle 1244

**Confirmed, and given a mechanism.** The load and the FSM start are **two
different calls in the same ENTER arm, in that order**: `vt[+0x2C]` builds the
units, then `vt[+0x34]` arms every leader. So "the placement does not run at
load" is not merely true — it is true because arming is a separate, later call.

**And "only a Set leader performs the push" now has a cause.** `Add`
(`0x8226FEC0`) has exactly one `bl` caller in 100% of `.text`, at `820a7650`
inside `0x820A7070` — and it sits **outside** the child loop. Children are
constructed and stored into the leader's array, never registered, never reach the
reset pass, never get `vt[+0x38]`, never ticked. That was an observation in cycle
1244 and is a mechanism now.

## Two rivals rejected, and the second is the instructive one

**A false positive that would have contradicted cycle 1244.** `820a7908` is a
strict slot-`+0x3C` virtual call **inside the loader itself** — read at face
value, the tick runs during the load. It passes an **integer in `r4`**
(`820a7904 lwz r4,0x50(r1)`) and never touches `f1`, while the tick's signature
is `(this, float dt, r5)`. A displacement collision, killed by reading the
neighbours.

**And the one nearly published.** `SetInitialState<+0xF0>` has **exactly one**
call site, which invites "the constructor starts it". It does not: `0x822980C8`
*is* called twice at construction, and the `CAce6Unit` base constructor writes
`822a23ac stw r9,0xdc(r31)` with `r9 = 0`, so at both construction-time calls the
guard reads **0** and the PMF installed is NULL. `[+0xDC]` is only set at
`820a7c78`, much later.

**Eight instructions above the `bl` decided that.** Stopping at the call — the
natural boundary — would have produced a confident wrong answer. Sixteenth shape,
caught by the rule that was written this morning.

## An open contradiction I am not resolving

Cycle 1244 attributes `[unit+0x188]` to `0x820A7070` as **the parent pointer**.
`0x822980C8` writes a **float** to `[this+0x188]` at `82298118`, derived from
`extsb([this+0xD0])`.

**One of those readings is wrong, or they are different objects.** Both were read
from instructions. Neither bears on this cycle's conclusion, and I am recording
the contradiction rather than picking the one that suits.

## Not established, stated plainly

- That HSM state `0x822EDC58` is reached in Mission 01. It is a direct transition
  out of the initial state `0x822EE6E8` installs, but the transition's guard was
  not read, and the receiver's dynamic type at `8219a204` was not established —
  only the vtable value.
- `0x8226FE30` iterates all 256 slots while the tick iterates only
  `[mgr+0x40C]`. A unit above the count would be armed and never ticked; nothing
  was found writing above the count, and that was not proved.
- The FSM-owner class has **no RTTI**. Its identity as a `CAce6Unit` subclass
  rests on a constructor chain, not a type read.
- Which of the two `bl 0x82269A40` arms executes. Both reach the reset pass, so
  the conclusion is arm-independent.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
8219ab38 / 82295080 and the 822980ec guard re-read here
```

No product code changed.
