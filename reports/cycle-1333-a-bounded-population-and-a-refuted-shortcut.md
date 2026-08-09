# Cycle 1333 — a bounded population, and a refuted shortcut

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** Nothing executed.
- No product C++ changed, no contract changed.
- **The question this cycle set out to answer is not answered**, and the report
  says so rather than picking the most plausible candidate.

## The population of `unit+0xE0` writers

`0xE0` is 224, and a displacement scan for it is the collision trap in its purest
form: **179 stores at `+224` across the image**, most of them `224(r1)` — stack
frames with nothing to do with `CAce6Unit`.

Dropping the stack base leaves **45 sites in 38 functions**, one of which is the
constructor `0x822A2330` writing zero. So 44 candidates, and a candidate list is
not an answer.

`CLAUDE.md` prescribes the next step — *a field belongs to the structure its
neighbours belong to* — so the filter was applied mechanically: for each site,
look eight instructions either side for another access **on the same base
register** at an offset the constructor establishes (`+0x60`, `+0x10`, `+0x80`,
`+0xD0`, `+0xD4`, `+0xD8`, `+0xDC`, `+0xE4`). That leaves **32 sites**, four of
which carry the full constructor neighbourhood:

```
0x820E5170  sub_820E4F08     0x820E57C4  sub_820E5550
0x821CBDB0  sub_821CBC10     0x822A23B0  sub_822A2330  (the constructor, writing 0)
```

**This is plausibility, not proof, and the campaign has a rule about that.** Cycle
1242 established that a plausibility control is only strong where the field
borders a differently-encoded one, and `+0xD0`…`+0xE4` is a run of same-width
words. Several unrelated classes can have a pointer run there. So the 32 are
recorded as a bounded population and none of them is named the writer.

## The shortcut, and it is refuted

The unit constructor has exactly two callers: `0x822986C0`, and `0x822A6574` —
which is inside the **player** constructor `0x822A6560`, chaining to its base.
That is ordinary C++ base-class construction and it is the first thing in this
chain that behaves like an inheritance rather than a composition.

The player factory is called from `0x820A7FB8` and `0x820A803C`. One of the
`+0xE0` write candidates sits at `0x820A7C80` — a few hundred bytes earlier, with
three of the constructor's neighbours on its base. **"The function that creates
the player also wires `+0xE0`"** is an attractive sentence and I nearly wrote it.

It is false. `0x820A7C80` is in `sub_820A7070`, which ends at `0x820A7EAF`. The
factory calls are in `sub_820A7F48`. **Different functions**, adjacent in the
image, and image order is not evidence — the *refuted link* shape, which this
file's own discipline index lists and which cost one command to check.

## What is established

- `unit+0xE0` is written zero at construction and is a pointer filled later.
- The writer is in a bounded set of 44 sites, 32 after a plausibility filter.
- `CAce6Unit`'s constructor has two callers, one of them the player constructor
  chaining to its base.

## Not established

- Which site writes `unit+0xE0`, and what the container is.
- Why a unit has two `CGaLocator`s, and what separates `+0x10` from `+0x80`.
- What `0x82296E40` passes as the argument block.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

The two locators are the better thread, and it is better precisely because it
does not need a scan. `+0x10` and `+0x80` are constructed identically, so what
separates them is in their *users* — and the users of `+0x80` are already known
from four cycles of work. Reading the users of `+0x10` is a bounded question with
a known-good method, where `+0xE0` is a scan with a plausibility filter and no
control.
