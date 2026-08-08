# Cycle 1168 — a contradiction in my own reading of 0x820A7070

## The contradiction

Cycles 1148 through 1159 all rest on one sentence: in `0x820A7070`'s per-record
loop, `r28` is word 0 of a 0x20-byte record taken from `[r25 + 8]`, and `r25` is
the function's first argument. Cycle 1163 then derived that the first argument is
a `CX360UnitManager`.

Both cannot be true as I have been reading them.

```
820a7090  or   r25,r3,r3      ; r25 = the first argument
...
820a7c00  lwz  r11,0x0(r25)   ; the loop bound:
820a7c10  lbz  r11,0x0(r11)   ;   a BYTE at [[r25+0]+0]
820a7c14  cmpw cr6,r24,r11
820a7c18  blt  cr6,0x820a76f8
```

If `r25` is a `CX360UnitManager`, then `[r25 + 0]` is its vtable pointer — the
constructor `0x82270280` writes one at `0x822702A8` — and `lbz` of that reads the
first byte of a vtable as a record count. That is not a thing this code would do.

So one of the following is wrong, and I do not yet know which:

- `r25` is reassigned between `0x820A7090` and the loop, and I have not found it.
  A search of `0x820A7220`–`0x820A7700` shows no `or r25,…` and no reload from
  the stack slot `0x154(r1)` where the argument is saved.
- one of the three instructions above is misread.
- the first argument is not what cycle 1163 established.

## Why it is being written down rather than resolved

The resolution needs a careful pass over the whole of `0x820A7070`, which runs
from `0x820A7070` to about `0x820A7E9C` and which I have so far read only in
windows — and reading it in windows is exactly how the contradiction survived
this long. That pass is delegated and running.

Publishing the contradiction first matters because four cycles of downstream
reasoning cite the sentence. Specifically at risk:

- cycle 1148's *"`r28` is word 0 of the 0x20-byte record array"*, and with it its
  conclusion that byte `+0x61` lives on a third structure. That conclusion also
  has independent support — `+0x61` measured as zero across all 230 unit record
  blocks and all 434 Obj node blocks, with the block spacing checked — so it
  survives even if the provenance of `r28` changes.
- cycle 1159's reading that the container at `r1+0x80` comes from the first
  argument's vtable slot `+0x0C`. That one is on firmer ground: it is a direct
  read of `0x820A70B4`–`0x820A70D4`, and cycle 1163 confirmed the writer's
  arithmetic against the MDLP header independently.

What is *not* at risk is anything measured from the container files themselves —
the MDLP index, the pack layout, the decoder — none of which depends on this.

## The rule this breaks

"Measure the instrument before trusting it." I read `0x820A7070` in eight
separate windows across ten cycles and never once read it end to end, and a
contradiction between two of those windows sat unnoticed because no single view
contained both. The campaign has now produced four scope errors and this, and
they share a shape: the tool was fine and the frame I put around it was too
small.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed. Nothing in the product cites the disputed sentence.
