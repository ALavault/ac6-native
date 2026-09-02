# AC6 retail NTSC-U/J — real fix: `VdQueryVideoMode` struct fill, offsets derived from this XEX's own disassembly (r169)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, `-readOnly -noanalysis`. XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used for the struct offsets (real disassembly only — see below).
Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target
ntsc-uj --profile native` (`ctest` 9/9), the full retail-native pytest
suite (151/151, up from 150/150), and a live import trace confirming the
import no longer reaches the generic offline-import fallback.

## What this closes

r168 deferred `VdQueryVideoMode` explicitly because Xenia Edge's own
public source's `X_VIDEO_MODE` struct layout could not be independently
verified against this XEX's own compiled offsets, and this project's
discipline refuses assuming byte-for-byte agreement with another
project's reimplementation. This cycle derives the offsets from this
XEX's own disassembly instead, using only real, read evidence.

## Evidence for the four fields implemented

Both of this XEX's own real static call sites, read in full:

**`sub_821F0DB0` @ `0x821F0E78`** (struct at `[r1+80]`):
```
821f0e80  lfs f13,0x64(r1)     ; struct+0x14, float
821f0e84  lwz r10,0x54(r1)     ; struct+0x04, u32
821f0e90  lwz r11,0x50(r1)     ; struct+0x00, u32
821f0e94  fadds f0,f13,f0      ; struct+0x14 combined with a constant, real arithmetic
821f0e98  stw r10,0x5418(r29)  ; struct+0x04 -> one output field
821f0e9c  stw r11,0x5414(r29)  ; struct+0x00 -> output field A
821f0ea0  stw r11,0x541c(r29)  ; struct+0x00 -> output field B (SAME value, duplicated)
```

struct+0x00's value is written to **two separate output fields** — the
classic "width" / "actual width" duplication pattern, and independently
matches the field-name pairing (`display_width`/`actual_display_width`)
in Xenia Edge's own public `VdGetCurrentDisplayInformation` (a *different*
function in the same file, consulted only for this naming-pattern
cross-check, not for byte offsets). struct+0x14 is used in real
floating-point arithmetic (added to a constant, then truncated to an
integer) — consistent with a refresh-rate-derived timing computation.

**`sub_821F2BC8` @ `0x821F2C00`** (struct at `[r1+112]`):
```
821f2c04  lwz r29,0x78(r1)     ; struct+0x08, u32
821f2c0c  cntlzw r11,r29
821f2c10  rlwinm r11,r11,0x1b,0x1f,0x1f
821f2c14  xori r11,r11,0x1
821f2c18  addi r10,r11,0x1
```

`cntlzw` + this exact shift/mask sequence is the canonical PowerPC
"normalize nonzero to 1, zero stays 0" boolean idiom — independently
confirming struct+0x08 is a boolean-shaped flag, from a **different** call
site than the one above, cross-checking the same struct.

**Four offsets are therefore evidence-confirmed directly from this XEX's
own code, not assumed from any external source**: `+0x00` (u32, a
width-shaped value used twice), `+0x04` (u32, used once — a
height-shaped value by position and pairing with the width field),
`+0x08` (u32 boolean, confirmed via the normalization idiom), `+0x14`
(float, used in real arithmetic). `+0x0C`/`+0x10` are not read by either
traced call site and are left unimplemented (not asserted).

## Values chosen

- `+0x00`/`+0x04` (width/height): `1280`/`720`. Not invented for this
  fix — this project's own pre-existing resolution assumption, already
  used throughout its PM4/swap-packet test fixtures
  (`native/fixtures/xenos-capsule-minimal.json`,
  `native/tests/native_xenos_tests.cpp`), kept internally consistent
  rather than introducing an unrelated number.
- `+0x08` (interlaced flag): `0` — progressive, correct for 720p by
  definition (interlaced only applies at 1080i).
- `+0x14` (refresh-rate-shaped float): `60.0f` (`0x42700000` as raw
  bits, computed via Python's own `struct.pack`, not eyeballed) — the
  standard NTSC 60 Hz value, matching this project's single-region
  NTSC-U/J target.

These last two are the only values not directly read off this XEX's own
bytes; both are ordinary, unremarkable production defaults for those
exact display parameters (not chosen to game any specific downstream
comparison), consistent with this project's own discipline against
synthetic values used to force a particular outcome (r53's precedent) —
this is implementing a real kernel API's real contract with the best
available evidence, the same category as r145/r148/r149/r162-r167, not a
targeted patch for a specific symptom.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 151/151
(150/150 before this cycle, +1 new test:
`test_vd_query_video_mode_fills_the_struct_not_a_status`). Live import
trace confirms the import no longer reaches `trace_offline_import()`.
`gdb` backtrace confirms the tracked `sub_821F7C80` crash still
reproduces identically — this call happens well before, and is unrelated
to, that chain. `git status` clean apart from the intended change set.

## Next

1. `+0x0C`/`+0x10` remain unconfirmed and unimplemented — a future cycle
   reaching a call site that reads them (none found this cycle) would
   supply real evidence for those two fields.
2. `VdQueryVideoFlags`/`VdGetCurrentDisplayGamma`/
   `VdGetCurrentDisplayInformation` (named in r168 as the same plausible
   family) remain unchecked.
3. Both of Gate 2's named frontiers are unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
