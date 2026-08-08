# Cycle 1257 — the ordinal rule, and a control Mission 01 cannot give

## Qualification

Instructions re-read from `ghidra-projects-xenon/ac6-xenon`, `default.xex`
SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, over
a listing verified at **912 of 912 instructions of `0x820A7070`, zero gaps**.
**No oracle pass was spent.**

## The defect

`build_units` in `src/retail_mission_state.cpp` advanced a branch's running
ordinal for **every** record whose faction side code was 0 or 3:

```cpp
if (faction.side_code == 0 || faction.side_code == 3) {
  const std::uint8_t branch = faction.side_code == 0 ? 0 : 1;
  const std::uint32_t ordinal = ordinals[branch]++;
```

Retail advances it only for **class-0** records. Both arms that walk the
16-entry participant table re-test the class byte first:

```
820a73f8  lbz    r11,0x8(r11)      arm 0, the class byte again
820a7400  cmplwi cr6,r11,0x0
820a7404  bne    cr6,0x820a7584    a non-zero class skips the walk entirely
820a74bc  lbz    r11,0x8(r11)      arm 3, the same shape
820a74c4  cmplwi cr6,r11,0x0
```

and the two ordinals are bumped only inside the blocks those tests guard:

```
820a748c  addi r22,r22,0x1
820a7550  addi r23,r23,0x1
```

**Exhaustiveness on the entries.** Every branch that reaches either bump block
from outside it was enumerated over the 912-instruction listing: four for each
(`820a7418`, `820a7424`, `820a7430`, `820a7450`; `820a74dc`, `820a74e8`,
`820a74f4`, `820a7514`). All eight originate downstream of the class test, so
there is no path to an ordinal bump that skips it.

## Why this sat unfixed for a cycle

Cycle 1254 found it and filed it rather than fixing it, **because Mission 01
cannot tell the two rules apart**. Mission 01 holds exactly one class-0 record
and it is record 0, so both rules assign it ordinal 0. The retail half of
`retail_mission_state_tests` passes identically under either — and a fix whose
only test is a corpus that cannot discriminate is a fix that will be silently
reverted by the next person who finds the guard "redundant".

## The control

`record_takes_ordinal(class_byte, side_code)` is now a named function carrying
the derivation, and `check_ordinal_rule` scores the rival on chosen values:

| case | derived rule | the rival |
|---|---|---|
| class 0, side 0 / side 3 | takes an ordinal | same |
| **class 1 or 2, side 0** | **no ordinal** | **takes one** |
| **class 3 or 4, side 3** | **no ordinal** | **takes one** |
| class 0, side 1 or 6 | no ordinal | same |
| class 0, no side code | no ordinal | same |

The four bold rows are the discriminator. The rival was **built and run**: with
`return true` in place of `return class_byte == 0`, ctest reports **26 of 27
passing**; with the derived rule, **27 of 27**. The retail half executed in both
runs — only the Vulkan smoke test skips — which is the measurement that Mission
01 agrees with both rules.

## Not established

- **Whether any shipped scenario exercises the difference.** Cycle 1254's corpus
  says the campaign containers cannot: 15 of 15 hold exactly one class-0 record
  at index 0. The non-campaign containers hold up to 30 class-0 records, some at
  non-contiguous indices, so they can — but nothing here ran `build_units` over
  one, because the loader path for those is not the campaign path this product
  implements.
- The fix therefore changes **no observable Mission 01 behaviour**. It is
  correctness against the retail rule, held in place by a control, and that is
  the whole claim.

## Decision taken

Extracted the decision into `record_takes_ordinal` rather than inlining the
extra condition. An inline `&& record.class_byte == 0` would have been smaller
and untestable without a scenario fixture; the named function is reachable from
the chosen-value half, which is the half that runs everywhere.
