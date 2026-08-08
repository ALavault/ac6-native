# Cycle 1221 — an instrument that states its own denominator

## What was built

`tools/ghidra_scripts/Ac6XenonForceScan.java`. It walks an address range in
4-byte steps, **disassembles anything the listing does not already have**, and
only then scans — closing the blind spot cycle 1220 explained.

Its output line is the point:

```
scanned=1015  already_listed=979  forced=35  undisassemblable=10  hits=1
```

Every scan in this repository has been reporting a hit count. **None has been
reporting how much of the program it looked at.** `INSTRUMENT_DISCIPLINE.md` has
demanded that since cycle 1212 and no tool supplied it, which is why the demand
was not being met — the discipline was asking the reader to do arithmetic the
instrument could do itself.

## The demonstration, on the case that cost a finding

Over `0x821B5000`–`0x821B6000`, a 4 KB window:

| | |
|---|---|
| instructions Ghidra had already disassembled | **979** |
| instructions it had **not** | **35** |
| bytes that are not code | 10 |
| hits for `0x1ad8` | **1**, at `821b54c8` |

And the same pattern through the old instrument, over the **whole image**:

```
0 hits
```

**Thirty-five invisible instructions in four kilobytes, and the one I needed was
among them.** Cycle 1220 found the mechanism by accident, from a zero that
happened to be checkable because I had disassembled the address separately. Most
zeros are not checkable that way.

## Why this is worth a cycle rather than a footnote

Fourteen entries in `INSTRUMENT_DISCIPLINE.md` are about believing a measurement
that was true of less than it seemed — of one corpus, of one branch, of one
population, of 91.5% of the code. **The fix that generalises is not more rules
for the reader; it is instruments that carry their own scope.**

This one does, and it makes the earlier rule enforceable rather than aspirational:
a negative from a scan that prints `already_listed` and `forced` can be judged on
the spot, and a negative from one that prints only `hits=0` cannot.

## What this does not do

- It does not re-open the session's negatives. The load-bearing ones —
  cycle 1192's FHM, cycle 1207's MATE, cycle 1205's `0x2005` census — rest on
  **byte scans of memory blocks or on data**, which never went through the
  listing. Cycle 1212 said so and it is still true.
- It does not make instruction-text scans complete. `forced` counts what it could
  disassemble; `undisassemblable` counts what it could not, and code hidden
  behind data-typed bytes stays hidden.
- It is slower than the old scan by roughly the cost of the forced disassembly,
  which on this 4 KB window was unnoticeable and over the whole `.text` will not
  be. The range arguments exist for that reason.

## Not established, stated plainly

- Whether a whole-`.text` force scan changes any figure this session published.
  The obvious candidate is the "N call sites" counts, which cycle 1212 already
  demoted to lower bounds. **I did not re-run them**, and until someone does they
  stay lower bounds.
- What selects creator index 45, which is what sent me here. Unchanged from
  cycle 1220.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
new instrument on 0x821B5000-0x821B6000: 1 hit; old instrument on the whole image: 0
```
