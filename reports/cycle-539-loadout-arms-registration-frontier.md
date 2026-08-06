# Cycle 539 — loadout ArmsManager registration frontier

Date: 2026-08-02

## Qualification

- Target: Xbox 360 PAL `default.xex`.
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`.
- Native worktree HEAD: `6c417820fd6a89aade3cb02c53ddf4f222b81991`.
- Cycle-539 runtime SHA-256:
  `29a96a6057de473770038478ad34d90b6abfc253e40f236f92ffd06e2ac0cbb4`.
- Runtime profile was isolated under the cycle output. Both experimental
  loadout force options were disabled.
- Generated C++ was used only for revision-pinned literal ABI/control-flow
  cross-match and was not modified.

## RTTI correction

The `.rdata` blocks previously described as unclassified function tables are
qualified virtual tables:

- `0x82054E6C`: `ACE6::CAce6ArmsManager`, complete-object locator
  `0x8206D3B0`, TypeDescriptor `0x8268F49C`;
- `0x820551B4`: `CX360ArmsManager`, complete-object locator `0x8206CF8C`,
  TypeDescriptor `0x8268F23C`.

Each table contains 39 slots. Thirty-four are identical; the five differences
are slots 0, 2, 3, 4 and 6. Shared slot 38 is `sub_82221A28` at
`0x82054F04`/`0x8205524C`.

`sub_82221A28` is not the ArmsManager constructor. It is a shared virtual
factory method that constructs a `0x600`-byte object at
`storage + index * 0x600`, installs vtable `0x820073B0`, then calls its slot
`+0x38`. That object's slot `+0x7C` is `sub_822FA748`, the selector-to-events
202–208 translator.

Canonical split address materialization identifies the real constructors:

- `sub_8221E6C0` installs the derived vtable from `0x8221E6D4..0x8221E6E0`;
- `sub_82094140` installs the base vtable during its destruction path from
  `0x82094150..0x82094158`.

## Missing lifecycle chain

Cycle 537 observes no call to `sub_8221E6C0`, `sub_82221A28` or
`sub_822FA748`. Therefore the derived ArmsManager itself is absent on the
bounded first-mission route, before any weapon-object factory or event
translation can occur.

Canonical `sub_82199F68` owns the mode-dependent ArmsManager lifecycle:

- only command `-3` enters the construction/publication path;
- it selects the allocation/constructor path from `level_root+120` mode;
- it publishes the resulting manager at `owner+648`;
- commands `-2` and other values take separate teardown/counter paths.

Cycle 538 observes no invocation of `sub_82199F68`, so the defect is not a bad
command value or wrong mode inside that function.

Canonical `sub_82199D08` registers callback `sub_82199F68` through
`sub_8219AAE8` on the owner subobject at `+0x268`. It appears in seven virtual
tables. In particular, it is slot `+0x64` (index 25) of the qualified
`CAce6MissionManagerCampaign` vtable `0x8206432C`.

Cycle 539 observes no invocation of `sub_82199D08`. The first missing native
transition is therefore the campaign mission manager's virtual registration
stage, upstream of the entire ArmsManager chain:

```text
CAce6MissionManagerCampaign slot +0x64 / sub_82199D08
  -> register sub_82199F68 on owner+0x268
  -> lifecycle command -3
  -> construct/publish CAce6ArmsManager at owner+648
  -> virtual slot 38 / sub_82221A28
  -> object vtable 0x820073B0
  -> slot +0x7C / sub_822FA748
  -> manager events 202..208
```

This chain directly explains why weapon quantities can remain empty. It does
not yet prove that the aircraft capability polygon uses the same ArmsManager
publication; aircraft-stat publication still needs a separate consumer join.

## Runtime evidence

Accepted scenario:
`scripts/ac6-first-mission-loadout-native-ready-probe.steps` with a fresh
per-cycle `user_data_root`. Reusing the global profile was rejected because
cycles 533–536 changed the save-dialog state and bypassed the recipe's initial
`type28=30` predicate.

- Rotated log SHA-256:
  `d8d58cde720bf3bccf05ccb9dc375cb3d1bfb4a47eea09cb0034cf9e294a26d6`.
- Current log SHA-256:
  `27411ef153250b437cda427ba1f766a36e0e81669777d565c69361b92b61e27d`.
- Aircraft-screen capture SHA-256:
  `9e9430ec1974db1e09b8ce349065102b251f637baeba5aa65d0acfcbe56bdf91`.
- Final observation capture SHA-256:
  `536efb13ea0eb54837e7922e11ba08574e81f70b5a6cfefb6dafed4ae4256bd3`.

The control remains unchanged: the loadout state setter requests only zero
twice and the manager receives event 22 twice. No registration, ArmsManager,
slot-38 factory or command-translator log is present.

The reference native build succeeds and the targeted AC6 CTest suite passes
6/6 after the registration/lifecycle probes.

## Next bounded checkpoint

Trace the active `CAce6MissionManagerCampaign` receiver and the caller that
should invoke virtual slot `+0x64`. Determine which qualified lifecycle stage
is skipped and why. Repair only that invocation/owner contract, then require
the natural sequence registration -> command `-3` -> non-null `owner+648` ->
weapon publication with both force options disabled. Do not call
`sub_82199D08` or synthesize ArmsManager state from a probe.
