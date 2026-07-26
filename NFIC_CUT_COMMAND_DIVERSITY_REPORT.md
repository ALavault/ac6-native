# DPL9 NFIC command diversity and generic collection boundary

Date: 2026-07-15

## Result

The 44 exported DPL9 CUT streams contain real non-camera diversity, but the
closed common behavior is only frame-local command preservation.  A native
generic collector now exposes every dictionary-backed non-lifecycle event in
serialized order, attached to its active `FrameStart`, while retaining the
payload as opaque bytes.

This is sufficient to expose and compare effect/post-effect command streams.
It is not sufficient to render lights/effects, skin geometry, or impose an
interpretation on their payload words.

## Observed command families

| Event form | Presence | Closed fact |
| --- | ---: | --- |
| `0x0101` / `MoveEffect` | 16 CUTs, 60,626 events | Each observed payload is 8 bytes; it is preserved frame-locally without semantic field names. |
| `0x3001` / `Fade` | one CUT (`0038`), 260 events | Preserved as a named opaque frame command. |
| `0x3002` / `DOF` | 22 CUTs, 5,406 events | Preserved as a named opaque frame command. |
| `0x3003` / `Vignetting` | 22 CUTs, 5,406 events | Preserved as a named opaque frame command. |
| `0x3004` / `Chromatic` | 22 CUTs, 5,406 events | Preserved as a named opaque frame command. |
| `0x3031` / `CloudDD` | 22 CUTs, 5,406 events | Preserved; observed payloads include a 4-byte form. |
| `0x3032` / `TreeDD` | 20 CUTs, 4,656 events | Preserved as a named opaque frame command. |

`MoveLightD`, `MoveLightP`, `MoveLightA`, `MoveLightS`, and `Skin` occur in
the dictionaries of the object-oriented CUTs, but no exported event stream
uses their tags/dispatch forms.  They are therefore not exposed as executed
light or skin behavior.

The camera-family dispatch forms (`0x2001`/`0x2002`) and the existing world
forms (`0x2003`/`0x2004`) are collected too, but retain their more specific
native paths where those paths are already proven.

## Native API and safety rule

`collect_nfic_frame_dictionary_commands` accepts an already parsed event list
and dictionary.  It:

1. reuses the bounded CUT/frame lifecycle validation;
2. opens an output bucket only at a valid `FrameStart`;
3. retains all dictionary-backed non-lifecycle events in original order;
4. rejects a named command outside an active frame and an unterminated CUT;
5. leaves dictionary-less events absent rather than inventing a name.

Each returned item is exactly `{tag, dictionary name, borrowed payload bytes}`
under a serialized frame number.  No payload word is labeled as a light
parameter, effect instance, material property, or post-process coefficient.

## Why this does not alter the Linux executable

The generic route is a closed parser/replay surface, not a closed renderer.
The root `bin/ac6-scene-shell` continues to render only the separately joined
camera and Rigid/AnimRigid world behavior.  Publishing an effect/light/skin
visual from the opaque command bytes would exceed the available data-to-runtime
evidence, so this pass adds no speculative `bin/` behavior.

## Validation

`ac6-nfic-cut-tests` now checks generic collection ordering and confirms that
a named command after `FrameTerminate` is rejected.  It passed after a clean
CMake/Ninja build with `-j 32`.

## Evidence

- `remaster-export/cut/*/symbols.csv` and `events.csv`;
- `reconstruction/ace-combat-6/include/ac6/nfic_cut.h`;
- `reconstruction/ace-combat-6/src/nfic_cut.cpp`;
- `reconstruction/ace-combat-6/tests/nfic_cut_tests.cpp`.
