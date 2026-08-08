# Instrument discipline — the false negative, and how to catch it

`CLAUDE.md` says *measure the instrument before trusting it*. This is what that
means in practice, written from eight failures in one session rather than from
first principles.

## The pattern

Every one had the same shape: **the tool worked and the frame around it was too
narrow.** Every one produced a *negative*. And a negative is the dangerous
direction, because it stops work rather than corrupting it — nothing downstream
contradicts a conclusion you never reached.

| # | the search | what it missed | how it was caught |
|---|---|---|---|
| 1 | retail archives "absent" | ran from a subdirectory | a later `ls` |
| 2 | `+0x188` writers | own output filtered to `^822` | a contradiction, 26 cycles later |
| 3 | `.mdlp` "absent" | `find -maxdepth 3` | a later full-tree count |
| 4 | the definition table | never listed the mission's own directory | reading the bundle |
| 5 | `r25` reassignment | searched `or r25,` not `lwz r25,` | a delegated full read |
| 6 | byte `+0x61` | measured the entry node, not its child | a delegated full read |
| 7 | `+0xC0` readers | two displacement spellings, offset is register-held | a third spelling |
| 8 | `li rX,0xc0` | regex syntax in a substring matcher | **a known-good hit** |

Seven were caught by accident, late. One was caught immediately.

## What made the eighth different

A **known answer to check against**. I had read `li r11,0xc0` minutes earlier, so
zero hits was impossible and the tool was wrong rather than the world.

That is the whole technique:

> Before believing a zero, run the same search against a case where you already
> know the answer is not zero.

If you have no such case, you cannot validate the search, and the negative is not
yet evidence. Say that in the report rather than banking it.

## The specific traps in this repository

- **`Ac6XenonRefs` matches substrings, not regex.** It now warns on regex syntax
  (cycle 1188). A zero under `^`, `$`, `\`, `[`, `|`, `+`, `*` means nothing.
- **`Ac6XenonDisasm` stops at the first undefined byte.** A short result is a
  halt, not an end. Restart past it.
- **Displacement searches miss computed addresses.** `0x188(rN)` will not find
  `addi rX,rY,0x188`, and neither finds `li rX,0xc0` + `stvx128 vrA,rB,rX`. Vector
  slots are almost always register-held.
- **Register searches miss the load form.** `or rN,` is not `lwz rN,`.
- **The canonical Ghidra project cannot decode VMX128** and halts. The Xenon
  import exists for exactly this; use it for anything touching vector code.
- **`-noanalysis` leaves the reference database empty.** `Ac6Xrefs` returns zero
  for data addresses regardless of truth. Use it for call targets, not fields.
- **`find` without `-maxdepth`** — the extracted assets are four to six levels
  deep.

## Two checkers that exist because of this

- `tools/audit_ac6_contract_artifacts.py` — the gate hashes the working tree, so
  an artefact regenerated and never staged passes locally and fails a fresh
  clone. It also checks the hash tables inside capture READMEs, which no contract
  cites and which had three stale rows.
- `Ac6XenonRefs`' regex warning, above.

Both were written after the failure they catch, and both were made to fail once
on purpose before being trusted. **A checker that has never been seen to fail is
not a checker.**
