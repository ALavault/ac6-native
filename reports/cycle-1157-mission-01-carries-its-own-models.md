# Cycle 1157 — Mission 01 carries its own models, and they were always beside the scenario

## What is in the file

`reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/001_MDLP.mdlp` — 29,097,984
bytes, in the same bundle as `000_00_00_00_10.bin`, the scenario container this
product has been reading since J2, and `002_PLAD.plad`.

The header, validated rather than assumed:

```
+0x00  'MDLP'
+0x04  entry count          94
+0x08  total size           29,097,984  == the file size exactly
+0x0C  entry table offset   0x1000
+0x10  base for the table   0x2000
```

Three checks, and a wrong table offset or stride breaks all three at once:

```
entries starting with 'FHM '   94 / 94
offsets monotonic              true
declared size == file size     true
```

Every entry is an `FHM ` bundle. Across the 94:

```
NDXR  292      geometry
MATE  381      materials
NTXR   86      textures
GIDX  522      resource identifiers
```

`tools/ac6_mdlp_index.py` reads and checks this; retail bytes stay inputs and
are not copied.

## Why this matters more than the count

Step 2f — the class byte → object category → model binding — has been the
ladder's named risk since the plan was written: *"this is where JV would quietly
become J1 again"*, because a hand-written class→model table satisfies the gate
and loses the property the gate exists to protect.

Cycle 1148 established that the model selector is not in the scenario container
and concluded the derived route "needs the external definition table identified
and parsed first". It does. **That table's data is Mission 01's own MDLP**, and
the MDLP has been sitting in the same directory as the scenario container for
the entire campaign.

Cycle 1096 found that each constructed object carries a resource pointer at
`+0x15C`. Cycle 1147 found that `0x820A7070` fills it from two `0x8228E9B8`
lookups keyed by bytes `+0x61`/`+0x62` of a record that is *not* in the scenario
container. 94 entries, indexed by a byte, is the shape those lookups want.

I am not asserting that join. The byte→entry correspondence is exactly the thing
that has to be derived rather than fitted, and this cycle establishes only where
the data lives and that its index is sound.

## The same lesson, a fourth time

Cycle 1148 wrote that the binding "cannot be derived from the scenario container
alone" and that it "needs a second data source". Both true. What neither that
cycle nor cycle 1155 did was look in the mission's own bundle — the directory
whose other two files this product already parses.

Cycle 1156 counted three scope errors in this campaign, all producing false
negatives. This is a fourth of the same family and the most expensive, because
it was not a search that ran with the wrong bounds. It is a search that was
never run at all: I reasoned about which archive would hold the definition table
instead of listing the directory I already had open.

## What is now unblocked, and what is not

**Unblocked.** The inputs for both halves of step 2d's binding and for step 2f
are present and indexed: 292 NDXR, 381 MATE, 86 NTXR and 522 GIDX for Mission 01
specifically, not for the campaign at large.

**Not established, and each is real work:**

- the FHM child layout inside an entry, so a bundle can be walked to its NDXR,
  MATE and NTXR rather than regex-scanned for signatures;
- the join from a unit's class byte to an MDLP entry index — the one that must
  be derived;
- the MATE material → texture id → GIDX resolution, which
  `AC6_MATERIAL_TEXTURE_LINK_REPORT.md` has exact for two aircraft and explicitly
  does not promote to all wrappers.

The signature counts above come from a regex sweep, which finds chunks but does
not prove containment or ordering. That is enough to say the data is there and
not enough to parse it, and the distinction is deliberate.

## Decided rather than asked

No FHM walker, no join, no parser. The temptation here is precisely the one the
ladder names: the inputs are suddenly in hand, the counts look conclusive, and a
class→model table could be produced this afternoon by matching entry ordinals to
unit classes and checking whether the result looks plausible. That would be
fitting, not deriving, and it would pass the gate.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed.
