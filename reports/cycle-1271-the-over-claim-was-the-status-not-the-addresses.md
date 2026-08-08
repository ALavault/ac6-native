# Cycle 1271 — the over-claim was in the evidence list, and JV was never red

## Qualification

`default.xex` SHA-256 `acc302c1…11bcde`. **No oracle pass was spent.** No
product code changed; the contract changed.

## What was actually wrong

Cycle 1261 found ten addresses the `ndxr_container` behaviour declares and no
product source implements, and refused to remove them — correctly, because they
had not been read. Cycles 1262, 1263 and 1266 read all ten. With that done, the
shape of the error is visible, and it is **not** in the address list:

1. **`ndxr_container` is declared `status: "passed"`** while the auditor rejects
   it. A contract asserting a pass the auditor refuses is the over-claim.
2. **Both `retail_ndxr_container.h` and `.cpp` are listed as `derivation`
   evidence**, and the auditor requires *every* derivation file to cite *every*
   address. The header cites **28 of 28**. The `.cpp` cites **8 of 28**.

The second is mine and it is the real defect. The header is where the derivation
is written — stage by stage, each with the address that establishes it. The
`.cpp` implements it. Listing the `.cpp` as a derivation asserts that it
independently derives all twenty-eight, which it does not, and which it could
only be made to satisfy by pasting twenty addresses into comments beside code
that does not implement them. That is what task #16 warned against in its own
text: *"Do not cite it in a comment that does not correspond to code: that would
satisfy the auditor and defeat it."*

## What changed

- The ten unimplemented addresses moved out of `retail_addresses` into a new
  **`established_addresses_not_implemented`** map, each with the reading that
  attributes it — the registry find and `+0x80` hop, the `Type` decoder and its
  size table, an element-list terminator, the `3 × 6 × 8` clear, the D3D
  resource-type dispatch, the container initialiser, the device-state setter and
  the vertex declaration allocator. **Nothing is lost**: they stay in the
  contract, with a sentence each, and the auditor ignores the field.
- `retail_ndxr_container.cpp` removed from the `derivation` evidence. The header
  is the derivation and cites everything it claims.

`retail_addresses` is now 28, all cited by the sole derivation file, and the v4
contract audits clean.

## And JV was never red

I have written "JV is red" several times in the last few cycles. **That was
imprecise, and the repository already said so.** `MISSION01_LADDER.md`, line
181, unchanged for many cycles:

> There is deliberately **no `--require JV`** in the auditor. One domain of
> several is not a gate, and adding the name before the domains exist would make
> the gate assert something no evidence supports.

Confirmed against the tool: `--require` accepts `J0`, `J1`, `retail`, `JF` and
nothing else. **JV is not a gate that fails; it is a gate that does not exist
yet.** What was failing was the v4 contract's internal consistency, which is a
different thing and is what this cycle fixed. Calling that "JV is red" gave a
missing gate the appearance of a measured verdict.

This is the second time this session the repository knew something I asserted
otherwise — the first was the `.pdata` distinction, which cycle 1225 had printed
and cycle 1265 lost.

## The cost, stated

`reconstruction/ace-combat-6/src/retail_ndxr_container.cpp` **is now cited by no
contract**, so nothing pins its hash. An edit to it will not be caught by
`audit_ac6_contract_artifacts.py`, because that tool checks cited artefacts and
it is no longer one.

The evidence vocabulary has no kind for "the implementation of a derivation" —
`derivation`, `static`, `microexec`, `differential`, `capsule`, `native-test`,
`native-capture`, `bridge`, `retail`. Adding one is a change to the gate's
schema and belongs in its own decision, not in a cycle that was correcting an
attribution. The file remains built and tested; only its pin is gone.

## Corrections

- **My own, repeated:** "JV is red" should have been "the v4 contract fails its
  own consistency check; JV has no gate".
- **`MISSION01_LADDER.md` item 1 of the JV list** said the halves were fused in
  cycle 1144. True of `RetailSession`, the class. False of
  `run_retail_session`, the command, which called `render_world_markers` never
  until cycle 1269 — 125 cycles of a HUD-only capture behind a ladder entry
  marked done. The item now names both.
