# AC6 retail NTSC-U/J — full chain closed: `sub_82338388` genuinely returns `0` for a category=1/setting=3 query, which fails a strict `>0` gate in `sub_82339D10` and produces the exact garbage size (r139)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Four
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R139_DIAG`) were added to gitignored, regenerated build-tree files
(`generated/ppc_recomp.31.cpp`, `.38.cpp`, `.52.cpp`), used to take live
measurements, and fully reverted via `cp` from `/tmp/*.orig` backups.
Confirmed reverted via `grep -c "r139"` returning 0 in all three files, a
clean `tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9),
and 139/139 Python tests. `git status` on `recompilation/ace-combat-6-retail`
shows only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change.

## Establishing the real call chain first, per r138's own instruction

r138 explicitly named "confirm the call chain for the specific failing
`sub_82222D80` call before reading further" as the required first step,
to avoid repeating the correlation gap it flagged in its own diagnostic.
A call-entry counter plus a `backtrace()` capture on the third
`sub_82222D80` call (the one r137 already identified as the failing one)
confirms:

```
[r139] sub_82222D80 ENTRY #3 size(r4)=0xfefffff9 align(r5)=16 flags(r6)=0
```

resolving via `addr2line` to `_xstart -> sub_821D7DE0 -> sub_821D5F48 ->
sub_821CC288 -> sub_82222D80`. This is the exact chain r135/r136 already
established from the allocator side, now independently confirmed from the
size-computation side with the exact garbage value (`0xFEFFFFF9`) present
at the call itself, closing the correlation gap r138 left open.

## Finding the real source: an exact-value match, then a direct measurement

Reading `sub_822834C0`'s full body precisely (correcting a small error in
this cycle's own earlier reading: its **actual** return value comes from
`sub_82338568`, not `sub_82338410` -- both are called, but only
`sub_82338568`'s result is kept) led to `sub_82339D10`, which contains:

```cpp
if (r30 > 0) { ... real work ... }
else {
  return 0xFEFF0000 | 65529;  // = 0xFEFFFFF9, exact bit-for-bit match
}
```

`0xFEFF0000 | 65529 = 0xFEFFFFF9` computed directly in Python matches
r137's measured garbage size **exactly**, not approximately (the earlier,
refuted r137/r138 hypothesis was only one bit off). This same error
constant/shape (`0xFEFF0000 | <code>`) is reused across four sibling
"require a strictly positive count" functions in this subsystem, so
r138's caution about not trusting a static match applies here too --
this cycle followed through with a live measurement rather than stopping
at the string match.

A live diagnostic at both `sub_822834C0`'s call into `sub_82338388` and
`sub_82339D10`'s own gate, in the same run, shows:

```
[r139] sub_822834C0: sub_82338388(cat=1,setting=3,idx=4) returned=0x00000000 (signed=0)
[r139] sub_82339D10: gate r30(count)=0
```

**`sub_82338388` -- an `ExGetXConfigSetting`-shaped guest wrapper
(category/setting bounds-checked, backed by a real internal pool
r138 already confirmed pops successfully every time) -- genuinely and
correctly returns `0` for category=1, setting=3, index=4.**
`sub_822834C0` treats `0` as a valid, non-error result (`>= 0` check) and
passes it into `sub_82338568` -> `sub_82339D10`, which requires a
**strictly positive** count and rejects `0` with the hardcoded error.
This is not memory corruption, not an uninitialized read, and not this
project's own HLE stub -- it is one piece of real, compiled guest code
(`sub_822834C0`/`sub_82338568`) passing a legitimately-zero value to
another piece of real, compiled guest code (`sub_82339D10`) that a
different contract (a caller several layers up, `sub_821CC288`) then
misuses as an allocation size without checking for the error return at
all.

## The full, now completely closed causal chain

1. `sub_82338388(category=1, setting=3, index=4)` queries a real,
   populated internal pool/registry and returns `0` -- a legitimate
   result under its own contract.
2. `sub_822834C0` accepts `0` as valid, calls `sub_82338568(0)`.
3. `sub_82339D10(count=0, ...)` requires `count > 0` and returns the
   hardcoded error `0xFEFFFFF9` for exactly this input.
4. `sub_82338568` and `sub_822834C0` propagate that value unchanged as
   their own return value -- neither distinguishes "a real error code"
   from "a real size" at this layer.
5. `sub_821CC288` uses this value directly as an allocation size (r137),
   never checking it as an error code, and requests it from `sub_82222D80`.
6. `sub_82222D80`'s allocator correctly rejects the effectively-4GiB
   request and returns NULL (r135/r136/r137).
7. `sub_821CC288` doesn't check for allocation failure and stores
   `NULL + 8 = 8` as a descriptor pointer (r135).
8. `sub_821CC508` reads a bogus "file size" of `0` from that pointer,
   producing a zero-length `NtReadFile` (r134).
9. `DATA.TBL`'s buffer is never filled, and `sub_82234B88` parses poison
   bytes as a real header, computing a wild pointer (r133).
10. The wild pointer's in-place byte-swap loop corrupts the notification
    list at `0x823F0C30-0x823F0C5E` (r132), which `sub_821F7C80` walks
    and crashes on (r131).

Every link in this ten-step chain has now been established with a live
measurement, not an inference.

## Decision

No native code changed this cycle -- four rounds of temporary,
env-gated diagnostics, all reverted and verified reverted (`ctest` 9/9,
139/139 Python, clean `git status`). This closes the entire causal
investigation from r130 through r139 down to a single, precisely
identified root: category=1/setting=3/index=4 legitimately evaluates to
`0`, and three separate pieces of real guest code -- an error-return
mismatch between `sub_82339D10`'s producer contract and
`sub_822834C0`'s consumer contract, and `sub_821CC288`'s complete absence
of error checking on the allocation size it computes -- combine to turn
that `0` into a crash five layers away. Whether category=1/setting=3
*should* be nonzero at this point in boot (a native-runtime gap this
project could fix) or is genuine, correct game state this investigation
is the first to reach (not a bug at all) is the one question this cycle
still cannot answer, and is named as next rather than guessed at.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Determine what category=1/setting=3/index=4 semantically represents in
   this subsystem -- read `sub_823382A8` (the local-buffer setup shared
   by `sub_82338388`/`sub_82338568`/`sub_82338410`) and the pool entries
   `sub_82343F20` pops, to see if their contents name a real config
   category (this project's own `ExGetXConfigSetting` HLE stub -- a
   *different*, already-implemented import -- is not obviously connected
   to this guest-internal pool, and that connection, if any, has not been
   traced).
2. Determine whether this value is populated by something earlier in
   boot that this probe has not yet reached, or whether it is expected to
   start at `0` and get populated by a step this project's native runtime
   does not yet trigger.
3. Do not add a defensive fix to `sub_821CC288`, `sub_822834C0`, or
   `sub_82339D10` without first answering (1) and (2) -- per this
   project's own discipline, patching a symptom in code this project
   does not own is not the same as fixing a real native-runtime gap.
4. This cycle's technique of computing an exact hex match from a static
   read, then confirming it live rather than trusting the string
   coincidence, closed the investigation's most important open thread in
   one pass -- prefer this pattern (compute, then verify) over further
   guessing.
