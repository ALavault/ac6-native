# Cycle 1310 — the input contract closes on a memcpy

## Qualification

- Ghidra project `ghidra-projects/ace-combat-6`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No game code ran.
- No product C++ changed.

## The six entry points are locked wrappers

Every one has the same body: enter a critical section on `svc+0x04`, call one
function on the `DriverContext` at `svc+0x24`, leave.

| thunk | wrapper | context call | note |
|---|---|---|---|
| `0x82337E28` | `0x82343838` | `0x82343A30` | returns a flag |
| `0x82337E40` | `0x82343888` | `0x82343A68` | **takes `f1`** |
| `0x82337E58` | `0x823438E0` | `0x82343A90` | — |
| `0x82337E18` | `0x82343928` | `0x82343AD0` | argument is the constant 7 |
| `0x82337ED0` | `0x823437F0` | `0x82343978` | first call initialises the lock |

`0x823D6A7C` / `0x823D6A8C` are enter/leave and `0x823D6A9C` is the one-time
init, guarded by `[svc+0x08+0x1C]`.

## The context is an array of five drivers

```
82343a30 or      r11,r4,r4
82343a34 cmplwi  cr6,r11,0x5
82343a38 bge     cr6,0x82343a5c     ; index >= 5 -> out[+0x04] = -1
82343a3c addi    r11,r11,0x1
82343a44 rlwinm  r11,r11,0x2,0x0,0x1d
82343a48 lwzx    r3,r11,r3          ; driver = ((u32*)context)[index + 1]
82343a4c lwz     r11,0x0(r3)
82343a50 lwz     r11,0xc(r11)       ; vtable slot +0x0C
82343a58 bctr
```

`context+0x04` holds **five driver pointers**, bound-checked at 5 — the four
`DriverController`s cycle 1308 found embedded at `+0x18` stride `0x88`, plus the
fifth object at `+0x238`. The `+1` is the context's own vtable at `+0x00`.

`0x82343A68` is the float one and does **not** dispatch virtually: it takes
`driver[index]`, returns if null, and tail-calls the fixed `0x8234D2B0` with
`f1` intact. Called three times consecutively from `0x821CAA50` at
`821cb5b0`/`5c0`/`5d0`.

## And the accessor is a memcpy

`DriverController` vtable slot `+0x0C` is `0x8234D0A0`, five instructions:

```
8234d0a0 or   r11,r3,r3
8234d0a4 or   r3,r4,r4
8234d0a8 addi r4,r11,0x4
8234d0ac li   r5,0x40
8234d0b0 b    0x82382f70      ; memcpy(out, this + 0x04, 0x40)
```

**`memcpy(out, device + 0x04, 0x40)`** — and that range is the derived block
exactly. `+0x04` through `+0x43` covers the connection state, the button edges,
the eight split axis halves and the four raw thumbs, and stops **one byte before
`+0x44`**, where the `XINPUT_STATE` begins.

The copy boundary falls precisely where cycle 1307's derived fields end and the
kernel structure starts. Nothing was fitted to make that true; it is what the
`li r5,0x40` says, and it corroborates the whole layout from a direction the
layout was not derived from.

## Which explains the return value

`0x82343838` ends:

```
82343874 lwz    r11,0x4(r30)
82343878 cntlzw r11,r11
8234387c rlwinm r3,r11,0x1b,0x1f,0x1f    ; r3 = ([out+0x04] == 0)
```

`out+0x04` is `device+0x08` after the copy — the connection state, which
`0x8234D510` sets to `0` on a successful poll and to `-1` or `-2` otherwise. So
**the API returns "this pad is connected and its snapshot is valid"**, and the
caller reads the fields directly out of its own copy.

## The contract, end to end

```
0x82337E28(index, out)                 public read
  0x82343838                           lock svc+0x04
    0x82343A30                         driver = context[index+1], index < 5
      0x8234D0A0                       memcpy(out, device+0x04, 0x40)
  returns [out+0x04] == 0              connected
```

with the snapshot laid out as

| in `out` | in device | meaning |
|---|---|---|
| `+0x04` | `+0x08` | connection state, 0 = valid |
| `+0x10` | `+0x14` | buttons pressed this frame |
| `+0x14` | `+0x18` | buttons released this frame |
| `+0x18` | `+0x1C` | buttons currently held |
| `+0x1C` | `+0x20` | complement of held |
| `+0x24`…`+0x32` | `+0x28`…`+0x36` | eight axis halves, all non-negative |
| `+0x38`…`+0x3E` | `+0x3C`…`+0x42` | the four raw thumbs |

That is portable, scalar, and derived instruction by instruction from the kernel
boundary. It is the first thing this session has that is ready to be written as
native source with a derivation citing its addresses.

## Also read

`0x8234D0B8` is the capability mapper the reconnect path uses on the caps byte
`device+0x59`: `1→1, 2→4, 3→2, 4→3, 5→5`, anything else `0`. Its result is stored
at `device+0x10`.

## Not established

- What `0x82343A90` and `0x82343AD0` do. The constant `7` passed to the latter
  is not an index — `0x82343A30`'s bound is 5 — so it is something else.
- What `0x8234D2B0` does with its float. Rumble is the obvious guess and is
  recorded as a guess.
- What `0x821CAA50` does with the snapshot. 744 instructions, still unread.

## Gates

```
mission01_final_gate=audit-valid JF=pass open=none
ctest: 100% tests passed, 0 failed out of 27
contract_addresses=pass cited=103 supported=103 unsupported=0
tools/tests: Ran 72 tests, OK
```

## Next

Write it. The chain from `XamInputGetState` to the snapshot is complete and every
step is a read instruction, so the native side can carry `retail_input_snapshot`
as a behaviour with a derivation citing `0x823911C0`, `0x8234D3F0`, `0x8234D510`,
`0x8234D110`, `0x8234D378`, `0x8234D0A0`, `0x82343A30` and the table at
`0x8201250C`. That is a contract entry, which is what thirteen cycles of
instrument work did not produce.
