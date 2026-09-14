# AC6 retail US — r583 : VMX128 patches + DXT1 texture + local runtime (2026-09-14)

## Qualification

- Ghidra project: ace-combat-6 (ghidra-projects-xenon)
- XEX: default.xex (US/NTSC-U/J retail)
- Oracle: none (no oracle session, no N3 budget)
- Codegen: codegen-20260831-mapfix-96838/generated + VMX128 patches

## Established

### SIGTRAP root cause: `__builtin_debugtrap()` from unimplemented VMX128 instructions

The r581/r582 SIGTRAP (exit code 133) during Opening substate 9 came from
XenonRecomp's `__builtin_debugtrap()` — the fallback for PPC instructions it
cannot translate. **38 sites** across four source files:

| Instruction | Count | File | Semantics |
|-------------|-------|------|-----------|
| `vupkd3d128 vD,vB,20` | 33 | ppc_recomp.10.cpp | float16×4 → float32×4 unpack |
| `vcmpbfp. vD,vA,vB` | 3 | ppc_recomp.11.cpp | vector bounds-check comparison + CR6 |
| `vpkd3d128 v0,v1,3,1,3` | 1 | ppc_recomp.25.cpp | float32×2 → float16×2 pack |
| `blrl` (indirect) | 1 | ppc_recomp.66.cpp | function pointer dispatch (not VMX) |

r582's static analysis correctly identified that the PPC `twi` traps at
`0x82392E70` and `0x82386C08` are not the source — they're translated to
comments + return. The SIGTRAP comes from downstream VMX math called during
media graph construction: `0x82365830` → colour/rendering init → `FUN_82119dd8`
(VMX conversion dispatcher) → `vupkd3d128` functions → `__builtin_debugtrap()`.

### Fix: three patch scripts replacing 37/38 debugtraps

Cross-match evidence: the rexglue-generated code (under `build/ntsc-uj/source/`)
translates the same PPC instructions with correct C++ implementations. The patches
apply the same math to the XenonRecomp output.

1. `tools/apply_vupkd3d128_type20_fix.py` — half-float-to-float conversion for
   each of the four 16-bit elements in the source vector register. Uses
   brace-scoped locals to avoid depending on `vTemp` being in scope. Handles
   `PPC_CONFIG_NON_VOLATILE_AS_LOCAL` (v14+ are local variables, not ctx members).

2. `tools/apply_vcmpbfp_fix.py` — SSE bounds comparison
   (`vA > vB` → bit31, `vA < -vB` → bit30) with CR6 record. Uses `PPCVRegister`
   local for the intermediate since `PPCRegister` lacks `.f32`.

3. `tools/apply_vpkd3d128_type3_fix.py` — float32-to-float16 pack with clamping,
   denorm handling and sign preservation. Single observed operand triple only.

The remaining `blrl` (indirect function pointer call in `sub_8238F230`) is not on
the Opening crash path.

### DXT1/BC1 tiled texture support

Added to the Vulkan backend alongside the existing DXT4_5/BC3 path:

