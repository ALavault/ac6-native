# Cycle 1482 — verifying the sweep

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`.
- No product C++ changed; ctest stays **60**. **No contract entry.**
- Six parallel investigations were launched at the reviewer's suggestion; **three
  have returned**, and this cycle checks the one that contradicts work already
  landed.

## Why this cycle exists

A subagent reported, with an address:

> "'tone%s.xml supplies the LevelCorrection/Saturation values at runtime' —
> killed at `0x820FCA6C..0x820FCA84`: the buffer returned by `0x82101A18` goes
> straight into r4 of the deallocator `0x82222F20`."

Cycle 1481 had just applied those values. A result that contradicts landed work
is the one to read first, and an agent's report is a claim, not evidence.

## What the instructions actually say

```
0x820FCA38  addi r5,r1,0x58          the size out-parameter
0x820FCA44  bl   0x82101A18          load "tone%s.xml"  -> r3
0x820FCA4C  bc   4,26,0x820FCA6C     if it loaded, skip the fallback
0x820FCA60  bl   0x82101A18          otherwise "tonedef.xml"
0x820FCA6C  lwz  r11,0x64(r1)
0x820FCA70  mr   r4,r3               the blob
0x820FCA74  lwz  r10,0x60(r1)
0x820FCA7C  stw  r11,0x1398(r10)     ** something is stored first **
0x820FCA80  bl   0x82222F20          then the blob is freed
```

**The half that holds:** the buffer is freed at `0x820FCA80`, by the same
deallocator this loader uses for `.pdl` and `.edl`. The tone XML's bytes do not
persist in that buffer.

**The half that overshoots:** a store to `[r10+0x1398]` happens *immediately
before* the free. So something is kept. The agent's conclusion — that the values
never reach runtime — does not follow from these instructions.

And my own first reading of that store was wrong too: I took `r11` for the file
size, but the size out-parameter is `r1+0x58` and `r11` comes from `r1+0x64`, a
different slot. What is stored is unread.

## What it means for cycle 1481

The post-process values stay. They are in the archive, the file states them, and
nothing here shows them wrong. What changes is the confidence: **whether retail
applies them at runtime is now an open question with an address on it**
(`0x820FCA7C`), where before it was an unexamined assumption. That is a better
position than the cycle had yesterday, and it came from a contradiction rather
than from agreement.

## The three investigations that returned

Recorded here because they are evidence I did not gather; each is a claim to be
checked before use, exactly as this one was.

- **the `.sph`**: the buffer is inline at `CMapManager+0x6850` with a pointer at
  `+0x6840`; the loader byte-reverses a hard-coded **88 words = 352 bytes = 2 x
  176**, so the file is 352 bytes and stored **little-endian**; `022_FHM/002` is
  352 bytes and is the file; two `CSkySphere` objects sit inline at `+0x62E0`
  and `+0x6590`, stride `0x2B0`. It refuted the 1,654-byte candidate cycle 1474
  had favoured on its opening floats.
- **the mapset**: `sky1` and `sky2` are the **same 127 keys in the same order** —
  two states of one schema, 56 differing numerically. Full `HDR`, `Vignetting`
  and `LensFlare` values, the last being eight lens elements with position,
  radius and colour.
- **the trees**: `CTreeManager` is embedded in `CMapManager` at `+0x161D0`, and
  `CTreeGenerator` loads exactly three files — `sim:map/m01/m01.wsd`,
  `sim:map/m01/m01.wpd`, `sim:map/TreePreset.xml` — **hardcoded to `m01`**, not
  `%s`-parameterised like every other name in the map loader. It also refuted a
  standing hope: the 86 `.bin` entries in `014_FHM`/`015_FHM` are **all
  zero-length**.

## Not established

- What `[r10+0x1398]` receives, and therefore whether the tone values are used.
- The other three investigations, still running.
- Every claim above, until read. They are listed as an agent's output and not as
  this campaign's findings.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**Read `[r10+0x1398]`.** One store, one address, and it decides whether cycle
1481's twenty numbers describe this map's picture or only its file. It is the
smallest open question on the board and it was produced by disagreement.
