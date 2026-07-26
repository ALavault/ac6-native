# AC6 event receiver reached from `0x820DB500`

Date: 2026-07-15

## Proven pointer chain

The input-condition routine entered through `0x820DB4F8 -> 0x820DB500`
submits its event record with the pointer stored at context offset `+0x04`:

```text
SWG context
  +0x04 -> 0x150-byte event receiver
             constructed only at 0x8237EDB0
             consumed by 0x8237E4C0
```

The context initialization fragment at `0x820DBD90..0x820DBF84` proves the
identity. It allocates `0x150` bytes, calls `0x8237EDB0`, and stores the return
value at context offset `+0x04`. `0x8237EDB0` has one direct call site,
`0x820DBF78`. This is stronger than matching field offsets in unrelated
functions.

The constructor initializes the receiver as follows:

| Receiver field | Proven initialization/use |
| --- | --- |
| `+0x00..+0x02` | inactive/initial-state flags (`0, 0, 1`) |
| `+0xDC` | interface passed by the SWG context; slot `+0x2C` is called after key transitions |
| `+0xE0` | context-owned interface; used by the receiver update path |
| `+0xE4` | optional callback |
| `+0xE8`, `+0xEC` | callback data/interface; the latter is notified by receiver methods |
| `+0xF4`, `+0xFC` | initially null runtime handles |
| `+0x104` | update quantum, initialized to `0x3C` by this caller |

## First receiver writes

`0x8237E4C0` is the first receiver reached by the 14-condition adapter cluster.
Its direct state delta is fully bounded:

- event type 1 writes two 32-bit values at receiver offsets `+0x124` and
  `+0x128`;
- event type 5 sets one bit in the receiver bitset beginning at `+0x12C` and
  records the signed-byte code at `+0x14C`;
- event type 6 clears the corresponding bit;
- types 5 and 6 call interface slot `+0x2C` through receiver field `+0xDC`,
  then call the runtime leaves `0x82381570` or `0x82381540` respectively.

No direct write targets a proven player-aircraft, velocity, camera transform,
or projection object. Scalar searches for `0x124`, `0x128`, and `0x14C` were
used only as a candidate generator. In the receiver's proven region, the
relevant instructions are the writes at `0x8237E570`, `0x8237E578`, and
`0x8237E564`; same-offset reads in unrelated objects are not pointer-identity
evidence.

## Exact static blocker after the receiver

The receiver's synchronous key-transition handoff is indirect. The object
placed at `+0xDC` is created in the same SWG initialization fragment and its
slot-like table begins at `0x8205A8EC`. Slot `+0x2C` contains `0x820D9A28`.
In the corrected Ghidra project this address is not a complete callable leaf:
it is an interior basic block of the larger `0x820D99C0..0x820D9B38` flow and
assumes inherited `r30/r31` state. The complete flow copies five `float4`
groups to `+0xAA0..+0xAEC` of a graphics command/state object and calls the
command-buffer emitter at `0x821E24D8`. Treating the interior entry as a normal
method of the 0x150-byte receiver would imply out-of-bounds writes and is
therefore not a sound type or ownership recovery.

Consequently, the static chain stops at a malformed/split indirect-dispatch
boundary, not at an aircraft or camera writer. Recovering the real entry and
calling convention for the `0x8205A8EC + 0x2C` dispatch, or obtaining a runtime
trace of the target and object identities, is required before the path can be
continued safely.

The graphics-state decomposition is recorded in
`FUNCTION_820D99C0_GPU_STATE_REPORT.md`. It proves that this candidate target
is not itself a gameplay-aircraft or camera writer.

The nearby `0x8237EED0` and `0x8237EF50` matrix helpers are not downstream
evidence: no receiver pointer handoff to them is established. Likewise,
functions elsewhere that happen to use offsets `+0x124`, `+0x128`, or `+0x14C`
remain excluded.

## Native boundary

No native helper was added in this tranche. The already tested
`function_820db500_conditions()` covers the last exact, portable delta before
event submission. Reproducing `0x8237E4C0` now would only clone generic runtime
input state and would not advance the player/camera reconstruction objective.

Raw instruction, constructor, reference, and dispatch evidence is retained in:

- `reports/logs/input-event-8237e4c0-receiver-trace.log`;
- `reports/logs/input-event-820db9a0-constructor-dump.log`.
