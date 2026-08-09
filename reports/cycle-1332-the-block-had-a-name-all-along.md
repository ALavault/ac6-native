# Cycle 1332 — the block had a name all along

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## `0x822A2B08` is a virtual method, and the class map names its owners

`FindDataPointersTo` gives four hits: one `.pdata` row, which the tool labels as
such, and **three `.rdata` words**. Each is slot `+0x34` of a vtable, and the
class map names all three from their RTTI locators:

| vtable | class |
|---|---|
| `0x82056874` | `ACE6::CAce6Unit` |
| `0x820568D4` | `ACE6::CAce6UnitPlayer` |
| `0x82056934` | `ACE6::CAce6UnitOtherPlayer` |

All three carry the **same function pointer** at that slot, so the derived
classes do not override it. `0x820568D4` is the vtable the plan named for the
player, reached here from the other end.

## The transform block is `galib::CGaLocator`

`CAce6Unit`'s constructor is 42 instructions at `0x822A2330`. It installs its own
vtable at `this+0x00`, and then installs **`0x82054D94` twice** — at `this+0x10`
and at `this+0x80`. The class map names it: **`galib::CGaLocator`**.

A unit carries **two** locators. `0x822A1E80` is handed the one at `+0x80`, by
callers that do `r3 = unit` and `r31 = r3 + 0x80`.

**So the block cycles 1327–1331 have been characterising had a name in the
repository the whole time**, and it was reachable in two commands once the
population was bounded properly. Bounding it was the work; the name was a lookup.

## Which answers what `+0x00..+0x0F` is

Open since cycle 1327, when a sentinel `(17, 29, 43, 61)` placed there survived
all fourteen runs untouched. The first word is the **vtable pointer**.

It is not that `0x822A1E80` happens not to write there. Writing there would
destroy the object's type. The measurement was right and the explanation is now
structural rather than statistical.

## And the constructor confirms the row convention independently

Cycle 1330 read the translation-in-the-last-row convention off the lane-3
pattern: basis rows ending in `0.0`, the fourth vector ending in `1.0`.

`0x822A2330` initialises each locator's `+0x40`…`+0x4C` to
**`(0.0, 0.0, 0.0, 1.0)`** at construction — three zeros from `0x8200082C` and a
one from `0x82001348` — before any transform has run. A column layout would put
that `1.0` somewhere else entirely.

Two derivations, one from a running transform and one from a constructor that
never runs a transform, agreeing.

## `unit+0xE0` is null at construction

The container `0x822A2B08` walks — `r9 = [unit+0xE0]`, then two pointer hops and
an 8-byte stride — is written **zero** by the constructor, along with `+0xD4`,
`+0xD8`, `+0xDC` and `+0xE4`. `+0xD0` gets `255`. So `+0xE0` is a pointer filled
in later by something else, and finding that writer is the next thread rather
than a guess to be made now.

## Not established

- What fills `unit+0xE0`, and what the container is.
- Why a unit has **two** locators, and what distinguishes `+0x10` from `+0x80`.
- What `0x82296E40` — the third caller — passes.
- The argument block's `+0x24`…`+0x2E`, and the four branches on `+0x2A`.
- Nothing in the product changed.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

What writes `unit+0xE0`. And the two locators: `+0x10` and `+0x80` are
constructed identically, so whatever separates them is in how they are used, not
in how they are made — which means reading the users, not the constructor.
