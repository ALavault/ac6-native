# Cycle 1196 — the `0x200` NDXR header is four section extents, and the control discriminates

## The path, end to end

Cycle 1195 established that all 537 retail NDXR files carry code `0x200`, which
`0x8234CB58` routes to `0x82350CA0` or `0x82350C50`. Those two are the same
function but for one constant:

```
82350c70/82350cc0  stw r29,0x8(r31)    ; this+0x08 = size
82350c84/82350cd4  stw r10,0x98(r31)   ; this+0x98 = 0
82350c88/82350cd8  stw r10,0x9c(r31)   ; this+0x9C = 0
82350c8c  addi r11,r11,0x283c -> stw   ; vtable 0x8201283C
82350cdc  addi r11,r11,0x28b4 -> stw   ; vtable 0x820128B4
82350c90/82350ce0  bl 0x82352b88       ; the load sequencer
```

`0x82352B88` runs vtable slots `+0x18`, `+0x10`, `+0x20`. Reading vtable
`0x820128B4` directly:

| slot | target |
|---|---|
| `+0x10` | `0x8234D098` |
| `+0x18` | **`0x82350F08`** |
| `+0x1C` | `0x82350FC8` |
| `+0x20` | `0x8234D098` |

and `0x8234D098` is a single `blr`. **Two of the three load stages are stubs**, so
the whole parse is `0x82350F08`, and the teardown twin `0x82352BE8` reaches
`0x82350FC8` at slot `+0x1C`.

## The parse

```
82350f20  stw r3,0xc(r31)     ; this+0x0C = buf
82350f24  lwz r11,0x10(r3)
82350f28  add r11,r11,r3
82350f2c  addi r11,r11,0x30
82350f30  stw r11,0x84(r31)   ; this+0x84 = buf + [buf+0x10] + 0x30
82350f34  lwz r10,0x14(r3)
82350f38  add r11,r10,r11
82350f3c  stw r11,0x88(r31)   ; this+0x88 = this+0x84 + [buf+0x14]
82350f40  lwz r10,0x18(r3)
82350f44  lwz r9,0x1c(r3)
82350f48  add r11,r10,r11
82350f50  beq cr6,0x82350f64  ; if [buf+0x1C] == 0 ...
82350f54  stw r11,0x8c(r31)   ; this+0x8C = this+0x88 + [buf+0x18]
82350f5c  add r11,r10,r11     ;   advance by [buf+0x1C]
82350f68  stw r10,0x8c(r31)   ; ... else this+0x8C = 0
82350f70  stw r11,0x90(r31)   ; this+0x90 = the end
82350f78  stw r10,0x20(r31)   ; this+0x20 = 1
82350f7c  bl 0x823556e0       ; sub-parse on this+0x10
```

So the `0x200` header carries **four consecutive section extents at `+0x10`,
`+0x14`, `+0x18`, `+0x1C`**, over a body whose base is `buf + [buf+0x10] + 0x30`,
and the object publishes the four boundaries at `+0x84`, `+0x88`, `+0x8C`,
`+0x90`.

## The control, and the one it had to beat

In-bounds alone proves nothing here, and I checked that before trusting it:
`base + [+0x14] + [+0x18] + [+0x1C] <= filesize` holds for **537 of 537 with the
`+0x30` and 537 of 537 without it**. A test that cannot fail is not a control, and
this one cannot discriminate the constant the derivation turns on.

The test that can: **what lies at the computed end.**

| hypothesis | 16 printable bytes at the end |
|---|---|
| derived, `+ 0x30` | **537 of 537** |
| rival, no `+0x30` | **0 of 537** |

```
sample idx_0119/021_FHM/014_FHM/000_NDXR.ndxr
  derived end 0x1dc0 of 0x2449  ->  b'mapparts_m01_x_p1_sh_0032_0_O_OBJ\x00\x00...'
  rival   end 0x1d90            ->  b'\x00\x00<\x00\xff\xff\xff\xff\x00\x00\x00\x00\xc1\x9c?\xd1...'
```

Unanimous both ways. The `0x30` is load-bearing and the name table begins exactly
where the derivation says the body ends.

## A third dead branch, caught before it was written up

`[buf+0x1C]` is **zero in 537 of 537**. So `this+0x8C` is always 0 for retail
content, and the tail of `0x82350F08`:

```
82350f80  lwz r11,0x8c(r31)
82350f88  beq cr6,0x82350fb0   ; this+0x8C == 0 -> skip everything below
...
82350fa4  rlwinm r3,r11,0x3,0x0,0x1c
82350fa8  bl 0x82333510        ; allocate this+0x98 * 8
82350fac  stw r3,0x9c(r31)
```

**never runs on any file this game ships.** That is the third dead path in five
cycles — 1193, 1195, and this one — and the first found *before* being written up
as behaviour, because the census existed first. The fourth section extent is real
in the format and unused in the content.

## Not established, stated plainly

- What `0x823556E0` does with `this+0x10`. It is the remaining call and it is
  unread.
- What the three live sections contain. The extents are derived; the contents are
  not, and the name table past `this+0x90` is a measurement.
- `this+0x98` — zeroed by both constructors and read only on the dead branch, so
  nothing establishes what it counts.
- Whether `0x8201283C` (the `0x82350C50` twin) has the same slot layout. I read
  `0x820128B4` only; the two differ by the dispatcher's flag bit 2, and I did not
  establish what that bit selects.

## Decided rather than asked

Nothing is written into the product. Four offsets and a base constant are a
header, not a loader, and `0x823556E0` is between here and anything drawable.

The discriminating control is worth keeping as the standing test for this
structure, alongside cycle 1195's: **`[+0x04] == filesize`, `[+0x08] == 0x200`,
and printable bytes at `[+0x10] + 0x30 + [+0x14] + [+0x18] + [+0x1C]`, all on 537
of 537.**

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```
