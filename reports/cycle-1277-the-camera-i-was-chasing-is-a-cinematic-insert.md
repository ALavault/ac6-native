# Cycle 1277 — the camera I was chasing is a cinematic insert

## Qualification

Delegated investigation; **the load-bearing instructions and every float below
were re-read here** — floats directly from `analysis-input/ACE6_X360.exe`,
instructions in `ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256
`acc302c1…11bcde`. **No oracle pass was spent.** No product code changed.

## The correction, and it is to a brief I wrote

Cycle 1273 established that the camera manager's per-frame update `0x82263A50`
dispatches on `manager+0x190` through a table at `0x822646F0`, and that **modes 4
and 5 reach the ArmsCamera**. I carried that into the next brief as *the* flight
camera arm.

**The dispatcher has two switches, and that was one of them.** Re-read here:

```
82263e7c  lis   r12,-0x7dda
82263e80  addi  r12,r12,0x3e94     ; a SECOND table, at 0x82263E94
82263e88  lwzx  r0,r12,r0
82263e90  bctr
```

Modes 1, 2, 3 and 13 take that second switch and **never touch the ArmsCamera**.
They call a virtual on an object the manager holds, and take the projection FOV
straight from a field:

```
82263f78  lwz  r7,0x198(r31)      ; the camera object at manager+0x198
82263fcc  lwz  r7,0x4(r7)         ; its vtable slot +0x04
8226400c  bctrl
82264010  lwz  r11,0x19c(r31)
82264014  lfs  f0,0x378(r31)      ; the FOV that mode installs
8226401c  stfs f0,0xd4(r31)       ; -> the projection field
```

then copy the basis rows and position of the object at `manager+0x19C` into the
manager's own matrix. **That is the persistent camera.** Mode 4 is the only one
reaching the ArmsCamera at all (`82263d1c cmpwi cr6,r11,0x4`), and when its
update returns 0 the mode is abandoned outright.

*Correction to the delegated report, by address*: it put the FOV store at
`82264018`; the instruction there is `addi r10,r31,0x140` and the store is at
`8226401c`. One instruction, and it is the sort of slip that survives because
everything around it is right.

## Established — what the ArmsCamera actually is

Its parameters are `.rdata` literals, read out of the image:

| field | value | |
|---|---:|---|
| `beh+0xB0` start focal | **10 mm** | → 81.26° vertical |
| `beh+0xB8` end focal | **38 mm** | → 33.79° vertical |
| `beh+0xA8` blend | 1.0 s | |
| `beh+0xC0` boom | 100.0 | along local Z |
| `beh+0xE0` lateral | 50.0 | random sign, random magnitude to half |
| `beh+0xD8` radius | 1500.0 | target-proximity |
| `beh+0x98` lag | 0.3 | |
| `beh+0xC8` expiry | **3.0 s** | |

**A dolly-zoom from 81° to 34° over one second, on a fixed 100-unit boom with a
±50-unit random lateral offset and a three-second expiry.** That is a cinematic
insert, not a chase camera, and it explains why the parameters looked odd rather
than wrong.

The FOV is **time-based and reads no speed**: `fov_v = (2·atan(18/f))·24/36`
from `0x82275500` with the 35 mm frame at `[0x82008790] = 36` and
`[0x82008794] = 24`, blended over elapsed time by an easing curve, not linearly.

They come from code, not from `paramArmsCameraA.xml`: the constructor's
parameter block terminates in `.rdata` literals, and the per-field random jitter
is **0.0 for all fourteen pairs**, so the defaults survive intact.

## What this changes

The 29-of-95 marker count is not a chase camera the product has failed to
reproduce. **The camera to derive is the object at `manager+0x198`, its vtable
slot `+0x04`, and whatever writes `manager+0x378`** — a different investigation
from the one two cycles have spent, and the delegated report named it as such.

## Not established

- **Which behaviour type the player's aircraft carries.** The selector is
  `unit+0xB4`, written from a per-record `+0x21C` field in a stride-`0xE0`
  runtime array. Nothing in the image writes it, so no static read decides it.
  What is decidable: types 1004 and 1008–1012 select the un-overridden base
  class, and 1005, 1020 and 1022 select nothing.
- **`0x822D9A30`**, the update for the other seven behaviour classes: not read.
- **The `!(flags & 2)` branch** of `0x822D8220` — roughly 600 instructions, of
  which about 200 were read.
- **`0x822D80C0`**, which can modify eye and target after the arithmetic above,
  so the derived formula is the **pre-lag** value.
- None of the behaviour vtables carries RTTI, so the class names are not
  recoverable from the image.

## The shape

Two cycles of camera work were spent on an arm reached by one of two switches in
a function I had read once. The first reading was correct about what it saw. It
is *the instrument sampled a third of it* again — not a truncated listing this
time, but a truncated **control-flow graph**, and nothing in the output said so
because a `bctr` looks the same whether or not another one follows it.
