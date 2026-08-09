# Cycle 1486 — the terrain is textured

## Qualification

- **No Ghidra run and no oracle pass.** The image, the archive, and a six-way
  parallel sweep whose every claim used here was re-measured before use.
- No product C++ changed; ctest stays **60**. **No contract entry.**

## The sweep, and the rule for using it

Seven agents, 792k tokens, six investigations and a synthesis. Nothing below is
taken on the agents' word: each claim reported here was re-run against the image
or the archive by this cycle, and the two that were checked and failed are named
in the previous two cycles.

## Correcting cycle 1478's account, not its fix

Cycle 1478 added an alpha test with a cutoff of 128 and the bridge's hangers
appeared. Re-measured here, over every material in the package:

```
materials read 4318
  +0x0C blend    0x0000:4318
  +0x0E alphafn  0x0000:4318
  +0x10 alpharef 0x0000:4318
distinct raw 32-byte materials: 2
  30000010 ...  x4313
  30000090 ...  x5
```

**Two distinct materials in the whole map**, differing only in a shader id, with
the blend index, alpha function and alpha reference all zero. The sweep read the
draw and found those three fields at `0x823647C0`/`0x823647DC`/`0x82364838`,
with the `1/255` constant at `0x82069B34` proving `+0x10` is an 0..255 reference.

So on the **material** path this map is uniformly opaque with the fixed-function
alpha test disabled. Cycle 1478's picture was right and its account was not: the
alpha it needs is not in the material record. The draw's other path —
`if (item.flags & 0x80) { alpha test on; func 6; ref = item+0x10; }` — and the
two shader ids are where it must come from, and neither has been read.

## The terrain's texture, verified

The sweep's largest finding, re-derived here from the instructions rather than
accepted:

```
0x820FAE08  li  r10,0xF          -> [this+0x6D6C] = [this+0x6D70] = 15
0x820FAE10  li  r9,0x7           -> [this+0x6D64] = 7
0x820FAE18  li  r8,0x110         -> [this+0x6D68] = 272
0x820FAE28  lfs f0,-0x5E04(r11)  -> 0x8206A1FC = 0.06640625
0x820FAE34  stfs f0,0x6D78(r31)
0x820FAE38  stfs f0,0x6D74(r31)
```

`0.06640625` is exactly **272 / 4096**.

> **Seven atlas pages of 4096 x 4096, each a 15 x 15 grid of 272-pixel tiles.**

And the container has exactly that: `021_FHM/016_FHM` holds **seven** NTXR — six
of 22,417,408 bytes and one of 5,640,192. That is the 140 MB container this
campaign has listed as "unexamined" since cycle 1445, and it is the terrain's
texture set.

The per-cell assignment is `.mti` — `010_00_00_00_01.bin`, 24 x 512 bytes, one
record per patch, 256 cells of two bytes: page index and tile index.

**So "the ground is flat colour" is a defect with a named cause**, not a missing
asset.

## And a reading of mine the sweep killed

Cycle 1440 called `013` "386 x 512 with a nibble alphabet". It is
**1024 + 24 x 8192** — a 256-entry u32 table over 24 fixed 8192-byte blocks, each
128 x 128 nibbles. 386 and 512 are divisors of 197,632 and that is all they were.
The lesson is the file's own: a size that divides is a candidate, not a reading,
and I published the divisor.

## Not established

- Where the alpha test comes from. Two candidates, both unread.
- The atlas's UV convention and the meaning of `[this+0x6D80] = 0.9393382`.
- Every sweep claim not re-measured in this cycle, of which there are many. They
  are in `reports/` only as an agent's output.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**Bind the atlas.** Seven pages, a 15 x 15 grid, a 272-pixel tile, a UV step of
272/4096, and `.mti` giving page and tile per cell — every number verified, and
the decoder for a 4096 x 4096 NTXR already exists and is contracted. It is the
single largest change left to what a frame looks like, and for the first time in
this thread nothing about it is a guess.
