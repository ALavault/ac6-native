# Cycle 1278 — the tool caught my own correction being short

## Qualification

Flat image `analysis-input/ACE6_X360.exe`; instructions re-read in
`ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Why the tool exists

Cycle 1277 corrected two cycles of camera work: `0x82263A50` has a **second**
dispatch switch, and the modes the game runs in take it. The lesson was that a
`bctr` looks identical whether or not another follows it, so I wrote the command
the twenty-fifth shape demands — `tools/count_indirect_branches.py`, which counts
`bctr` / `bctrl` / `blr` over a range and, for each `bctr`, recovers the
`lis`+`addi` pair that builds its jump table.

**Run against that same function it immediately found the correction was still
short.**

```
0x82263BAC  bctr    table 0x82263BB0   built at 0x82263B98 / 0x82263B9C
0x82263E90  bctr    table 0x82263E94   built at 0x82263E7C / 0x82263E80
0x822646EC  bctr    table 0x822646F0   built at 0x822646D8 / 0x822646DC
indirect_branches  bctr=3 bctrl=5
```

**Three, not two.** And the table cycle 1273 read — `0x822646F0` — belongs to the
**third**, not the first. Its report said "the dispatcher dispatches on
`manager+0x190` through a table at `0x822646F0`" and named that the first switch;
cycle 1277 then said "the dispatcher has two switches and that was one of them",
which was right about there being another and wrong about the count.

## Established — the three switches and their selectors

Re-read here, each selector transcribed:

| switch | selector | bound | table |
|---|---|---:|---|
| `0x82263BAC` | `82263b88 lwz r11,0xdd0(r31)` — **`manager+0xDD0`** | `cmplwi cr6,r11,0x5` → 6 arms | `0x82263BB0` |
| `0x82263E90` | `82263e6c lwz r5,0x190(r31)` | `cmplwi cr6,r11,0x2e` → 47 arms | `0x82263E94` |
| `0x822646EC` | `822646c8 lwz r5,0x190(r31)` | `cmplwi cr6,r11,0x2f` → 48 arms | `0x822646F0` |

So the update passes over the camera mode **twice**, at `0x82263E90` and
`0x822646EC`, with bounds differing by one — and before either of them dispatches
on a **different field entirely**, `manager+0xDD0`, which nobody has read.

Its table's first six targets, decoded from the image: `0x82263BC8`,
`0x82263BD4`, `0x82263BE0`, `0x82263BEC`, `0x82263BEC`, `0x82263BF8` — six arms
of twelve bytes each, two of which share a target.

## What this does and does not change

**It does not change cycle 1277's finding.** Modes 1, 2, 3 and 13 still take the
`0x82263E90` switch, still call the virtual on `manager+0x198`, and still install
the FOV from `manager+0x378` at `8226401c`. The persistent camera is where that
cycle said it is.

**It changes what "I have read this dispatcher" means.** Two cycles were spent
inside a function whose control flow had been surveyed by nobody, and the
correction that identified the miss repeated the same method — reading further
rather than counting first.

## Not established

- **What `manager+0xDD0` selects**, and what its six arms do. Located, bounded,
  and unread.
- **Why the two `+0x190` switches have bounds of 47 and 48.** One arm exists in
  the later table and not the earlier, and which mode that is was not determined.
- The `bctrl` sites are five; the tool reports them as having no `lis`/`addi`
  pair, which is consistent with virtual calls but is not a reading of any of
  them.

## The shape

The command written to prevent an error found that the correction announcing the
error was itself incomplete, within a minute of existing. That is the argument
for the twenty-fifth shape in its strongest form: **the rule had been stated
twice and obeyed by reading harder, which is exactly what it warns against.** A
count is not a substitute for reading — it is the thing that tells you how much
reading is left.
