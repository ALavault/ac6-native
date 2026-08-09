# Cycle 1371 — the step that threads one float

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus for mnemonics,
  `analysis-input/ACE6_X360.exe` for vtable words, RTTI and float constants.
- **No product C++ changed, no contract changed.** New artefact
  `analysis/flight/flight-model-vtables.tsv`.
- `tools/map_object_layout.py`, added last cycle, was used rather than re-derived.

## The caller of the integrator, and it was never going to be found by offset

Cycle 1370 ended on: nothing forms `entity + 2224` at runtime, so the component
is reached through a stored pointer. Three candidate paths were checked and
**all three came back negative**:

| candidate | result |
|---|---|
| slot-31 dispatch on an object loaded from `entity+4912` | **0 sites** |
| slot-11 dispatch on an object loaded from `entity+4912` | **0 sites** |
| any of the 155 slot-31 sites resolving to `+4912` by dataflow | **0** |

The one that worked was the opposite question: **which slot-31 dispatches are on
the function's own `this`, in a function that is itself a method of this class
family?** Twenty-two dispatch on `this`; exactly **two** are methods of the
family, and one of them is the answer.

## `sub_82283898` is the step, and it is slot 11 of the base class

```
0x822838B4  fmr   f31,f1                  its own float argument
0x822838BC  lwz   r11,112(r31)
0x822838C0  addi  r30,r11,96              r30 = [this+112] + 96
            call slot 30 (this)
            call slot 31 (this, f1 = f31, r5 = r30)     <- sub_82303110
            call slot 32 (this, f1 = f31, r5 = r30)
   if [this+332] bit 0:
            zero this+360, +364, +304, +308, +312
            call slot 33 (this, f1 = f31, r5 = r30)
            call 0x82282938 (this, f1 = f31, r5 = r30)
            call 0x82326FE8 (this)
```

**One float, threaded unchanged into four stages of the update.** That settles
what `f31` in the integrator *is structurally*: not a per-stage constant, but the
step's single scalar parameter, shared by the whole flight update.

## Slots 30, 31 and 32 are pure virtual

In the base vtable `0x82008B10` all three hold **`0x82380428`**, which is
`_purecall`: it loads a handler pointer, calls it if present, then `li r3,25` and
aborts. It appears as an aligned word **357 times** in the image.

So the base declares three pure virtuals and both concrete subclasses override
all three:

| slot | base | `0x8200F270` | `0x8200F310` |
|---:|---|---|---|
| 30 | `_purecall` | `0x82302DB0` | `0x82303E68` |
| **31** | `_purecall` | **`0x82303110`** | `0x823042D0` |
| 32 | `_purecall` | `0x82302C88` | `0x82306038` |

Thirty of the thirty-six slots are inherited unchanged by `0x8200F270`, and slot
0 is the deleting destructor `0x82302C28` found last cycle. The alignment is
exact, which independently confirms `0x8200F270` is a vtable **start** and that
index 31 is index 31.

## The sibling implements the same method, and it is aerodynamics

`0x823042D0`, 505 instructions: normalise by `vrsqrtefp` with a Newton–Raphson
refinement, **two cross products** built from `vpermwi128` with immediates
**135 and 99** — `0x87` and `0x63`, two of the three pairs the plan's
`vpermwi128` arbitration table was derived from — `sin`/`cos` via `0x82380F98`
and `0x82381068`, a chain of clamps, and a final `stvx128 v0,r31,1328` writing a
16-byte vector to `this+1328`.

Two independent implementations of one pure virtual, one scalar and one vector.
That is what the method *is*, and neither had to be guessed from a name.

## And the sibling disables the step

`0x8200F310` overrides slot 11 with **`0x822DDBE8`, which is a single `blr`** —
the image's shared empty virtual, also occupying base slots 19–25, 27, 34 and 35.

So of the two flight models inside one entity, the one at `+3536` — the one the
entity's `+4912` pointer is initialised to — has **no step at all**, while the one
at `+2224` inherits the real one. `sub_82293C28`, the 12,160-byte derived entity,
repoints `+4912` at a third object at `+10672`.

`+4912` is the most-read field in this entire object: about a hundred sites load
it, for `[+332]` flag bits, for floats at `+344`, `+368`, `+376`, `+992`, `+1000`,
`+1148`, `+1268`. It is the *queried* flight model. Whether it is also the
*stepped* one is not established, and this cycle found no path where it is.

## Correcting an assumption I had not written down

Cycle 1368 reported the integrator's stores as `72(r30)`, `68(r30)`, `64(r30)`
without saying what `r30` was. It is **`r5`** — `mr r30,r5` at `0x8230312C`, the
only instruction in 359 that writes it.

So the integrated position is **not a field of the component**. It is at
`r5 + 64/68/72`, and `sub_82283898` sets `r5 = [this+112] + 96`, so the absolute
destination is **`[this+112] + 160, +164, +168`**. Anything that read those three
offsets as belonging to the flight model itself would be reading the wrong
object.

## What the 1/3.6 now suggests, and what it does not prove

Re-read from the image: `0x82069B40` = **0.2777777910232544**, exactly 1/3.6, and
`0x82003214` = **10.0**. Cycle 1368 established the integrator computes
`position += (factor * f31) * rate` per component.

1/3.6 is the km/h→m/s conversion, and this game displays speed in km/h. If the
rates are km/h, then `factor * f31` carries seconds and `f31` is the frame's
elapsed time. That is **an interpretation, and it is written here as one** — the
constant is measured, the unit assignment is not. A capsule that steps the same
object twice with a known `f31` and compares the displacement would settle it,
and that is a differential this campaign is equipped to run.

## Not established

- Who calls slot 11. Thirty-eight slot-11 dispatches exist; their objects come
  from `+4`, `+0`, `+16`, `+220`, `+80`, `+84`, `+76` and a dozen singletons —
  the `+4`/`+0` shape is a list walk, not a fixed field. None is `+4912`.
- What `[this+112]` is, hence what the position block at `+96` belongs to.
- Whether the object at `+2224` is ever the one `+4912` points to.
- The unit of `f31`, per above.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 21 (1351–1371) |
| implementation/integration spent on A3.2 | 3 (1354–1356) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 14 behaviours
ctest                                 100% passed, 0 failed out of 33
tools/tests                           Ran 77 tests, OK
contract_addresses                    pass cited=177 supported=177 unsupported=0
contract_derivations                  pass behaviours=32 gaps=0 multiple=0
```

`audit_ac6_contract_artifacts.py` reports the same 7 pre-existing failures in the
superseded `mission01-native-gate.json`, unchanged from cycle 1370.

## Next

`[this+112]`. It is the context the step passes to every stage, it holds the
position at `+160/164/168`, and it is written by whatever sets up the flight
model — a single field with one obvious writer to find. Finding it also names
the object that owns the position, which is the object a port has to model.

The alternative, if that stalls: `sub_82283898` is 59 instructions with no
indirect state beyond four vtable slots, so it is **micro-executable end to end**
against synthetic objects whose slot 31 is `sub_82303110`. That would measure the
float's propagation rather than infer it, and it is the shape of differential the
plan requires before any flight behaviour enters the contract.
