# AC6 `0x821B9048` input-consumer classification

Date: 2026-07-15

## Exact owner identity

The state-machine update beginning at `0x821B9048` is not an aircraft,
mission-selection, spawn, or gameplay-camera consumer. Its constructor at
`0x821B8EF0` installs the primary vtable at `0x82065634` and the secondary
vtable at `0x82065684`. The complete-object locator immediately before the
primary table leads through `0x82077720` to type descriptor `0x826EA89C`.
The descriptor's decorated type name begins at `0x826EA8A4` and is:

```text
.?AVCModeTaskTitleMovie@@
```

This proves the owner as `CModeTaskTitleMovie`. The misleading exported
function boundary at `0x821B9110` is an interior instruction of this update,
not a standalone constructor or consumer. In particular, `0x821B9110` masks
bit 0 from the logical-input word loaded by the surrounding block.

## Bounded state delta

Initialization at `0x821B8FC0` creates an auxiliary object through the global
service at `0x82671308` (virtual slot `+0x14`, arguments `2, 0, 0`), stores it
at owner offset `+0x270`, sets state `+0x0C` to 0, and sets countdown `+0x48`
to 3.

The update has four bounded phases:

| Owner state | Exact behavior |
| --- | --- |
| `0` | If `+0x270` is absent, call owner virtual `+0x48` and set state to `2`. Otherwise, auxiliary predicate slot `+0x10` gates auxiliary slot `+0x04` and the transition to state `1`. |
| `1` | Logical-input bits `0` or `4` call auxiliary slot `+0x08`. Auxiliary predicate slot `+0x18` then gates owner virtual `+0x48` and the transition to state `2`. |
| `2` | Decrement `+0x48`. At expiry, release `+0x270` through service slot `+0x18`, write `{1, 3}` to global flow fields `DAT_8293BA10 + {0x18, 0x1C}`, and notify the optional object at global `+0x08` through virtual slot `+0x20` with argument `3`. |
| `>=3` | No state mutation in this update. |

The same `{1, 3}` global-flow write/notification pattern occurs in other mode
task updates, including the flow beginning at `0x821ADE00`. It is therefore a
generic mode-task handoff, not evidence of an aircraft spawn or camera write.

## Typed blocker for the gameplay-input route

No instruction in `0x821B8EF0..0x821B91D0` obtains or writes an object whose
identity is tied to an active aircraft, mission actor, spawn transform,
gameplay camera, velocity, or weapon state. The only exact visible effect is
title-movie control followed by a generic global mode transition. Promoting
the logical bits to flight or mission actions from this path would be a type
error.

The earlier apparent derived-class tables around `0x82066ED4` came from
accepting damaged exported boundaries near `0x821B9110` as constructors. Raw
instruction flow corrects this: constructors in the `0x821BC5E0..0x821BC708`
range install those tables, while `0x821B9110` itself is inside the title-movie
update. Those tables must not be used to type the `0x821B9048` owner.

No native helper and no re-agent output were added. Reproducing a title-movie
skip state machine would not advance the active-aircraft/camera/spawn parity
gate. The next valid static pivot must start from an independently typed
gameplay mode, actor owner, or gameplay-camera owner and then trace backward
to logical input by pointer identity.

Raw instruction, vtable, RTTI, and comparison evidence is retained in:

- `reports/logs/function-821b9048-state-owner.log`;
- `reports/logs/function-821b9048-flow-tail.log`.

The unchanged native product still passes all 15 tests, including the SDL
scene-shell smoke, under both GCC 15.2 and Clang 21.1 with
AddressSanitizer/UndefinedBehaviorSanitizer.
