# AC6 retail NTSC-U/J — real fix: `sprintf`/`_vsnprintf` implement the exhaustively-verified specifier set (r236)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. `ctest` 10/10 (`build/ntsc-uj/native/native-cmake`), pytest
211/211 (210 passed, 1 pre-existing skip).

## What changed since r234/r235

r234 scoped `sprintf`/`_vsnprintf` and declined a bounded fix, estimating
a general varargs printf engine was needed. r235 dug further and found
the real internal consumer (`Function_821EF4E0`/`Function_821EF458`) has
28 real callers across ≥4 functions, reinforcing that estimate with
concrete evidence rather than overturning it.

This cycle went one step further than r235: rather than stopping at "many
real callers," it **exhaustively enumerated and decoded every format
string reaching every one of those real callers** (`scripts/DumpBytes.java`
on each address, including a dynamic 11-entry literal-string table at
`0x82691074` that one call site selects from at runtime), plus all 7 direct
`sprintf` call sites. The result: across every single reachable format
string in this qualified XEX, the **complete** specifier vocabulary is
bare literal text, `%s` (optionally with a decimal minimum-width, e.g.
this XEX's own `%25s`), `%d`, and `%x`/`%X` (optionally zero-padded with a
decimal width, e.g. `%08x`/`%08X`). No precision, no length modifiers, no
`%f`/`%p`/`%c`, nothing else appears anywhere. This is a closed,
verified set, not a sample — every real caller of both imports was
checked, not a subset.

Separately, `_vsnprintf`'s real 4th argument (the `va_list`-equivalent
pointer) was resolved precisely from raw disassembly of
`Function_821EF4E0` (`scripts/DumpRange.java`): the wrapper spills its own
incoming `r5`–`r10` as six sequential 8-byte doublewords
(`std r5,0x20(r1)` … `std r10,0x48(r1)`, before its own frame growth) and
hands `_vsnprintf` a pointer to the first slot. Each 8-byte slot's low 32
bits (offset `+4`, big-endian) hold the actual value — confirmed directly
from the store-instruction widths, not assumed.

## Fix

Added a shared parser, `guest_vprintf()` (HEADER, this file), implementing
exactly the verified specifier set above and nothing more — an
unrecognized specifier is copied through literally (both characters,
unconsumed) rather than guessing an argument width, since a wrong guess
would misalign every subsequent argument read and produce garbage that
looks like real data, which is worse than the prior no-op.

- `sprintf`: reads up to 6 varargs directly from `ctx.r5`–`ctx.r10` (no
  traced call site needs more than 2, and none needs a stack-spilled
  argument beyond `r10`), writes through `guest_vprintf` with a defensive
  `0x2000`-byte cap (real `sprintf` has no size argument to bound the
  write with; this cap exists purely to bound the native stub's own
  worst case, not to model real hardware behavior).
- `_vsnprintf`: reads varargs from the guest `va_list` pointer (`ctx.r6`)
  using the confirmed 8-byte-slot/low-32-bit convention, and honors the
  real `size` argument (`ctx.r4`) as the write cap.

Both return the number of characters written, matching the real contract;
neither traced caller inspects the return value, so this is
forward-looking correctness rather than a behavior any current caller
depends on.

## Verification

Beyond `ctest`/pytest (which confirm the generated code compiles and the
Python-level contract is right, but don't exercise the formatter's actual
output), the extracted `guest_vprintf` body was compiled and run
standalone against a simulated guest-memory buffer, checking it against
every real decoded format string from this XEX plus defensive edge cases:

- `"%s 0x%02x: "` with `("TC", 0xab)` → `"TC 0xab: "`
- `"%08x "` with `(0x1a)` → `"0000001a "`
- `"%d%s"` with `(42, "x")` → `"42x"`
- `"%s:\%s"` with `("a","b")` → `"a:\b"`
- `"   %25s: 0x%08x "` with `("RBBM_STATUS", 5)` →
  `"                 RBBM_STATUS: 0x00000005 "` (width-padding verified)
- `"%s 0x%08X"` with `("CD3D", 0xdeadbeef)` → `"CD3D 0xDEADBEEF"`
  (uppercase hex verified)
- Truncation: an 8-char literal into a 4-byte capacity correctly stops at
  3 chars + NUL.
- An unrecognized specifier (`%q`) is passed through literally rather
  than consuming an argument.

All eight checks passed.

## Gates

- `python3 -m pytest tests/` — 211/211 (210 passed, 1 pre-existing skip;
  new tests `test_sprintf_uses_the_shared_guest_vprintf_parser`,
  `test_vsnprintf_reads_varargs_from_the_guest_va_list_pointer`).
- `python3 tools/build.py --target ntsc-uj --profile native` — clean,
  the generated stub (including the new `guest_vprintf` helper) compiles
  without warnings.
- `ctest` (`build/ntsc-uj/native/native-cmake`) — 10/10.
- Standalone runtime smoke test of the extracted parser body (see above)
  — 8/8 passed.
- `git status` after `ctest` (not before): only the two edited source
  files changed.

## Next

1. This closes the last named candidate from r234/r235's catalog. The
   offline-import sweep (r148–r236) has no further bounded candidate.
2. If a future cycle finds this XEX reaches `sprintf`/`_vsnprintf` through
   a call site not covered by this cycle's exhaustive enumeration (e.g. a
   different game mode reaching new code), re-run the same
   `DumpBytes.java`-based decoding against the new format string before
   assuming the existing specifier set still covers it.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
