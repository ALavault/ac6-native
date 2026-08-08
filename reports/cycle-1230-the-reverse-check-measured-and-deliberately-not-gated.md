# Cycle 1230 — the reverse check, measured and deliberately not gated

Cycle 1229 built a gate for the direction *contract → evidence* and listed the
other direction as unchecked. This measures it and decides against gating it,
with the numbers that made the decision.

## The measurement

For every behaviour, extract every `0x8xxxxxxx` from its **derivation** sources
and compare with what the contract declares:

| behaviour | declared | in the source | in source, not declared |
|---|---:|---:|---:|
| `scenario_container` | 4 | 44 | **40** |
| `unit_construction` | 4 | 12 | 8 |
| `sub_mission_flow` | 3 | 12 | 9 |
| `mission_counters` | 3 | 12 | 9 |
| `mission_area` | 3 | 12 | 11 |
| `mission_state_machine` | 3 | 1 | 0 |
| `playable_session` | 3 | 7 | 4 |
| `mission_completion` | 6 | 12 | 7 |
| `texture_decode` | 17 | 16 | 2 |
| `ndxr_container` | 31 | 34 | 9 |

## Why this must not be a gate

`scenario_container` declares 4 and its source cites 44 — because the source
implements **ten readers** and cites each retail function it reproduces. That is
the derivation doing its job. A gate demanding the contract list all 44 would
either bloat every behaviour into an index or push authors to stop citing in the
source, and the second is far worse than the problem.

**`retail_addresses` is a curated summary of what a behaviour turns on, not an
index of what its source mentions.** The asymmetry with cycle 1229's check is
real and deliberate: an address in the contract with no evidence behind it is a
citation to nowhere, while an address in the source and not the contract is
usually just detail.

## What the measurement was still worth

Two of `ndxr_container`'s nine are not detail. `0x82340870` (the NTXR pack
registrar) and `0x8234BEC8` (its insert into registry `0x828C8100`) are the
**other end** of the material → texture join — the half that makes `GIDX+0x08` the
key. The behaviour's statement rests on them and the contract did not name them.
Both are now declared, and both are supported by cycle 1207, already in evidence.

The rest of the nine are sub-addresses inside declared functions
(`0x82355358`, `0x82355394`, `0x8235539C` all sit inside the declared
`0x82355318`) or data globals (`0x828711F0`, `0x828C8100`, `0x828CCB80`), which
is exactly the detail the gate would have punished.

## After

```
contract_addresses=pass  cited=108 supported=108 unsupported=0
contract_artifacts=pass  cited=30 match_head=30 readme_rows=36
mission01_final_gate=audit-valid JF=pass open=none
ctest                    27 tests, all passed
```

## Not established, stated plainly

- Whether the other behaviours' undeclared addresses hide something load-bearing
  the way `ndxr_container`'s two did. **I looked at one behaviour's nine and not
  at the other seven's eighty-eight**, because the ones I could judge were the
  ones I had derived. Somebody who knows `scenario_container` should look at its
  forty.
- The presence test remains a presence test. Cycle 1229 said so and nothing here
  changes it.

## Verification

```
ten behaviours measured; two addresses promoted; 108 cited, 108 supported
```

The contract changed; no product code did.
