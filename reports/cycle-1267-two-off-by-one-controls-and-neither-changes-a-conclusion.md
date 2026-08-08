# Cycle 1267 — two off-by-one controls, and neither changes a conclusion

## Qualification

`ghidra-projects-xenon/ac6-xenon` via the corrected `Ac6XenonFindWord`;
`default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass was spent.** No
product code changed.

## Why

Cycle 1266 found that a `.pdata` row had been read as a dispatch-table slot, and
cycle 1266's own lesson — chase a wrong reading to every site rather than the
one you noticed — applies to it. Every published claim in the repository that
rests on "appears as data" was re-run through the fixed instrument.

## The audit

Four addresses, one scan:

```
pdata=[82079e00,82089fb0)
  82345098 = 0aligned/0unaligned/0pdata
  82345100 = 0aligned/0unaligned/1pdata
  8229C920 = 6aligned/0unaligned/1pdata
  8229ADF8 = 0aligned/0unaligned/1pdata
```

**Two published numbers are wrong, and both are corrected in place.**

### Cycle 1263 — "neither address appears as aligned data"

`0x82345100` **does**, once, at `0x82085D20` — its own exception record.
`0x82345098` has no row and no hit at all.

The conclusion drawn from it ("neither is reached through a vtable or a dispatch
table") survives, because an exception record is not a vtable. The observation
as written is false. It is **the same error cycle 1266 caught in cycle 1265,
present one cycle earlier and unnoticed** — which is what a systematic re-run is
for, and what noticing-one-at-a-time is not.

### Cycle 1244 — "control `0x8229C920` = 7"

The control is **6**. The seventh hit was that function's own `.pdata` row.

The claim it supported is untouched: `0x7D1` and `0x7D4` occur zero times as
data, and a small message code cannot collide with a `BeginAddress` in the first
place, so the codes' zero was never at risk. Six is as good a positive control
as seven. The number was still wrong, and it stood in `MISSION01_LADDER.md` as
well as in the cycle report.

## What survives unchanged

- **Cycle 1255's `0x8234CDC0` = 0.** That function has no `.pdata` row, so its
  zero data hits genuinely means no vtable and no dispatch table reaches the
  registry insert. This was luck at the time — the scan could not tell — and it
  is now visible in the output as `0aligned/0unaligned/0pdata`.
- **Cycle 1244's placement census.** Three `li 0x7d1` and three `li 0x7d4` in
  851,718 instructions, both codes zero as data.

## The shape of it

Neither correction changes a conclusion. That is exactly why they are worth
making: a repository whose numbers are right only where the answer depended on
them is a repository whose numbers cannot be cited. The next reader has no way
to know which figures were load-bearing on the day they were written.

Two of the four addresses scanned here were published in the same sentence as a
conclusion that did not need them. Both were wrong. Neither was ever going to be
caught by a gate.

## Not established

- Whether any claim **older than this session** rests on the same conflation.
  The grep covered `reports/` and the root documents for the phrasings this
  session used — "as data", "as aligned data", "occurrences as data" — and a
  cycle that phrased it differently would not have been found.
