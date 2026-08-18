# `menu_endMode`'s argument selects the destination task, and two natural values are now known

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Static reads of
`ppc_recomp.15.cpp`/`ppc_recomp.4.cpp` and one live probe run (neutral store,
no injection, `AC6_DEMO_WATCH_SWG_NATIVE_CALL=1`, `--max-ticks 4500`, no
button press).

## Where this session's own priority stood, corrected

This session's whole prior arc (`1fcc88b3`→`29da1b05`, every EndMode/`ASContext`
report committed today) investigated whether `menu_endMode`'s low-level
bytecode dispatch (`box`+opcode-`0x4D` invoke) ever fires for title, and
exhaustively falsified every candidate at that layer. That work is correct on
its own terms, but it was not the campaign's standing priority: `5dc58584`
("SendMsgI is not the lock, and neither is GetCurrentMode"), committed
*before* this session's arc began, explicitly rules out the script/query
layer and states the priority is "le constructeur absent de
`CX360MissionManager<...>`/`CX360UnitManager`, et non la suite du script" —
the absent constructor, not further script investigation. `3e0c76d0` ("The
renderer gate is mission-scoped: the two frontiers are one") independently
shows the black-frame renderer gate and the frontend stall are the same
mission-scoped resource. Nothing between `5dc58584` and this report's own
commit executes that redirect. This report is the first step back onto it.

Also folding in a correction owed from `29da1b05`: `ASContext`'s destructor
(`sub_820D2B28`) tears down **five** embedded lists, not four — `+312` is
also destroyed (`ppc_recomp.4.cpp:18566-18581`), and `+292` is destroyed
**last** in the reverse-order pass, not first. Doesn't change that report's
conclusions.

## The mechanism: `menu_endMode`'s own integer argument is a destination selector

`AC6_DEMO_THE_SCRIPT_VOCABULARY.md` (`09c1090a`) already traced the call
chain from `menu_endMode` (`0x820EA4A8`) through `0x820EA500` → mode-task
base slot `+0x54` (`0x8217C890`) → title's own vtable slot `+0x48`
(`0x8218AB98`). Reading `sub_8218AB98` in full for the first time
(`ppc_recomp.15.cpp:26111+`):

```
this = r30 = r3      (mode-task base, from the prior link in the chain)
arg  = r31 = r4       <- menu_endMode's own raw integer argument, unmodified
r3 = (arg > 0) ? arg-1 : arg
sub_8218E948(r3)                       ; normalizes/looks something up
sub_8218EA88(global, result)           ; another lookup, target unread
if (arg == 2 || arg == 3): goto loc_8218ABE8
else:                       goto loc_8218ABFC     (or further, unread)
```

**The raw `menu_endMode` argument is tested directly**, `== 2` or `== 3`
taking one branch, everything else taking another. Separately, title's own
periodic state machine (`sub_8218A7A8`, read in full this cycle) has a
state-2 arm that, once a countdown at `[title+68]` reaches zero, calls
title's own vtable slot `+0x4C` (`sub_8218AA30`) — a second, later stage in
the same overall transition. Reading `sub_8218AA30` in full
(`ppc_recomp.15.cpp:25899+`):

```
this = r29 = r3
if ([this+112] == 0 || [this+112] == 1):
    ... poll a loop (sub_8219BF40/sub_8219B6B8/sub_8219B8F8/sub_8219B6F8,
        the shape of an asset/streaming-completion wait) ...
    compute an array-indexed slot address from [this+112]-derived bits
    call sub_82184390(slot)     ; re-initializes state=0 on a pool slot
                                 ; (sub_82184390 -> sub_82183FD0 sets
                                 ; [slot+8]=1, [slot+12]=0 -- no vtable
                                 ; install found in either function, so
                                 ; this is a POOL SLOT REINIT, not a fresh
                                 ; `new` -- the vtable is presumably
                                 ; already resident on the slot)
else:
    goto 0x8218AB90    (unread)
```

**Not established this cycle**: whether `[title+112]` is written from
`menu_endMode`'s own argument (plausible — it sits at the same conceptual
point in the chain — but no direct write from `sub_8218AB98`'s body into
`[this+112]` was found in the portion read; the connection is inferred from
position in the chain, not traced data flow). Also unread: `sub_8218E948`,
`sub_8218EA88`, `loc_8218ABFC`, `loc_8218AB90`, and what specifically
distinguishes the "0/1" pool-slot-reinit path from whatever `0x8218AB90`
does.

## Two concrete natural argument values, read live for the first time

Bracketed `AC6_DEMO_WATCH_SWG_NATIVE_CALL` across a natural (no injection, no
button press) run to tick 4500, past both attract-loop transitions:

```
tick=2425  target=0x820EA4A8 (menu_endMode)  first_arg=0x00000000
tick=4251  target=0x820EA4A8 (menu_endMode)  first_arg=0x00000002
```

Tick 2425 is startup's own successful call (`da27b1db`), transitioning
startup→title. Tick 4251 is title's own attract-timeout call
(`09c1090a`: "à 4251, il appelle `StopBGMFadeOut` puis `menu_endMode`"),
transitioning title→startup. **The two known-successful natural transitions
use different arguments — `0` and `2`** — direct, live confirmation that the
argument is not a constant and genuinely varies by destination, consistent
with (though not yet proof of) it selecting which mode gets constructed.

`0x00000002` also matches `sub_8218AB98`'s own `arg == 2` branch exactly —
title's natural attract-timeout call is live proof that value **2** takes
the `loc_8218ABE8` path in that function, whatever it turns out to do.

## Why this matters more than this session's earlier EndMode work

The whole earlier arc asked "does `menu_endMode` ever fire for title after
the press." It structurally does not (confirmed many times over). This
report asks a different, higher-priority question: **if it did fire with a
different argument, would that argument construct something other than the
two already-observed attract-loop endpoints (startup, or back to startup)?**
That is the falsifiable next step, and it is a genuinely new angle, not a
restatement of the exhausted query-forcing thread — this targets the
argument to the transition call itself, not the three read-only query
results already ruled out (`c73498cb`, `6fc7b184`, `3b12d584`).

## Not established

- The full value space `menu_endMode`'s argument can take, and which value
  (if any) leads somewhere other than startup or title.
- Whether `[title+112]` is written from this argument, from a different
  argument earlier in the chain, or from something else entirely.
- What `sub_8218E948`, `sub_8218EA88`, `loc_8218ABFC`, and `0x8218AB90` do.
- Whether forcing this argument on a *natural* call (rather than an
  entirely synthetic injection) would even be reachable post-press, given
  `651e7878`'s finding that pressing START suppresses the callback that
  would fire this chain again — the next falsifier likely needs to act on
  the neutral attract-loop's own tick-4251 (or tick-6432, etc.) call, not a
  post-press state, unless that suppression itself can be worked around.

## Process note

Forward-checked with `git log --oneline --reverse 29da1b05..HEAD`: empty,
`29da1b05` is `HEAD`. This report does not continue the swg/`ASContext`
object-identification thread that arc closed; it returns to the
higher-priority thread `5dc58584` named and nothing since has executed.
