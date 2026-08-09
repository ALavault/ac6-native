# Cycle 1283 — the persistent camera, and a FOV that is not in the XEX

## Qualification

Delegated investigation; **the load-bearing claims were re-read here**, decoded
directly from `analysis-input/ACE6_X360.exe`. `default.xex` SHA-256
`acc302c1…11bcde`. **No oracle pass was spent.** No product code changed.

## Established — `manager+0x198` is an embedded object, not a pointer

```
82263f78  lwz  r7,0x198(r31)     ; the vptr
82263f88  addi r3,r31,0x198      ; this = &manager[0x198] -- the ADDRESS
82263fcc  lwz  r7,0x4(r7)
8226400c  bctrl
```

Its vtable is `0x82054D9C`, and the chain resolves in full — locator
`0x8206D7FC`, type descriptor `0x8268F728`, name **`.?AVCGaObjDesc@galib@@`**.
The manager embeds seven such objects, each named from its own descriptor:
`CAce6CameraManager` at `+0x000`, `CGaCamera` at `+0x0F0`, `CGaLocator` at
`+0x140` and `+0x2B0`, `CGaObjDesc` at `+0x198` and `+0x1A4`, and cycle 1277's
`CAce6ArmsCamera` at `+0x550`.

**Slot `+0x04` computes nothing.** Decoded from the image, four words:

```
82093860  90830004   stw  r4,0x4(r3)      ; the object pointer -> manager+0x19C
82093864  816400b0   lwz  r11,0xb0(r4)
82093868  91630008   stw  r11,0x8(r3)     ; its serial -> manager+0x1A0
8209386c  4e800020   blr
```

It is a **bind**, and the object bound is the player: `S = *(G + 0x29FC8)` is the
`CAce6ObjManager` singleton and `X = *(S + 0x1008)` its player slot.

## Established — the camera formula

Mode 2's handler `0x82260930`, read in full by the delegated work:

**`camPos = playerPos + R_player · record.offset`**, basis = the player's own
locator rows (`player+0x20/0x30/0x40`, position `player+0x50`), with two
rotations from `manager+0x3A0/+0x3A4` applied after.

The FOV is **eased, not set**:

```
82260958  lfs   f13,0x378(r31)       ; the target FOV
822609ac  lfs   f13,0x37c(r31)       ; the alternate, above player+0x874 > 0.3
822609bc  lfs   f12,0x368(r31)       ; the rate, default 6.5 per second
822609c8  fmadds f13,f13,f31,f0      ; f31 = dt
822609cc  stfs  f13,0xd4(r31)
```

so `8226401C stfs f0,0xd4(r31)` — the store cycle 1277 found — is the **snap**
that this smoother then takes over.

## The finding that matters for the product

**`manager+0x378` is not a constant. It is field `+0x68` of a 144-byte record
copied every frame from a runtime-loaded table.**

`0x8225C4A0(table, aircraft, mode)` maps mode 1→0, 2→1, 3→2 and returns
`table+0x4 + 0x90·(aircraft·3 + viewIndex)`; `0x8225C510` copies exactly `0x90`
bytes into `manager+0x310`. The table lives at `[G+0x31070]`.

The `46.0°` this campaign has quoted — `[0x82007F64] = 0.8028514385` rad, which
is 46·π/180 exactly — is written by the field initialiser `0x8225CCF0` and is
**only the fallback**. Verified here, along with the ease rate
`[0x82069F28] = 6.5`.

**So Mission 01's actual field of view is not in the XEX.** It is per-aircraft
and per-view data in a loaded resource. Any native camera that hard-codes 46°
is choosing a number retail only uses before the table arrives — which is worth
knowing before the product grows a camera, and is why cycle 1273 was right to
import the far plane and not the FOV.

## Corrections

- **My own brief** told this investigation that table `0x822646F0` belongs to the
  switch at `0x82263BAC`. It belongs to `0x822646EC`, as cycle 1278 had already
  measured with `count_indirect_branches.py` — I wrote the brief before running
  my own tool on the question it asked about.
- The delegated work found the **three** `bctr` independently, and identified the
  third as the compute stage: mode 1 → `0x8225E508`, 2 → `0x82260930`,
  3 → `0x82260B98`.

## Not established

- **Which mode Mission 01 opens in.** The gameplay mode is `player+0x880`, one of
  1, 2 or 3, written only by `0x82226D80` — from the view-cycle button, the
  co-op swap, and a re-apply path. No read of a saved option into `+0x880` was
  found, so the opening view is undetermined.
- **The table's contents**, and therefore the real FOV and offset. Runtime
  resource.
- `0x8225E508`, mode 1's handler, a larger VMX128 function on the same inputs.
- The manager's own base: users load `*(*(0x826E4EB4) + 0x2F9A0)` while the
  constructor installs at `ctorThis + 0x2F9B0`. Those agree only if `+0x2F9A0`
  holds `+0x2F9B0`, and the store that would prove it was not found.

## An instrument note worth keeping

The delegated work found `0x8225B7C8` — an FSM state referenced **only as data**
— with `tools/find_materialised_address.py`, not with a branch scan. That tool
was written two cycles ago to correct a thirty-cycle false negative, and it has
now found a second thing no other scan here could see.
