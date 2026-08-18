# Correcting `1b87123e`: the two mainline traps check one object — the ASContext::String boxing pool's own head

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Reads against
`.build/Default.xex.base.bin` and
`recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/ppc_recomp.*.cpp`
(control-flow evidence, never copied). Live runs against
`recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe`, same
neutral store and injection recipe as `1b87123e`
(`AC6_DEMO_INJECT_ENDMODE_AT_TICK=2571`, `AC6_DEMO_INJECT_ENDMODE_OFFSET=0xE0C`,
`--input-at 3000,16,...`).

## Correction

`1b87123e` described trap 1 (`sub_820DBA18`, the `0x16`-prefix path,
`lr=0x820DBAF0`) and trap 3 (`sub_820E7638`, the real invoke step,
`lr=0x820E7840`) as checking "presumably three related-but-distinct objects"
across the campaign's three known trap sites. That is wrong for traps 1 and 3.
Direct register comparison, done for the first time this cycle instead of
eyeballing the two dumps separately:

| | trap 1 (`sub_820DBA18`) | trap 3 (`sub_820E7638`) |
|---|---|---|
| checked object | `r4`=`r31`=`0x2E3E3E38` | `r4`=`r27`=`0x2E3E3E38` |
| its `+4` field | `r5`=`0x2E3DF850` | `r5`=`r10`=`r11`=`0x2E3DF850` |

Both dumps hold the identical decimal `775831096` for the checked object and
the identical `0x2E3DF850` for its `+4` field. Trap 3's own `r31=0xE20` is
`0x2DCB2040 − 0x2DCB1220`, exactly the `r4 = pc − table_base` `sub_82324CE0`
computes and passes onward — corroboration that this run reached the invoke
step through the real mechanism, not a coincidence of the injection.

