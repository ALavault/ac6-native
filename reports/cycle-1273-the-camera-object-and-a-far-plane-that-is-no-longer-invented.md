# Cycle 1273 — the camera object, and a far plane that is no longer invented

## Qualification

Delegated investigation; **every constant and every load-bearing instruction
below was re-read here** — the floats directly from `analysis-input/ACE6_X360.exe`,
the instructions in `ghidra-projects-xenon/ac6-xenon`. `default.xex` SHA-256
`acc302c1…11bcde`. **No oracle pass was spent.**

## Established — the camera manager and its four projection fields

`[[0x826E4EB4] + 0x2F9A0]` is the **`ACE6::CAce6CameraManager`** instance —
RTTI `.?AVCAce6CameraManager@ACE6@@` at `0x8268F47C`, vtable `0x82054F0C`.

`0x8209B398` hands four of its fields to the perspective builder:

```
8209b42c  lfs f4,0xe0(r31)     +0xE0 far
8209b430  lfs f3,0xdc(r31)     +0xDC near
8209b434  lfs f2,0xd8(r31)     +0xD8 aspect
8209b438  lfs f1,0xd4(r31)     +0xD4 vertical FOV
8209b43c  bl  0x82271828
```

and the initialiser `0x8225DF88` stamps them, re-read here:

```
8225dfb0  lfs  f0,0x7f64(r11)      8225dfb8  stfs f0,0xd4(r31)
8225dfc0  lfs  f0,-0x5e8c(r9)      8225dfc8  stfs f0,0xd8(r31)
8225dfdc  lfs  f30,0x1348(r9)      8225dfe4  stfs f30,0xdc(r31)
8225dff8  lfs  f0,-0x6494(r9)      8225e000  stfs f0,0xe0(r31)
```

The constants, read out of the image rather than named:

| address | value | |
|---|---:|---|
| `0x82007F64` | 0.8028514 rad | **exactly 46.0000 degrees**, vertical |
| `0x8206A174` | 1.7777778 | 16/9 |
| `0x82001348` | 1.0 | near |
| `0x82069B6C` | **24000.0** | far |
| `0x82069B28` | −1.0 | the builder's `m23`, so right-handed |

Depth runs 1 at the near plane to 0 at the far one — reversed-Z. **That is a
reading of the arithmetic, not a measurement**; `m23 = −1.0` is read, the
near/far role assignment rests on the two stamped values.

The rest of the chain, from the delegated work and not re-verified here: the
per-frame update `0x82263A50` dispatches on `manager+0x190` through a 48-entry
table at `0x822646F0`; **modes 4 and 5** reach `0x8226485C` → `0x8225C118` →
`0x822D7CE8`, `CAce6ArmsCamera::Update`, which calls a virtual at
`ArmsCamera+0x850` and unpacks **eye, look-at target, up and FOV** from it,
writing the FOV straight back into `+0xD4` and building the basis with a LookAt
at `0x822D7BE8`.

## The product change

`native_geometry_raster_target.cpp`'s default far plane was **4096**, invented.
It is now **24000**, retail's own, with the derivation beside it.

**What is imported is the number, not the projection.** Retail's builder is
right-handed reversed-Z; this rasteriser keeps its linear `view_z / far`. The
46-degree FOV is not applied here — and it is retail's *initial* value in any
case, because the per-frame behaviour overwrites `+0xD4` every tick and that
behaviour is not derived.

### My own control caught the change, and was right to

Cycle 1271's far-plane control asserted `with_derived > with_default`. With the
default at 24000 it **failed**, because 24000 already reaches most of a
66,456-unit world.

That failure said what the control had really been testing: *"4096 is too
small"*, not *"the far plane matters"*. The second is the fact worth holding, so
the comparison is now against an explicitly undersized plane — the product's
former default — rather than against whatever the default happens to be.
Positive control re-run: making both renders use the derived plane turns it red,
restoring it green, 27 of 27.

The marker count is **unchanged at 29 of 95**. The far plane was not what was
limiting it, which is a useful negative: the remaining loss is the camera, and
the camera's per-frame behaviour is the next derivation.

## Not established

- **Which mode Mission 01 runs in.** Modes 4 and 5 are the ArmsCamera arm; that
  Mission 01 selects them was not shown, and how `+0x194` (requested) becomes
  `+0x190` (active) was not read.
- **The flight behaviour itself** — which of the 19 types at `0x822D7740`
  (1004..1022) is the default chase camera, and the arithmetic behind its
  `vtable+0x08` that produces eye, target, up and FOV. The delegated work stopped
  at the virtual dispatch, and named `paramArmsCameraA.xml` and
  `CX360ArmsCameraGeneralTunerA` (vtable `0x82063210`) as where it will land.
- **The path from the frame loop `0x821D7A90` to `0x82263A50`.** Five callers
  were listed, none walked.
- **Which of the three scene routines runs for Mission 01 gameplay.** All three
  read the same four fields, so the projection answer is invariant, but
  `0x8209B398` sets a 208×144 viewport, which is not a 720p main pass.
