# AC6 retail NTSC-U/J — the zero-length `NtReadFile` is traced to a boolean-gated store inside `sub_821CC508` that never populates a descriptor field 16 bytes past r129's own filename/handle table (r134)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Several
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R134_DIAG`) were added to gitignored, regenerated build-tree files
(`native-import-stubs.cpp`, `generated/ppc_recomp.22.cpp`,
`generated/ppc_recomp.27.cpp`), used to take live measurements, and fully
reverted -- `ppc_recomp.22.cpp` via `cp` from a `/tmp/*.orig` backup,
`ppc_recomp.27.cpp` by a matched inverse text replacement (confirmed
identical to the pre-cycle version by diffing against the r132/r133
already-verified-clean state), `native-import-stubs.cpp` by a full
`tools/build.py` regeneration. Confirmed via `grep -c "r134"` returning 0
in all three files, a clean `tools/build.py --target ntsc-uj --profile
native` (`ctest` 9/9), and 139/139 Python tests. `git status` on
`recompilation/ace-combat-6-retail` shows only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change.

## Starting point: r133's named next step

r133 closed the causal chain from a zero-length `NtReadFile` call through
to the `sub_821F7C80` crash, and named "find what determines the requested
length" as the next step, tentatively guessing an unimplemented
size-query import (`NtQueryInformationFile`-shaped). This cycle traced it
concretely instead of implementing that guess -- and the guess turns out
to be wrong in its specifics, corrected below.

## Tracing the call chain with `addr2line`

A `backtrace()`/`backtrace_symbols_fd()` capture at `NtReadFile`, resolved
against the built `ac6recomp` binary with `addr2line -f -C`, gives the
exact chain for the one `NtReadFile` call in the whole run:

```
_xstart -> sub_821D7DE0 -> sub_821D5F48 -> sub_821CC508 -> sub_821F4E70 -> NtReadFile
```

`sub_821D5F48`/`sub_821CC508` are the retry-loop pair this investigation
has traced since r117; `sub_821F4E70` is the read-issuing helper r124
already named. Reading `sub_821F4E70`'s generated C++ against the
already-confirmed real `NtReadFile` ABI (r122/r126: r3=Handle, r4=Event,
r5=ApcRoutine, r6=ApcContext, r7=IoStatusBlock, r8=Buffer, r9=Length,
r10=ByteOffset) shows its own third argument (`ctx.r9`, unmodified from
entry to the indirect call) becomes `NtReadFile`'s `Length`. That argument
is supplied by the caller, `sub_821CC508`.

## `sub_821CC508`'s chunked-read-size calculation

`sub_821CC508` has two structurally identical call sites into
`sub_821F4E70` (one per direction it can be entered), each preceded by:

```
r9 = [record + 8]                     // intended: file size
r9 = round_up(r9 + 2047, 2048)        // sector-align upward
r11 = r9 - [state + 22884/22892]      // minus bytes already read
r27 = min(r11, [state + 340])         // cap at a fixed chunk size
```

`r27` is then passed as `sub_821F4E70`'s length argument. A live diagnostic
on the actually-taken call site (confirmed via a matching entry print on
`sub_821F4E70` itself, correlated in the same run) measured every input to
this formula directly:

```
[r134] chunk-size calc #2: r30(record)=0x00000008 filesize_field[r30+8]=0 already_read[r31+22884]=0 chunk_cap[r31+340]=262144
[r134] chunk-size result #2: r27=0
[r134] sub_821F4E70 entry: arg1(r3)=0x00000002 arg2/buffer(r4)=0x173a0020 arg3/length(r5)=0 arg5/reqobj(r7)=0x829e36cc
[r134] NtReadFile handle=0x00000002 length=0
```

**`r30` (the "record" pointer the size field is read from) is
`0x00000008` -- not a real guest address, a near-null value.** This is a
sharper, more precise finding than r133's guess: the length is not zero
because a missing size-query import returns zero; it is zero because the
pointer used to look up the size resolves to `0x00000008`, and reading
`[0x00000008 + 8] = [0x00000010]` off effectively-unmapped low memory
yields zero.

## Why `r30` is near-null: `r26 - 18100` is a field this project can now name precisely

`r30` is computed as `[rotated 16-bit index] + [r26 - 18100]`, where
`r26 = 0x82940000` (a fixed global base, `lis r26,-32108`, confirmed
identical to the base r129 already used for the filename/handle table).
`r26 - 18100 = 0x8293B94C` -- **exactly 16 bytes past `0x8293B93C`**, the
address r129 identified as the start of the small filename/handle table
(`sub_821CC370`'s target). With `[0x8293B94C]` reading as zero, `r30`
collapses to just the small rotated-index term (`8`), landing near null.

A targeted grep for stores to this exact offset (`-18100(r26)`), the same
technique r129 used to find `sub_821CC370`, finds **exactly one** writer,
and it is inside `sub_821CC508` itself:

```cpp
lbz r11, -18120(r11)         // flag byte at 0x8293B938 (4 bytes before r129's table)
cmplwi cr6, r11, 0
bne cr6, 0x821cc338          // branch taken only when the flag is NONZERO
  ... [fallthrough, flag==0]: writes -18096, -18080, -18088, -18084 -- NEVER -18100
loc_821CC338:                 // [flag != 0]
  stw r11, -18096(r10)        // = [r31+0]
  stw r11, -18100(r10)        // = r31+8   <-- the field we need
  stw r11, -18092(r10)        // = [r31+4]
```

**`[0x8293B94C]` is written only when the flag byte at `0x8293B938`
(4 bytes before r129's own table start) is nonzero.** When it is zero --
apparently its state on this run, given the field is observed unwritten
-- a *different* branch runs, populating four *other* fields
(`0x8293B950`, `0x8293B954`, `0x8293B958`, `0x8293B95C`) but never this
one. This is not a missing import; it is a live boolean condition inside
already-implemented, already-executed code choosing the wrong branch for
this run.

## Decision

No native code changed this cycle -- three rounds of temporary,
env-gated diagnostics, reverted and verified reverted (`ctest` 9/9,
139/139 Python, clean `git status`). This cycle **corrects** r133's own
guess (an unimplemented size-query import) with a measured, more precise
finding (a boolean-gated store inside code this project already executes,
never taking the branch that would populate the needed field), following
this project's own discipline of correcting predecessors -- including this
investigation's own immediately-preceding cycle -- by name. The flag
byte's meaning (`0x8293B938`) and why it reads zero on this run are named
as next step rather than guessed at.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Find what the flag byte at `0x8293B938` represents and what is
   supposed to set it nonzero before `sub_821CC508` runs for the
   `DATA.TBL` read -- grep for stores to `-18120(r26)` (the same base,
   same technique that found this cycle's writer) across the generated
   codebase.
2. Determine whether the flag's current (zero) state on this run is
   itself correct given what has and has not happened yet (e.g., it may
   legitimately gate a *second* read style this project hasn't reached
   the setup for), or whether something this project's own HLE surface is
   supposed to trigger before this point is missing or misordered.
3. Only once that mechanism is understood, decide whether the fix belongs
   in this project's native runtime (an HLE gap) or is simply this
   project observing real, correct game logic reaching a state it hasn't
   previously reached (in which case the fields at `0x8293B950-0x8293B95C`
   -- the ones the flag=0 branch *does* populate -- are the ones actually
   worth reading next, not `0x8293B94C`).
4. This cycle's tracing techniques (a `backtrace()`-based caller ID at any
   single import call, and a targeted grep for stores to one fixed
   `[base+offset]` address) are cheap, reusable, and already proven twice
   in this exact investigation (r129, this cycle) -- prefer them over
   further GDB-based approaches (r128's watchpoints remain unreliable on
   this 18-thread probe).