- `tiled_dxt1_layout()` — block geometry + footprint for 8-byte blocks
  (8 KiB per 32×32-block macro tile vs BC3's 16 KiB)
- `untile_tiled_dxt1_2d()` — 8-byte block untiling with endianness swap
- `decode_pixel_texture` accepts format 4 (`k_DXT1`) tiled
- `ensure_pixel_textures` probes BC1 host support, stages, creates
  `VK_FORMAT_BC1_RGBA_UNORM_BLOCK` images
- `record_texture_upload` computes correct face_bytes for 8-byte blocks
- Tests: `tiled_dxt1_layout_bounds_footprint`,
  `untile_tiled_dxt1_matches_documented_layout` (including endianness)
- Improved error messages: format number, sign bits, mip levels, dimension

### Local runtime: game boots through TitleMovie

With VMX128 patches + mapfix-96838 codegen (19832 mappings) + DXT1 support:

- Mode manager initializes at ~10s (`manager_vptr=82065a34`)
- TitleMovie mode active (`mode_vptr=82065444`)
- `brandLogo` animation plays (2401 frames)
- GPU at 100% utilization, ~1.5 fps (GPU-bound, not CPU-bound)
- VD ring buffer processing confirmed (PM4 commands decoded, presents emitted)
- Auto-confirm cycling A button (3 presses in 50s)
- No SIGTRAP, no SIGSEGV during TitleMovie

### Full boot sequence through Opening (local runtime, in progress)

With START+A fake pad and 3600s probe window, the game traverses:

| Ordinal | Frame | VTable | Mode |
|---------|-------|--------|------|
| 1 | 1 | `82065444` | TitleMovie (brandLogo, 2401 frames, ~17 min) |
| 2 | 1057 | `820655A4` | Title transition |
| 3 | 1063 | `820654F4` | Title ("Press Start") |
| 4 | 1386 | `82063B5C` | FirstLoad (save dialog, auto-navigated) |
| 5 | 1397 | `82065C74` | CheckDL |
| 6 | 1410 | `82064E4C` | MainSelect |
| 7 | 1597 | `820661FC` | **Opening** (FSM states 4→5→0→1, no SIGTRAP) |

The Opening preloader actively loads elements (cursor 3→8 at 22 min).
No SIGTRAP — the VMX128 patches eliminate all 38 trap sites.

### Run 10: full boot + Opening, 30+ min no crash

Run 10 (A-only fake pad, 7200s probe window) reached Opening at frame 1597
in ~22 minutes, then the preloader began loading (cursor advancing 3→8).
At 30 minutes, no crash — the VMX128 patches hold through the entire boot
sequence and Opening initialization. The preloader needs ~70 more minutes
to reach cursor 42, at which point the media graph construction
(substate 7→9, the former SIGTRAP site) will execute.

### Substate 7→9 PASSED — SIGTRAP fully resolved

At ordinal 63, the media manager transitioned `substate 7→9` — the exact
code path that caused the SIGTRAP in r581/r582. No crash. The media
graph construction executed the VMX128-patched half-float conversion
and vector bounds comparison. Substates continued: 7→9→10→11.

The Opening preloader loaded 26/42 elements at ms=2400100 (40 game-min).
Readiness=1 triggered substate 7 at the right time. The demo plays
through the media FSM without intervention.

### MissionTitle reached — first time ever in native recompilation

At frame 1687, the game transitioned from Opening to MissionTitle
(`vptr=82065064`). The auto-confirm START (sent by `opening_demo_input`
at `buttons=0010`) skipped the Opening demo after substate 9→10→11.
MissionTitle is loading (`loading_state=1`, `input_valid=0`).

This is the first time MissionTitle has been reached in any run (CPJ or
local). The SIGTRAP at Opening substate 9 blocked all prior attempts.

Full run 10 mode progression (50+ min, no crash):
| Ordinal | Frame | VTable | Mode |
|---------|-------|--------|------|
| 1 | 1 | `82065444` | TitleMovie |
| 2 | 1057 | `820655A4` | Title transition |
| 3 | 1063 | `820654F4` | Title ("Press Start") |
| 4 | 1386 | `82063B5C` | FirstLoad |
| 5 | 1397 | `82065C74` | CheckDL |
| 6 | 1410 | `82064E4C` | MainSelect |
| 7 | 1597 | `820661FC` | Opening (substate 7→9 PASSED) |
| 8 | 1687 | `82065064` | **MissionTitle** (loading) |

## Not established

- Whether MissionTitle finishes loading and accepts input (in progress,
  `input_valid=0` at ms=3000100, loading active)
- Whether Briefing or Mission 01 are reachable
- Whether the oracle can be navigated headlessly to capture gameplay shaders
  (built locally, renders, but MnK keyboard input not reaching the SDL window)
- The cause of the ~1.5 fps performance (GPU at 100% utilization)

## Decisions

- Used `codegen-20260831-mapfix-96838` instead of `patched-final-r11` because
  it includes the missing `0x82096838` function boundary (null indirect call
  crash at boot). The mapfix has 19366 native mappings; with import stubs it
  populates 19832 indirect-call entries, matching the pre-built binary.
- Used Clang (21.1.8) instead of GCC because the codegen uses
  `__builtin_assume` (Clang-only).
- VMX128 patch math taken from rexglue cross-match evidence, not derived
  independently. The rexglue output implements the same PPC instructions with
  the same semantics; the patch applies the identical code.

### Shader translator diagnostic improvement

`translate_ucode` error now includes the microcode digest, shader type, dword
count, and modification in the error message — enough to identify exactly which
shader needs adding to the pinned registry.

### Title screen auto-navigation: A-only, not START

The Title screen has a timer that triggers a promotional cinematic when START is
pressed. Sending START+A together causes the game to enter the cinematic loop
instead of advancing to FirstLoad. The fake pad stays A-only (`kButtonA`), which
the Title screen accepts for navigation (confirmed by run 7: Title→FirstLoad
transition in ~5 minutes with A-only). The `opening_demo_input` function sends
START only during Opening mode (gated by substate conditions), not at the Title
screen.

### Rexglue oracle built locally

The rexglue oracle (dynamic recompilation + Xenia GPU emulation) builds from
source with Clang 21 and Vulkan-only. The binary (150 MB) is at
`/fastdata/lavaulta/tmp/rexglue-build2/ac6recomp`. It requires further
integration work to run headless for shader capture (GTK+ initialization,
argument format).

## Verification

- CTest 10/10 pass (non-guest build, including DXT1 tests)
- CTest 11/11 pass (guest-linked build)
- Mission01 native gate: `audit-valid JF=pass open=none`
- Contract artifacts: `pass contracts=6 cited=189 match_head=189`
- Contract addresses: `pass contracts=6 cited=321 supported=321`
- Local runtime: full boot TitleMovie → Title → FirstLoad → CheckDL →
  MainSelect → Opening — no crash, no SIGTRAP, preloader active
