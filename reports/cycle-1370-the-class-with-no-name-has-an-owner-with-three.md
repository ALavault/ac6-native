# Cycle 1370 — the class with no name has an owner with three

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus was read for
  mnemonics, and `analysis-input/ACE6_X360.exe` for the RTTI chains.
- **No product C++ changed, no contract changed.** New tooling
  (`tools/map_object_layout.py`) with five regression tests; `tools/tests` is
  **77**, was 72.
- New artefact: `analysis/flight/integrator-owner-layout.tsv`.

## The question, and why the obvious form of it was useless

Cycle 1369 ended on *"who invokes slot +0x7C"*. Enumerated across the corpus —
`lwz rX,124(rY)` then `mtctr`/`bctrl` — that is **155 sites**. Slot 31 is a
common method on many classes, so the population is not the integrator's callers;
it is every virtual call at index 31 in the game.

The narrow question that does work is the mirror of it: **who builds a
`0x8200F270` vtable**. That is three sites, all adjacent to the integrator:

```
0x82302B28   constructor          calls base ctor 0x82282220, installs 0x8200F270
0x82302B68   destructor           installs 0x8200F270, tail-calls 0x822815C8
0x82302C28   deleting destructor  the `clrlwi r11,rX,31` / operator delete triple
```

An MSVC ctor/dtor/deleting-dtor triple. The class is real, has a base class
(`0x82282220`, vtable `0x82008B10`, also without RTTI), and is instantiated in
exactly one place.

## Where it is instantiated

`sub_8222BEC8` — one caller, a constructor of a much larger object — builds it as
a **member subobject at `this + 2224`**. The whole object map:

| this+ | what |
|---:|---|
| 0 | vtable `0x82007A10`, no RTTI |
| 544 | `ACE6::CAce6Thread`, then overwritten with `0x82007A08` (a derived thread vtable) |
| 672, 1388, 1776 | subobjects |
| 1372, 5044 | **`ACE6::CAce6ObjDesc`** |
| **2224** | **the integrator's class** — `sub_82302B28` |
| 3536, 6192, 6480, 9460, 10240 | subobjects |
| 4928, 7680, 7792, 7904, 8016, 8128, 10592 | **`galib::CGaLocator`** — seven of them |

`sub_82227B08` is the matching destructor: it mirrors the constructor in reverse
and calls `0x82302B68` on `this + 2224`. The offset is confirmed from both ends.

## And the size comes from the factory

`sub_820A8138` switches on a type id in 1..4 through a jump table at
`0x820A832C` and builds four object kinds, each with its allocation size in the
instruction stream:

```
id -> sub_8222BEC8   10672 bytes
id -> sub_82293C28   12160 bytes   (calls 0x8222BEC8, then a subobject at +10672,
                                    and re-installs the vtables at +0 and +544)
id -> sub_8229D9B0     592 bytes
id -> sub_822A47E8     928 bytes
```

So the integrator's owner is a **10,672-byte ACE6 entity** that *is* a
`CAce6Thread`, carries a `CAce6ObjDesc`, holds **seven `CGaLocator` poses**, and
keeps a movement component at +2224 whose 31st virtual method steps position.
`sub_82293C28` is a 12,160-byte derived entity built on the same base.

`sub_820A8138` sits four instructions before `0x820A8678`, the address the plan
already names as where the aircraft comes from the profile.

## The instrument was right and my copy of it was wrong

My first pass reported **every** class as unnamed, including
`.?AVCGaLocator@galib@@`. The throwaway RTTI reader guarded the type descriptor
with `descriptor < 0x82400000`; AC6 keeps its type descriptors at **`0x8268F…`**,
so the guard rejected all of them and returned `None` for a binary that has RTTI.

`tools/whose_vtable.py` had the bound right all along — `BASE <= x < BASE + len`
— which is why the class map's 811 named vtables and its 306 unnamed ones are
**not** affected. Cycle 1369's statement that `0x8200F270` has zero at
`vtable − 4` also stands: the word is literally `0x00000000`, not a pointer that
was misjudged.

