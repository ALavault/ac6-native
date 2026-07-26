# AC6 player-input observable at `0x821CE088`

Date: 2026-07-15

## Bounded result

The corrected 15,333-function PowerPC project exposes a non-cinematic input
path. `0x821CE088` polls four device slots through `0x8233B470` and
`0x8233B428`, matches the returned records, and writes four canonical states at
`0x826EDB98` with an exact `0xA0` stride. This is distinct from the previously
closed CUT controller.

The raw button word is loaded at `0x821CE190`. The normal-device path maps:

| Raw mask | Canonical mask |
| ---: | ---: |
| `0x0001/2/4/8` | `0x0001/2/4/8` |
| `0x8000` | `0x0010` |
| `0x1000` | `0x0020` |
| `0x4000` | `0x0040` |
| `0x2000` | `0x0080` |
| `0x0100/0x0200` | `0x0100/0x0200` |
| `0x0010/20/40/80` | `0x0400/0800/1000/2000` |

These raw masks agree with the standard XInput button layout, including raw
`0x1000` for A. The XEX import table independently confirms that the program
imports `XamInputGetState`, but the corrected loader's imported-symbol address
labels do not point to usable import thunks. Therefore this report does not
claim a direct static call to the mislabeled import address.

`0x8221543C..0x82215498` then reconstructs the same canonical base, walks four
states at `0xA0` stride, and calls `0x82215140`. That leaf tests the canonical
button field at state offset `+0x08` against 32 configurable masks and produces
a 32-bit logical-action field. `0x82214F88` computes exact edge observables:

```text
just_pressed  = current & ~previous
just_released = previous & ~current
```

This closes a physical-button to configurable logical-command to
`just_pressed` path. It does **not** yet identify which logical bit is gun,
missile, throttle, menu confirm, or another gameplay action.

## Retail-manual cross-check

The visually checked controller diagram on page 4 of
`.tools/manuals/ace-combat-6-manual.pdf` agrees with every single digital
XInput button preserved by `0x821CE088`. The native table now records the exact
raw/canonical pair beside the manual's Normal-control use for D-pad directions,
Y/A/X/B, LB/RB, Start/Back and both stick clicks. In particular, the manual
identifies A as machine gun, B as missile/special weapon, X as map, Y as target,
LB/RB as yaw, and right-stick click as change view.

This is physical-control metadata, not a recovered logical-slot assignment.
The D-pad is retained as context-dependent directional input because the manual
lists menu, Operation ID and ally-command uses for the same buttons. LT/RT
throttle, left-stick pitch/roll, right-stick camera, LB+RB autopilot and LT+RT
High-G are not ported into this table: the current `0x821CE088` contract does
not close the analog axes or chord consumer. No logical bit is named from the
manual alone.

## Native reconstruction

Address-based native functions are retained in `input_821ce088.*` until the
input subsystem and its action table are fully recovered:

- `function_821ce088_map_buttons` reproduces the exact normal-device button
  mapping;
- `function_821ce088_manual_button_mappings` exposes only the independently
  concordant manual/raw/canonical physical-button rows;
- `function_82215140_map_actions` reproduces the configurable 32-mask remap;
- `Function82214f88Edges::update` reproduces press/release edges.

The SDL shell gives this bounded path a visible diagnostic interaction. Return
supplies raw XInput-A (`0x1000`), and the recovered default table at
`0x821BE268` maps canonical `0x20` to logical bits 0 and 23. A top-right marker
plus window-title state shows held/just-pressed state. This is explicitly an
input observable, not inferred flight behavior. See
`FUNCTION_821BE268_DEFAULT_BINDINGS_REPORT.md`.

## Evidence and verification

Instruction evidence is retained in
`reports/logs/player-input-821ce088-path.log`. The corrected re-agent export
contains 15,333 functions; `re-agent reverse --address 0x821ce088 --dry-run`
resolved the address without making an LLM call, respecting the current model
budget.

- GCC native build and 15/15 CTest tests pass.
- Clang ASan/UBSan build and 15/15 CTest tests pass.
- The input unit test covers every recovered raw-button mapping, multi-binding
  action output, and press/hold/release edges.
- SDL dummy-driver retail smoke passes.
- `--capture-input` deterministically exercised the diagnostic raw-A binding;
  the visually checked result is `captures/player-input-a-pressed.png`, with
  its direct SDL BMP source beside it.

## Open next edge

Find the aircraft/flight receiver behind an opaque logical-slot consumer. Only
then should any bit move an aircraft, fire a weapon, or alter throttle.