**Two independently-dispatched code paths (the never-taken `0x16` prefix and
the real `box`+`0x4D` invoke pair) link-check the exact same object.** Only
trap 2 (`sub_820D7700`, the old `0xDF0`-offset run, now understood to have
landed on box's own argument data rather than a real statement) involves a
different address — that one is not re-examined here. `demo-render-chain.md`'s
"on 3 different objects" line is corrected the same way.

## What the object and its link target actually are

This cycle bracketed both addresses with `AC6_DEMO_WATCH_ADDR_LO/HI` across the
**entire** run (tick 0 through the trap at tick 2571), not just around the
injection point, in two separate probe runs sharing the same store snapshot
and injection recipe as `1b87123e`. Both runs reproduce the identical trap
(`outcome.kind=trap`, `completed_ticks=2571`, identical register dump) —
confirms the bracket instrumentation does not perturb the replay.

### `0x2E3E3E38` — never constructed as its own object

Its `+0` word is written exactly once in the whole run: the boot-time pool
poison-fill (`0xFEFEFEFE`, tick 40, `sub_823273E0`, the same bulk allocator
pre-fill that touches every bracketed address in this cycle). No guest code
ever installs a vtable there. Its `+4` word is written exactly **once** after
that poison fill:

```
tick=2451 lr=0x820CF974 function=sub_820CF958  →  [0x2E3E3E3C] = 0x2E3DF850
```

and never again through the trap tick. `0x2E3E3E38` itself is never revisited
by any traced write after tick 2451. It is not a freestanding constructed
object — no vtable, one write to one field, then silence. It reads as a
**cached snapshot slot** embedded in something else not yet identified (its
owning structure is out of scope of this bracket; see Not established).

### `0x2E3DF850` — the ASContext::String/VariableBase boxing chain's own pool head

RTTI-walked directly (vtable → `RTTICompleteObjectLocator` at `vtable-4` →
type descriptor name at `+0x0C`, the same walk `tools/whose_vtable.py`
documents) against the two vtable-shaped values this bracket caught live:

```
0x82006B44 -> .?AVString@?$ASContext@V?$lwallocator@E$0A@$0EA@@stx@@@swg@@
0x820066F4 -> .?AVVariableBase@swg@@
```

These are not new — `AC6_DEMO_THE_DOMINANT_OWNERS_ARRAY_ALSO_CROSS_VALIDATES_AND_STAYS_CLEAR_OF_ENDMODE.md`
already named this exact pair as "the generic value-constructor and
vtable-installer pair `346255b2` identified at the very start of this whole
investigation arc (the `ASContext::String` boxing chain)." This cycle connects
that pair to the EndMode trap object for the first time. Full write history of
`0x2E3DF850`, filtered to the constructive events:

```
tick=2428 lr=0x820DA4A4 sub_820DA488 (box())   [+0] = 0x40
tick=2428 lr=0x820DA4D4 sub_820D4A30 (ctor)    [+4] = 0x82006B44  (String vtable)
tick=2428 lr=0x820D4CA8 sub_820D4C38           [+4] = 0x82006B44  (re-affirm)
tick=2428 lr=0x820D4C68 sub_820D4C38           [+4] = 0x820066F4  (VariableBase vtable)
tick=2451 lr=0x820CF974 sub_820CF958           [+0] = 0x2E3DF850  (self)
tick=2451 lr=0x820CF974 sub_820CF958           [+4] = 0x2E3DF850  (self)   ← same tick 0x2E3E3E38's own +4 is set
tick=2452.. sub_820DA8D0 / sub_820CFC58 / sub_820DA488 / sub_820D5878 /
            sub_820E7638 / sub_820E0568 / sub_820DC408 ...
            [+0]/[+4] cycle through fresh heap addresses (0x2E3F1CD0,
            0x2E3F1C50, 0x2E400450, 0x2E403ED0, ...), repeatedly returning
            to 0x2E3DF850 itself via sub_820CFC58/sub_820CFC60
tick=2571 lr=0x820D5880 sub_820D5878           [+4] = 0x2E3DF850  (self, the exact trap tick)
```

`box()` (`sub_820DA488`) constructs `0x2E3DF850` as a boxed `String` at tick
2428 — **~140 ticks before the injected EndMode dispatch, for a wholly
unrelated purpose**: box() is a general VM primitive running continuously
through normal execution, not something EndMode-specific. At tick 2451,
`sub_820CF958` recycles it: overwrites the `VariableBase` vtable with a
self-pointer at both `+0` and `+4`, the shape of an intrusive free-list node
returning to an empty/sentinel state. **In the same tick**, `0x2E3E3E38+4`
gets set to `0x2E3DF850` — consistent with `sub_820CF958` (or something it
calls) recording a snapshot of this pool's head at the exact instant it went
empty. From tick 2452 the pool head churns continuously — pushed and popped as
the VM boxes and releases further values — and, notably, `sub_820E7638`
itself (trap 3's own function) writes to this same address at tick 2452,
proving it participates in this pool's normal traffic before it ever traps on
it. At tick 2571, the exact injected dispatch tick, the pool head happens to
read back self-pointing again (written moments earlier that same tick by
`sub_820D5878`) — which is what the shared link-check (`sub_820D5B90`,
comparing `[[obj+4]+4]` against `[obj+4]`) reads as "not linked."

## Interpretation

The object EndMode's invoke step queries is not a permanently-broken,
dedicated EndMode object — it is **the ASContext::String/VariableBase boxing
pool's own free-list head, the same general-purpose value-boxing
infrastructure `346255b2` first flagged at the start of this investigation
arc**, sampled at whatever phase it happens to be in. `0x2E3E3E38+4` is a
stale cached pointer into that pool, set once at tick 2451 and never
refreshed; the trap condition depends on the pool's live phase at query time,
which happens to be "empty" at tick 2571 in this run. This is consistent with
— and sharpens — the standing explanation that the real per-tick dispatcher
never fetches this statement naturally (`390cfe33`, `346255b2`): no natural
tick ever queries this pool for EndMode's own purposes, so the injected
experiment is the first and only caller doing so, at a tick borrowed from an
unrelated event (the button press), landing on whatever phase the pool is in
at that arbitrary moment.

This does not establish that the invoke step would succeed at some other tick
— that is untested — only that the specific failure observed is a property of
the pool's instantaneous state, not of `0x2E3E3E38` carrying a permanently
dead link.

## Not established

- What structure owns `0x2E3E3E38`. Its `+0` field is never written by guest
  code in this run (only the boot poison-fill), so it is not a freestanding
  constructed object; it is most likely a field inside a larger, still
  unidentified structure. Proximity check only: `0x2E3E3E38` is `0x4430`
  bytes after title's own execution-context slot (`0x2E3DFA08`, `7a565550`)
  — too far to assume a direct sibling-field relationship without a wider
  bracket or a backward heap-header walk, neither done this cycle.
- Whether `sub_820CF958`/`sub_820CFC58`/`sub_820DA8D0`/`sub_820E0568`/
  `sub_820DC408`/`sub_820E6CD0`/`sub_820DAFA8`/`sub_820DC2B8` are named
  anywhere in existing engine documentation, or which one specifically is the
  pool's push/pop primitive versus a generic caller passing through it. Only
  `sub_820D4A30`, `sub_820D4C38`, and `sub_820DA8D0` were previously named
  (`346255b2`, this thread's own `AC6_DEMO_THE_DOMINANT_OWNERS_ARRAY_...`
  report); the rest are new to this cycle and unread.
- Whether querying this pool at a *natural* tick (rather than the injected
  2571) would ever find it non-empty at the moment EndMode's invoke step
  needs it — untested, and per the paragraph above, moot until the real
  per-tick dispatcher is shown to ever fetch this statement at all.
- What `sub_820D5B90` is validating in engine terms beyond "is this pool's
  head currently linked to something other than itself" — still not named to
  a specific engine subsystem, but now scoped precisely: it is a boxing-pool
  liveness check, not a generic object-graph link check.

## Process note

Followed the standing forward-check rule (`73bdeefc`): `git log --oneline
--reverse 1b87123e..HEAD` is empty — `1b87123e` is still `HEAD`, so this is
the first available continuation, not a re-tread.
