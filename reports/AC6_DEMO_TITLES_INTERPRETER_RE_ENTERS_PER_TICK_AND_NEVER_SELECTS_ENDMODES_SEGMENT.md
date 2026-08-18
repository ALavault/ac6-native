# Title's interpreter re-enters the buffer once per tick from a small table of entry points, and never selects EndMode's

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run beyond one: a fifth,
env-only bracket (`AC6_DEMO_WATCH_ADDR_LO/HI=0x2E3DFA1C/0x2E3DFA20`,
title interpreter's own program-counter field, plus the existing
`AC6_SWG_BOX_CALL`/`AC6_SWG_LOOKUP_KEY` traces), same binary, store, and
input as every run in this thread — address-deterministic across all
five. Everything else here is re-analysis of logs already captured this
thread (`346255b2`'s bracket run, the `AC6_SWG_BOX_CALL` run behind
`1fcc88b3`). No source change.

## Two corrections to `1fcc88b3`'s evidence section

The "caught false lead" section misdescribed its own numbers. The raw
word-adjacency `(0x16, 0x17)` hit was at `0x2DCB2EF8`
(`768290552 = 0x2DCB2EF8`), not `0x2DCB2554` — `0x2DCB2554` is SendMsgI's
*real*, template-validated marker; the report's parenthetical conflated
the two. And the two tail-region floats (`100.0f`/`40.0f`) sit at the
`+0x14` offset from their own `0x16` markers (`0x2DCB2ED4`/`0x2DCB2EF8`),
matching the same statement stride the validated template uses — not "the
naive `+4` offset" as written. Neither error changes `1fcc88b3`'s
conclusion (EndMode's own `pc_after` address, checked independently, is
still absent from all 4299 events); both are corrected here by name.

## A scope limit on the nine-row template table

The routine per-tick triple (below) ends in `box(0x2F)` — the campaign's
long-tracked per-tick heartbeat lookup, confirmed live: all 11 occasions
in the `2990`-`3000` observation window pair with an
`AC6_SWG_LOOKUP_KEY category=0x2F` line, 11-for-11. But its own marker
slot holds `0x3F`, not `0x16`. **The `[0x16, 0x19, 0x2E, 0x08, 0x00,
category]` template `1fcc88b3` validated is one statement *form*, not
every lookup** — the nine-row table it produced is complete for that
form only, and EndMode's row is form-matched and address-checked
independently of this limitation.

## The interpreter re-enters per tick, from a small, fixed set of addresses

Bracketing the title interpreter's own PC field (`[r31+20]`,
`r31=0x2E3DFA08`) names every write to it, not just the fetch-advance:
`sub_823251E0`'s own write at its `generated_line=1133` (call site
`lr=0x82323F2C`, a stable, single caller) is, in every tick sampled
(`2452, 2511, 2571, 2572, 3000, 3001, 3033, 3034, 3035`), **the first PC
write of that tick** — the entry point the interpreter resumes from.
Across the whole run it takes only **38 distinct values**, and the ticks
map onto them very unevenly:

| entry address | ticks using it |
|---|---|
| `0x2DCB20B4` | 417 (the routine idle-check segment) |
| `0x2DCB2130` | 158 |
| `0x2DCB2098` | 5 |
| `0x2DCB2430` | **1 — tick 3001 only** |
| `0x2DCB268C` | **1 — tick 3033 only** |
| (33 more, each used exactly once) | |

The dominant entry, `0x2DCB20B4`, runs the routine triple
(`box(0x29), box(0x3F), box(0x2F)`, addresses `0x20CC/0x20DC/0x20F0`)
every tick from **2572 through 3000 with zero gaps** (429 consecutive
ticks), then stops. **Tick 3001 — the tick `--input-at 3000,16,...`'s
press takes effect on — gets its own distinct entry, `0x2DCB2430`**,
landing right at the start of the six-lookup response batch
(`1fcc88b3`), not a continuation of the idle segment. Reading as a
finding, not proven: the handoff from the routine entry to the
press-response entry is input-driven, one tick after the press. Tick
3033 (the post-press burst this campaign's `AC6_SWG_RECORD_KEY_CALL`
trace already flagged, this session's earlier work) gets a third,
again-distinct entry, `0x2DCB268C`, right after the batch's own end
(`0x2680`) — the script moving forward into a new segment, not repeating
the same one.

## The entry table brackets EndMode's statement without ever landing on it

EndMode's statement occupies `0x2DCB2024`-`0x2DCB2038`. The entry-setter's
38 values include several below it (`...0x2DCB1F04, 0x2DCB1F14,
0x2DCB1F24, 0x2DCB1F2C`) and several just above it (`0x2DCB2048,
0x2DCB2050, ...`) — **but none inside `[0x2DCB2024, 0x2DCB2044)`**. The
gap is exact: the highest sub-EndMode entry is `0x2DCB1F2C`, the lowest
super-EndMode entry is `0x2DCB2048` — `0x2DCB2044` short of touching
EndMode's own five-word statement on either side. Combined with
`1fcc88b3`'s address-level check (EndMode's own marker-fetch address,
`0x2DCB2028`, has zero hits in the full `AC6_SWG_BOX_CALL` capture, and
this report's own re-check of `0x2DCB2038` — the statement's last word —
independently confirms zero), this is now two independent negatives
(never fetched; never an entry) rather than one.

## Not established

- **What selects which of the 38 entry values gets written each tick.**
  `sub_823251E0`'s own body, and its one caller (`lr=0x82323F2C`), are
  unread. This is the concrete next static step — the shape to expect,
  per this thread's own prior finding for symbol registration
  (`25d092bc`) and the loader (`346255b2`), is a small dispatch/lookup
  keyed by some event or state value, not a a branch tree; whether it is
  is what the read settles.
- Whether the 38 values are a static table in the image or themselves
  computed at runtime — not distinguished.
- The other PC-field writers surveyed but not read: `sub_82325160`
  (5801 writes, far too frequent to be an entry-setter — noted, not
  chased), `sub_82325050`, `sub_82324CE0`, `sub_823248C8`,
  `sub_82324290`, `sub_823242D8`, `sub_82324930` — all touch this field
  somewhere in the interpreter's broader call graph, unread.
- The loader's own compact source (`346255b2`), the `M102` string's
  assembly (`sub_82327D90`, `1fcc88b3`), and the second interpreter
  instance's own buffer (`r31=0x2E3BFA08`) — all still open, carried
  forward.

## Gates

No source changed; report-only commit. The bracket used
(`AC6_DEMO_WATCH_ADDR_LO/HI`) and both trace instruments
(`AC6_DEMO_WATCH_SWG_BOX_CALL`/`AC6_DEMO_WATCH_SWG_LOOKUP_KEY`) already
existed and are unmodified.
