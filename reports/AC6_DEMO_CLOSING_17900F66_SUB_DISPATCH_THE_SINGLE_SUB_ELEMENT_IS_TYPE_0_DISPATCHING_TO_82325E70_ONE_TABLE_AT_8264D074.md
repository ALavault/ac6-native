# Closing `17900f66`'s open item: the single sub-element of `sub_82326420`'s
# list is type 0, dispatching once per tick to `sub_82325E70` through one
# shared table at `0x8264D074` (not a per-layer table family)

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe (`--until frontend --max-ticks 3600`,
START tuple, neutral store, bracket `[0x2DCB2E28, 0x2DCB2E38)`) plus static
reads: the table region `0x8264D000-0x8264D0DC` from
`.build/Default.xex.base.bin`, re-read in a fresh headless Ghidra
`DumpU32Range.java` run against the demo project (byte-for-byte match), and
the generated code for `sub_82326420` (`ppc_recomp.44.cpp:3910-4134`),
`sub_82325E70` (`:3017-3081`), `sub_823221A0` (`ppc_recomp.43.cpp:12141`),
`sub_823227F8` (`:13116+`), `sub_82321F70` (`:11778-11844`) and
`sub_82326608` (`ppc_recomp.44.cpp:4211-4278`).

## The question `17900f66` left open

`17900f66` closed the frame-table chain down to `sub_82326420`'s own
sub-list: count `[0x2DCB2E24] = 1`, first sub-element at `0x2DCB2E28`,
"dispatching each through table `0x8264D074`" — but the table contents, the
sub-element's type value, and the resolved handler were not established.

## The table at `0x8264D074` is one shared array

The three "tables" used by the parallel layer are windows into a single
16+ entry array starting at `0x8264D074`. Bases are materialized by
`lis -32155; addi -12172/-12164/-12140` → `0x8264D074` (`sub_82326420`),
`0x8264D07C` (`sub_82326608`), `0x8264D094` (`sub_823266F8`). Ghidra-qualified
contents (base.bin and Ghidra dump agree exactly):

| offset | value | |
|---|---|---|
| `0x8264D074` | `0x82325E70` | entry 0 |
| `0x8264D078` | `0x82325EE0` | entry 1 |
| `0x8264D07C` | `0x82326420` | entry 2 (`sub_82326608`'s "slot 0") |
| `0x8264D080` | `0x82326278` | entry 3 |
| `0x8264D084` | `0x82325F50` | entry 4 |
| `0x8264D088` | `0x823260E0` | entry 5 |
| `0x8264D08C` | `0x00000000` | entry 6 (NULL) |
| `0x8264D090` | `0x82325DF8` | entry 7 |
| `0x8264D094` | `0x82326588` | entries 8-11 (4x same) |
| `0x8264D0A4` | `0x82326608` | entry 12 |
| `0x8264D0A8` | `0x82326680` | entry 13 |
| `0x8264D0AC` | `0x82325CB8` | entry 14 |
| `0x8264D0B0` | `0x82325CC8` | entry 15 |
| `0x8264D0B4` | `0x82325CB8` | continues |

## The sub-element is type 0 → `sub_82325E70`

Live bracket `[0x2DCB2E28, 0x2DCB2E38)` (loader `sub_82278F78`, tick 2435,
thread 9; `FEFEFEFE` pre-fill from `sub_823273E0`, tick 39):

| address | value | meaning |
|---|---|---|
| `0x2DCB2E28` | `00 00 00 00` | type = **0** |
| `0x2DCB2E2C` | `00 00 1C 2C` | `0x1C2C` (next-element offset; unused, count=1) |
| `0x2DCB2E30` | `00 00 00 13` | `[element+8]` = `0x13` (stride index) |
| `0x2DCB2E34` | `00 00 00 01` | `[element+0xC]` = `0x1` |

`sub_82326420`'s loop (`:4060-4110`): `index = [element+0] = 0` →
`[0x8264D074 + 0] = 0x82325E70` — **non-NULL** — so each tick calls
`sub_82325E70(owner, element, stack+192, stack+80)` once (r3=owner,
r4=element, r5=`r1+192`, r6=`r1+80`).

## What `sub_82325E70` does each tick

`ppc_recomp.44.cpp:3017-3081`, args r31=owner, r29=element, r30=r6:

1. `sub_823227F8(r3=stack+80, r4=stack+192, r5=[[owner+32]+8] +
   [element+8]*64)` — 4x4 matrix build: zeroes 8 qwords at the destination,
   seeds two diagonal floats, then a `fmadds` chain. `[element+8]=0x13`
   selects a 64-float-stride (0x13*64 = 0x4C0) into the `[[owner+32]+8]`
   data block.
2. `sub_823221A0(r3=[owner+12], r4=stack+80)` (`:12141`): copies
   `[[owner+12]+284]` and `[[owner+12]+288]` into `[stack+80]+96` and
   `[stack+80]+100` — stamps two counters into the built matrix.
3. Final indirect call: `r3 = [[owner+12]+224]`, `[vtable+28]` slot,
   `bctrl(r3=that object, r4=element, r5=stack+80)` — the real per-tick
   consumer of the computed matrix.

## Where the stamped counters come from

`sub_82321F70` (`:11778-11844`, driver): writes `[owner+284]=r30`,
`[owner+288]=r29`, calls `sub_823266F8(r3=[owner+244])`, then vtable slot 40
of `[owner+224]`. `sub_823266F8`'s tail (`:4478`): if `[owner+412] != 0`,
calls `sub_82321F70(r3=[owner+412], r4=[[owner+12]+284],
r5=[[owner+12]+288])` — the counters hand off between the driver and the
`[owner+12]` object that `sub_82325E70` reads at +284/+288. The chain
`sub_82321F70 -> sub_823266F8([owner+244]) -> sub_82326608 -> sub_82326420 ->
sub_82325E70` is now fully linked end-to-end with static and live evidence.

## Reading

The single sub-element is **type 0** (live), resolving to **`sub_82325E70`**
(static slot, Ghidra-qualified) — a real per-tick operation that builds a
4x4 matrix, stamps the driver counters, and forwards it through a vtable
slot-28 call on `[[owner+12]+224]`. The "apparent no-op" theory from
`7513c9dc` is fully dead: every hop from the frame-table entry to a live
matrix consumer is now identified. `0x8264D074`'s region is one shared
dispatch array, not a per-layer table family.

## Not established

- `[[owner+12]+224]`'s vtable slot-28 target is runtime data (heap object),
  not static; its identity and the matrix's downstream use are a live-only
  frontier.
- `[[owner+32]+8]` data block's producer and stride semantics (who writes
  the `0x13`-indexed rows).
- Whether the `[owner+412]` recursion in `sub_823266F8`'s tail ever fires
  post-press (live `[owner+412]` not yet bracketed).

## Evidence inventory

- `git` commit `17900f66` (the report this one closes), `main` at
  `17900f66`, merged from `ba19cacc` (remote `c8476a49`).
- Probe: `/fastdata/lavaulta/tmp/frontier-j.stdout` (bracket
  `[0x2DCB2E28, 0x2DCB2E38)`), `.trace`, `.report.json` (JSON trace events).
- Static: `.build/Default.xex.base.bin` offset `0x64D074` (table, 224 bytes
  read from `0x64D000`); headless Ghidra `DumpU32Range.java 0x8264D074
  0x8264D0B4` on project `ace-combat-6-demo`, program `Default.xex` —
  byte-for-byte match.
- Generated code references as listed under Qualification.