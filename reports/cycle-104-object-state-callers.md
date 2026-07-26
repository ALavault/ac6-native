# AC6 convergence of the `0x822a4f98` object-state entry

Date: 2026-07-17

## Target

Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Caller convergence

Fresh read-only headless decompilation qualifies four direct caller families
without assigning a gameplay class:

- `0x8226acf0` (`0x8226ace0` body): maps the frame-record key to an object,
  toggles the candidate `+0x70` state with the observed `0x82`/`0x02` masks,
  and calls `0x822a4f98(object, state, 1)`.
- `0x822735a0`: iterates an object-pointer array, calls
  `0x822a4f98(object, object[+0x70?] | 1, 0)` for entries not in the observed
  state value `1`, then checks vtable methods at `+0x04`, `+0x08` and `+0x58`.
  The exact array base is not recovered in this pass.
- `0x822a6090`: conditionally calls `0x822a4f98(object, 1, 1)` before checking
  the same vtable family. Its surrounding control flow contains bad
  instruction data, so this caller remains lower confidence.
- `0x822a5250`: calls `0x822a4f98(object, object[+0x70] & ~0x08, 1)`.

The convergence supports an object-state update contract with a virtual-method
owner and a state word at `+0x70`. It does not identify the object as an
aircraft, unit, camera, weapon or flight controller.

## Boundary and confidence

The caller arguments and the `+0x70` field are `cross-match`/`heuristic`
object-layout evidence. The shared Xenon save/restore entries around
`0x822a4f98` remain compiler support. The owner constructor, vtable identity,
and callback semantics behind the indirect dispatch are still `unknown`.

## Next useful static join

Follow the vtable pointer used by the convergent callers and locate the first
constructor or factory that writes the object pointer array. If the pointer
array and vtable cannot be joined statically, classify the remaining owner
identity as `needs-dynamic-evidence` rather than inventing a flight name.

## Validation

- Read-only Ghidra headless with `DecompileAt.java` and existing corrected
  project; no project writer.
- No Xenia, Wine, VNC, GUI or human session.
- AC6 native CTest remains **41/41**.
- `git diff --check`: pass.
