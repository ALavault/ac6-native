# Cycle 1461 — twenty-seven and six

## Qualification

- **No Ghidra run and no oracle pass.** `scripts/MicroExecuteFunction.java`, read
  as text.
- The harness is **unchanged**, so no calibration re-run is owed. ctest stays
  **56**. **No contract entry.**
- New: `tools/audit_microexec_reset_completeness.py`.

## The answer

**27 per-instance fields, 6 static constants, 0 unreset.** After cycle 1460's
one-line fix, `resetCase()` is complete.

That is a parse rather than a reading, which is the whole point: cycle 1460 found
its omission by accident, and "I read the list and found no second omission" was
the weakest sentence in that report.

## Three attempts, because a pattern is not a parse

The first scan matched `^    private ...`. It found 33 declarations, 6 unreset,
all of them `static final` — and it was **only correct by luck**: it would miss
any field declared without `private`.

The second widened the modifier set and matched any four-space-indented
declaration. It reported **177 unreset fields** — `index`, `bytes`, `line`,
`region`, `break`, `continue`. A local variable's declaration looks exactly like
a field's, and `[\w<>\[\],. ]` includes a space, so the type group swallowed the
indentation of deeper lines.

The third capture bug was subtler: `((?:static|final)\s+)+` keeps only the **last
repetition**, so `private static final String` yields `final ` and the `static`
test fails. Five constants were reported as per-instance state.

What works is a discriminator locals cannot satisfy: **the declaration begins
with an access modifier or `static`**. Indentation is a style; a modifier is a
token.

```
    private final List<String> dumpRegions = new ArrayList<>();   field
            String line = rawLine;                                local
```

Three wrong scans in one cycle, on a file of 89 lines of field declarations. The
census in cycles 1458–1460 was wrong twice for the same reason, and this is the
third instrument in four cycles to be fixed by parsing instead of matching.

## Measured before use

Deleting the very line cycle 1460 added:

```
  NOT RESET  dumpRegions              List<String>
microexec_reset_completeness=fail fields=27 constants=6 missing=1   exit 1
```

restored, and back to `pass`. It catches the exact defect it was written for,
which is the least a checker can be asked to prove.

## Not established

- Whether every field that *should* be per-case **is** an instance field. A
  per-case value hidden in a `static` would be reset by nothing and reported by
  nothing; the six statics are constants by inspection, and that inspection is a
  reading.
- Whether `.clear()` and `=` are the only reset forms. They are the only ones
  this file uses today; a field reset by a helper call would read as unreset and
  fail loudly, which is the right direction to be wrong in.

## Gates

```
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 56
microexec_reset_completeness            pass fields=27 constants=6 missing=0
microexec_calibration_coverage          pass calibrated=6 exercised=9 nowhere=0
claude_md_numbers                       pass checked=3 mismatched=0
tools/tests                             Ran 79 tests, OK
```

`CLAUDE.md` now names it beside the calibration, and says to run it **first**:
it is instant, needs no Ghidra, and catches the class of defect that cost cycle
1460 a headless run to find.

## Next

**Back to the map.** Four cycles have gone into the instrument since the terrain
landed, and the open question there is unchanged: `CMapManager+0x30`, the array
of per-coarse-cell offsets reaching 256 eight-byte records, which `0x82102148`
consults before it draws anything and which the loader at `0x820FBC28` never
writes.
