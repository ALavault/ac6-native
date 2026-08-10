# Cycle 1506 — the camera offset has an explicit state

## Qualification

- Target ID `ace-combat-6-pal`; module `default.xex`, Xbox 360 PAL,
  SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project `ghidra-projects/ace-combat-6`, opened read-only.
  No Xenia or other runtime oracle was used.
- Mission 01 is DATA.TBL entry 9, 42,446,032 bytes, SHA-256
  `cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05`.
  The qualified store index remains
  `349f5f49fe1acf19984c6470a5d3f16adf3029e36c93e24da8cb3ec58b4cdfd0`.
- Canonical listings and raw words were read for `0x8225D9F0` and
  `0x8225D0A8`. Their `.pdata` records declare respectively 119 and 185
  instructions; Ghidra recovers 111 and 173. The 8 and 12 missing words are
  VMX128 instructions. `0x8225C178` has 39 declared instructions and a complete
  scalar listing.
- Missing literal instructions were cross-checked against the generated
  listing in the revision-pinned `AC6_recomp` checkout at
  `dcd41b7457fcac8242f8ef40de83d1719390d5af`. Its asset resolves to the same
  XEX. Generated output supplied instruction/control-flow cross-match only;
  no generated code was copied into the native product.
- No retail byte enters this commit.

## The three-function boundary

The previously open call at `0x82260A84` is now a typed, pure state transition:

```text
0x8225D9F0  guard player/current-player/flag bit 1
            |-- abs(player+0x8A0) >= 2^-16     direct amplitude
            |-- player+0x890 > scaled +0x894  scaled amplitude
            `-- otherwise                     clear qualified shake fields

0x8225D0A8  state at manager+0x1C0
            +0x00 start, +0x10 target, +0x20 output,
            +0x30 velocity, +0x40 elapsed

0x8225C178  when elapsed > 0.1, consume two 0..32767 RNG results
            and map them to the next target x/y; target z becomes zero
```

`0x8225D0A8` retains the observed operation grouping: strict period refresh,
phase clamp, separate multiply/add rounds below phase 0.65, doubled/scaled
vector subtraction above it, fused x/y output integration, amplitude clamp,
velocity-z clear and elapsed update. The following qualified constants are
spelled by their retail float bits in the product:

```text
0x82002FD4  0x3DCCCCCD  period 0.1
0x82005EF0  0x3F266666  phase branch 0.65
0x82007F60  0x38800100  random-result scale
0x82007F7C  0x3F6A64C3  player-field scale
0x82069C2C  0x37800000  epsilon 2^-16
```

The results of `0x82380798` are injected as two explicit 15-bit values. This
keeps ownership of the global RNG in the replay/session layer and makes draw
consumption auditable: zero draws without a target refresh, exactly two with
one. Non-finite fields, results outside 0..32767 and arithmetic overflow fail
closed.

## Executed controls

`ac6-retail-mode2-camera-tests` now pins both integration phases by float bits,
including the retail fused outputs:

```text
low phase   velocity x/y  3E0F5C29 / 3E851EB8
            output x/y    3E4E3BCD / 3E9AEE64
high phase  velocity x/y  3CA3D70C / 3DA3D70B
            output x/y    3E4D013B / 3E9A0276
```

A strict-period refresh with draws 0 and 32767 produces target
`(-2, 2, 0, 0)` and consumes exactly two draws. Separate controls execute the
guarded-out, reset, direct and scaled branches; invalid state, invalid draw and
overflow are refused. The dynamic reset plus locator transform succeeds for
all 15 real camera groups in the sealed store.

Two micro-execution probes also measured the current instrument ceiling. With
the broad VMX callback set enabled, the harness stops before execution because
this Ghidra language does not register `loadVectorLeftIndexed128`. Without that
set, execution reaches `0x8225D174` and stops on its undecoded `lvx128`. This is
not a product contradiction and no executed bit-identity claim is made for the
VMX portion; the limitation is recorded instead of weakening the harness or
inventing an oracle result.

## Product composition and capture

`RetailMission01CpuCompositor` optionally accepts the dynamic input beside the
mode-2 locator state, applies it before the base transform and returns the next
shake state for the following 60 Hz tick. Dynamic input without locator state,
another view mode, invalid draws and non-finite state are refused.

The frame report is `ac6.mission01-cpu-frame.v3`. The qualified capture
executes the retail reset branch, so geometry and pixels intentionally remain
identical to cycle 1505 while the provenance changes:

```text
camera_source                         retail_mode2_dynamic
camera_dynamic_branch                reset
camera_random_draws_consumed          0
camera_next_shake_state               all qualified fields zero
camera_dynamic_offset_retail          true
camera_runtime_state_retail           false
camera_mode_selection_retail          false
camera_pose_retail                    false
jv_eligible                           false
```

The report moves `camera_dynamic_offset` from `open_boundaries` to
`closed_domains`; it does not move the live-state, selection or complete-pose
boundaries. Two frames still agree bit for bit:

```text
terrain considered / visible / rasterised       65,536 / 1,817 / 437
terrain candidate / written triangles           58,144 / 3,920
city considered / visible / rasterised            4,226 / 2,724 / 430
city candidate / written triangles              38,089 / 721
terrain / water / city fragment writes     27,572 / 108 / 761
depth and colour coverage                             27,746
marker writes                                               0
colour digest                              c5366eda993a572d
depth digest                               4ef0a2fbe98353f3
PPM SHA-256  11feffe65b23caf77451a49bd3d74f71dc70d1a7a1b4ae5ff030a7f52665e95c
```

The cold reference run takes 8.35 seconds and 771,056 KiB maximum RSS. This is
not the Vulkan 720p30 performance gate.

## Validation

```text
Release build                                                        pass
qualified dynamic mode-2 coverage                                    15/15
qualified CPU frame and deterministic second frame                    pass
qualified PAL cache / Mission 01 session                              pass
qualified CTest, SDL_AUDIODRIVER=dummy + Xvfb                       67/67
tools/tests                                                         87/87
sealed-cache audit                                                    17/17
mission01-final-gate-v3 --require JF                                   pass
mission01-playable-gate-v1 --require JF                                pass
contract addresses                                                   321/321
contract derivations                                              52, gaps 0
C++ complexity                                                    221 files
contract artefacts                                       146/146 match HEAD
```

## Residual boundaries

JV and JP are not passed. The next camera closure is the live producer of the
manager/player fields, including opening-view selection and state carry-over
from the mission player rather than the bounded reset fixture. The capture
still has a black sky and no vegetation, active mission units or HUD. Vulkan
timing, frontend/PAL localisation, retail mission-rule progression and the
human controller replay remain open.
