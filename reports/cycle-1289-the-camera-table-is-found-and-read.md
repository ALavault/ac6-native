# Cycle 1289 — the camera table is found, and read

## Qualification

Delegated investigation; **the load-bearing values were re-verified here** from
bit patterns and arithmetic. `default.xex` SHA-256 `acc302c1…11bcde`. Retail
bytes were extracted outside the repository and none were written into the tree.
**No oracle pass was spent.** No product code changed.

## Established — where the numbers live

`G + 0x31070` is **not a pointer slot**. It is a 16-byte struct inlined in the
global object holding **two** parallel arrays:

| field | meaning |
|---|---|
| `+0x00` / `+0x08` | array A base and count, stride **192** |
| `+0x04` / `+0x0C` | array B base and count, stride **144** — the camera records |

So cycle 1283's "table" argument is `&(G+0x31070)`.

It is filled **once, at boot**, inside `0x821D5EF8`, from FHM children **35** and
**36** of a container at `R + 0x15AA78`, via `0x8225C450` and `0x8225C478` —
each with exactly one caller. The setters divide the entry size by the stride
with a reciprocal multiply, which is what pins the strides: `0xAAAAAAAB >> 7`
for 192, `0x38E38E39 >> 5` for 144.

The container is **`DATA.TBL` entry 1**, read through the FHM header parser
`0x82234C18` — count at `+0x10`, offsets at `+0x14`, sizes after them, which is
byte for byte what `tools/ac6_fhm.py` does.

## The controls, and they could have failed

- **Divisibility forces the assignment.** Child 36 is 6,480 bytes:
  `6480 / 144 = 45` exactly, and `6480 / 192 = 33.75` — **not an integer**.
  Child 35 is 5,184 = `27 × 192`. Neither could be the other's table.
- **The container has 55 children**, so indices 35 and 36 exist. Entry 9's FHM
  has 26; the wrong container would have failed here.
- **Every `record+0x68` lies in 0.349…0.873 rad** — 20° to 50°. Nothing outside
  a plausible vertical FOV.
- **The record and the fallback agree field for field.** Every record carries
  `0.1745329, 0.1745329, 0.1, 0.1` at `+0x70…+0x7C` and `3.0` at `+0x80` — the
  same constants `0x8225CCF0` writes into the corresponding manager fields.

## The numbers

**45 records = 15 groups × 3 views**, view index = mode − 1, which is exactly
what `0x8225C4A0` computes.

| view | offset `+0x00` | ease `+0x58` | FOV `+0x68` | alternate `+0x6C` |
|---|---|---:|---|---|
| 0 (mode 1) | `(0, 2.75…3.75, +14.5…+20.0)` | 7.0 | `0x3F29C91F` = **38.00°** | 46.00° |
| 1 (mode 2) | `(0, 0.415…1.55, −5.66…−11.0)` | 7.0 | `0x3F4D87AC` = **46.00°** | 20.00° |
| 2 (mode 3) | as view 1 | 7.0 | 46.00° | 50.00° |

Verified here from the bit patterns: `0x3F29C91F` is 0.663225 rad = 38.00°
exactly, `0x3F4D87AC` is 0.802851 = 46.00° exactly.

Two things the fallback did not say:

- **The ease rate in data is 7.0, not the 6.5 the initialiser writes.**
- **A record holds four offset vectors**, at `+0x00`, `+0x10`, `+0x20`, `+0x30`,
  stepping in small increments — not one. `0x8225C510` copies all of them.

## Two open items closed on the way

- **Cycle 1283's manager-base gap.** The store it could not find is in the same
  constructor: `82213878 lis r11,2` / `82213880 ori r11,r11,0xf9a0` /
  `82213884 addi r3,r3,-1616` / `82213888 stwx r3,r31,r11` — so
  `*(this+0x2F9A0) = this+0x2F9B0`, and the `G` every user loads **is** the
  constructor's `this`.
- **Cycle 1285's `ori` bug, found independently.** The delegated work hit the
  same defect and reported 6 sites for `0x31070` where the truth is 9. My fixed
  scanner now returns **9**, naming the same three `ori` sites —
  `0x821D69A4`, `0x82263D4C`, `0x822781F0`. Two independent corrections agreeing
  on a number neither started from. And `0x821D69A4` is the loader that answers
  this whole question, so the blind spot was load-bearing twice over.

## Not established

- **Which of the 15 groups Mission 01 uses.** The index is `r25` at
  `82263d18`, from a virtual call on the object at `G+0x70`, vtable slot `+0x0C`,
  argument 5. That vtable was not resolved, so no row is claimed for any
  aircraft, and none is guessed.
- **Array A** — child 35, 27 × 192 — read by the function around `0x8225DF08`.
  Untouched.
- **Most record fields.** `+0x60/+0x64` is 7.0 in view-0 records and 0.35 in the
  others, so it is not an angle in general.

  > **Corrected by cycle 1290.** This bullet continued: "while the fallback puts
  > 46° at the manager offsets those fields land on. That mismatch is
  > unresolved." **There is no mismatch.** The initialiser's `-0x8(r11)` store
  > uses `f13 = [0x82002FD4] = 0.1`, not `f12`; only `+0x378/+0x37C` receives the
  > 46°. The claim came from cycle 1283 being one register wrong and from me
  > repeating it without reading the loop.
- Whether a per-mission write overrides the table. Nothing else calls the
  setters and nothing else writes the range by a statically computable form; a
  runtime write through a captured pointer is not excluded.
