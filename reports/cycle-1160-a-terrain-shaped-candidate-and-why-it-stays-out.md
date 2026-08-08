# Cycle 1160 — a terrain-shaped candidate, and why it stays out

## What the MDLP looks like when ranked by size

Mission 01's 94 bundles, largest first:

```
    bytes  entry  NDXR  MATE  NTXR   largest NDXR
  5578752      3     0     1     1              0
  1912832      2     5     5     0         703910
   598016     56     8     8     1          66086
   598016     48     8     8     1          66326
   581632     64     8     8     1          60886
```

Two entries stand apart from the other 92.

**Entry 3** is 5.5 MB — 19% of the whole MDLP — and carries **no geometry at
all**: one MATE, one NTXR. A texture bundle an order of magnitude larger than
any other.

**Entry 2** carries five meshes and its largest NDXR is **703,910 bytes**,
against roughly 66,000 for the biggest mesh in any other geometry bundle. Ten
times the size of an aircraft.

## Why this changes nothing

`MISSION_VISUAL_BOOTSTRAP_REPORT.md` holds static-environment rendering
fail-closed:

> No executable ownership join yet identifies terrain or another
> static-environment mesh for this selected group. This pass therefore adds no
> synthetic sky, ground plane, proxy geometry, or guessed environment.
> Static-environment rendering remains fail-closed until an archive resource
> **and its retail Scene/CUT ownership edge** are both proved.

A 704 KB mesh in the mission's own bundle is an archive resource. It is not an
ownership edge. Nothing read so far says which Scene group owns entry 2, or that
entry 2 is terrain rather than a carrier, a hangar, a city block or a cutscene
prop — the size is consistent with all of them.

The reasoning that would inject it goes: *the mission needs terrain, terrain is
big, this is the big one, draw it and see*. That is the shape of every rule this
campaign has killed, and it would be worse here than usual, because a large mesh
drawn under a camera that is also unproved would produce a picture, and a picture
is what makes people stop checking.

So the candidate is recorded and not used. The plan's instruction on this step
was explicit — *prove the ownership edge; do not lift the fail-closed rule* — and
the value of writing the candidate down is that the next cycle can go looking for
the edge with a specific entry in hand, rather than re-deriving which one to ask
about.

## What would actually discharge it

An executable join from a Scene or CUT structure to an MDLP entry index, read in
the image, in the same way cycle 1158 derived that the unit model index space is
the MDLP's. That is a Ghidra question and it is the next one for this step, not a
data question.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed, and no static environment is drawn.
