# Cycle 1483 — the agent was right

## Qualification

- **No Ghidra run and no oracle pass.** The image via `tools/ppc_read.py`.
- No product C++ changed; ctest stays **60**. **No contract entry.**

## Correcting cycle 1482, which corrected an agent

Cycle 1482 read a subagent's claim that `tone%s.xml` is loaded and freed with
nothing kept, checked it, and returned a split verdict: the free is real, but

> "a store to `[r10+0x1398]` happens **immediately before** the free. So
> something is kept. The agent's conclusion does not follow."

Two more instructions settle it. The stack slots are filled in the loader's
prologue, four hundred instructions earlier and long before any file is touched:

```
0x820FBC40  addi r30,r11,-0x63B0     r30 = 0x829B9C50, a global
0x820FBC54  stw  r30,0x60(r1)
0x820FBC60  addis r10,r31,0x8        \  r10 = this + 0x79604
0x820FBC6C  addi  r10,r10,-0x69FC    /
0x820FBC7C  stw  r10,0x64(r1)
0x820FBC80  stw  r11,0x1388(r30)     [global+0x1388] = this + 0x79518
0x820FBC8C  stw  r11,0x138C(r30)     [global+0x138C] = this + 0x7956C
```

So `0x820FCA7C`'s

```
stw r11,0x1398(r10)      ->   [0x829B9C50 + 0x1398] = CMapManager + 0x79604
```

is **a registration of a sub-object pointer into a global**, in exactly the
pattern the prologue already uses at `+0x1388` and `+0x138C`. Its value was
computed before the file existed and does not depend on it.

> **Nothing derived from the tone XML is kept at that site. The agent was right
> and cycle 1482 was wrong.**

Cycle 1482 refused a claim on the strength of a reading, and the reading was
mine. It was careful in the right direction — an agent's output is a claim, and
checking it was correct — and it stopped one instruction short of the answer,
which is the same *stopping at a natural boundary* this file indexes.

## What it costs cycle 1481

The post-process values are still in the archive and the file still states them.
What is now established is that **the map loader does not extract them at
`0x820FCA44`** — it loads the bytes and frees them.

So `apply_mapset_post`'s twenty numbers describe a file this loader discards.
They may still be the map's intent; they are no longer supported by "retail reads
this". The header claims only that the values are retail's, which remains true,
and that is the whole of what it claims.

## The one caveat, stated rather than assumed

`0x82101A18` is the same loader used for `.mha`, `.mhd` and `.pdl`, and for those
the caller keeps the pointer. Here the caller frees it. **Whether `0x82101A18`
itself parses or registers anything as a side effect is not established**, and
until it is, "the values never reach runtime" is a claim about this call site
only.

## Not established

- `0x82101A18`'s side effects.
- What `CMapManager + 0x79604`, `+0x79518` and `+0x7956C` are. Three sub-objects
  registered into one global at `0x829B9C50` — and `+0x7956C` is the sub-object
  cycle 1462 already tripped over, when it mistook that object's `+0x30` for
  `CMapManager`'s.

## Gates

```
mission01_final_gate (final-v3)         JF=pass open=none
mission01_final_gate (playable-v1)      JF=pass open=none, 34 behaviours
ctest                                   100% passed, 0 failed out of 60
tools/tests                             Ran 79 tests, OK
```

## Next

**`0x829B9C50`.** Three sub-objects of the map manager register themselves into
one global at `+0x1388`, `+0x138C` and `+0x1398`, and one of them has already
cost a cycle by being mistaken for the map manager itself. Naming that global —
`tools/whose_vtable.py` on what it holds — is one command, and it turns three
unexplained offsets into a structure.
