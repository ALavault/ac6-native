# AC6 retail NTSC-U/J — root cause closed: an unchecked 16-byte guest heap allocation (`sub_82222D80`) returns NULL, and the null propagates through five real functions to the `sub_821F7C80` crash (r135)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No committed native source changed this cycle.** Seven
rounds of throwaway, env-gated diagnostic instrumentation
(`AC6_R135_DIAG`) were added to gitignored, regenerated build-tree files
(`generated/ppc_recomp.22.cpp`, `generated/ppc_recomp.23.cpp`), used to
take live measurements, and fully reverted via `cp` from `/tmp/*.orig`
backups. Confirmed reverted via `grep -c "r135"` returning 0 in both
files, a clean `tools/build.py --target ntsc-uj --profile native` (`ctest`
9/9), and 139/139 Python tests. `git status` on
`recompilation/ace-combat-6-retail` shows only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change.

## Starting point: r134's own finding, and a self-caught mistake within this cycle

r134 found a single writer for `0x8293B94C` (a store gated by a flag byte
at `0x8293B938`) and, seeing the field read as effectively zero,
implicitly assumed the write never ran. This cycle set out to check that
directly and immediately corrected it: a live diagnostic on the writer
(`sub_821D5F48`, confirmed the exact same 16-bytes-past-r129's-table
address) and on both of `sub_821CC508`'s flag checks showed the flag
genuinely reads `2` (nonzero) at every check, and the write genuinely
executes. Reading `0x8293B94C` directly at the crash-adjacent chunk-size
calculation confirmed it holds `0x00000008`, not zero -- **a real,
deliberate store, not an unwritten field.** r134's own conclusion
("the flag is never set") is wrong; corrected here, per this project's own
discipline of correcting the immediately preceding cycle by name.

A second self-correction happened within this same cycle: the first
attempt read the flag-checking code at `ppc_recomp.22.cpp:35128` as being
inside `sub_821CC508`, based on proximity, without verifying the
enclosing `PPC_FUNC_IMPL` boundary. A live entry-counter on
`sub_821CC508` itself showed exactly one call in the whole run, with
`arg1=0x829ddd80` -- a real, static, nonzero address -- directly
contradicting an `r31=0` measurement taken at line 35128. Re-checking the
function boundary (`awk` scan for the nearest preceding `PPC_FUNC_IMPL`)
showed line 35128 is inside **`sub_821CC288`**, a completely different
function, not `sub_821CC508`. This mistake was caught and fixed within
the same cycle, before it reached a report or a commit -- consistent with
this project's evidence discipline, applied to its own live work rather
than only to predecessors' reports.

## The real chain, each link measured live

With the function boundary corrected, `sub_821CC288`'s own use of `r31`
is not its own argument at all -- it is **the return value of an
allocator call**, reassigned partway through the function:

```cpp
// bl 0x82222d80  (sub_82222D80(heap, size, class) -- a real, compiled
//                  guest allocator, not an HLE import)
sub_82222D80(ctx, base);
r31.u64 = ctx.r3.u64;          // r31 = allocation result
...
lbz r11,-18120(r11)            // read the flag (0x8293B938)
cmplwi cr6,r11,0; bne ... loc_821CC338
loc_821CC338:
  ...
  stw r11,-18100(r10)          // [0x8293B94C] = r31 + 8
```

A live diagnostic directly on `sub_82222D80`'s return value (a 16-byte
allocation: `size=16`, computed from `[some heap object] + 16`, `flags=0`)
shows:

```
[r135] sub_821CC288: sub_82222D80(size=16) returned r31=0x00000000
```

**The allocation returns NULL.** `sub_821CC288` never checks for
allocation failure before computing `r31 + 8` and storing it as a real
descriptor pointer, so `0x00 + 8 = 0x00000008` gets written to
`0x8293B94C` -- a small integer that later code treats as a valid guest
address.

## The full, now-closed causal chain

1. `sub_82222D80` (a real, compiled guest heap allocator) is asked for 16
   bytes and returns NULL.
2. `sub_821CC288` does not check for failure; it stores `NULL + 8 =
   0x00000008` into the descriptor field at `0x8293B94C` (unconditionally,
   once the already-nonzero flag at `0x8293B938` selects that branch --
   r134's own finding, still correct as far as it went).
3. `sub_821CC508` later reads that field as a "record" pointer (`r30 =
   0x00000008`), and reads `[r30 + 8] = [0x00000010]` -- near-null,
   unmapped-or-zeroed memory -- as the file's intended size, getting `0`.
4. Its chunked-read-size formula (`round_up(0, 2048) - 0`, capped) yields
   `0`, and this becomes the `Length` argument threaded unmodified through
   `sub_821F4E70` into `NtReadFile` (r134's own finding, still correct).
5. `NtReadFile` copies zero bytes into the `DATA.TBL` destination buffer
   (r133's finding).
6. `sub_82234B88` parses that unfilled buffer's poison bytes as a real
   header field, computing a wild array-base pointer (r133's finding).
7. Its in-place byte-swap loop scribbles across the critical-section and
   notification-list memory at `0x823F0C30-0x823F0C5E` (r132's finding).
8. `sub_821F7C80` (r131's crash site) walks the corrupted list and calls
   through a garbage function pointer -- SIGSEGV (r130/r131's finding).

Every link above r5 was established in an earlier cycle; this cycle closes
the two links r133/r134 left open, down to the actual failing primitive.

## Decision

No native code changed this cycle -- seven rounds of temporary,
env-gated diagnostics, all reverted and verified reverted (`ctest` 9/9,
139/139 Python, clean `git status`). This is the deepest point this
investigation has reached: a real, compiled guest heap allocator call
returning NULL, unchecked by the caller. Whether the *right* fix is
native-runtime-side (this project's heap backing the allocator is
mis-sized, mis-initialized, or missing a setup step relative to what real
hardware provides) or is simply this investigation reaching further into
the game's own logic than any prior cycle -- is not yet established, and
is named as the next question rather than guessed at, per this project's
own discipline against a plausible rule with no control.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Read `sub_82222D80` and its callee `sub_82221C68` (the size-class
   lookup) in full to understand what makes a 16-byte allocation fail --
   trace the heap object argument (`r31`, this allocator's own arg1) back
   to where it is set up, and check whether this project's native runtime
   ever initializes whatever backs it (a guest-side heap the game itself
   manages, likely via an earlier `RtlCreateHeap`-equivalent or a
   static pool this project's XEX-loading path is expected to prepare).
2. Determine whether the heap is *supposed* to be empty/uninitialized at
   this point in real hardware execution too (in which case this
   allocation failure, and everything downstream of it, may be real,
   correct game behavior this investigation is the first to reach -- not
   a bug to fix at all), or whether this project's own runtime is missing
   a setup step real hardware performs earlier.
3. Only once that is settled, decide the narrowest fix -- if any is this
   project's to make -- rather than adding a defensive null check to
   `sub_821CC288` (that would be patching a symptom in code this project
   does not own, not the actual XEX behavior).
4. This cycle's techniques -- reverse-computing a fixed `[base+offset]`
   guest address from `lis`/`addi` immediates, grepping for its unique
   writer, and a call-entry counter to disambiguate which of several
   static call sites actually fires -- have now found three real writers
   this way (r129, r134, this cycle) and caught one of this cycle's own
   attribution mistakes before it shipped. Prefer this over GDB-based
   approaches, which remain unreliable on this 18-thread probe (r128).
