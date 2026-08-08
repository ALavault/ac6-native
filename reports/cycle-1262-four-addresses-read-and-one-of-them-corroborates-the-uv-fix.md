# Cycle 1262 — four addresses read, and one of them corroborates the UV fix

## Qualification

`ghidra-projects-xenon/ac6-xenon`; `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Why

Cycle 1261 listed ten addresses the `ndxr_container` behaviour declares and no
product source implements, and said of four of them: *"not re-read in this
cycle; they are listed as uncited, not characterised."* Deciding what to do with
an address you have not read is guessing, and cycle 1261's two honest routes —
implement them, or split the behaviour — both need to know what they are.

## Established

### `0x821DE898` — the vertex declaration allocator, and it counts 12-byte elements

```
821de8b8  lhz     r9,0x0(r30)
821de8bc  b       0x821de8cc
821de8c0  addi    r10,r10,0xc        ; advance one element: TWELVE bytes
821de8c4  addi    r11,r11,0x1        ; count it
821de8c8  lhz     r9,0x0(r10)
821de8cc  cmplwi  cr6,r9,0xff        ; the terminator: stream 0xFF
821de8d0  bne     cr6,0x821de8c0
821de8d4  mulli   r11,r11,0xc        ; count * 12
821de8d8  addi    r3,r11,0x38        ; plus a 0x38 header
821de8dc  lis     r4,0x2480
821de8e0  bl      0x821d7458         ; allocate
821de8e4  or.     r31,r3,r3
821de8e8  bne     0x821de8f4
821de8ec  li      r3,0x0             ; allocation failed -> null
821de8f4  ...
821de8fc  bl      0x821de7a8
```

It walks a `D3DVERTEXELEMENT9` array to `D3DDECL_END` — whose `Stream` field is
`0xFF` — counting elements, then allocates `0x38 + 12·N`.

**This corroborates the correction cycle 1233 made to the product.** That cycle
moved the UV read from offset 16/20 to 20/24, and the whole change rested on
Xbox 360's `D3DVERTEXELEMENT9` being **12 bytes** rather than the 8 of the PC
struct. The evidence then was the element lists the renderer's own builder walks
at `0x82345100`. This is a second, independent instruction: a different function,
in a different subsystem, striding the same array by `0xc` and sizing an
allocation by `12·N`. A stride can be misread once; two functions in different
places do not agree by accident.

### `0x821DE790` — a device-state setter with a dirty bit

```
821de790  stw   r4,0x2e24(r3)
821de794  ld    r11,0x10(r3)
821de798  oris  r11,r11,0x8         ; set bit 0x0008_0000 of the 64-bit word
821de79c  std   r11,0x10(r3)
821de7a0  blr
```

Stores a pointer at `device+0x2E24` and raises one bit in a 64-bit dirty mask at
`device+0x10`. Read together with the allocator above, which ends in
`bl 0x821de7a8` — the function immediately following it — the pair is
**declaration creation and binding**, on the D3D device rather than in the
container.

### `0x8233E6B0` — an initialiser that reaches the stride builder

```
8233e6c4  addi  r29,r30,0x8
8233e6cc  bl    0x823d6a7c
8233e6d4  addi  r3,r30,0x28
8233e6d8  bl    0x82346fc0
8233e6e4  bl    0x82345100          ; on this+0x170
```

Three sub-objects wired at `+0x08`, `+0x28` and `+0x170`, the last through
`0x82345100` — the stride function already established as the NDXR chain's last
stage. This one is genuinely in the container's neighbourhood.

### `0x821FC070` — a dispatcher on a value from `0x821E4078`

```
821fc08c  bl    0x821e4078
821fc090  cmpwi cr6,r3,0x6
821fc094  bgt   cr6,0x821fc0e0
821fc098  beq   cr6,0x821fc0d0
821fc09c  cmpwi cr6,r3,0x1
821fc0a4  cmpwi cr6,r3,0x2
```

A switch over at least `{1, 2, 6, >6}` on a queried kind. **What kind was not
established** — `0x821E4078` was not read.

## What this changes for the open decision

Cycle 1261 offered two routes for the ten uncited addresses and warned that the
attribution "the Type decoder belongs with `texture_decode`, the registry hops
with a registry behaviour" was **plausible and unverified**. Two of the four are
now read, and they point somewhere neither route anticipated: `0x821DE790` and
`0x821DE898` are **D3D device** code — vertex declaration creation and binding —
not container parsing and not texture decoding. If the behaviours are split,
those two belong with the geometry path, beside the element-layout derivation
they corroborate.

`0x8233E6B0` belongs with the container. `0x821FC070` is still unattributed.

## Not established

- **`0x821E4078`**, and therefore what `0x821FC070` dispatches on.
- The `0x38` header of the declaration object, and the meaning of the `0x2480`
  allocation tag.
- Which bit `oris r11,r11,0x8` names in the device's dirty mask — the bit is
  read, its meaning is not.
- Nothing here was turned into product code, so the ten-address gap in the v4
  contract is **unchanged at ten**. This cycle bought information, not a gate.
