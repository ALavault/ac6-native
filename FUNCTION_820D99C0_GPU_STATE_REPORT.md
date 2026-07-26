# AC6 `0x820D99C0..0x820D9B38` state-bank classification

Date: 2026-07-15

## Complete block

The complete prologue begins at `0x820D99C0`, eight bytes before the requested
`0x820D99C8` save instruction. The routine takes its destination in `r3` and a
source record in `r4`, preserving them as `r31` and `r30`.

The source record is bounded by direct accesses:

- `+0x00..+0x4C`: five consecutive groups of four 32-bit floats;
- `+0x78`: a positive count checked to be less than `0x100000`.

The destination writes are:

- a global state word to `+0x2E24` before the table's interior entry;
- dirty-state bits in the 64-bit words at `+0x00` and `+0x10`;
- the five source `float4` groups to `+0xAA0..+0xAEC`;
- a call to `0x821E24D8` with `r4 = 13`, `r5 = 0`, and `r6` equal to the
  source count.

The copy is exact and does not perform matrix multiplication, integration,
camera projection, or an entity-relative write.

## Destination identity

The destination is a graphics command/state object, not an aircraft, player,
or camera object. This follows from the callee rather than from field-name
guessing. `0x821E24D8`:

- consumes the destination's dirty masks at `+0x00..+0x20`;
- updates command-stream cursors at `+0x30/+0x38`;
- emits state blocks for hardware register ranges `0x2000`, `0x2100`,
  `0x2180`, `0x2200`, and `0x2280`;
- writes packet words including `0x2102` and advances a command-buffer pointer;
- uses destination shadow-state areas around `+0x2880..+0x2964`.

The repeated neighboring routines at `0x820D8B40..0x820D99B8` copy related
`float4` groups into the same `+0xAA0` bank and update the same dirty masks.
Therefore `+0xAA0..+0xAEC` is a five-register shadow bank feeding the graphics
command path. It may contain transforms supplied by upstream rendering code,
but it is not itself an owned gameplay transform and does not establish the
identity of any contributing player or camera.

## Relation to `0x8205A8EC + 0x2C`

The table word at `0x8205A918` is `0x820D9A28`, an interior specialization of
this graphics-state routine. Entering at `0x820D9A28` skips the prologue and
the setup of `r30/r31`; it assumes both are already valid. The receiver call at
`0x8237E4C0`, however, supplies only its `+0xDC` interface in `r3` and does not
establish the source record in `r30`. The static project therefore still does
not prove that this interior address is the runtime target of that callback.

This decomposition narrows the ambiguity: if the table value is used exactly
as recovered, it points into graphics shadow-state submission, not player or
camera mutation. A runtime table/object trace is required to determine whether
the `+0xDC` object's table is replaced or adjusted before key transitions.

## Player/camera boundary

No write in `0x820D99C0..0x820D9B38` is connected by object identity to a
player, aircraft, velocity, or camera structure. The first writes are to a
graphics command/state cache. Equal-sized `float4` data is insufficient to
promote this to a camera matrix claim.

No native helper and no re-agent output were added. A helper for this copy
would reproduce a platform graphics shadow cache without advancing the native
gameplay reconstruction, and the interior entry is not a healthy standalone
leaf for re-agent.

Instruction, table, and command-buffer evidence is retained in:

- `reports/logs/function-820d99c8-aa0-trace.log`;
- `reports/logs/function-821e24d8-flush-tail.log`.
