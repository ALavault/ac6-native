# AC6 logical-input consumer effects around `0x820DB500`

Date: 2026-07-15

## Four-consumer classification

The corrected XEX project separates the four previously opaque consumers as
follows. Address names remain intentional until the surrounding systems are
recovered.

| Consumer | Caller or ownership evidence | Measurable effect |
| --- | --- | --- |
| `0x8214C038` | entry `0xB0` bytes into the vtable at `0x8205DAEC`; its locator `0x8205DAE8 -> 0x82074164` resolves to RTTI type descriptor `0x826E8D0C`, whose name is `CSelectAircraftManager` | logical just-pressed bit `0x10` can cycle one per-entry word at object offset `0x8D54 + 4*i` through `0 -> 1`, `1 -> 2`, and `2 -> 1`; no gameplay-aircraft transform is written |
| `0x821B3870` | virtual-table entry at `0x820653B0`; the method is also listed in the function directory at `0x8207EA98` | always calls virtual slot `0x20` on subobject `+0x1C`; logical just-pressed bit `0x10` additionally calls slot `0x54` on subobject `+0x268`; no direct memory write |
| `0x821B9110` | fragment of the state-machine update beginning at `0x821B9048/0x821B9050`; directory entries `0x8207ED80..0x8207ED98` cover the split blocks | logical bits `0` or `4` call slot `0x08` on object `+0x270`; a later virtual predicate can cause a virtual call on the owner and write owner state `+0x0C = 2` |
| `0x820DB578` | interior instruction of the routine entered through thunk `0x820DB4F8 -> 0x820DB500`; 14 direct thunk call sites occur in `0x820DB2C8..0x820DB4B4` | combines digital action conditions with an analog threshold and submits stack event records through `0x8237E4C0`; it does not directly write an aircraft, player, or camera object |

The `0x820DB2C8`, `0x820DB368`, and `0x820DB408` caller groups repeatedly
invoke the thunk with logical action indices and keyboard-like values such as
`0x77`, `0x73`, `0x6B`, and `0x6C`. This is consistent with a generic input
event adapter, not with a flight-state integrator.

## Shortest proven aircraft-side identity

`0x8214C038` reaches an aircraft-related structure most quickly, but RTTI
proves that structure to be the selection/presentation manager. The adjacent
camera class is distinct: `CSelectAircraftCamera` has type descriptor
`0x826E8C74`, locator `0x82074088`, and vtable `0x8205DA0C`. No call or field
handoff from `0x8214C038` to that camera vtable has yet been established.

The result therefore closes an object identity without converting it into a
gameplay claim: this is selected-aircraft presentation state, not the player
aircraft in flight.

## Analog delta at `0x820DB544..0x820DB674`

The routine first tests one digital logical-action bit against:

- current actions at `DAT_826E4EB4 + 0x27698`;
- just-pressed actions at `+0x276A0`;
- just-released actions at `+0x276A4`;
- initial/repeat pulses at `+0x276A8`.

For action indices in the analog-assisted range, it selects another logical
bit and loads its float from the analog array beginning at `+0x276AC`. If the
selected sample multiplied by its direction scale is strictly greater than
zero, presence of that analog logical bit is ORed into the current,
just-pressed, and repeat conditions. The just-released condition is not
augmented.

This is a measurable state delta: with the analog bit present, a positive
directed sample changes those three conditions from false to true; zero or the
opposite direction does not. The damaged switch table at `0x820D7FD8` does not
yet permit an exact action-6..9 to analog-slot/direction mapping, so no axis is
named and the bounded native helper takes both bits and the scale explicitly.

## Native and re-agent boundary

`function_820db500_conditions()` reproduces only the condition-building slice
and has tests for zero, positive, and opposite-direction samples. It does not
emit events or mutate the scene.

Re-agent was run on the smallest complete leaf, `0x821B3870`, against the
corrected project with one review round. It returned `PASS` and objective
`PASS`; its generated code is retained only as analysis evidence and is not
used to assign gameplay names. The run log is
`reports/logs/re-agent-821b3870-smallest-leaf.log`.

Instruction, caller, RTTI, and memory-effect evidence is retained in
`reports/logs/input-consumers-callers-effects.log`.

No native flight or camera command is connected by this tranche.
