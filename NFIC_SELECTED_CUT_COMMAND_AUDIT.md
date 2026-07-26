# Selected DPL9 CUT non-camera command audit

Date: 2026-07-15

## Result

The selected Scene group `22.1.0` has no remaining serialized effect, light,
skin, or other non-camera command family to replay.  Its complete `0x3040`
event stream contains lifecycle commands, the two camera commands
`MoveCamera`/`GazeCamera`, and the two world-track forms already represented
by the native Linux scene shell.  This audit deliberately leaves the remaining
camera command out of scope.

## Exact selected-CUT event inventory

`remaster-export/cut/0000/events.csv` has the following tag counts:

| Tag | Dictionary name/form | Count | Payload size |
| --- | --- | ---: | ---: |
| `0x8001` | `CutStart` | 1 | 0 |
| `0x8002` | `FrameStart` | 120 | 4 |
| `0x1001` | `MoveCamera` | 120 | 8 |
| `0x2001` | `GazeCamera` dispatch form | 120 | 8 |
| `0x2003` | `Rigid` dispatch form | 1,680 | 8 |
| `0x2004` | `AnimRigid` dispatch form | 240 | 8 |
| `0x8003` | `FrameTerminate` | 120 | 4 |
| `0x8004` | `CutTerminate` | 1 | 0 |

The dictionary additionally names four `MoveLight*` forms, `Skin`, and
`MoveEffect`, but no event in this selected stream uses any of their tags or
dispatch forms.  Dictionary presence is not treated as command execution.

## Closed world behavior

For every active frame, the non-camera payloads have the common proven layout:

```
word 0: high 16 = one-based Scene object id; low 16 = flags
word 1: high 16 = serialized frame;       low 16 = flags
```

The native collector accepts only zero-flag `0x2003`/`0x2004` records whose
frame agrees with the active `FrameStart`, then joins the one-based id through
the local Scene path/resource index.  In group `22.1.0`, each of 120 frames
has the same ordered membership: 14 `Rigid` and 2 `AnimRigid` commands for
Scene objects 3 through 18.  Those 16 bounded MOP/MDLP joins are already
rendered by `ac6-scene-shell` as CUT-local native world presentation.

This is the complete data-backed non-camera behavior available in the selected
CUT.  The `Rigid`/`AnimRigid` dictionary names distinguish serialized track
form only; they do not prove actor role, player ownership, collision, or an
animation system beyond the bounded transform sampling already used.

## Static consumer check

The nearby executable helper `0x823695c0` asks `0x8236b6e8` for tag `0x2001`.
The latter is only a generic indexed-tag lookup: it returns the requested
occurrence of a supplied tag.  Here it is evidence for the already excluded
`GazeCamera` family, not an effect/light/skin consumer and not a data join for
the world tracks.

No selected-CUT command reaches the dictionary-only `MoveLight*`, `Skin`, or
`MoveEffect` forms.  Extending the renderer with lights, effects, or skinning
from those names would therefore fabricate behavior absent from this selected
serialization.

## Native consequence

No code, test, or `bin/` update is warranted in this pass.  The existing
Linux executable already covers every closed non-camera world-track command in
the selected CUT; remaining command families require a selected stream that
uses them plus an independently joined data/consumer path.

## Evidence

- `remaster-export/cut/0000/symbols.csv`;
- `remaster-export/cut/0000/events.csv`;
- `reconstruction/ace-combat-6/include/ac6/nfic_cut.h`;
- `reconstruction/ace-combat-6/src/nfic_cut.cpp`;
- `AC6_LINUX_SCENE_SHELL_REPORT.md`;
- `exports/823695c0.json` and `exports/8236b6e8.json`.