But the failure was one guard away from silently deleting a third of the class
map, and it was written by someone who had read the correct version an hour
earlier. That is why it is now a test
(`test_a_descriptor_past_the_rdata_bound_is_still_read`) and not a paragraph.

## A displacement that is a string

`sub_821F5AD8` contains `addi r10,r11,2224` and is **not** a use of `this+2224`.
Its base is a materialised constant, so 2224 is the low half of `0x820008B0` — a
string, compared byte by byte. The same integer, one lattice apart.

Exactly three sites in the binary form the number 2224, and only two of them are
the offset: the constructor and the destructor. The tool separates constants from
this-relative pointers for this reason, and
`test_a_string_address_is_not_read_as_a_displacement` fails if they are merged.

## The instrument, kept

`tools/map_object_layout.py CORPUS IMAGE CTOR [CTOR...] [--tsv]` prints an
object's subobject map from its constructor: every pointer install at its
`this`-relative offset with the RTTI name where one exists, and every subobject
constructor call. It counts the branches it did **not** follow rather than
leaving the omission silent, and it refuses to claim a size, because a
constructor does not carry one.

This is infrastructure, not a cost charged to this behaviour. A5a–A5d and A3.3
all need the same walk, and doing it by hand is what produced the bad guard.

## Not established

- **What `f31` is.** Still open. It is the function's own float argument
  (cycle 1369) and the call is a virtual dispatch, so the answer needs the site
  that invokes slot 31 on the component — and no code forms `entity + 2224`
  outside the constructor and destructor, so the component is reached through a
  pointer stored elsewhere or through a component list.
- Which of the seven `CGaLocator`s the integrator writes. `r30` in
  `sub_82303110` is not yet tied to any of the offsets above.
- The entity's own class name. `0x82007A10`, `0x82009130` and `0x82008B10` all
  lack COLs.
- `sub_820A8138`'s BSS table at `0x82A218A0` — fifteen rows of
  `{function, id, index}`, an integer-keyed registrar. Noted, not pursued; it is
  the same shape the JV decision describes and belongs to that thread.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 so far | 20 (1351–1370) |
| implementation/integration spent on A3.2 | 3 (1354, 1355, 1356 — the binding layer) |

A3.1 closed at 1 implementation cycle after 8 research. A3.2's research total is
now high because sixteen of those cycles followed call edges that structurally
could not reach an unnamed class invoked only through a vtable slot; cycles
1366–1370 are the four that did, plus this one.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 14 behaviours
ctest                                 100% passed, 0 failed out of 33
tools/tests                           Ran 77 tests, OK
contract_addresses                    pass cited=177 supported=177 unsupported=0
contract_derivations                  pass behaviours=32 gaps=0 multiple=0
instrument_discipline_index           pass shapes=22 unindexed=0
```

`audit_ac6_contract_artifacts.py` reports **7 failures, all in
`analysis/contracts/mission01-native-gate.json`**. That is not a regression: the
file's own `superseded_note` records that it is historical, run by no gate, and
cites seven artefacts that no longer exist. Verified identical at HEAD in a clean
worktree — same seven, same contract — so this cycle changed nothing about it.
Naming it here rather than letting a non-zero exit be absorbed by "it was already
like that".

## Next

The component at `entity+2224` is constructed and destroyed at a fixed offset but
never addressed there at runtime, so something stores a pointer to it. Two
enumerable candidates, both narrow: a `stw` of a register equal to `this+2224`
that my fall-through trace missed on a branch, or a component list built by the
base constructor `0x82282220` at `+428`/`+544`. Whichever it is names the caller,
and the caller's `f1` is `f31` — at which point `sub_82303110` is the flight
integrator outright and the deliverable is a port plus a micro-execution
differential, which is what the plan requires before any flight behaviour enters
the contract.
