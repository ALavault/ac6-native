# Cycle 1272 — a writer shows the format for what that writer wrote

## Qualification

Corpus `reports/logs/cycle-739-pac-mission-gate/fhm/**/*.ntxr`, `runtime_idx_*`
excluded — 346 wrappers. `default.xex` SHA-256 `acc302c1…11bcde`. **No oracle
pass was spent.**

## How this started, and the reflex it nearly produced

Cycle 1256 hunted for where a wrapper's resource id lives, built two wrong
readers, and settled on locating the `GIDX` tag and reading `+0x08`.

`include/ac6/ntxr_texture.h` has documented the answer since cycle 1167, derived
from retail's own NTXR **writer** `0x821D9478`, which lays the structure out in
immediates: `+0x24` width, `+0x26` height, `+0x30` the data offset, `'eXt'` at
`+0x40` sized `0x20`, **`'GIDX'` at `+0x50` sized `0x10` with its identifier at
`+0x58`**. The header even carries the argument for preferring it:

> A writer settles a layout question more firmly than a reader: a reader shows
> what the code tolerates, a writer shows what the format is.

This would have been the third "the repository already knew" of the session, and
I was one paragraph from writing it. **It is not true, and the measurement is
more interesting than the reflex.**

## Established — the fixed offsets hold in 165 of 346

| descriptor length | first `eXt` | first `GIDX` | wrappers |
|---:|---:|---:|---:|
| 0x30 | 0x40 | 0x50 | **165** |
| 0x40 | 0x50 | 0x60 | 2 |
| 0x50 | 0x60 | 0x70 | 117 |
| 0x60 | 0x70 | 0x80 | 55 |
| 0x70 | 0x80 | 0x90 | 7 |

The per-entry descriptor is **variable-length**, measured from the `0x10` base
that `8234b0c0 addi r3,r3,0x10` establishes. `'eXt'` sits at `0x40` only when
the descriptor is `0x30` long — which is what that writer emits. So the header's
offsets are right about the writer and wrong as a general reader rule, and cycle
1256's tag search was not redundant.

**What holds in 346 of 346 is the relative structure**: `'eXt'` immediately
follows the descriptor, `'GIDX'` is exactly `0x10` after `'eXt'`, and the
identifier is at `GIDX+0x08`.

## The trap inside it

The `'eXt'` size word reads **`0x20` in all 346 wrappers**, and the next chunk
begins **`0x10`** later. Both measured, both universal, and they disagree.

**The size field is not a stride.** A parser that walks chunks by adding it
lands `0x10` past `GIDX` and reads whatever follows as an identifier — which is
a wrong id rather than a failure, so nothing announces it. That is very close to
how cycle 1256's first reader failed: it read the ASCII tag `GIDX` itself as an
id and produced 33 distinct ids for 336 distinct files.

## Recorded in the product

`ntxr_texture.h` now carries the measurement beside the writer's layout, with
the boundary stated: **a writer shows what the format is for what that writer
wrote.** Locating a chunk by its tag is the general instrument; the fixed
offsets are the special case.

## Not established

- **Why the descriptor length varies**, and what the extra `0x10`, `0x20`,
  `0x30` or `0x40` bytes hold. Five sizes were counted; none was read.
- **What `'eXt'`'s `0x20` size word means** if not a stride — a payload
  capacity, a reserved extent, or a field this reader has mis-identified.
- Whether the same variance appears in wrappers outside this extraction root.

## The correction to my own reflex

Twice this session the repository knew something I asserted otherwise, and both
times the right response was to go and check. **The third time, checking is what
stopped a false correction** — the header's claim is narrower than the use I was
about to make of it, and "the repository already knew" would have been a
comfortable sentence to write and wrong.

A prior derivation is evidence about what it derived. Reading it is the first
step; establishing its scope is the second, and it is the one that gets skipped
when the answer looks familiar.
