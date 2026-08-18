# `0x820FEFA8` is a traced, instruction-level no-op on every measured slot;
# the consume loop and slot array are now fully structural, not inferred

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Static read of
`recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.7.cpp`
(`sub_820FFCA0` at line 47341, `sub_820FEFA8` at line 45358). No probe run,
no source change.

## What this closes

`75c5d1ac` named `0x820FEFA8` -- the render-queue consumer's own call site
-- as the precise unchased next step: what does it do with the
always-zero payload it reads. This report reads it, and the loop that
calls it, in full to that question's resolution.

## The consumer is a real ring-consume loop, now traced exactly

`sub_820FFCA0` (`this` = `r31` = `r3`, the render-queue object) does, each
time it is entered:

```
r11 = [this+24784]   // producer index  (kRenderQueueBase+0x60D0)
r10 = [this+24788]   // consumer index  (kRenderQueueBase+0x60D4)
if (r10 == r11) goto loc_820FFD90        // empty: exit
slot = this + 208 + r10*96               // slot[consumer_index], stride 96
copy slot[0..92] to a stack buffer       // 4x16B vectors + 7 scalar words + 1 float
sub_820FEFA8(stack_copy)
r10 = r10 + 1
[this+24788] = r10                       // persist consumer index immediately
goto loc_820FFCCC                        // re-check producer vs consumer
```

`this+24784`/`this+24788` are exactly `kProducerOffset`/`kConsumerOffset`
(`0x60D0`/`0x60D4`) from `scheduler.hpp`'s `yield_guest_thread_if_due` --
confirming `this == 0x82386CC0` (`kRenderQueueBase`) precisely, not by
address-arithmetic coincidence but by reading both offsets used as the
loop's own exit condition. **The slot array base, `this+208`, is
`0x82386D90`** -- exactly the address `cycle-1690`/`1691`/`1777` named as
the consumer's slot and SHA-256-hashed as always-zero. The address
`0x82386DD0` those reports separately named is `slot[0]+64`: not a second
slot, but the dispatch-selector field inside the first slot (see below).

This also settles, more precisely than `75c5d1ac`'s framing, why the old
reports saw the consumer run "every tick": it is a **while-not-empty
loop**, re-checked via `goto loc_820FFCCC` after every consumed entry, and
the consumer index is written back to guest memory immediately after each
iteration -- not batched, not deferred. Real per-tick producer activity
(the 7,495 advances `5a7c3511` measured) drives this loop to run its body
at least once per tick on the routes measured.

## `sub_820FEFA8`: values 1/2/3/4 branch, everything else -- including the
## only value ever observed -- returns immediately

The copied slot's byte offset `+64` (the field at guest address
`0x82386DD0` for `slot[0]`) is loaded into `r11` and switched:

```
r11 = [this+64]
if (r11 == 1) goto loc_820FF600
if (r11 == 2) goto loc_820FF580
if (r11 == 3) goto loc_820FF560
if (r11 != 4) goto loc_820FF688   // <-- every other value, including 0
// r11 == 4: read [this+68]/[this+72]/[this+76] as three optional
// sub-slot pointers, dispatch each through sub_820D2C60/sub_820FEA88
// if non-null, then an indirect vtable call if slot 2 is non-null.
```

`loc_820FF688` (`ppc_recomp.7.cpp:46374`) is:

```
r1 += 240
f31 = restore
__restgprlr_27
return
```

Nothing else. **This is a bare, unconditional, instruction-level no-op.**
Since every slot byte this campaign has measured (across multiple fresh
probe windows, both routes, SHA-256-confirmed identical) is zero, the
dispatch field is always `0` -- never `1`, `2`, `3`, or `4` -- so this
return path is the *only* path this function has ever taken on any
measured run. The value==4 branch (three optional slots,
`sub_820D2C60`/`sub_820FEA88`, an indirect vtable call gated on a global
loaded via `sub_8226D6A0`) is real code that exists and would do
something, but is unreached by every route measured so far -- it requires
a nonzero dispatch field this campaign has never observed being written.

## Consequence for the plan

The chain from `5a7c3511` through `a3d9ef48` through `75c5d1ac` is now
closed at the instruction level, not just the counter level: the
render-queue's plumbing (producer index, consumer index, ring-consume
loop, per-slot dispatch) is entirely intact and running correctly every
tick; the thing it dispatches to is empty on every observed run, and the
function it calls with an empty dispatch value does, verifiably, nothing
but return. This is not a stuck consumer and not a broken dispatcher --
it is a queue that is fully alive and permanently idle for lack of any
producer ever writing a nonzero `slot+64`.

This reframes the open question one more level upward, matching what the
task-dispatcher evidence (`cycle-1777`: exactly three stable task-list
entries, no menu/mission task ever added) already independently pointed
at: **something upstream of this queue -- likely the same something that
never constructs a menu/mission task owner -- is also the thing that
never pushes a real (nonzero-selector) entry onto this queue.** Both
symptoms read as the same underlying gate: the frontend's world-state
queries (`GetCurrentMode`/`GetCurrentMission`, `3c7e7291`) return a
permanently-gated fallback, so nothing downstream -- neither the task
list nor this render queue -- ever receives real content to act on. The
render queue itself is not where the defect lives; it is one more
symptom of it.

## Not established

- What writes `[sub112+8]` (`3c7e7291`'s gate field, the word that makes
  `GetCurrentMission` return its fallback unconditionally) -- not read
  this cycle. This is now the highest-value open thread: a single-word
  data-flow trace that sits one level above both the empty task list and
  this empty render queue, and both of those symptoms are consistent with
  it being the shared cause.
- What the value==4 branch (`sub_820D2C60`/`sub_820FEA88`/the indirect
  vtable call through a global from `sub_8226D6A0`) does, since it is
  unreached on every route measured and reading it now would be
  archaeology of code that cannot currently run.
- What the value==1/2/3 branches (`loc_820FF600`/`loc_820FF580`/
  `loc_820FF560`) do, for the same reason.
- Whether any producer call site ever writes a nonzero value to a slot's
  `+64` field on any reachable route -- not searched this cycle
  (`sub_820FF710`, the producer, was read in an earlier report for its
  index-advance write only, not for what it stores into the slot body
  itself).

## Process note

Per `advisor`'s guidance this cycle, the case-1/2/3 branches and
`sub_820FEA88`/`sub_820D2C60` were deliberately not chased -- they are
unreached on every measured route, and reading them now would not change
the standing priority. The one thing worth verifying before closing this
thread was the structural correspondence between `sub_820FEFA8`'s
`this+64/68/72/76` fields and the guest addresses the old cycle reports
named -- traced directly via the caller's own index arithmetic
(`this+208+idx*96`) and the loop's own use of `this+24784`/`this+24788`
as its exit condition, rather than assumed from field-offset pattern
matching alone.
