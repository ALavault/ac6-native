# AC6 retail NTSC-U/J — the failing allocation's requested size is garbage (`0xFEFFFFF9`), not a literal 16 bytes; corrects the size characterization implicit in r135/r136 (r137)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Two
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R137_DIAG`) were added to gitignored, regenerated build-tree files
(`generated/ppc_recomp.31.cpp`, `generated/ppc_recomp.22.cpp`), used to
take live measurements, and fully reverted via `cp` from `/tmp/*.orig`
backups (the two available `ppc_recomp.22.cpp` backups from r134 and
r135 were diffed and confirmed byte-identical before use). Confirmed
reverted via `grep -c "r137"` returning 0 in both files, a clean
`tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9), and
139/139 Python tests. `git status` on `recompilation/ace-combat-6-retail`
shows only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change.

## Correcting this investigation's own working assumption

r135 characterized the failing call as "a 16-byte allocation" -- taken at
face value from `sub_821CC288`'s literal `li r5,16` immediate at its
`sub_82222D80` call site. This cycle traced `sub_82222D80`'s own
parameter usage directly and found `r5` feeds a size-class/alignment
helper (`sub_82221C68`), while the actual requested **size** is `r4`,
which `sub_821CC288` sets from `r30` -- **the return value of a helper
call**, not the literal 16. This distinction was never verified in
r135/r136; both cycles' "16-byte allocation" framing is an unverified
assumption that turns out to be wrong, corrected here.

## What `sub_82222D80` actually does with its inputs, confirmed live

A live diagnostic across all four `sub_82222D80` calls in one run shows
its structure clearly: a size-class lookup (`sub_82221C68`) returns a
class index; a small class dispatches to `sub_822222A8`, an invalid class
(`0xFFFFFFFF`, i.e. `-1`) *also* dispatches to `sub_822222A8` (the branch
test is on a derived flag, not the class value directly), and other
classes dispatch to `sub_82222908`:

```
[r137] sub_82222D80: sub_82221C68 returned class=0x00000005
[r137] sub_82222D80: branch flag(r11)=1 (0=SMALL path, nonzero=LARGE path)
[r137] sub_82222D80: LARGE path sub_82222908 returned=0x16f90200
[r137] sub_82222D80: sub_82221C68 returned class=0xffffffff
[r137] sub_82222D80: branch flag(r11)=0 (0=SMALL path, nonzero=LARGE path)
[r137] sub_82222D80: SMALL path sub_822222A8 returned=0x16fa0000
[r137] sub_82222D80: sub_82221C68 returned class=0xffffffff
[r137] sub_82222D80: branch flag(r11)=0 (0=SMALL path, nonzero=LARGE path)
[r137] sub_82222D80: SMALL path sub_822222A8 returned=0x00000000   <-- the failing call
[r137] sub_82222D80: sub_82221C68 returned class=0x00000000
[r137] sub_82222D80: branch flag(r11)=1 (0=SMALL path, nonzero=LARGE path)
[r137] sub_82222D80: LARGE path sub_82222908 returned=0x173a0020
```

Four allocations happen in the whole run, not one. **The failing call is
the third**, and it is not unique in taking the `class=0xFFFFFFFF` path --
the second call takes the identical path and succeeds
(`0x16FA0000`). This rules out "any allocation reaching this size class
always fails" -- something specific to the *third* call's actual size
differs from the second's.

The **fourth** allocation, previously assumed (r132/r133) to be *the*
`DATA.TBL`-processing allocation because its result (`0x173A0020`)
matches `sub_82234B88`'s source pointer, is confirmed here to succeed
cleanly -- consistent with everything r132/r133 already established about
that buffer's downstream misuse (unfilled by a zero-length read, not
unallocated).

## The failing call's actual requested size: garbage, not 16

A second diagnostic, at `sub_821CC288`'s own call site immediately before
`sub_82222D80`, printed the real size argument:

```
[r137] sub_821CC288: requested alloc size(r30)=4278190073 align(r5)=16
```

`4278190073 = 0xFEFFFFF9` -- as signed 32-bit, `-16777223`. **This is not
16.** The literal `16` (`li r5,16`) that r135/r136 both read as "the
allocation size" is a separate argument (structurally an alignment or a
size-class hint to `sub_82221C68`, given r5 is what that helper consumes,
not what the allocator itself copies as byte count). The real size comes
from `r30`, itself the return value of a call chain
(`sub_82283728` -> `sub_822834C0`) this cycle read partially: `sub_822834C0`
does not operate on the buffer `sub_82283728` filled at all -- its own
body calls `sub_82338388`/`sub_82338568`/`sub_82338410` with fixed
immediate arguments (`1, 3, 4, 0`), a shape more consistent with a
config/state query than a string-length computation. **This cycle's
earlier working guess that it computes a string length is itself
unverified and should not be trusted** -- named here explicitly so it is
not silently carried into a future cycle's assumptions the way "16
bytes" was.

## Decision

No native code changed this cycle -- two rounds of temporary, env-gated
diagnostics, reverted and verified reverted (`ctest` 9/9, 139/139
Python, clean `git status`). This cycle's contribution is a correction:
the allocator itself behaves consistently and correctly (rejecting a
genuinely absurd size, the third call, the same way any real allocator
would; the second call with the *same size class* succeeds because its
actual byte count presumably differs). The interesting question moved
one layer further upstream, to whatever computes `0xFEFFFFF9` as a
"size" -- a value this cycle cannot yet explain and refuses to guess at.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Read `sub_82283728` (the buffer-filling call preceding the size
   computation) and `sub_82338388`/`sub_82338568`/`sub_82338410` (what
   `sub_822834C0` actually calls) in full -- do not assume a "string
   length" semantics without verifying it from the disassembly/generated
   C++, per this cycle's own self-correction.
2. Determine whether `0xFEFFFFF9` is itself derived from real,
   previously-established poison bytes (r133's `fe fe fe fe` DATA.TBL
   header read shares the same leading byte, `0xFE` -- worth checking
   for a real connection, not assuming one from a single shared byte) or
   is an independent computation this project has not yet traced.
3. Compare the second (successful) and third (failing) calls' actual
   requested sizes directly -- both take the same `class=0xFFFFFFFF`
   path, so whatever differs between them is the size argument itself,
   not the allocator's logic.
4. Continue preferring the live-diagnostic-plus-revert technique over
   GDB-based approaches (unreliable on this 18-thread probe, r128), and
   continue checking multi-call sites for disambiguation (this cycle's
   own four-call trace, following r135's single-call-site mistake, is
   exactly the kind of check that catches an unverified "the only call
   that matters" assumption).
