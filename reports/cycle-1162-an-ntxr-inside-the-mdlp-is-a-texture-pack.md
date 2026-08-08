# Cycle 1162 — an NTXR inside the MDLP is a texture pack, and the size check caught it

## Why the first contact sheet was the wrong population

Cycle 1161's sheet showed fonts, HUD frames, radar overlays and briefing art.
That is what the corpus holds: those 692 wrappers are the FHM *children* of the
mission bundles, which are the UI and presentation side.

Mission 01's **world** textures were never in that corpus. They are the 86 NTXR
chunks nested inside `001_MDLP.mdlp`, and nothing had extracted them. Walking
the MDLP with `tools/ac6_fhm.py` — including the 47 entries that nest a further
FHM — pulls out all 86.

## What happened when they were decoded

```
wrote     4
refused  82
```

Every one of the 82 refusals is the same cause: a wrapper declaring **one level**
whose payload is larger than the surface rule requires.

```
chunk            GIDX     W     H   payload      rule  ratio
e01_05.ntxr         5   512   512    311296    131072   2.38
e03_02.ntxr         8  2048  2048   5570560   2097152   2.66
e04_29.ntxr         5   128   128     81920     16384   5.00
e05_05.ntxr         8   128   128     98304     16384   6.00
```

And the explanation is in the census that was already on record. The MDLP holds
**522 GIDX for 86 NTXR** — about six identifiers per chunk. Counting GIDX
records per chunk:

```
GIDX per chunk   1: 4    2: 3    4: 12    5: 19    6: 13    8: 29    9: 2   10: 3   11: 1
```

**The four chunks that decoded are exactly the four with a single GIDX.** An NTXR
inside the MDLP is a texture *pack*: N surfaces identified by N GIDX records,
with the header describing only the first.

## The size check earned its place

`decode_ntxr_base_level` refuses a wrapper whose payload disagrees with the
surface rule. Cycle 1152 added that check on general principle and it cost 82
textures here — which is the correct outcome, not a shortfall.

Without it the decoder would have addressed the first surface of every pack and
returned an image. That image would have been *right*, because the first surface
really is at the start; and the wrapper's remaining five would have been silently
invisible, with nothing anywhere reporting a problem. A decoder that quietly
drops five sixths of a mission's art while producing convincing output is worse
than one that refuses, and only the size check distinguishes them.

It also retroactively explains `first-linked-0x10002215.ntxr`, whose 507,904
bytes against a 262,144-byte surface cycle 1152 flagged and
`AC6_MATERIAL_TEXTURE_LINK_REPORT.md` had already called an *aggregate* tail. It
is not a truncation artefact. It is a pack, and the report's word was right.

## What the four show

256×256, format `0x14` (BC3), nine mip levels, decoded through the declared base
size: concrete facades, guard rails, brickwork and industrial panelling. Mission
01 is the defence of Gracemeria, an urban mission, and building surfaces are what
it should hold.

They are shown for inspection only. No parity claim, and the pixels stay local.

## What is next, and what is not being guessed

The GIDX record layout — where each surface's descriptor and data live inside a
pack. `AC6_MATERIAL_TEXTURE_LINK_REPORT.md` has the GIDX identifier read for two
aircraft, so the identifier itself is known; the pack's internal directory is
not, and the ratios above (2.38, 2.66, 5.00, 6.00 against GIDX counts of 5, 8, 5,
8) are not a rule and are not treated as one.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed. The decoder's behaviour is unchanged; this cycle found
out what it was refusing and why.
