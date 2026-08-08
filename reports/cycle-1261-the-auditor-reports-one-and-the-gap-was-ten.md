# Cycle 1261 — the auditor reports one, and the gap was ten

## Qualification

`default.xex` SHA-256 `acc302c1…11bcde`; instructions re-read in
`ghidra-projects-xenon/ac6-xenon`. **No oracle pass was spent.**

## How this started

Cycle 1259 re-ran the gates after a product change and the v4 contract failed:

```
mission01_native_gate=fail reason=ndxr_container derivation
  reconstruction/ace-combat-6/include/ac6/retail_ndxr_container.h
  never cites retail address 0x8233EF88
```

It was pre-existing — it had been sitting behind an evidence size mismatch, and
**the auditor stops at the first failure**. That is worth stating on its own: a
contract that has been green can be masking a second fault behind the one just
fixed, and re-running after any change is what surfaces it.

## Established — 0x8233EF88, and it is real

The address is the third function in a run that begins with the two magic
tests, `0x8233EF48` for `'NDXR'` and `0x8233EF68` for `'GIDX'`, both branchless
through `subf`/`cntlzw`. It reads the halfword at `+0x08` and masks one bit:

```
8233ef9c  lhz     r11,0x8(r31)
8233efa0  rlwinm. r11,r11,0x0,0x11,0x11    ; MB = ME = 17, so the mask is 0x4000
8233efa4  bne     0x8233effc               ; already resolved: nothing to do
```

That is exactly `NdxrContainer::kResolvedBit = 0x4000`, which the reader already
implemented and had never cited. The citation is now at the constant, with the
instructions that name the bit.

## The finding — the gap was ten, and the other nine are not citations

Rather than fix one and re-run, I enumerated the whole block. **`ndxr_container`
declares 38 retail addresses; ten appear nowhere in its derivation source.** And
a grep across every product source and header shows the other nine appear
**nowhere in the product at all**:

| address | what it is |
|---|---|
| `0x8233EBB0` | the registry find wrapper |
| `0x8234AE00` | the registry `+0x80` hop |
| `0x821EC598` | the `Type` dword decoder |
| `0x82068278` | that decoder's size table |
| `0x820111C0` | inside the vertex-element declaration tables |
| `0x82345098` | beside the stride function `0x82345100` |
| `0x821FC070`, `0x8233E6B0`, `0x821DE790`, `0x821DE898` | investigation findings with no consumer |

These are established results — `audit_ac6_contract_addresses.py` passes at
84 of 84, because each one **is** mentioned by one of the behaviour's evidence
files. They are documented. They are simply **not implemented**, and the
contract declares them as addresses the behaviour derives from.

## The decision, and it is the whole point of the cycle

**I did not remove them, and JV stays red.**

Removing ten addresses from `retail_addresses` would turn the gate green in one
edit, and every individual removal would have a defensible sentence attached —
"this belongs to the texture behaviour", "this is a table, not a derivation".
That is exactly the shape of quietly lowering a bar, and it is easier to do
here than anywhere else in the repository, because the contract is a file I
wrote and the auditor believes it.

The gate is telling the truth. **JV is not green because JV is not done**: the
container behaviour implements 28 of the 38 addresses it claims. A red gate that
states an incomplete behaviour is worth more than a green one that has been
trimmed to fit the code, and the difference between those two states is invisible
six months later unless somebody writes this paragraph.

What the contract needs is not fewer addresses but more implementation — or an
honest split, with the `Type` decoder and the registry hops moved to the
behaviours that will implement them. Both are real work, and neither is a
bookkeeping edit made at the end of a session to see a green line.

## Not established

- **Whether the other nine belong in other behaviours or in none.** The obvious
  reading — that `0x821EC598` and `0x82068278` belong with `texture_decode`, and
  the two registry addresses with a registry behaviour that does not exist yet —
  was not checked against what those behaviours actually implement.
- `0x821FC070`, `0x8233E6B0`, `0x821DE790` and `0x821DE898` were not re-read in
  this cycle; they are listed as uncited, not characterised.

## Correction

- **An earlier cycle of mine put 38 addresses in this block** when the
  derivation implemented 28. The over-claim was invisible because the two cheap
  contract checkers do not test it: `audit_ac6_contract_artifacts.py` checks that
  cited artefacts are in HEAD, and `audit_ac6_contract_addresses.py` checks that
  each address is mentioned by an evidence file. Only the gate auditor requires
  the **derivation source** to cite it, and only for the first failure it finds.
