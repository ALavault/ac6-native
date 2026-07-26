# AC6 analog and chord receiver audit after `0x821CE088`

Date: 2026-07-15

## Result

The retail chain is closed from raw controller fields through canonical input
and the logical analog mapper, but not from those logical outputs to a proven
player-aircraft or camera receiver. No native pitch, roll, camera, throttle,
yaw, autopilot, or High-G mapping is added by this tranche.

This is an evidence boundary, not an assertion that the controls do not exist.
The retail manual supplies their intended physical meanings, but those names
remain documentary hypotheses until a code path reaches an identified flight
or camera field.

## Proven raw-to-canonical sources

The normal-device branch in `0x821CE25C..0x821CE8F0` combines or scales the
following raw fields and stores them in the canonical device state:

| Raw field(s) | Canonical source bit | Canonical float offset |
| --- | ---: | ---: |
| halfwords `+0x84`, `+0x86` | `0x20000` | `+0x50` |
| halfwords `+0x88`, `+0x8A` | `0x10000` | `+0x4C` |
| halfwords `+0x8C`, `+0x8E` | `0x80000` | `+0x58` |
| halfwords `+0x90`, `+0x92` | `0x40000` | `+0x54` |
| byte `+0x94` | `0x4000` | `+0x44` |
| byte `+0x96` | `0x8000` | `+0x48` |

The two byte sources also set their corresponding canonical digital bits above
the retail threshold `0x1E`. The alternate type-3 device path synthesizes
additional buttons and is deliberately excluded from this normal-controller
table.

The default gameplay context initialized by `0x821BE268` maps these sources to
logical slots 10 through 17 in this exact order:

`0x10000, 0x20000, 0x40000, 0x80000, 0x100, 0x200, 0x4000, 0x8000`.

That proves source and slot identity. It does not prove that slots 10--13 are
pitch, roll, or camera, nor that slots 16--17 are throttle controls.

## Proven same-frame order

The update at `0x821CDF08` first polls and ingests the raw device state. It then
calls `0x822153F8` for five contexts in this order:

1. `DAT_826E4EB4 + 0x2458C`;
2. `DAT_826E4EB4 + 0x256F0`;
3. `DAT_826E4EB4 + 0x26854` (the default gameplay context);
4. `DAT_826E4EB4 + 0x279B8`;
5. the manager-local context at `+0x20`.

Inside that update, `0x82215418` clears the current analog arrays, advances the
digital previous/current state, calls the digital remapper at `0x82215140`,
then calls the analog remapper at `0x82215210` for each active device before
edge/repeat processing at `0x82214F88`.

`0x82215210` checks the analog-enabled logical-slot mask at context offset
`+0x88`, resolves the canonical source bit, reads its canonical float, applies
deadzone/threshold and optional inversion from `+0x8C`, then writes the logical
analog sample and presence bit. Thus raw ingestion precedes logical mapping in
the same frame.

## Receiver audit and blocked semantic edge

The only direct retail read currently tied to this gameplay context's analog
outputs is the generic adapter entered through `0x820DB4F8 -> 0x820DB500`.
Its slice at `0x820DB544..0x820DB674` can read logical analog slot 10, apply a
directional positive threshold, and augment digital current/just-pressed/repeat
conditions before submitting an event through `0x8237E4C0`. It neither writes
an aircraft/camera object nor identifies the analog axis.

No direct reader was recovered for logical analog slots 11--17. Exact searches
for combined logical chord masks produced no consumer:

- `0xC000` (logical slots 14 and 15, whose physical defaults are LB and RB);
- `0x30000` (logical slots 16 and 17, whose physical defaults are the two
  trigger sources).

The `0x76D4` scalar hits at `0x82222508`, `0x82223AB0`, and `0x82223B88` are
unrelated constructors loading a vtable address; they are not reads of
`DAT_826E4EB4 + 0x276D4`. Likewise, `0x82214F60` returns an unrelated global
used by debug/data handlers and is not an input accessor.

The likely remaining handoff is indirect: either a data-driven event receiver
behind `0x8237E4C0`, or a pointer/virtual accessor whose base is passed rather
than reconstructed as the global gameplay-context address. Until that handoff
ends in a proven flight-control or camera write, manual labels are not safe
native function or field names.

## Native boundary

The existing native module retains exact digital button transforms, the retail
default binding table, logical digital mapping, edge state, and the bounded
generic analog-threshold condition helper. No analog-axis or chord API was
added, because the required `raw -> canonical -> logical -> receiver` proof is
incomplete.
