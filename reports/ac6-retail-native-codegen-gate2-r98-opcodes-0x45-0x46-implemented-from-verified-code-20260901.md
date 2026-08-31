# AC6 retail NTSC-U/J — opcodes 0x45/0x46 implemented from verified retail construction code; predicate still the live blocker (r98)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed: `native/include/ac6/native_xenos.h`
(two new opcode constants), `native/src/native_xenos.cpp` (two new decode
cases), `native/tests/native_xenos_tests.cpp` (one new test).

## Continuing r97's frontier

r97 named opcodes `0x45`/`0x46` as the higher-value, lower-risk target
(unlike the predicate question, an unimplemented opcode fails loudly
rather than risking silent incorrect rendering). This cycle traced both
to their real retail construction sites rather than guess payload
semantics from the captured data alone.

## Opcode `0x45`: verified construction, structural implementation

`FindInstructionScalar.java 0x1925` (the literal constant every captured
`0x45` packet's payload word 1 carries) found exactly one static site:
`0x821eb918`, inside `sub_821EB8B8` (`0x821eb8b8`-`0x821eb994`). Reading
the full function: it writes a fixed 6-dword-plus-header packet shape 256
times in a loop (`cmplwi cr6,r9,0x100; blt cr6,...`), packing three
parallel `uint16` values read from a caller-supplied table (`r31`, the
function's second argument) at 512-byte-spaced offsets (`r31+0`,
`r31+0x200`, `r31+0x400`, walked in 2-byte strides) via `rlwimi`/`rlwinm`
bit-merge instructions into the packet's one varying payload dword. The
constant header `0xC0054500` this function constructs matches the real
captured packets exactly.

Traced the table's own producer, `sub_821F00C0` (the function that calls
`sub_821EB8B8`): it fills three 512-byte regions with values computed by
dividing a loop counter by `127` or `255` (`divwu r10,r9,r7` with
`r7=0x7f`/`0xff`) into quotient/remainder pairs — the shape of a linear
ramp or quantization table, not arbitrary data. This is consistent with,
though not independently proven to be, a display gamma or dither table
upload (this project's own `XGetVideoMode`/`VdGetCurrentDisplayGamma`
imports, both still unhandled, are the obvious real-hardware analog).

**Implementation**: added `kOpcodeSetGammaOrDitherTable = 0x45u`, decoded
identically to the existing `SET_BIN_MASK`/`SET_BIN_SELECT` family —
structurally accepted (`count == 6` required) with the payload not
semantically modeled, matching this codebase's own existing precedent for
data this project doesn't yet consume further downstream. No claim is
made about the exact table semantics beyond what the retail code itself
verifies.

## Opcode `0x46`: verified construction, matches an existing precedent exactly

`FindInstructionScalar.java 0x4600` found several candidate sites; the
one nearest the already-identified `0x45` builder (`0x821ebcb8`, inside
`sub_821EBB40`) confirmed the real construction. This function's logic
reads several dirty/state-change flag bits from the same object fields
already characterized in r89/r93/r94 (`+0x2abd`, `+0x2abe`, `+0x2940`,
plus several others), and — critically — reaches the **exact same**
cursor-vs-limit overflow check and `sub_821E60A8` call already fully
characterized in r93/r94's ring-packet-writer analysis
(`lwz r3,0x30(r31); lwz r10,0x38(r31); cmplw; ble; bl 0x821e60a8`) before
constructing the `0xC0004600` header with payload `0xF`, matching the
real captured packet exactly. This is the same producer family as the
already-implemented `sub_821E64A8`/`sub_821E65B0` writers, for a
different specific state group.

**Implementation**: added `kOpcodeInvalidateStateExtended = 0x46u`,
decoded identically to the existing `INVALIDATE_STATE` (`0x3B`) case —
`count == 1` required, no further state effect modeled, matching the
existing comment's own reasoning ("the immutable native state snapshot is
already replaced transactionally below") applied to a structurally
identical packet for a different state group.

## Verified: correct, but not yet observable in the live probe

Unit tests (`decoder_accepts_verified_retail_opcodes_0x45_and_0x46`)
reproduce the exact real packets (header, payload) captured from guest
`0x125c0000` and confirm both decode successfully. Rebuilt `ac6recomp`
and re-ran the bounded probe: **behavior is unchanged from r97** — decode
still stops at the predicated `DRAW_INDX_2` (offset 239), which r97
deliberately left unfixed. Opcodes `0x45`/`0x46` sit *after* that point in
the real packet stream (offset 400 onward), so this cycle's fix, while
correct and verified, has no observable effect on the live probe until
the predicate question is resolved. Stated plainly rather than implied
otherwise.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on the same pre-existing,
  unrelated N2 evidence mismatch as r90-r97, not touched.
- `ctest` (native profile): **9/9** passed, including the new test.
- `pytest` (full retail suite): **130/130** passed (no Python changes).
- `audit_test_assert_liveness.py`: pass, suites=8 vacuous=0.
- `git status` after `ctest`: only the 3 intentionally-edited files
  changed.

No scripts left behind (`DumpR98Opcode45.java`, `DumpR98Caller.java`,
`DumpR98Opcode46.java` used read-only against `ghidra-projects/ac6-us`
and deleted; confirmed by `git status --porcelain=v1 -- scripts/` showing
empty).

## Next

The predicate question (r97) remains the live blocker — resolving it
(via a verified source, or a decision to accept the architecturally
plausible "predication disabled by default" interpretation with explicit
sign-off on the risk) is now the single remaining item before this
specific indirect buffer decodes end-to-end. r94's other open thread
(identifying the object behind the `sub_821E6AC8`/`sub_821F03B0` wait)
also remains untouched.
