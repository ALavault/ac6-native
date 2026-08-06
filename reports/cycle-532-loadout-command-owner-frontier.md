# Cycle 532 — loadout command-owner frontier

Date: 2026-08-02

## Qualification

- Target: Xbox 360 PAL `default.xex`.
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`.
- Native worktree HEAD: `6c417820fd6a89aade3cb02c53ddf4f222b81991`.
- Cycle-532 runtime SHA-256:
  `d7063cefeae596af817d62f77b99c74e2e74d121926fef2b35ffe06987998244`.
- Generated C++ was read only as revision-pinned literal control-flow and ABI
  cross-match evidence. It was not modified.

## Corrected outcome

The cycle-524 interpretation of `level_root+0x276A0` as a resource-readiness
predicate is invalidated. Cycles 525–528 prove that this word is the current
controller-button bitset:

- Back / `Tab` -> `0x20`;
- B / `Shift_L` -> `0x2`;
- X / `r` -> `0x00300008`;
- Y / `e` -> `0x00400004`;
- LB / `q` -> `0x4000`;
- RB / `f` -> `0x8000`;
- Start / `Escape` -> `0x10`;
- D-pad Up, Down, Left, Right -> `0x40`, `0x80`, `0x100`, `0x200`;
- A / `space` -> `0x00800001`.

The two exact writers, reached every frame, have return addresses
`0x82211E00` and `0x82211EA4`. Literal control flow in `sub_82211DF8` clears
the word and `sub_82211988` publishes button differences. Therefore event 207
is a Back-command result in one consumer path, not proof of loadout resource
completion.

Cycle 529 injects the native Back edge at the aircraft screen. The input
dispatcher observes `edge_field=0x20`, but `sub_82146DB8` is not invoked on
that edge and event 207 remains absent. No synthetic input or event should be
added to compensate.

## State and command producer trace

The manager fields have distinct literal contracts:

- `manager+35984`: state word, written by vslot `+0xA8` / `sub_8214B5F0`;
- `manager+35994`: boolean toggled by event 207;
- `manager+35995`: status byte, initialized to zero and set to one only on the
  qualified error arm in `sub_82147070`.

The experimental `ac6_force_loadout_ready` name is misleading: it writes
status value 5, for which no native literal producer was found.

Cycles 530–532 observe only two state-setter calls across the complete route:

```text
LR 0x82146DAC requested=0 current=0 ready=0 status=0
LR 0x82147F50 requested=0 current=0 ready=0 status=0
```

The second call republishes the current state from `sub_82147070`; it is not a
nonzero transition overwritten later. No state 1–5 is requested.

Canonical `sub_822FA748` translates command selectors 1–7 into manager events
202–208 through the event target at `level_root+0x36054`. It is vtable slot
`+0x7C` of vtable `0x820073B0`. Cycles 531 and 532 observe no invocation of
this translator.

Canonical constructor `sub_82221A28` builds the corresponding `0x600`-byte
object and installs vtable `0x820073B0`. It occurs at offset `0x94` in two
qualified `.rdata` function-pointer tables beginning at `0x82054E70` and
`0x820551B8`.
Cycle 532 observes no invocation of the constructor on the bounded first-
mission loadout route. The command-owner object is therefore not constructed
through this qualified constructor before the stalled aircraft screen.

The table role is not yet qualified. Canonical Ghidra reports no references to
either table base or `sub_82221A28` entry, no data pointer to the bases, and no
adjacent or split `lis`/`addi`/`ori` materialization within 64 instructions.
Calling them factory arrays would therefore exceed the evidence.

## Reproduction and evidence

- Scenario:
  `scripts/ac6-first-mission-loadout-native-ready-probe.steps`.
- Cycle-532 logs:
  `reports/logs/cycle-532-loadout-command-owner/`.
- Rotated runtime log SHA-256:
  `4d53d9cc2937bc18e94112e5393e2afda695e09ee01d532c13c5b9663f7fe4b8`.
- Current runtime log SHA-256:
  `8b06c50ed107d363429a74550cb62a139fc1f7a4bbe416810a25d5b4b535d0b1`.
- Aircraft-screen capture SHA-256:
  `a33d169eba43aede5bd239bc4cf776c12fa7d65f43e84d7c35002bb90af39df5`.
- Final observation capture SHA-256:
  `733cba0c7f02ff1e56f5352cb35d13d51e801724d78b72c9e39f6745581f2311`.

The reference build completes and the targeted AC6 suite passes 6/6. The
resulting `build-rt/ac6recomp` SHA-256 is
`6f7be3e1ed4cd5e5a754f09d74187414b4265001db7952c144d5f0ddf514330a`.

### Rejected whole-program trace attempt

An instrumented follow-up was not accepted as evidence. The initial build was
stale and treated new cvars as a game-data path. After rebuilding, the scenario
waited for `type28=30` after the instrumented route had already advanced to
types 6, 8 and 10. It never reached loadout. The early trace files were deleted
(126 files / 201.6 MB from the stale attempt, then 2 and 8 files from the two
bounded corrected attempts); their run logs remain under cycles 533–535 only
as diagnostics. This method must not be repeated without a state recipe that
starts from the actually observed instrumented save-dialog state.

## Next bounded checkpoint

First classify the two `.rdata` tables at `0x82054E70` and `0x820551B8` and
recover their indirect owner/consumer. If they are selected dynamically,
capture the table, entry index, destination storage and caller before the
aircraft screen. Repair only a qualified missing owner/route contract.
Acceptance requires a natural nonzero manager state transition and populated
aircraft statistics with both experimental force options disabled.

The separate `state+40` timeline-owner defect remains deferred until this
loadout lifecycle boundary is closed.
