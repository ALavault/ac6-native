# AC6 retail NTSC-U/J — decode-error hex formatting fixed; the register-range rejection precisely named, not fixed (r95)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed: `native/src/native_xenos.cpp` (fixes
decode-error address/header formatting), `native/tests/native_xenos_tests.cpp`
(one new test).

## Continuing r94's frontier

r94 left the new decoder rejection loop (`TYPE0 register range exceeds
Xenos state (IB 0x308019200, header 0x346112)`) as the next frontier,
flagging the `IB` address as suspicious for exceeding 32 bits.

Reading `Pm4Bridge::pump()`'s indirect-buffer error-annotation code
(`native_xenos.cpp:520-527`) found the actual cause: both the `IB` address
and the nested packet's `header` were formatted with `std::to_string()`
(which produces a **decimal** string) under a literal `" (IB 0x"` /
`", header 0x"` prefix — decimal digits printed as if they were hex
digits. `0x308019200` was never a real 34-bit address; it was decimal
`308019200`, which in real hex is `0x125c0000` — an entirely ordinary,
in-range guest address, and one that appears verbatim as a dword in the
top-level ring content r94's own probe log already captured. The header
value had the identical bug: decimal `346112` is real hex `0x00054800`.

## Fix

Added a small `to_hex()` helper (`snprintf("0x%08x", ...)`) and used it at
both formatting sites instead of `std::to_string()`. Grepped the rest of
`native_xenos.cpp` and the other native sources for the same pattern
(`std::to_string` applied to something later labeled `0x`) — the only
other use (`native_services.cpp:175`) formats a decimal counter, not an
address, and was not affected.

Verified against the real product binary (`ac6recomp`, rebuilt after
syncing the build-tree source copy): the bounded probe now prints `IB
0x125c0000, header 0x00054800` — matching a hand-decode of the previously
-corrupted decimal values exactly.

## The register-range rejection, precisely characterized

Decoded `header 0x00054800` against the project's own TYPE0 field
functions (`type_of`, `count_of`, `low_register_of` in `native_xenos.cpp`):
`type=0` (TYPE0, correct), `base register = 0x4800` (18432), `count = 6`.
`XenosState::kRegisterCount` is `0x4000` (16384) — so this packet
genuinely, legitimately asks to write 6 consecutive registers starting
well past the register file this emulation currently models. This is a
real, distinct limitation, not a decode-desync artifact (confirmed
because the corrected hex address is a real, resolvable, in-range guest
pointer, and the header decodes to well-formed TYPE0 fields, not garbage).

**Not fixed this cycle.** Expanding `XenosState::kRegisterCount` to
accommodate this would require either a verified real-hardware Xenos
register-space bound (not available; guessing one would be exactly the
kind of unverified constant the project's evidence discipline refuses) or
a bounded, data-driven scan of the actual maximum register index this
specific 2840-dword indirect buffer touches (a concrete, verifiable next
step, not undertaken here). Writing a larger `kRegisterCount` without
either would be an unjustified guess.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on the same pre-existing,
  unrelated N2 evidence mismatch as r90-r94, not touched.
- `ctest` (native profile): **9/9** passed, including the new
  `indirect_buffer_decode_error_reports_real_hex_address` test.
- `pytest` (full retail suite): **130/130** passed (no Python changes
  this cycle).
- `audit_test_assert_liveness.py`: pass, suites=8 vacuous=0 (unchanged
  from r94's fix).
- `git status` after `ctest`: only the 2 intentionally-edited files
  changed.

No scripts left behind this cycle (no Ghidra pass was needed — this was
entirely a native-source read/fix/verify cycle).

## Next

Scan the full guest indirect buffer at `0x125c0000` (2840 dwords) for
every TYPE0 write's register range to find the actual maximum register
index this content requires — a concrete, bounded, verifiable basis for
sizing `XenosState::kRegisterCount` correctly, rather than guessing a
number. Separately, r94's other open thread (identifying the object
behind the new `sub_821E6AC8`/`sub_821F03B0` wait) remains untouched.
