# AC6 retail NTSC-U/J — XenosState register file widened from real content; predicated TYPE3 named as the next gap (r96)

Date: 2026-09-01.

## Qualification

No Ghidra pass this cycle (GDB memory dump + Python parsing only). No
oracle used. Native code changed: `native/include/ac6/native_xenos.h`
(widens `XenosState::kRegisterCount`), `native/tests/native_xenos_tests.cpp`
(fixes one r95 test invalidated by the widening, adds one new test).

## Continuing r95's frontier

r95 named, but deliberately did not fix, a real register-range rejection
(`TYPE0`, base register `0x4800`, count 6) and recommended a bounded,
data-driven scan of the actual indirect buffer rather than guessing a
`kRegisterCount` bound.

## The scan

Dumped the full 2840-dword (11360-byte) indirect buffer at guest
`0x125c0000` from a live probe (raw-entry breakpoint capture of `base`,
the same verified method used throughout r85-r95; `dump binary memory` in
GDB). Parsed it in Python using the project's own PM4 field-decode logic
(`type_of`, `count_of`, `low_register_of`, `one_reg_of` — copied exactly,
not reimplemented from guesswork): the stream parses cleanly end-to-end
as 371 `TYPE0` packets and 281 `TYPE3` packets, no desync, no unknown
packet types. The maximum register index any `TYPE0` packet in this
content touches is **`0x5002`** (20482).

## The fix, and why `0x8000` rather than the observed `0x5003`

`low_register_of()` masks a `TYPE0` header's base-register field to 15
bits (`header & 0x7FFFu`) — `0x8000` is the complete range that field can
ever address, derived from the packet format itself, not from an assumed
or half-remembered real-hardware register-space size. It comfortably
covers the observed `0x5003` requirement with headroom for other content
this session hasn't captured. Checked every other decode path in the file
(`TYPE1`'s `type1_reg_a`/`b` mask to 11 bits; `SET_CONSTANT`'s base masks
to 11 bits) — `TYPE0` is the widest field, so `0x8000` is the correct
bound for the whole file, not just this one packet family.

`XenosState::kRegisterCount` changed from `0x4000` to `0x8000`
(`registers_` grows from 64KB to 128KB per instance — negligible).

## Verified

Rebuilt and reran the bounded probe. The `TYPE0` register-range rejection
is gone. Ring publish count **advanced from 31 to 37** dwords (more guest
content submitted than any prior cycle observed) before hitting a new,
different, explicit rejection:

```
vd drain rejected decode_ok=0 code=7 offset=239 detail=predicated TYPE3
packets are not supported (IB 0x125c0000, header 0xc0003601)
```

This is a deliberate `if ((header & 1u) != 0u) return {..., "predicated
TYPE3 packets are not supported"}` guard (`native_xenos.cpp:169-171`) —
real, unimplemented predicate-execution semantics, not a bug. Predicated
`TYPE3` packets should conditionally execute or no-op based on the
current predicate register state; implementing that correctly is
substantive feature work, not a one-line fix, and is named here as the
next frontier rather than attempted this cycle.

## Test changes

r95's `indirect_buffer_decode_error_reports_real_hex_address` used base
register `0x4800` to trigger the rejection — now in-range under the wider
bound, so that assertion would have silently stopped testing anything.
Changed it to use a count that overruns the top of the (still finite)
register file (`base=0x7ffe, count=3`), keeping the same hex-formatting
coverage. Added `register_count_covers_type0_full_field_width`,
reproducing r95's exact real packet (header `0x00054800`, base `0x4800`,
count 6) and asserting it now decodes successfully.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on the same pre-existing,
  unrelated N2 evidence mismatch as r90-r95, not touched.
- `ctest` (native profile): **9/9** passed.
- `pytest` (full retail suite): **130/130** passed (no Python changes
  this cycle).
- `audit_test_assert_liveness.py`: pass, suites=8 vacuous=0.
- `git status` after `ctest`: only the 2 intentionally-edited files
  changed.

No scripts left behind (GDB dump + Python parse only, no Ghidra headless
pass this cycle).

## Next

Predicated `TYPE3` execution is the concrete next frontier: read the
current predicate register/flag this decoder already tracks (if any),
determine the correct skip-vs-execute semantics, and implement it —
scoped work, not undertaken here. r94's other open thread (identifying
the object behind the `sub_821E6AC8`/`sub_821F03B0` wait) also remains
untouched.
