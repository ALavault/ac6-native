# Cycle 1258 — the product classifies a campaign mission by the online arm

## Qualification

Instructions re-read from `ghidra-projects-xenon/ac6-xenon` over a listing
verified at **912 of 912 instructions of `0x820A7070`**; vtable slots and RTTI
read directly from `analysis-input/ACE6_X360.exe` (`default.xex` SHA-256
`acc302c1…11bcde`). **No oracle pass was spent.** No product code changed in
this cycle — an attempted change was reverted, and why is the report.

## What I set out to do

JV 2b unblocked in cycle 1254: the campaign path identifies the player's unit by
**class byte 0**. The product has no notion of a player unit on the retail path
at all — only `LocalPlayerSlot`, which is the online participant model supplied
by the caller. Adding `campaign_player_record` to `RetailUnitBuild` looked like
the obvious next step.

It compiled, the suite stayed at 27 of 27, and **it could never fire.** Finding
out why produced the actual result.

## Established — the campaign path never reaches the faction switch

The constructor calls the mission manager's virtual slot `+0x04` twice and tests
its result against 2 and then 3:

```
820a7330  lwz    r11,0x4eb4(r18)
820a7334  lwzx   r3,r11,r19
820a7338  lwz    r11,0x0(r3)
820a733c  lwz    r11,0x4(r11)
820a7340  mtspr  CTR,r11
820a7344  bctrl                     ; r3 = the manager's category
820a7348  cmpwi  cr6,r3,0x2
820a734c  beq    cr6,0x820a7374     ; category 2 -> r11 = 1
820a7350  ...                       ; the same call again
820a7368  cmpwi  cr6,r3,0x3
820a736c  or     r11,r24,r24        ; r24 is zero, so r11 = 0
820a7370  bne    cr6,0x820a7378     ; category != 3 -> r11 stays 0
820a7374  li     r11,0x1            ; category 2 or 3 -> r11 = 1
820a7378  rlwinm r11,r11,0x0,0x18,0x1f
820a737c  cmplwi cr6,r11,0x0
820a7380  beq    cr6,0x820a7608     ; r11 == 0 -> SKIP the whole faction block
```

**The campaign manager's category is 1.** Read from the image rather than
inferred: vtable `0x82064264` carries RTTI `.?AVCAce6MissionManagerCampaign@ACE6@@`
at `vtable−4`, and its slot `+0x04` is `0x82266390`:

```
82266390  li  r3,0x1
82266394  blr
```

For comparison, the base class's slot `+0x04` is `0x822663A8` and the replay
class's is `0x82199B70` — different functions, so the value is a class property
and not a shared stub.

Category 1 satisfies neither test, `r11` stays 0, and `820a7380` branches past
`0x820a7608`, **skipping the nine-way faction switch, the participant table and
the side flags entirely.** The class-byte switch at `820a72c0`–`820a7330` has
already run by then and its `r15`/`r14` stand.

## The defect

`build_units` passes `faction.side_code` unconditionally:

```cpp
const std::optional<UnitClassification> classification =
    classify_unit_record(record.class_byte, faction.side_code, is_local_player);
```

`classify_unit_record` models both arms correctly — its own header says
`side_code` "only applies when the game reports mode 2 or 3 with a faction table
present; pass nullopt for the other modes". **The caller never honours that.**
Every Mission 01 faction entry carries side code 0, so all 230 units are
classified through the online arm of a campaign mission.

The consequence is not cosmetic. Under the campaign arm each unit takes
`flags = 0` and `category = object_category(class_byte)`; under the arm the
product actually runs it takes `flags = kSideFlagsFirst` and
`category = 1 or 2`. Those are the values `retail_mission_state_tests` asserts,
so **the test currently pins the wrong model** rather than catching it.

This also explains the reverted addition: `record_is_campaign_player` answers
false whenever a side code is present, which — through this defect — is always.
Wiring it in would have added a field that is always empty and a test that passes
for the wrong reason.

## Not established, and why the fix is not in this cycle

- **What `build_units` should consult to know the mode.** Retail asks the
  mission manager object at `*(0x826E4EB4) + 0x29C80`. This port has no mission
  manager; the signature would have to carry the category, and choosing where it
  comes from is a design decision that belongs with its own control, not with a
  bug report.
- **The corrected expectations.** Changing this alters the asserted category and
  flags of all 230 Mission 01 units in a JF-gated behaviour, and the contract
  evidence for `retail_mission_state.cpp` would move with it. Re-deriving 230
  expected values and refreshing four contract entries in the same cycle that
  discovered the defect is exactly how a wrong number gets committed with
  confidence.
- **Whether any other consumer makes the same assumption.** Not swept.

## Decisions taken

- **Reverted rather than shipped.** A field that cannot be set and a helper that
  cannot return true are worse than the gap they fill, because the next reader
  sees coverage.
- **Filed the defect with its derivation** instead of fixing it under time
  pressure at the end of a long session. The instruction chain above is the
  whole of what a fix needs; nothing about it will be cheaper to re-derive later.

## Correction

- **Cycle 1254** wrote that "the entire category-2/3 block of `0x820A7070` is the
  online path" and I carried it as context. It is right, and I did not follow it
  to its consequence for the product until this cycle — the same sentence had
  been available for two cycles and I read it as a fact about retail rather than
  as a claim about our own code.
