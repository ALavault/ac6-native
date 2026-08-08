# Cycle 1205 — the Scene/CUT ownership edge is refuted, and the fail-closed rule gets a better reason

`MISSION_VISUAL_BOOTSTRAP_REPORT.md` has held `mission_scene_group_activation_proved = false`
since cycle 760, with four `mapobj_m01` NDXR candidates deliberately uninjected.
The rule was "no join found". **It is now "no join exists", and the difference
matters.**

## The class exists; the data does not

The Demo/Scene instance factory is `0x82127A88`, reached only through
`IObjectDemo` vtable slot 1 (`0x82183800`, vtable `0x82062EEC`). It dispatches at
`0x82127AD4` on a `u16` type code taken from a Scene object record. Every branch
allocates and stores a class vtable, read from the stores rather than inferred:

```
82129b24  cmpwi cr6,r11,0x2005
82129b28  beq   cr6,0x82129c10      ; the only path to CDemoDDMapObj
```

`0x8205D774`, the `CDemoDDMapObj` vtable, has **exactly one reference in the
whole image** — its own construction store at `0x82129C58` — and zero data
pointers. There is no second construction site.

Across **all 44 Mission 01 Scene groups**, 575 records and 11 distinct type
codes: **zero records of type `0x2005`**, and zero of `0x2011`
(`CDemoDDContainer`). A raw `u16` scan at every even offset of all 2,676,164
bytes of NFICCUT payload finds `0x2003` 41,747 times and `0x0101` 61,347 times —
and `0x2005` **zero**.

## And the branch would not own a mesh anyway

`0x82129C10` allocates `0x50` bytes (`li r4,0x50` at `0x82129C30`) — not a model.
It remaps a `u32` object index, then:

```
82129cf0  bl  0x8226f9b8    ; look up an ALREADY-EXISTING world object by index
82129d00  lwz r3,0x15c(r3)  ; its model handle
82129d04  bl  0x82222f80    ; toggle a flag on it
82129d10  bl  0x8212c020    ; bind a name from the demo resource table
```

`+0x15C` is the field `0x820A7070` fills from the two `0x8228E9B8` MDLP lookups.
**The Scene record borrows a model handle the gameplay loader already built.**
The identical `0x8226F9B8` → `+0x15C` → `0x82222F80` triple appears in the
`CDemoDDLod` branch. MapObj is a demo-time handle binding a `.mop` track to a
world object — a reference, not an ownership edge.

## The namespace it could reach holds no geometry

`0x8212C020` searches a table filled by `0x8212C360`, whose two feeders register
either the Scene group's path table or names suffixed `.nud`, `.hir`, `.mop`,
`.bin`, `.nut`. In entry 9 — 42 MB, the whole of it — the counts are:

| `.mop` | `.nud` | `.nut` | `.hir` | `.bin` |
|---|---|---|---|---|
| **1106** | 0 | 0 | 0 | 0 |

while the same scan finds `NDXR` 292, `NTXR` 114, `GIDX` 550, `MATE` 381.
**The geometry is there and no Scene name can reach it.**

## The accounting closes with no remainder

553 named records — 247 MoveEffect, 143 Lod, 33 LodR, 22 each of MoveCamera,
DOF, Vignetting, ChromaticAberration and Tree(20), 16 DDPlayer, 5 DDWingman,
1 Fade — **exactly the 553 resolved Scene paths**, with the remaining 22 being
nameless clouds. Every printable ASCII run of four or more characters in every
record payload ends in `.mop`, without exception.

## The control that makes the zero mean something

Three that could have failed:

- the byte scanner reproduced cycle 1192 exactly (`NDXR` at `0x8200A24C`, `GIDX`
  at `0x82067EC8`, `NTXR` at `0x82067EC0`);
- the address scanner reproduced cycle 1193 exactly (`0x8293BA2C` read at
  `0x821CC1E8`, written at `0x821CC310`);
- **the retail data names the type codes itself.** Chunk `0x30410000` of every
  NFICCUT carries a dictionary — `0x1001 "MoveCamera"`, `0x0003 "AnimRigid"`,
  `0x1102 "MoveLightP"` and more — and **seven agree exactly with the class the
  executable's dispatcher constructs**. That table was not used to build the
  mapping; the vtable stores were read first and the dictionary was found to
  agree afterwards.

And the singleton control, which is what stops the zero from being an artefact:
type `0x3001` occurs **exactly once** in all 44 groups, and the parser found it.
A lone `0x2005` anywhere would have been visible.

## Not established, stated plainly

- **Whether `CX360DemoManagerDD` runs during Mission 01 at all.** Its init
  `0x82183E58` is reached only through a vtable and no `bl` targets it. The
  global is constructed by the CRT initialiser `0x823D2530`, which proves
  existence, not execution.
- **Where `*0x8291889C + 0x5174` is sourced from.** Published at `0x82185C84`,
  24 entries, from an archive node resolved by two `0x821D2FC0` calls. It is the
  only door through which a Scene-adjacent path could name a `.nud`. Entry 9 has
  zero of those extensions, so if it is sourced there it registers nothing — but
  that was not proved. **This is the single probe that would reopen the question.**
- Dictionary codes `0x1002`, `0x1103`, `0x1104` have no branch in the tree and
  fall to the default. The dictionary is engine-generic and a superset of this
  build.
- Whether the four `mapobj_m01` NDXRs are owned by the scenario Obj→MDLP path
  instead. Out of scope here and untouched.
- A repository inconsistency: `analysis/address_catalog.tsv:54` places the m01
  mapobj MDLP at `DATA.TBL[34]/root/0001`, while
  `reports/cycle-760-qualified-entry9-mapobj-batch.md` is titled "entry9". One
  provenance is loose and neither was corrected here.

## Decision

`mission_scene_group_activation_proved` stays **false**, and the four
`mapobj_m01` NDXRs stay uninjected — for a stronger reason than before. Mission
01's Scene/CUT system provably owns 553 `.mop` track payloads and nothing else,
every record accounted for; the one class that could have referenced a map object
has zero records in 2.68 MB of the mission's own CUT data; and it would not have
owned the mesh if it did. **A picture obtained by injecting those NDXRs would be
a fabrication with no retail edge behind it.**

JV step 2e is closed negatively. Terrain does not come from here.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed. Qualified against `default.xex` SHA-256
`acc302c1…11bcde` and entry 9 SHA-256 `cd81e021…4b6d7a05`. No oracle used.
