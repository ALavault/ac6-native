# Cycle 1349 — the array is the fifth argument

## Qualification

- Ghidra was used once, to list call sites. **No oracle pass.**
- No product C++ changed, no contract changed.

## Where a unit's child array comes from

Eleven instructions at `0x820A7C58`, in the dispatch arm cycle 1345 located after
the last of five jump tables:

```
lwz  r11,0(r25)          r11 = [r25]
addi r10,r17,2           index + 2
lwz  r9,356(r1)          the pointer held in that stack slot
rlwinm r10,r10,2,0,29    (index + 2) * 4
add  r10,r10,r9
lbz  r11,0(r11)          a BYTE
stw  r10,216(r16)        unit+0xD8 = table + (index + 2) * 4
stw  r11,220(r16)        unit+0xDC = that byte
lwz  r11,4(r26) ; stw r11,224(r16)    unit+0xE0 = [r26 + 4]
lwz  r11,0(r26) ; stw r11,228(r16)    unit+0xE4 = [r26 + 0]
```

And `r1+356` has exactly **one** writer, `stw r5,356(r1)` in the prologue.

**The child array is a window into a table the caller passes as its fifth
argument.** Nothing is allocated here; `unit+0xD8` is an address computed into
somebody else's table, at a stride of four bytes per unit index.

Three details worth keeping:

- **the count is one byte**, zero-extended into the word at `+0xDC` that cycle
  1341 read back with `lwz`;
- `+0xE0` and `+0xE4` come from **one two-word structure, read in reverse** —
  `+0xE0` takes the second word;
- so the four fields the constructor zeroes together and this arm writes together
  come from **three different sources**. "One cluster" describes where they live,
  not where they come from.

## What is path-dependent and therefore not established

The count's base register `r25` has three writers — `mr r25,r3` in the prologue,
`lwz r25,8(r26)` at `0x820A76C4`, and `lwz r25,340(r1)` later. The second is
before the read, so which one is live at `0x820A7C58` depends on the arm taken.

Cycle 1330 published a register as constant on exactly this mistake and had to
withdraw it inside the cycle. It is not repeated here: the count's source is
listed as unresolved.

## The link cycle 1348 named is still open, and the population grew

To show the children carry vtable `0x820078D0` I now need to know what the
**fifth argument** holds, and `sub_820A7070` has **nine** call sites.

That is a narrowing and a widening at once. Where the array comes from went from
unknown to precise; what it contains went from one function to nine callers. The
cycle is honest about which of those it did.

## Not established

- What the caller-supplied table holds.
- The count's source.
- Whether the children carry `0x820078D0`.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none, 11 behaviours
ctest                                100% passed, 0 failed out of 30
tools/tests                          Ran 72 tests, OK
```

## Next

The nine call sites, and what each passes in `r5`. Nine is small enough to read
completely — the size this thread has learned to prefer, and the fourth time the
answer has turned out to live one level up from where the search started.
