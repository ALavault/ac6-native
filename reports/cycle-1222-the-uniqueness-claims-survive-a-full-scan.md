# Cycle 1222 — the uniqueness claims survive a full scan

Cycle 1221 built an instrument that forces disassembly before scanning, and ended
by saying the session's "N call sites" figures had not been re-run and stayed
lower bounds. This re-runs the four that matter.

## The four claims

Everything cycle 1213 proved about the boot chain, and the forced links in the
NDXR chain, rest on four uniqueness claims. Each was originally taken with the
instrument that sees 91.5% of the code.

Re-measured over `0x82090000`–`0x823D772C` with disassembly forced first:

```
scanned=852724  already_listed=846087  forced=6637  undisassemblable=6871  hits=4
```

```
bl 0x821d5ef8  at 821d7d9c  in Function_821D7D90
bl 0x821d7d90  at 821f6024  in Function_821F5E90
bl 0x82343010  at 82337c8c  in Function_82337C68
bl 0x8234cb58  at 82343078  in Function_82343010
```

**Exactly one site each, four for four.** The chain

```
0x821F5E90 (XEX entry) -> 0x821D7D90 (main) -> 0x821D5EF8 (boot resource mount)
```

and the forced links `0x82337C68 -> 0x82343010 -> 0x8234CB58` are now established
over **852,724 instructions**, with 6,637 of them disassembled by this run because
Ghidra had never reached them. Six thousand six hundred instructions that every
previous scan in this repository was blind to, and none of them contained a
second caller.

The claims were right. **What changes is what they are worth**: "one call site in
the analysed portion" and "one call site in `.text`" are different statements, and
until now only the first had been made.

## An unexplained number

`already_listed` here is **846,087**, while `Ac6XenonRefs` has reported
`scanned_instructions 786122` on every run this session. The second figure should
not be smaller than the first, since one is a subset of `.text` and the other is
the whole program.

I do not know why, and I am not guessing. Possibilities I can name but have not
tested: disassembly created by earlier scripts persisting in the project despite
`-readOnly`; the two iterators counting different things; or `786122` being stale.
**Until that is resolved, the 91.5% figure in `INSTRUMENT_DISCIPLINE.md` should be
read as "the two instruments disagree by a large margin", which is the part that
matters and is true either way.**

That is a worse answer than I would like, and stating it is better than picking
the explanation that flatters the last three cycles.

## What is still not re-run

- The **110** slot-`+0x2C` sites and the **53** `0x82335F18` sites from cycles
  1213 and 1218. Both were counted with the old instrument or with a private
  decoder, and both carry conclusions — the receiver-offset uniqueness, and "the
  six `mode = 0` sites are the mission's". They remain as their reports state
  them: bounded by the instrument used.
- Every "no writer found" negative, including the gate byte at
  `[0x8293BA10] + 0x15A946`. A forced scan would strengthen those, and I did not
  spend it.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
852,724 instructions scanned, 6,637 forced, 4 patterns, 4 hits
```

No product code changed.
