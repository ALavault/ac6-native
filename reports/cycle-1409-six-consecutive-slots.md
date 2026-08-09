# Cycle 1409 — six consecutive slots

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- ctest 49 → **50**. One contract entry added: **28 behaviours**.
- New artefacts `analysis/flight/flight-input-router-microexec.tsv` and
  `tools/audit_flight_input_router_microexec.py`.

## The question cycle 1408 left, answered

Cycle 1408 ended: *"`0x82229250`'s other four writes — what fills `+2096`,
`+2100`, `+2112` and `+2116`."*

A bounded scan over the whole corpus finds **three** functions that write those
offsets: `sub_82227E10` (2), `sub_82229250` (22), `sub_822338F0` (4). Tracing
each store in the big one to its source register's provenance gives the answer in
one table, and it is duller and better than expected:

```
0x82229360  +2096  <- [r28+3688]
0x82229368  +2100  <- [r28+3692]
0x82229380  +2112  <- [r28+3680]
0x82229388  +2116  <- [r28+3684]
0x822293E4  +2096  <- [r28+3684]     <- a different path, and SWAPPED
0x822293EC  +2100  <- [r28+3680]
0x82229464  +2104  <- [r28+3672]
0x8222946C  +2108  <- [r28+3676]
```

`r28` is the binding-layer object, and `[r28+3672]`/`[r28+3676]` were already
established at cycle 1404 as the first two outputs of its first output array.
So **all six entity input fields are that same array**, at six consecutive
offsets 3672…3692 — the six values `apply_input_binding` produces.

The remaining fourteen stores all load `[r11+2092]` where `r11` is `0x82000000`;
that address holds `0.0`. They are clear-to-zero paths, not sources, and reading
them as sources is what made the first pass of this scan look like twenty-two
different answers.

## Three arms, not one mapping

The routing is selected by two words:

- the **device mode**, resolved before the block through
  `[0x826F4EB4] + index*42976 + 19588` with `index` clamped to 0..2. Zero enters
  at `0x82229310`; anything else at `0x82229370`.
- the **layout** word `[0x826EDB5C]`, compared against 1 at `0x82229374`.

| arm | +2096 | +2100 | +2104 | +2108 | +2112 | +2116 |
|---|---|---|---|---|---|---|
| digital device | array[4] | array[5] | array[0] | array[1] | bit 2 | bit 3 |
| analog, layout ≠ 1 | bit 4 | bit 5 | array[0] | array[1] | array[2] | array[3] |
| analog, layout = 1 | array[3] | array[2] | array[0] | array[1] | bit 5 | bit 4 |

**Layout 1 transposes both pairs and reverses each.** The analog pair layout 0
sends to `+2112/+2116` lands on `+2096/+2100` swapped; the bit pair layout 0
sends to `+2096/+2100` lands on `+2112/+2116`, also swapped. That is the retail
control-layout option, and which of the two the menu calls which is a label this
port does not have and does not invent.

`array[0]` and `array[1]` reach `+2104` and `+2108` on **every** arm, in the
common tail at `0x82229460`.

## The differential rejected my table before the port was written

`tools/audit_flight_input_router_microexec.py` enters at whichever arm head the
case selects, seeds the binding object and the layout word, and stops before
`0x82229470` with a step count **computed by simulating the block's own control
flow** — so `exit == step_limit` is a real assertion rather than a hope.

The first run: **13 cases, 78 values, 3 failures.** All three were mine. I had
written the layout-1 digital pair as bits 2 and 3, copied from the digital arm;
`rlwinm r11,r11,0,26,26` at `0x822293F4` and `,27,27` at `0x82229420` say bits
**5 and 4**. Corrected, it is 13 cases, 78 values, **0 failures**.

That is the instrument doing the job it exists for, one step before the wrong
table would have reached a header and been read back as established.

## Two things the harness taught, again

- **Entering mid-function loses the prologue's registers.** `f28` is the `0.0`
  every branch selects against, loaded at `0x82229274`, which the mid-function
  entry skips. The emulator zero-filled it and the answer came out right *by
  accident*; the spec now seeds `fpr f28 f:0.0` so it is right on purpose.
- **A stub returns without touching `r3`.** The common tail calls `0x82228480`
  with `r3 = entity` and branches on `clrlwi r11,r3,24`, so the **entity's own
  address** decides the path. It sits at `0xB8000000` deliberately, and the
  measured path is the one that does not suppress `+2096`.

## What is ported, and what is not

`route_flight_input_fields` covers `0x82229310..0x8222946C`. Three things next
to it are deliberately outside it, and the header says so:

- the **config chain** producing the device mode — three indirections through
  globals this port has no state for, so the mode is a parameter;
- the **`+2096` suppression** at `0x8222945C`, gated on the 35-instruction
  predicate `0x82228480` and the entity byte at `+10116`, both unread;
- the **layout-1 response curve** at `0x82229470`, which rewrites `+2104` and
  `+2108` as `sign(x)·(1 − cos(|x|·π/2))`, snapped to `sign(x)` once that exceeds
  `0.9` (π/2 at `0x82069E48`, `0.9` at `0x820078C0`). It runs on layout 1 only,
  so on layout 0 the six returned values are final rather than pre-curve.

## The demo's invented list is one step shorter

`demo_flight_input.cpp` now builds the binding layer's six outputs and hands
them to retail's own router. What was chosen is now only **which raw axis fills
which of the six slots**, plus the choice of arm.

It also **deleted an invention rather than confirming it**: the previous version
fed the analog trigger straight into `+2096`, and on the analog arm retail puts a
*button bit* there.

The frames came back byte-identical, and the README says why that proves almost
nothing: the only field whose source changed is `+2096`, and the thirty-second
pitch-roll-centre manoeuvre never touches the trigger. The router is verified by
its differential, not by the pictures.

## A predecessor corrected

`retail_flight_input_apply.h` said *"What fills +2096, +2100, +2112 and +2116 is
NOT established"*, and the contract entry's derivation claim repeated it. Both
are updated. The header now also warns what its own six-field struct hides: on
two arms some of those fields carry `1.0`/`0.0` from a button bit, so a caller
reading that header alone would be wrong to treat all six as continuous.

## Not established

- **Which arm the game actually runs.** The device mode comes from a config
  chain that was bounded, not read, and the layout word from a global nothing in
  this cycle traced a writer for. The table is complete; which row is live is not.
- What `sub_822338F0`'s four writes to the same fields are for.
- The `+2096` suppression predicate, and the layout-1 curve — both named above.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 28 behaviours
ctest                                 100% passed, 0 failed out of 50
```

## Next

The **layout-1 response curve** at `0x82229470`. It is 34 instructions, one
callee (`0x82381068`, a libm-shaped routine that must be identified before it is
named `cos`), two constants already resolved, and it sits on the two axes the
demo actually drives — so unlike this cycle's `+2096` change, porting it would
move the pictures. It is also the last thing between `0x82229250`'s six fields
and a complete account of what reaches the flight model on either layout.
