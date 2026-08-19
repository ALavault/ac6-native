# `sub_82322A80` is cleanly absent from the post-press cursor writers;
# `2ce5c350`'s "exactly one iteration" is weaker evidence than it looked

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Re-analysis of the already-collected probe log
from `1a9b432c`/`7513c9dc` (`[0x2E4035D0, 0x2E403830)` bracket, no new
probe run) — specifically the cursor field `[owner+248]` = `0x2E4036C8`,
which the bracket already covers and had not been examined before this
report.

## Self-correction to `2ce5c350`

`2ce5c350` read the frame-table entry's `+4` word as a loop count and,
seeing the new entry's `+4 = 1`, concluded the new frame dispatches
"exactly one iteration." Re-mining the cursor field's own write log
across the press shows this is not the whole picture: **the cursor
setup value computed by `sub_82323BB8` itself (`lr=0x82323DDC`, line
16659) is `0x2DD6A840`** at tick 3001 onward — not `0x2DD6A854` (the
frame-table entry's own address, `[owner+40]`'s new value). These are two
distinct, nearby addresses (`0x14`/20 bytes apart) in the same
background-loaded region — the cursor computation (`r26`'s fields,
`2ce5c350`'s own open item) does not simply return the table entry's
address. **Whether "one iteration" is still correct is not settled by
this report** — see "Not established."

## What the cursor's own write trail shows, cleanly, regardless

Sampled every tick 2995-3010: **before the press**, the cursor is written
by up to seven distinct call sites per tick, including `sub_82322A80`
**twice** (its own read-then-advance-by-8 pattern, `704b27b6`), alongside
`sub_823239F0`, `sub_823266F8`, `sub_82326608`, `sub_82325CB8`, and the
loop-setup write itself. **From tick 3001 onward, sampled continuously
through 3010, only two writers ever appear again: `sub_823239F0` and
`sub_82326608`** (alternating the cursor between `0x2DD6A840` and
`0x2DD6A854` every single tick) — plus the setup write. `sub_82322A80`,
`sub_823266F8`, and `sub_82325CB8` are all absent, cleanly and completely,
from every sampled tick after the press.

**This is the strongest and simplest confirmation of `1a9b432c`'s
original finding available: `sub_82322A80` does not merely stop appearing
in isolation — it drops out of a *shared* dispatch stream together with
two other, previously-co-occurring handlers (`sub_823266F8`,
`sub_82325CB8`), while two different, already-present handlers
(`sub_823239F0`, `sub_82326608`) continue running every tick, unchanged.**
Whatever the new frame's element set contains, it plausibly retains only
a subset of the old frame's dispatch types — not a blank/empty state, but
a narrower one that happens to exclude the enqueue-capable type. This
reading does not depend on resolving the exact loop-iteration count.

## Reading

The overall conclusion from `7513c9dc`/`1a9b432c` — a real, live,
address-verified stop in `sub_82322A80`'s own firing, caused by the
press-triggered frame-table swap — remains solid and is reinforced, not
weakened, by this closer look. What `2ce5c350` overstated was the
precision of "exactly one iteration"; the honest state is "fewer dispatch
targets survive the swap, `sub_82322A80` is not one of them," which this
report establishes independently of the loop-count question.

## Not established

- Whether the loop truly runs once or twice per tick after the swap (the
  two-writer pattern is consistent with either "two loop iterations" or
  "one iteration whose callee makes an internal, further cursor-advancing
  call" — not distinguished here).
- What `r26` is (still open, `2ce5c350`), and therefore what the
  `0x2DD6A840` cursor-setup value's own provenance is, precisely.
- What `sub_823239F0` and `sub_82326608` actually do — both are named
  only as writers in this trace, neither has been read.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change, no new probe run — re-analysis of
already-collected data.
