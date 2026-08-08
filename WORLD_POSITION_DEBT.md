# The world-position debt — state of the investigation

One page instead of ten cycle reports. Everything here is static, from the
canonical Ghidra project `ghidra-projects/ace-combat-6` (PAL `default.xex`,
SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`) and
the non-canonical VMX128 import `ghidra-projects-xenon/ac6-xenon`. No oracle has
been used. Cycles 1122 and 1124 to 1131.

## The question

`build_retail_world` gives each unit a position. The goal named that as a debt:
where do Mission 01's world coordinates actually come from?

## What is established

**The object layout.** An object's spatial state is a four-row transform at
`+0x20`: X at `+0x20`, Y at `+0x30`, Z/forward at `+0x40`, **translation at
`+0x50`**. A second four-row block sits at `+0x70..+0xA0` — cycle 1124 read it as
a previous-frame copy, and cycle 1134 found it is also the **staging** area:
`0x8229BE98` commits `+0x70..+0xA0` into `+0x20..+0x50` inside one object. **An
authored position is written at `+0xA0`, not `+0x50`**, which is why ten cycles
of scanning `+0x50` found only copies — the copies are the commit. Accessors take a
pointer biased by `+0x10`, because `0x82270380` returns `object+0x10`
(`0x82270434`). Established twice independently: the constructor writes the
identity basis from `0x8204F7F0/F800/F810` into `+0x20/+0x30/+0x40` and zeroes
`+0x50`, and the tag-2 order reads `+0x50` as the current position at
`0x82295C48`.

The constants that claim carries were read rather than assumed, after a review
pass pointed out that they had not been: the value the constructor stores into
`+0x50/+0x54/+0x58` is `DAT_820542B8` = **0.0**, and the two other constants used
throughout are `DAT_8200082C` = **0.0** and `DAT_82001348` = **1.0**, the w
component. So "born at the origin" is measured, not inferred from a register
name.

**Two families, not one.** `0x820A7070` builds one **unit** per record — 230,
through `CX360UnitManager`'s virtual `+0x10` (`0x820A7F48`), inserted by
`0x8226FEC0`, class `ACE6::CAce6UnitPlayer`, vtable `0x820568D4`, size `0x100` —
and one **Obj entity** per `ObjBin` record — 434, from record word 2, class
constructed by `0x8228F6B0`, vtable `0x82008F58`, size `0x340`. The loader's
three calls differ: selectors 0 and 1 each build units into their own manager
from the *same* slot 0, so both unit managers hold the same 230 records; selector
2 sets `r16 = 0` at `0x820A760C` and builds **no** unit at all. The order
program steers the *unit*: at `0x82295C0C` it reads `+0xE4`, a field only the
unit carries.

**The world coordinates that exist.** 890 tag-2 order records in Mission 01,
resolved by `0x822953F0`: mode byte `+0x42` other than 1 means the triple *is*
the world position; mode 1 means anchor-relative, resolved through
`0x82270380` and a yaw rotation about the vertical axis. Measured: 811 mode-0
records span x ∈ [−57600, 63000] and z ∈ [−63000, 55600]; 79 mode-1 records have
median exactly 0 on all three axes. Ported in `retail_world_position.{h,cpp}`.

**The route.** `unit+0xE0` is the Obj list, `unit+0xF0` the cursor into it,
`unit+0xC0` the destination. Vtable `+0x34` (`0x822A2B08`) selects entry N,
`+0x44`/`+0xA4` (`0x822A2C80`) searches for a matching entry, and `0x822A23D8`
resolves the entry's Param block by a mode byte `+0x2A` with an anchor pair at
`+0x2C/+0x2D` — the same record shape as the tag-2 order at other offsets.

**The mass transfer.** State `0x822E7760` (entry code 8) calls `0x8226E950`,
which clones every object of the manager at `context+0x2A0` onto the
corresponding object at `context+0x2A4` — transform included, via `0x8226CF90`
and `0x8226B368`. Both managers are filled from the same slot 0 at load, so this
is a reset of a live set from a pristine one, not an origin.

**`PLAD`.** The mission's FHM bundle carries, as child 2, a counted array of
`0x10`-byte records (`0x82249BA8` exposes it, `0x82249BC8` indexes it). Mission
01 declares one: `(-2025.0, 1500.0, 1345.0)` then the word `0`. All three
mission loaders read **only the fourth word**, index the array by the player
slot at `global+0x4B40`, and store it to `+0xF0` — the route cursor — of the
object at `context+0x12B844`, which is unit manager #1's `+0x404`, its elected
unit. So `PLAD` names **which route entry a player starts on**.

## What is ruled out

| candidate | verdict | cycle |
| --- | --- | ---: |
| the `Obj` sub-record's three floats | not a position: three separate consumers behind three guard bytes, `+0x08` reloaded into a countdown at `object+0x314` | 1125 |
| the load path (`0x820A7070`) | writes only `+0x60`, `+0xD0`, `+0xD4`, `+0xD8`, `+0xDC`, `+0xE0`, `+0xE4` of a unit; every object leaves at the origin | 1124, 1126 |
| the tag-2 sub-kinds `0x822961CC`, `0x82296260`, `0x822962BC` | all read `+0x50` as current position, none writes it | 1126 |
| `0x8226B618`, the mission-start prep | resets the faction and counter tables, places nothing | 1126 |
| `CAce6UnitPlayer`'s 120 vtable slots | no slot writes a non-stack `+0x50` | 1127 |
| the `Maneuver` block at `+0x210` | no world-scale float in any of the 434 | 1126 |
| the route's Param blocks | mode 0 everywhere, x ∈ [0, 60000], **y = 0 everywhere**, z ≈ 0 | 1127 |
| `PLAD`'s three floats | read by nobody on the load path | 1131 |
| every reachable `+0x50` write in the mission cluster | 26 copies, 1 matrix composition, 1 copy — none authored from data | 1132, 1133 |
| `0x820F9168`'s `vupkd3d128` | unpacks a *zero* vector: an identity init, not decompressed data | 1133 |

## The instrument, and three ways it was wrong

Finding writers of `+0x50` took four attempts, and the failures are the
transferable lesson:

| rule | hits | what it wrongly counted |
| --- | ---: | --- |
| any `addi rX,rY,0x50` or `stfs …,0x50(rY)` | 797 | `addi rX,r1,0x50` — every stack frame |
| the same, minus `r1` | 52 | three stores of one register — every bulk memset |
| three **distinct** sources | 12 | long runs of constants |
| effective offset, resolving `addi` bias and `li` index | 65 | — the only version that found anything |

`tools/ghidra_scripts/Ac6TransformWrite.java` is the last one. A struct offset is
not a handle until the idiom that writes it is known.

And it is still not enough. Cycle 1133 measured the blind spot: the scans see
`stvx128` with a resolvable index and `stfs` with a literal displacement, and are
blind to indexed stores, of which the binary has **1018** — `stfsx` 552, `stvlx`
218, `stvewx` 194, `stvrx` 54. So the classification below is exhaustive *for the
idioms scanned*, and cycle 1132's stronger claim was an overclaim, corrected in
1133.

## The enumeration, closed per idiom (cycles 1136-1138)

| idiom | sites | verdict |
| --- | ---: | --- |
| `stvx128`, ports `+0x50` and `+0xA0`, followable index | 108 | 74 copy, 34 assemble; 3 read a record — event stream, replay stream, a `0x2CF4` pool — none is the spawn |
| `stfs`, literal displacement, distinct sources | 12 | all explained: memsets, constant tables, copies |
| `stfsx`, indexed triple signature | 21 | none in the mission cluster |
| `memcpy` (`0x82382F70`), transform-sized | 38 | one in the unit class (`0x822A6090`), and it writes `this+0x04`, not the transform |

**No code in the mission cluster writes a unit's position from mission data, by
any store idiom this campaign can enumerate.**

Two readings survived that, and cycle 1139 killed the first. If units took their
place from their first tag-2 order, the 108 units without such an order could not
be mobile — they would be fixed targets of another class. Measured: the routed
group is `{class 1: 9, class 2: 113}` and the unrouted `{0: 1, 1: 31, 2: 75,
4: 1}`. Seventy-five units of the *same* class as routed ones have no order and
no position of any kind. There is no split, so the reading is refuted.

What is left is the uncomfortable one: **the position is written by something
still outside the enumerated list** — a variable-size copy, or a hand-written
loop. Four idioms are closed and something is still missing.

## What remains

No function has been found that gives a unit its first world position. The
sharpest remaining handles, in order:

1. **The other consumer of `PLAD`'s floats,** if one exists. The array is
   reachable through the resource system; only the two accessors were swept.
2. **What fills the objects of the manager at `context+0x2A0`,** the pristine
   set the mass transfer copies from. Selector 0 of `0x820A7070` is the only one
   of the three that runs two extra loops over its manager.
3. **The 1018 indexed stores** the scans cannot see. Resolving `stfsx` needs
   real value propagation for the index register, not just a constant `li`;
   doing it badly would add a fourth false-positive class to cycle 1128's three.

A working hypothesis worth stating so it can be killed: units may have no
authored start position at all, taking their place from the route entry `PLAD`
names, resolved at runtime through `0x822A23D8`'s anchor mode. Nothing supports
it yet, and the Param-block measurement above argues against it.
