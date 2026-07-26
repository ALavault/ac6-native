# AC6 state-change callback tail boundary

Date: 2026-07-17

## Target

Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Boundary

The call site in the body after `0x822a4f98` reaches code at `0x8229c128`
with the object in `r3` and the candidate state word in `r4`. Ghidra does not
currently expose this address as a complete function, so it is recorded as a
tail/code-entry boundary rather than assigned a guessed function name.

The observed operations are:

1. read object `+0x118` and test one bit;
2. when the bit is clear, store `r4` to object `+0x130`;
3. read an indirect object through `+0x180`, then inspect byte `+0x56`;
4. when that byte equals `6`, clear the observed mask range in `+0x130`;
5. compare the resulting `+0x130` word with its previous value and return if
   unchanged;
6. when changed, load a table entry at `DAT_826e4eb4 + 0x29c80`, pass the
   object in `r4`, and tail-branch to `0x8226ac30`.

At `0x8226ac30`, two adjacent table-selected tails load entries at
`DAT_826e4eb4 + 0x37024` or `DAT_826e4eb4 + 0x36240` and branch to
`0x822cd880` or `0x822537d0`. These are dispatch targets; their gameplay
meaning and callback ABI remain unresolved.

## Interpretation boundary

This proves a conditional object-state notification path keyed by the
object's `+0x130` value. It does not prove a flight, aircraft, camera, weapon
or spawn operation. The `+0x118`, `+0x130`, `+0x180` and `+0x56` fields stay
offset-qualified until an owner type or dynamic observation identifies them.

## Next useful join

Follow the owner construction for the object passed in `r3` and the provenance
of the `DAT_826e4eb4 + 0x29c80` table entry. A dynamic capture would help name
the callback later, but it is not required for this static tranche.

## Validation

- Read-only Ghidra headless with `DumpRange.java`, `DecompileAt.java` and
  `ReferencesTo.java`.
- No GUI, Xenia, Wine, VNC or human session.
- AC6 native CTest: **41/41**.
- `git diff --check`: pass.
