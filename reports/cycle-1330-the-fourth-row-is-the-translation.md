# Cycle 1330 — the fourth row is the translation

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed; four constants were read and a
  qualified instruction stream was followed.
- No product C++ changed, no contract changed.

## The block is a 4×4, and the convention is measured

Cycle 1328 declined to write the composition as a matrix product because the
row-versus-column convention had not been measured. It now has been, and the
evidence is four lanes.

The vector `0x822A23D8` stores at `transform+0x40` is

```
lane 0 = [r30+0x04]
lane 1 = [r30+0x08]
lane 2 = [r30+0x0C]
lane 3 = the float at 0x82001348 = 0x3F800000 = 1.0
```

and the three basis rows are seeded from constants whose **lane 3 is 0.0**.

Under a column-major layout the translation would live in the last **column** —
lane 3 of each basis row — and those lanes hold zero. Under a row layout it is
the fourth row, ending in 1. It is the fourth row.

```
+0x00   not written by the kernel
+0x10   basis row 0   (…, 0)
+0x20   basis row 1   (…, 0)
+0x30   basis row 2   (…, 0)
+0x40   (x, y, z, 1)
```

## `f3` is zero, and it is the same zero three times

`f3` at the `0x822A23D8` call site is `lfs f31,2092(r10)` with `r10 = 0x82000000`
— the float at `0x8200082C`, read as `00 00 00 00`. So the **third rotation is
the identity at that call site**, and only two angles are live.

The same `+2092` constant, from the same base, is loaded in `0x8230B030` and
`0x822955F0`. One zero, three functions.

## The four call sites drive different subsets

| caller | call | f1 → row 0, 2nd | f2 → row 1, 1st | f3 → row 2, 3rd |
|---|---|---|---|---|
| `0x822955F0` | `0x822957BC` | live, negated | **path-dependent** | 0.0 |
| `0x82295A88` | `0x82295E60` | live sum | live sum | live sum |
| `0x822A23D8` | `0x822A27F4` | `[r30+0x18]` | `[r30+0x1C]` | 0.0 |
| `0x8230B030` | `0x8230B44C` | 0.0 | live sum | 0.0 |

**This is not the disagreement I was worried about.** Cycle 1329 ended fearing
four different angle conventions above one shared kernel. What is measured is one
convention with different subsets used: every caller passes the same argument in
the same register to the same kernel, so `f1` always means *about row 0, applied
second*. Whether the **source** angles mean the same physical thing per caller is
a level above and is not established.

## Two mistakes of mine, caught before they were published

**An unbounded extraction ran past a function's end.** My first pass used `awk`
that started at a symbol and never stopped, so `0x822955F0`'s window swallowed
the following function and showed **two** calls to the kernel inside it. Bounded
correctly, every one of the four callers has exactly **one**. I nearly published
a call graph with a call in the wrong function.

**A constant that was not constant.** `0x822955F0` sets `f25` to the 0.0 constant
at one point, and to `-round(f1)` at another, with branches between that and the
call. Reading only the first write makes it look like a zero, and I had written
it down as one. It is listed above as unresolved because it is unresolved.

Both are the same failure at different scales — reading the first thing that
answers the question and stopping. The listing tool is qualified; the way I read
its output was not.

## Not established

- `0x822955F0`'s `f2`.
- What `r30` is. It is now read at `+0x04`…`+0x0C` as a three-float vector and at
  `+0x18`/`+0x1C` as two angles, plus a byte at `+0x2A` that selects a four-way
  branch. That is a shape, not a name, and it stays unnamed.
- What occupies `transform+0x00..+0x0F`.
- Whether the source angles share meaning across callers.
- Nothing in the product changed.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

`r30`: bound the population before scanning it — name the class from the vtable
if it has one, find the constructor, and only then read the fields. The recipe
`retail_input` established exists precisely so a structure with a known shape and
no name does not become a structure with an invented one.
