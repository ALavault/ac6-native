# Correcting `7c6597b7`: `sub_823266F8` did not drop out; three writers
# continue, three drop, one changes call site

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Re-verification of the already-collected probe
log (`1a9b432c`), no new probe run — precise, exhaustive re-count of the
cursor field's writer functions across ticks 2995-3010, the exact range
`7c6597b7` already quoted but miscounted.

## The error

`7c6597b7` claimed `sub_82322A80`, `sub_823266F8`, and `sub_82325CB8` all
"drop out cleanly and completely" after the press. **`sub_823266F8` does
not drop out** — it writes the cursor at `0x2E4036C8` on every one of the
ten sampled post-press ticks (3001-3010), the same as before. This was
visible in `7c6597b7`'s own quoted trace excerpt and was miscounted
anyway — an error worth stating plainly rather than letting stand, per
this repo's own discipline (`AC6_DEMO_CORRECTING_...` is this file's
entire purpose).

## The correct counts, exhaustively re-verified

Ticks 2995-3000 (pre-press, excluding the loop-setup writer
`sub_82323BB8` itself): **six** distinct callee writers —
`sub_82322A80`, `sub_82323968`, `sub_823239F0`, `sub_82325CB8`,
`sub_82326608`, `sub_823266F8`.

Ticks 3001-3010 (post-press, sampled continuously, ten writes each):
**three** distinct callee writers — `sub_823239F0`, `sub_82326608`,
`sub_823266F8`.

**Dropped**: `sub_82322A80`, `sub_82323968`, `sub_82325CB8` — three, not
`sub_823266F8`.
**Continuing, unchanged in count**: `sub_823239F0`, `sub_82326608`,
`sub_823266F8` — three, not two.

**One additional precise fact, not previously stated**: `sub_823239F0`'s
own call site changes across the press — `lr=0x82323A4C` before,
`lr=0x82323E4C` after. The post-press address matches
`sub_82323BB8`'s own loop `bctrl` return site
(`ppc_recomp.43.cpp:16679`, `ctx.lr = 0x82323E4C`) exactly — this
function is dispatched *through the loop* after the press, where before
it may have been reached differently (not established which call site
`0x82323A4C` itself is; not traced in this report).

## Reading

The core finding is unaffected: `sub_82322A80` (the enqueue-cursor-walker)
is genuinely and cleanly absent after the press, confirmed again by this
recount. But the surrounding picture is different from what `7c6597b7`
claimed — **three** handlers continue unchanged, not two, and one of
them (`sub_823239F0`) is now reached via the loop specifically, which it
apparently was not (or was not exclusively) before. Reading these three
continuing handlers' actual bodies — none of the three has been opened —
is the direct next step, not further inference from write-log patterns,
per `advisor`'s own diagnosis: three consecutive corrections on this
sub-thread (`2ce5c350`, `7c6597b7`, this report) all stemmed from
inferring semantics from write patterns rather than reading code; the
fix is reading the code.

## Also folding in a standing, still-unfixed errata

`7c833f03`'s Qualification section garbles its own run inventory ("two
symbol-table dump runs, 4400 and 8000 ticks" — both dump runs were
actually 4400 ticks each; the 8000-tick run was the separate lookup-key
run, omitted from that list despite being cited extensively in the body).
Noted here, six reports later, per the same discipline — non-blocking,
does not affect any conclusion, but should not go unmentioned indefinitely.

## Not established

- What `sub_823239F0`, `sub_82326608`, and `sub_823266F8` actually do —
  the direct next static read, not attempted in this report.
- Call site `0x82323A4C` (sub_823239F0's pre-press caller) — not
  identified.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change, no new probe run.
