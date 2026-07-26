# AC6 object-state entry after the frame-record toggle

Date: 2026-07-17

## Target and scope

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

This pass corrects the downstream edge from `0x8226ace0` and records the
compiler-split body around `0x822a4f98`. It does not assign aircraft, flight,
camera, weapon or spawn semantics.

## Corrected call edge

`0x8226ace0` forwards the mapped object, the new state word and literal `1` to
`0x822a4f98`. The first instructions at that address are:

```text
822a4f98  mfspr r12,LR
822a4f9c  bl    0x823864f4
822a4fa0  stwu  r1,-0x80(r1)
```

`0x823864f4` is a shared Xenon save-register helper. It stores nonvolatile
registers `r27` through `r31` and the link register at the caller's pre-frame
stack offsets, then returns. The corresponding restore helper is reached by
the later branch to `0x82386544`. These support entries are not game runtime
services and must not be used as semantic names.

## Body fragment at `0x822a4fa0`

The body continues after the save helper and uses the following observed
offset-qualified operations:

- copies argument `r3` into `r31` and argument `r4` into `r30`;
- loads a word from the object at `+0x60` and the previous state word at
  `+0x70`;
- tests a bit derived from `+0x60` and the low 24 bits of the incoming state;
- stores the incoming state to object `+0x70`;
- consults the global table word at `0x826e0000 + 0x4eb4`
  (`DAT_826e4eb4`) and, for an observed table result `7`, adds literal mask
  `0x101` before storing `+0x70`;
- scans the pointer array at object `+0xd8` for the count at `+0xdc`, passing
  each entry and the state word to `0x8229c128`;
- compares the new `+0x70` value with the saved old value and, when different,
  performs an indirect call through a table entry reached from the same global
  region;
- restores the stack/register state through shared compiler support before
  returning.

These are the currently defensible facts. The object type, table meanings,
callback ABI and event semantics remain unresolved. In particular, the body
does not establish that the object is an aircraft or that the update advances
flight state.

## Evidence correction

The previous cycle-101 wording that described `0x822a4f98`/`0x823864f4` as a
non-returning runtime wrapper is superseded. `0x823864f4` is compiler support;
the owning function boundary for the body beginning at `0x822a4fa0` is not
fully represented by the current Ghidra function table because the function
uses shared save/restore entries.

## Next useful evidence

The next static step is to qualify the owner type and the callback target
behind `0x8229c128`, preferably by following the caller's object construction
and the table entry provenance. A dynamic trace remains useful for assigning
game semantics, but no human or emulator session is required for this static
frontier.

## Validation

- Read-only Ghidra headless with `DecompileAt.java`, `DumpRange.java`,
  `ListFunctionsRange.java` and `ReferencesTo.java`.
- No Ghidra project writer, GUI, Xenia, Wine, VNC or human session.
- AC6 native CTest: **41/41**.
- `git diff --check`: pass.
