# Cycle 1401 — three are the auto-level subsystem

## Qualification

- **No Ghidra run and no oracle pass.** The corpus and the image.
- No product C++ changed; ctest stays 47. **No contract entry.**
- `analysis/flight/command-caller-search.tsv` extended.

## The one-line discriminator, run across all seven

Cycle 1400 promised the split would come from **what `f2` is at each site**. It
does:

| function | `f2` | reading |
|---|---|---|
| `0x82291A50` | 0.0 | **level only** |
| `0x82292540` | 0.0 | **level only** |
| `0x822911E8` | field +100, computed, 0.0 | carries a value |
| `0x82290500` | **−π/2**, 0.0 | a constant attitude |
| `0x821405F8` | field +3032 | carries a value |
| `0x822389D8` | two unresolved, two 0.0 | mixed |
| `0x82366AA0` | field +44 | refuted — see below |

**Three of the eight are the auto-level subsystem** — `0x82290E20` from cycle
1400 plus these two — all in `0x8229xxxx`, all passing only zero. Whatever steers
from a stick is not among them.

`0x82290500` is the interesting negative: it passes **−π/2**. Ninety degrees is a
*commanded manoeuvre*, not a control input.

## A collision caught by argument count

`0x82366AA0` is 33 instructions: it reads a state at `[+28]`, advances 9 → 10,
then dispatches offset 52 with

```
lfs  f2,44(r31)
lfs  f1,36(r31)
addi r4,r31,32
mr   r3,r31
```

`+36` and `+44` are exactly where the flight model keeps two of its command
accumulators, and for a moment that looks like the answer.

It is **four arguments**. The command setters take exactly `(this, f1, f2)` —
cycle 1393's differential compared the return code, the accumulator, the target
field and the flag byte across thirty cases, and no fourth argument appears
anywhere in their 45 instructions. So this dispatches a **different class's**
slot 52, whose `+36` and `+44` are that class's fields sitting at the same
displacements.

That is the fifth displacement collision this thread has caught, and **the first
caught by an argument count rather than by a class filter**. It is a cheaper
discriminator than either, and it generalises: a call site is a claim about the
callee's *whole* signature, not just its slot.

## Where this leaves the search

Four candidates unread: `0x822911E8`, `0x821405F8`, `0x822389D8`, and
`0x82290500`'s non-zero site. The demo's invented link is unchanged, and the
header still says so.

Two cycles of search have produced no caller and three refutations. That is a
worse rate than the flight thread's, and the reason is structural: the setters'
slot numbers are shared by 78–151 unrelated functions, so every filter has to be
built out of something *other* than the dispatch — the argument list, the
constant passed, the argument count. Each one that works becomes cheap; finding
which one works is the cost.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.3 | 5 (1395, 1396, 1399, 1400, 1401) |
| implementation/integration spent on A3.3 | 2 (1397, 1398) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 25 behaviours
ctest                                 100% passed, 0 failed out of 47
tools/tests                           Ran 77 tests, OK
```

## Next

`0x821405F8` — 281 instructions, **no callers**, so a virtual method, passing a
target from `[+3032]`. A field that far out belongs to a large object, and the
flight model's own is 1,296 bytes, so this `this` is something bigger — an entity
or a controller. That is the first candidate whose *shape* suggests the right
side of the boundary, and it is one read.
