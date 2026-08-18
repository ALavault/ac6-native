# EndMode is present in title's compiled bytecode and never fetched in 3200 ticks — "present but unreached" is now a program-level fact

## Qualification

AC6 demo PAL, same XEX SHA-256. No new probe run: this report is entirely
a static reconstruction and re-analysis of two logs already on disk from
this thread's own prior runs — `346255b2`'s bytecode-buffer bracket
(`AC6_DEMO_WATCH_ADDR_LO/HI=0x2DCB2000/0x2DCB3000`) and the preceding
`AC6_DEMO_WATCH_SWG_BOX_CALL` run that captured title's live tick-3001
sequence with `pc_after`. No source change; no rebuild; no rerun.

## The reconstruction

The bracket run's 4096 guest-level, size-1 `AC6_ADDR_RANGE_WRITE` events
(one `sub_82278F78` byte-fill, tick 2435, thread 9 — `346255b2`) are
exactly the 4096 bytes of `[0x2DCB2000, 0x2DCB3000)`. Grouping them by
address and assembling big-endian `u32` words reconstructs the buffer in
full. **Validated against the live capture, not assumed correct**: for
all 10 of the first `AC6_SWG_BOX_CALL` events at tick 3001, the word at
`pc_after - 4` in the reconstruction equals the `r4` the interpreter
actually boxed, exact match, 10-for-10.

## A caught false lead: raw word-adjacency is the wrong search

The first attempt at finding EndMode (title's local symbol index `3`,
`7a565550`) scanned the reconstructed buffer for adjacent words `(0x16,
3)` — reasoning that `0x16` appears to box() immediately before every
category in the live trace. It found exactly one hit, at `0x2DCB2ED4`,
and one `(0x16, 0x17)` hit elsewhere at `0x2DCB2554`... which is a
**different address than SendMsgI's own live-confirmed fetch site**
(`0x2DCB2554` is actually correct for the marker, but paired against the
word at `+4`, not the real category slot — see below). Checking gaps
between consecutive *live* `pc_after` values shows `0x16` and its
category are **never raw-adjacent** — 4 unboxed words sit between them
every time. The one `(0x16, 3)` raw-adjacency hit was therefore not
trusted, and rightly not: it does not survive the corrected search below
(the tail-of-window hits near `0x2DCB2ED4`/`0x2DCB2EF8` decode to IEEE-754
floats, `100.0f`/`40.0f`, at the naive `+4` offset — plausible operands to
some other instruction, not categories).

## The real template, and where EndMode's own statement is

Computing the exact gap between each `0x16` box event and the *next* box
event in the live tick-3001 sequence gives a **constant 0x14 (5 words)**
across all six observed statements (categories `4, 5, 6, 0xB, 0x13,
0x17`), no exceptions. Reading the full 5-word template at each of the
six live sites, and independently at the three new hits found by scanning
the whole window for `word[A]==0x16 ∧ word[A+0x14]∈{small integer}`,
gives the same exact prefix for **all nine**:

```
[0x16, 0x19, 0x2E, 0x08, 0x00, <category>]
```

| statement address | category | symbol (per `7a565550`/`25d092bc`) | ever fetched (any tick, 3200-tick run)? |
|---|---|---|---|
| `0x2DCB2024` | `3` | **EndMode** | **no** |
| `0x2DCB2400` | `0x19` (25) | SetBuffer | yes (once) |
| `0x2DCB2484` | `4` | GetCurrentLevel | yes |
| `0x2DCB24C0` | `5` | GetCurrentMission | yes |
| `0x2DCB24FC` | `6` | GetCurrentMode | yes |
| `0x2DCB2554` | `0x17` (23) | SendMsgI | yes |
| `0x2DCB25B4` | `0xB` (11) | (unregistered — lookup fails) | yes |
| `0x2DCB2628` | `0x18` (24) | **SendMsgV** | **no** |
| `0x2DCB2668` | `0x13` (19) | OnVoice2D | yes |

"Ever fetched" is checked by **address**, not value: grepping the full
`AC6_SWG_BOX_CALL` capture (4299 events, whole run) for each statement's
own `pc_after` (`fetch_address + 4`) rather than for its category value,
which would be ambiguous (a small integer like `3` can appear as an
operand elsewhere). `EndMode`'s `pc_after=0x2DCB203C`: **zero** hits.
`SendMsgV`'s `pc_after=0x2DCB2640`: **zero** hits. Every other row's
`pc_after` (already known from the live capture) confirms present with
exactly one hit each, and `SetBuffer`'s independently-computed
`pc_after=0x2DCB2418` also confirms exactly one hit — the template-based
address computation checks out against ground truth on both the known
and the newly-found rows.

**This is the campaign's oldest open question, closed at the program
level rather than inferred from the symbol table**:
`3c7e7291`/`f7c4e68f`/`1e90f723`/`33b549ef`/`7a565550` each asked, in
turn, whether EndMode is absent from title's script or present-but-
unreached; `7a565550` settled the *symbol-table* half (bound, at local
index 3); this settles the *program* half — the statement exists,
matches the same template every other confirmed lookup uses, and was
never fetched by the interpreter in any of the 3200 ticks this campaign
has ever traced it for.

## Corroboration already in the same data: the executed path skips a statement

Between the live `box(0x0A)` at `pc_after=0x2DCB25E8` and `box(0x12)` at
`pc_after=0x2DCB265C` (fetch addresses `0x2DCB25E4`→`0x2DCB2658`, a
116-byte/29-word gap), the entire `SendMsgV` statement (`0x2DCB2628`
through `0x2DCB263C`) sits unboxed inside the gap. **The executed path
demonstrably branches over at least one lookup statement within the
observed range** — direct, live precedent for the same thing happening to
EndMode's statement, elsewhere in the buffer. This is not proof of *why*
(a non-boxing fetch path inside `sub_823246C0`, unread, can't be
excluded as an alternative to a branch) — offered as corroboration, not
confirmation.

## Not established

- **What guards the branch that skips EndMode's statement.** The two
  live suspects, both already named by this thread: the failing category
  `0xB` lookup (`node=0`, resolves to nothing) and the unanswered `M102`
  query (`AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS`,
  `SendMsgI`/category `0x17` is in the same batch). Neither is confirmed
  as the actual gate; this is the concrete next move.
- **The buffer's true extent.** The bracket proves content from
  `0x2DCB2000` through `0x2DCB3000` (start ≤ `0x2DCB2000`, end ≥
  `0x2DCB3000`); neither edge is directly observed. `0x2DCB2024`'s
  statement is the earliest template-matching one *in this window*,
  preceded by nine small-integer, header-shaped words (`0x2DCB2000..
  0x2DCB2020`) — not established as the program's actual first
  statement.
- `sub_82278F78`'s own compact source, `sub_82327D90`/the `M102` string's
  assembly, `sub_823246C0`'s other fetch handlers, and the heartbeat
  instance's own buffer — all still open, carried from `346255b2`.

## Gates

No source changed; report-only commit, entirely static re-analysis of
data already captured under instrumentation gated and tested in
`346255b2`.
