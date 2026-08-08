# Cycle 1246 — the product must not get an FHM reader, and the reason was already on the table

Task 14 says a drawn retail mesh rests either on the measured FHM layout or on
offline extraction, and asks for the choice to be made and written down rather
than left to become invisible. Made.

## The choice as it looked

`ModelDirectory` (cycle 1174) ports `0x8228E988`/`0x8228E9A8`/`0x8228E9B8` and
stops at the entry boundary: it hands back an **`FHM ` bundle**, and its last
word on the subject is `every_entry_starts_with("FHM ")` — 94 of 94. Getting an
NDXR out of that bundle needs the FHM walk, and cycle 1192 established that walk
is **measured**: `tools/ac6_fhm.py`'s offsets rest on 94 of 94 bundles parsing
cleanly, not on a reading.

So the apparent options were: port a measured format into the product, or have
the extraction resolve the binding and hand over a mapping — which is a manifest,
and JF exists because manifests were eliminated.

## Why both are wrong, and the argument was already established

**Retail does not parse an FHM container.** Cycle 1192: the bytes `46 48 4D`
occur **zero times anywhere in the loaded image**, on a byte scan controlled by
the same scanner finding `NDXR` at `0x8200A24C`, `GIDX` at `0x82067EC8` and
`NTXR` at `0x82067EC0`. There is no FHM reader to port, and porting one would be
porting **something the shipped game does not do**.

What retail does instead is now known, from work that happened after cycle 1192
and was never connected back to it:

- `0x820A7070` uses the ObjBin bytes `+0x61`/`+0x62` to index the model
  directory, and stores the result at `object+0x15C` — a **handle**, held and
  passed, not parsed. Cycle 1205 found `0x82129D00` reading `+0x15C` and
  `0x82222F80` toggling a flag on it; nothing walks it.
- Actual resources are resolved **by integer id through the registries** —
  textures in `0x828C8100` keyed by `GIDX+0x08` (cycles 1207, 1209), shaders in
  `0x828CCB80` keyed by `material+0x00` — and those registries are filled at boot
  by `0x821D5EF8`'s pack mounts, not by walking a container.

**So the binding retail uses is by id, and the FHM container is packaging that
the shipped executable never opens.**

## The decision

**No FHM reader in the product.** The FHM walk stays where it already is —
offline, in `tools/ac6_fhm.py`, producing the extracted corpus — and is labelled
what it is: an extraction convenience for a format the game does not read.

The product's boundary is **NDXR bytes in, geometry out**, which is where
`NdxrContainer` already sits. That is not a compromise; it is the boundary retail
itself has.

This also means the honest version of task 14 is **not** "walk MDLP → FHM →
NDXR". It is: follow the id path retail follows, which needs the registries
populated — and cycle 1209 established those are **BSS, filled at runtime from
the packs**. That is a different and harder problem than the one the task
described, and pretending otherwise by porting a measured walk would be exactly
the "JV quietly becomes J1" failure the task warns about.

## What this costs, stated plainly

The product cannot currently go from a unit's model byte to NDXR bytes by any
derived route. It can decode an NDXR it is handed. Bridging that gap means either
reproducing the registry population — a large piece of runtime state — or
accepting an extraction-side index and **saying so in the contract**, with a
behaviour whose status is not "derived".

I am not choosing between those two here. **What is decided is the negative**: the
product does not acquire a reader for a format its subject does not read, and
that decision is now written down where the next cycle will find it instead of
being rediscovered as a temptation.

## Not established, stated plainly

- What `object+0x15C` is actually consumed by, beyond being held and flag-toggled.
  If something does eventually walk it, this decision needs revisiting — and the
  cycle-1192 byte scan says whatever walks it does not check an `FHM ` magic.
- Whether the registries can be populated from extracted files without
  reproducing the pack mount path. Unexplored.
- Cycle 1174's `ModelDirectory` remains correct and useful; nothing here retires
  it. It resolves the index that retail resolves. It simply does not, and should
  not, go further.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```

No product code changed. This cycle is a decision, and its value is that the
decision is refusable by a later reader who disagrees with the argument.
