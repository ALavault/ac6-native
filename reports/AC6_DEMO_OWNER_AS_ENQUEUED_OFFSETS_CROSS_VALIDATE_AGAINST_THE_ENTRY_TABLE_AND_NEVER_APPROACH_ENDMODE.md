# Owner A's enqueued offsets exactly cross-validate two prior entry-table findings, and never come near EndMode

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: the existing
`AC6_DEMO_WATCH_ADDR_LO/HI` bracket (no source change) pointed at
`[0x2E3EE1D4, 0x2E3EE1E4)` — the first two slots of the fixed array
`704b27b6` found `Add()` writing into, on the press/resume
`MovieController` instance's own `MovieMemory` (`0x2E3EDA90` →
`0x2E3EDCD0` → array `0x2E3EE1D4`, `a42271ec`/`704b27b6`) — `probe
--until frontend --max-ticks 3200`, correctly-timed START, no oracle.

## Two corrections to `704b27b6` first

**The 573/750 vs. 576/602 comparison in `704b27b6` conflated two
different phenomena, not one event.** `390cfe33`'s "twice on 576 of 602
ticks" counted PC-field writes on the *shared execution-context slot*
(`0x2E3DFA1C`), driven by whichever owner the slot was serving at the
time — predominantly `0x2E4035D0`, the dominant per-tick owner, during
the window `390cfe33` sampled. `704b27b6`'s 573/750 counted `Add()`
firing on **`0x2E3EDA90`'s own array** — a different owner, a different
object, and (as this report's own data shows) `Add()` here fires nearly
every tick from construction on, not "twice per dispatch tick" the way
`390cfe33`'s count worked. The two numbers landing close together is
coincidence — both are separate "fires almost every tick" phenomena on
different objects, not the same underlying write counted two ways. This
report's own headline finding does not depend on that comparison; it is
retracted here rather than left standing.

**`704b27b6` misspells the class name once as `MovieMedmory`** (in its
own "Not established" section) — `swg::MovieMemory`, matching every other
reference in that report and in `a42271ec`.

## The array-slot-0 history, tick by tick

Bracketing the array directly (rather than its pointer, which
`42731207` already showed never moves) catches every value `Add()` ever
writes to index 0 across the full run:

| tick(s) | array value | `+ table_base` (`0x2DCB1220`) | reading |
|---|---|---|---|
| 2452 | `0x1184` | `0x2DCB23A4` | matches this same tick's own dispatch PC exactly (`bfc927e1`'s live capture, independent confirmation of the instrument) |
| 2571–3000 (430 ticks, unbroken) | `0x1208` | `0x2DCB2428` | owner A's idle-window offset |
| 3001 | `0x1210` | **`0x2DCB2430`** | **exact match to `b67e7f6f`'s independently-found "tick 3001 gets its own distinct entry `0x2DCB2430`"** |
| 3033 | `0x146C` | **`0x2DCB268C`** | **exact match to `390cfe33`'s independently-found yield/resume target, "tick 3033's entry"** |
| 3051 | `0x171C` | `0x2DCB293C` | not previously named |
| 3061–3199 (139 ticks, unbroken) | `0x1788` | `0x2DCB29A8` | a second, later idle-window offset |

The two exact matches are the load-bearing result here: `b67e7f6f` and
`390cfe33` found `0x2DCB2430` and `0x2DCB268C` respectively by watching
the *shared execution-context slot's* entry field — a completely
different object, a completely different bracket, committed in reports
four and five commits before this one. This report reaches the identical
two addresses from `Add()`'s own write into owner A's array. Two
independent measurement paths, converging exactly, is about as strong a
confirmation as this campaign produces.

**Index 1** (`0x2E3EE1D8`, the array's second slot) is written only
twice in the whole run — tick 2452 (`0x1204` → `0x2DCB2424`) and tick
3001 (`0x142C` → `0x2DCB264C`) — both ticks where the count field
reaches `2` (`704b27b6`), i.e. `Add()` firing twice in the same tick.
Both values are new, not previously named by this campaign.

## EndMode's offset never appears

EndMode's own statement offset, computed once already (`0x2DCB2024 -
0x2DCB1220 = 0xE04`), does not appear anywhere in this bracket's log —
not at index 0, not at index 1, across all 3200 ticks. Every offset
`0x2E3EDA90` ever enqueues in this run sits in the `0x1184`-`0x1788`
range (`0x2DCB23A4`-`0x2DCB29A8`), well past EndMode's own `0xE04`
(`0x2DCB2024`) and never approaching it. For this owner, on this
correctly-timed run: **EndMode is not enqueued and dropped — it is
simply never among the offsets this owner ever produces.**

This is a genuine negative result, not a null instrument: the same
bracket, on the same object, correctly captures six other transitions
(construction, two idle plateaus, and three distinct one-tick offsets
around the press) with exact cross-validation against prior work — the
instrument is demonstrably working, and it shows nothing anywhere near
EndMode.

## Not established

- Only `0x2E3EDA90`'s own array was read. The dominant per-tick owner
  (`0x2E4035D0`) has its own, separate `MovieMemory` (`a42271ec`) with
  its own array, never bracketed — EndMode's offset could still appear
  there.
- Only this one deterministic run (correctly-timed START at tick 3000,
  `--until frontend --max-ticks 3200`) was checked. A different input
  sequence, a longer run, or a different path through the frontend could
  enqueue different offsets.
- The 26+ other owners `bfc927e1` found sharing the execution-context
  slot — none besides the two named in `a42271ec` have had their own
  arrays read.
- `0x171C`/`0x1788`/`0x1204`/`0x142C` (four of the six absolute addresses
  this report computes) are not otherwise named or cross-checked against
  any prior finding — only the two that matched `b67e7f6f`/`390cfe33`
  were.

## Gates

No source changed; this report reuses the existing
`AC6_DEMO_WATCH_ADDR_LO/HI` instrument on a new bracket, plus arithmetic
against already-established addresses. Native gate JF, demo `ctest`
(26/26), and both contract audits verified below before commit.
