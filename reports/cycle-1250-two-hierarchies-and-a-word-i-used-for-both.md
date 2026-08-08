# Cycle 1250 — two hierarchies, and a word I used for both

Cycle 1247 recorded a contradiction and refused to resolve it: cycle 1244 said
`[unit+0x188]` is a parent pointer, cycle 1247 said `0x822980C8` writes a float
there. **They are different objects**, and finding that out turned up something
larger.

## The resolution

Two class hierarchies, both **RTTI roots**, therefore disjoint — no `+0x188` can
be inherited from a shared base:

| | the pointer | the float |
|---|---|---|
| base vtable | `0x820572C0`, COL → **`.?AVCGaObj@galib@@`** | `0x82056874`, COL → **`.?AVCAce6Unit@ACE6@@`** |
| own vtable | `0x820078D0`, ≥ `0x138` (78 slots) | `0x82009440`, **`0x60`** (24 slots) |
| constructor | `0x8229A470` | `0x822986B0` → `0x822A2330` |

**The control, verified here.** `0x820A7070` dispatches slot `+0x130` on the
object it writes `+0x188` to (`820a7b4c lwz r11,0x130(r11)`). A `0x60`-byte
vtable has no such slot. And the boundary is real rather than assumed — the
`.text` pointer run ends at `0x8200949C` and `0x820094A0` onward is a float
block:

```
820094a0  3fb65718 48000000     ; 1.4245329, 131072.0
820094a8  42c80000 41200000     ; 100.0, 10.0
```

**This could have failed.** Had `0x820094A0` held another `.text` pointer, the run
would have continued unbroken and slot `+0x130` would exist, making the two
classes indistinguishable by this test.

**Cycle 1244 stands.** `0x8229AF80` reads its pointee with exactly the layout it
writes on itself — `+0x118` flags, `+0x184` descriptor, `+0x40`/`+0xA0`
translation, `+0x70/80/90` rows — so the pointee is same-class, and the class is
the `CGaObj` one.

## Cycle 1247's float, corrected in three details

`82298118 stfs f13,0x188(r31)` writes **the literal `0.0f`** — `f13` comes from
`8200082C`, which is zero, and is clobbered before anything else touches it. The
`[+0xD0]` derivation cycle 1247 attached to it is a different field and a
different width:

```
822980e0  lwz   r10,0xd0(r31)    ; lwz, a word — not lbz
822980f0  extsw r8,r10           ; extsw — not extsb
82298148  stfs  f0,0x198(r31)    ; +0x198 — not +0x188
```

Three errors in one sentence, all in the direction of making the collision look
sharper than it is.

## The larger thing: I used one word for two families

Cycle 1244 wrote that `0x820A7070` writes *"on each unit `+0x184`, `+0x170`,
`+0x118`, `+0x188`, and on the leader `+0xD8`, `+0xDC`, `+0xE0`, `+0xE4`"*.

Those are **two different class families**. The loop's `r31` — built by factory
slot `+0x14` — is the `CGaObj` object that receives the parent pointer. `r16` —
built by slot `+0x10`, assigned only at `820a7608`/`820a763c`, and **the only
object the function registers** (`820a7650 bl 0x8226fec0`) — is the `CAce6Unit`
object that receives the child array and, per cycle 1247, the order FSM.

And `+0xD0`/`+0xD4`/`+0xDC` collide the same way: `0x822A2330` writes them as
**integers**, `0x8229A470` as **floats** (`stfs f31,0xd0`, `stfs f31,0xd4`,
`stfs f0,0xdc`). The two even disagree about where their matrix sub-object lives —
vtable `0x82054D94` at `+0x10` **and `+0x80`** in one, `+0x10` **and `+0x60`** in
the other.

**Neither cycle 1244 nor 1247 is wrong about what it read.** What is wrong is the
prose: "unit" named both, so a reader following the placement flow would put the
child array and the parent pointer on the same object, and they are not.

That is the fifteenth shape — the displacement collision — but at the level of
**vocabulary rather than of a scan**. Reading the neighbours catches it in a
listing; nothing catches it in a paragraph except naming the class.

## Not established, stated plainly

- **Which** of the six `0x138`-byte vtables the loop's `r31` receives.
  `0x8229A470` installs `0x820078D0` and writes the loop's exact field set, but
  neither factory was followed to a concrete allocation. The *family* is
  established; the leaf class is not.
- That `r16` is specifically the `0x82009440` class rather than another
  `CAce6Unit` subclass. It is the only registered object and it receives the
  `CAce6Unit` `+0xD0..+0xE4` block, but its factory was not read.
- **Liveness.** This is an identity finding, not a reachability one — none of
  `0x8229AF80`, `820a7b2c` or `82298118` was re-derived as executing in Mission
  01. A true positive from dead code would look exactly like this.
- `0x82009440`'s class still has no RTTI; its `CAce6Unit` parentage rests on the
  constructor chain, not a type read.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
the vtable boundary at 0x8200949C read here; 0x820094A0 decodes as 1.4245329
```

No product code changed.
