# Cycle 1335 — one slot, and somebody else's beginning

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.

## `galib::CGaLocator` has exactly one virtual method

Its destructor, `0x82093818`, and the function names its own class: it
re-installs `0x82054D94` at `this+0x00` before testing bit 0 of `r4` and
conditionally calling `0x82380070`. That is the MSVC scalar-deleting-destructor
shape, and the vtable it writes back is the one the class map named.

Cycle 1334 said two slots. It is **one**.

## The word after it is somebody else's beginning

`0x82054D98` holds `0x8206D7FC`, which Ghidra reports as `<not-disassembled>`.
That is **not** evidence it is data — *the listing is not the code* is one of
this campaign's own shapes, and `-noanalysis` means Ghidra disassembles nothing
it was not asked to.

So the bytes were read instead:

```
8206d7fc  00 00 00 00 00 00 00 00 00 00 00 00 82 68 f7 28
8206d80c  82 06 d8 10 ...
```

Twelve zero bytes, then two pointers. That is an **RTTI Complete Object
Locator** — signature, offset and cdOffset all zero, then the type descriptor and
the class hierarchy descriptor.

The control is decisive and independent: the type descriptor at `0x8268F728`
spells **`.?AVCGaObjDesc@galib@@`**, and `analysis/class-map.tsv` independently
lists a vtable at `0x82054D9C` named `galib::CGaObjDesc` with exactly that
descriptor. Two sources, neither consulted while reading the other.

So the region is

```
0x82054D90   CGaLocator's COL pointer
0x82054D94   CGaLocator's vtable  — ONE slot, the destructor
0x82054D98   CGaObjDesc's COL pointer
0x82054D9C   CGaObjDesc's vtable
```

and cycle 1334's "91 slots" was a run of roughly forty-five interleaved
single-slot vtables, each preceded by its locator.

## Which makes the locator a data object

`CGaLocator` is polymorphic only in the sense that it has a destructor. It has
no other virtual method, so **nothing about a unit's transform is dispatched** —
`0x822A1E80`, the three rotations and `0x822A23D8` are free functions operating
on a plain structure.

For a port that is the good outcome: `RetailBasis` in
`include/ac6/retail_transform.h` is already a plain struct with free functions,
and this says that shape is retail's shape and not a simplification I chose.

## The twenty-ninth shape

Written down, because the failure is reusable and the fix is one lookup: **a
vtable's extent comes from the class map, not from the bytes.** MSVC packs
`COL | slots | COL | slots`, so a `.rdata` region of vtables *alternates* code
and data and a "does this look like code" terminator is measuring the section,
not the class.

Two cheap confirmations when the map is silent: the COL at `vtable[-1]` carries
the mangled name, and slot 0's destructor usually materialises its own vtable
base.

It is the sibling of *stopping at a natural boundary* — there a `blr` looked like
an end and was not; here a non-code word looked like an end and was somebody
else's beginning.

## Not established

- What separates the locator at `+0x10` from the one at `+0x80`. Both cheap
  probes are now spent: the address-of scan (cycle 1334) and the interface
  (this cycle, which found there is no interface to differ on).
- What writes `unit+0xE0`.
- Whether `CGaLocator` and `CGaObjDesc` are related by inheritance. They are
  **adjacent**, which cycle 1334 offered as a multiple-inheritance hypothesis;
  adjacency in `.rdata` is now shown to be how *every* vtable sits here, so it is
  no evidence of a relationship at all. That hypothesis is withdrawn.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
instrument_discipline_index          pass, 20 shapes, 0 unindexed
tools/tests                          Ran 72 tests, OK
```

## Next

The two locators are down to one distinguishing question and it is not about
their type: what reads `+0x10`. `0x822A1E80`'s callers reach `+0x80`, and the
class hierarchy descriptor at `0x8206D810` is a bounded, readable structure that
would say whether `CGaLocator` is a base of anything — which is the same question
asked where an answer actually lives.
