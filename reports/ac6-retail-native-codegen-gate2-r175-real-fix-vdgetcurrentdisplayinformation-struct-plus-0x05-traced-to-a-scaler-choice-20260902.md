# AC6 retail NTSC-U/J — real fix: `VdGetCurrentDisplayInformation` struct+0x05 traced to a real scaler-algorithm choice (r175)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(158/158, one assertion added to an existing test rather than a new test
function).

## Why this check was worth running

r170 confirmed `VdGetCurrentDisplayInformation` struct+0x05 as a real,
boolean-shaped field (read at both of its non-r0x821f0764 call sites) but
deferred implementing it because its immediate consumer (a normalize/
compare idiom feeding an unrelated flags bitfield) did not by itself pin a
correct value. This cycle traced further, into the two functions that
comparison actually selects between.

## What was found

**`0x821ea4d8`** (struct at `[r1+0x60]`): struct+0x05, read as `r11`, is
compared for equality against `1`:

```
821ea5c4  lbz r11,0x65(r1)      ; struct+0x05
821ea5d0  cmplwi cr6,r11,0x1
821ea5d4  bne cr6,0x821ea5e8    ; value != 1
821ea5d8  ori r11,r9,0x4
821ea5dc  stw r11,0xe4(r1)
821ea5e0  bl 0x821eb778         ; value == 1
821ea5e4  b 0x821ea5ec
821ea5e8  bl 0x821eb6e0         ; value != 1
```

Reading both callees:

- **`0x821eb6e0`**: a 256-iteration loop doing direct per-element
  table lookups and stores (`lhz`/`sth`, no arithmetic combining two
  lookups) — a **nearest-neighbor** style scaler.
- **`0x821eb778`**: the same lookup-table shape, but combines *two*
  neighboring lookups per output element (`subf r9,r9,r8` computing a
  difference between adjacent samples before scaling) — a genuine
  **linear-interpolation** scaler.

`value == 1` therefore selects the higher-quality interpolated scaler;
`value != 1` selects the plain nearest-neighbor one.

**`0x821ea2a4`** (struct at `[r1+0x1a0]`): struct+0x05 (at `+0x1a5`) is
normalized as `value != 1` and inserted as a single bit into a byte later
persisted (`stb r10,0x258(r29)`) — the bit is **set** when `value != 1` and
left **clear** when `value == 1`. Same direction as the first site: `1` is
the plain/default case, anything else is a marked deviation.

## Value chosen

`1` (`PPC_STORE_U8(ctx.r3.u32 + 0x5, 1u)`). Both call sites agree on the
same direction — `1` is the unmarked/default case — and the first site's
actual algorithmic consequence (interpolated vs. nearest-neighbor scaling)
gives a concrete, physically sensible reason to prefer it: a
higher-quality interpolated scaler is the sensible choice for this
project's own already-established HD/widescreen target (1280x720, r169),
and no evidence at either site points the other way.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 158/158
(one assertion added to `test_vd_get_current_display_information_fills_
width_height_fields`, no new test count). `git status` unchanged apart
from the intended change set and pre-existing, unrelated dirty state.

## Next

This closes every currently-named gap in the Vd/X platform-config family
opened by r168. A fresh, broader sweep of the offline-import fallback list
(mirroring r90/r93/r164's own method) is needed to name the next candidate.
Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
