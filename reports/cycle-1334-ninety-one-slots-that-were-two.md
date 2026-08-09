# Cycle 1334 — ninety-one slots that were two

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## A measured dead end, recorded so it is not repeated

The question was what separates a unit's two `CGaLocator`s, at `+0x10` and
`+0x80`. The obvious probe is to find who takes the address of each —
`addi rX,rY,16` against `addi rX,rY,128`, which is exactly how `0x822A1E80`'s
callers reach the second one.

Over the whole recompiled corpus:

```
addi rX,rY,16   : 1631 sites in  937 functions
addi rX,rY,128  : 3283 sites in 1822 functions
```

Small immediates are not a discriminator. This is the displacement collision at
larger scale than cycle 1333's, and it is written down as a **measured** dead end
rather than left for the next cycle to rediscover.

## The correction, and it is to a number from two commands earlier

The bounded question is `CGaLocator`'s own interface, so I read its vtable and
terminated the read at the first word that is not a code pointer. That gave
**91 slots**.

**It is 2.** The class map's next named vtable, `galib::CGaObjDesc`, begins at
`0x82054D9C` — eight bytes after `CGaLocator`'s base.

"Still looks like a code pointer" cannot separate two vtables that sit next to
each other, and `.rdata` here is a continuous run of them. The terminator I chose
measures where the *section's* pointers stop, not where the *object's* interface
stops. The class map has the boundary and I had already loaded it in this same
cycle for a different purpose.

The honest caveat: MSVC places several vtables adjacently for one class under
multiple inheritance, so `CGaLocator` and `CGaObjDesc` could be two sub-object
vtables of one object rather than two classes. The map names them separately and
cannot tell those apart, so neither does this report.

## What the corrected number establishes

With two slots, **none of the transform functions is a `CGaLocator` virtual
method** — not `0x822A1E80`, not the three rotations, not `0x822A23D8`. They are
free functions that take a locator by pointer.

That is worth having: it means the locator is a data object with a very small
interface, and the transform logic lives outside it. A port does not need to
model a 91-method class, which is what the wrong number implied.

## Not established

- What separates the locator at `+0x10` from the one at `+0x80`.
- What `CGaLocator`'s two virtual methods are.
- Whether `CGaLocator` and `CGaObjDesc` are two classes or two sub-object
  vtables of one.
- Still: what writes `unit+0xE0`.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

Read the two slots — `0x82093818` and `0x8206D7FC`. Two functions is a
population small enough to read completely, which is the opposite of every scan
this cycle and the last one tried.
