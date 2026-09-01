# AC6 retail NTSC-U/J — the `sub_821F7C80` crash is a corrupted notification-list sentinel; the writer is not `NtReadFile` (ruled out by direct trace); the actual corrupting write is still unlocated (r132)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (read-only, `-noanalysis`, no data
changed), XEX US `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
No oracle used. **No committed native source changed this cycle.** Two
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R132_DIAG`) were added directly to gitignored, regenerated build-tree
files (`generated/ppc_recomp.27.cpp`, `native-import-stubs.cpp`), used to
take one live measurement each, and fully reverted -- the first via `cp`
from a `/tmp/*.orig` backup, the second by simply re-running
`tools/build.py` (which regenerates `native-import-stubs.cpp` from
`tools/materialize_native_import_stubs.py` fresh every invocation, wiping
the hand-edit automatically). Confirmed reverted via `grep -c "r132"`
returning 0 in both files, `ctest` 9/9, and 139/139 Python tests
afterward. `git status` on `recompilation/ace-combat-6-retail` shows only
the pre-existing, unrelated `upstream/AC6_recomp` submodule pointer change.

## Static read of `sub_821F7C80`

Disassembled `sub_821F7C80` (r131's crash site), its caller `sub_82390B18`,
and `sub_82390B18`'s caller `sub_821F8008`, via
`tools/ghidra_scripts/Ac6XenonDisasm.java` against `ac6-us` (300-instruction
cap per block, all three blocks hit the cap -- **no completeness claim is
made past what was read**; `tools/check_listing_against_pdata.py` returned
"no .pdata row" for all three addresses, so the standard completeness check
this project relies on could not run here -- noted rather than skipped
silently). Cross-referencing the disassembly against the actual generated
C++ (`__imp__sub_821F7C80` in `ppc_recomp.27.cpp`, which does not truncate)
established `sub_821F7C80`'s real extent precisely: 0x821F7C80-0x821F7CDC,
ending in a tail-branch to the standard `__restgprlr`-style epilogue thunk
(`b 0x82382a68`), not `blr` -- the classic Xbox 360 MSVC save/restore-GPR
calling convention, confirmed by its mirror-image prologue (`bl 0x82382a18`)
and by the same shape recurring in the immediately following, structurally
identical `sub_821F7CE0`.

`sub_821F7C80(r3=arg)` is a **notification broadcast**: under
`RtlEnterCriticalSection`/`RtlLeaveCriticalSection` on a critical-section
object at guest `0x823F0C30`, it walks a doubly-linked intrusive list whose
sentinel is at guest `0x823F0C4C` (`node = *(sentinel+0)`; while
`node != sentinel`: call `*(node+8)` with `arg`; `node = *(node+0)`).
`sub_821F7CE0(r3=node, r4=insert_flag)` is the matching insert/remove
function for the same list, confirmed to be the **only** call site in the
whole codebase (`grep -rn "sub_821F7CE0(ctx" *.cpp"` finds exactly one, in
`sub_82383740`), registering a single fixed node at guest `0x82915FD8` with
real function pointer `0x82389BF8`, gated behind two runtime checks (a
non-null allocation and a vtable call returning nonzero) that can
legitimately skip registration without it being a bug.

`DumpBytes.java` against the static image confirms `0x823F0C4C` is
correctly self-pointing in the compiled XEX (`82 3f 0c 4c 82 3f 0c 4c ...`)
-- a standard `LIST_ENTRY{&self,&self}` data-only initializer, needing no
runtime init code. This rules out "the list was never initialized" as the
mechanism.

## Live measurement: the list is healthy for many calls, then its sentinel is overwritten with garbage

A throwaway diagnostic print at `sub_821F7C80`'s entry and inside its loop
body, run against the qualified ISO under `AC6_R132_DIAG=1`, shows the
broadcast succeeding cleanly and identically on every call for the whole
run up to the crash -- sentinel `0x823F0C4C`, single node `0x82915FD8`,
fnptr `0x82389BF8`, consistent with the one real registrant found above:

```
[r132] sub_821F7C80 entry: sentinel(r30)=0x823f0c4c head(r31)=0x82915fd8 arg(r29)=0x00000001
[r132] sub_821F7C80 node=0x82915fd8 next=0x823f0c4c fnptr=0x82389bf8
   ... (repeats cleanly, ~17 times across the run) ...
[r132] sub_821F7C80 entry: sentinel(r30)=0x823f0c4c head(r31)=0x00009182 arg(r29)=0x00000000
[r132] sub_821F7C80 node=0x00009182 next=0x00000000 fnptr=0x00000000
```

On the final, crashing call, `head` reads `0x00009182` -- not the sentinel,
not the known real node, and not zero either (which would at least be
consistent with an un-yet-initialized read). **The sentinel object's own
memory was overwritten with a small garbage value between two broadcast
calls.** This is memory corruption, not a race against an uninitialized
value and not a logic bug in the broadcast/insert functions themselves
(both are correct as written, confirmed by direct read of their generated
C++). `0x9182` also matches the `rcx` register value gdb's `info registers`
captured at the r131 fault (`rcx 0x9182 37250`), tying this measurement
directly to the same crash.

The `arg` changing from `1` (all healthy calls) to `0` on the crashing call
is a **real, separate, non-suspicious fact**: `sub_82390B18`'s disassembly
shows `li r3,0x0` immediately before its `bl 0x821f7c80` -- a distinct,
legitimate call site (this project's earlier trace already showed
`sub_82390B18` reached from an `ExCreateThread`-spawned background thread),
not evidence of a corrupted argument.

## `NtReadFile` ruled out as the corrupting writer, by direct trace, not by assumption

Given r130/r131 changed exactly this cycle's own new I/O path, the most
suspicious first candidate was `NativeGuestMediaService::read_file`'s new
streamed-copy `memcpy`/`stream.read` writing past its intended bounds. A
second throwaway diagnostic, added directly to the generated `NtReadFile`
stub, printed every call's destination guest-address range and length. The
entire run makes **exactly one** `NtReadFile` call before the crash:

```
[r132] NtReadFile handle=0x00000002 offset=0 dest_guest=0x173a0020..0x173a0020 length=0
```

`length=0` means zero bytes are copied by `NativeGuestMediaService::read_file`
regardless of the destination address's validity -- this call cannot have
written anything, anywhere. **This rules out this cycle's own new
`NtCreateFile`/`NtReadFile`/`NativeGuestMediaService` work as the corruption
source**, by direct measurement rather than by reasoning about the code in
isolation. The corrupting write is something else entirely, in code this
investigation has not yet examined.

## Decision

No native code was changed this cycle -- both diagnostics were temporary,
reverted, and verified reverted (`ctest` 9/9, 139/139 Python, `git status`
clean on retail source). The concrete, narrower finding kept from this
cycle is the report itself: the crash mechanism is fully characterized
(corrupted list sentinel -> null/garbage function-pointer call inside
`sub_821F7C80`'s own generated code, matching the observed
`rbp=rdx=r13=r15=0`, `rcx=0x9182` fault registers), and one plausible
culprit (this cycle's own new file I/O) has been positively excluded rather
than left as an open guess. Per this project's discipline against a
plausible rule with no control, the actual writer is named as the next
step rather than assumed.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Find the actual writer of guest `0x823F0C4C` (and neighboring
   `0x823F0C30`-`0x823F0C50`, the whole small-globals region this
   critical-section/list pair lives in). GDB watchpoints are already known
   unreliable on this 18-real-thread probe (r128) -- do not retry that
   route without a new argument for why it would behave differently now.
   A more promising approach: bisect with further `AC6_R132_DIAG`-style
   entry/exit snapshots in candidate functions reached between the last
   known-good broadcast and the crashing one, narrowing which function's
   execution window contains the write.
2. Once the writer is found, determine whether it is a genuine
   out-of-bounds/wrong-offset write bug in this project's own HLE surface
   (a stub writing through a miscomputed guest address) or a real
   guest-code bug this recompilation is legitimately reproducing (in which
   case the fix, if any, is scoped very differently and may not be this
   project's to make).
3. This cycle's two diagnostic techniques (function-entry/loop-body
   snapshot printing, and per-call I/O range tracing) are cheap and
   reusable -- worth keeping as a pattern for the bisection in (1), not
   worth generalizing into permanent infrastructure yet.
