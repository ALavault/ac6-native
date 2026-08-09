# Cycle 1494 — the historical contract is not current

## Qualification

- Product target: Ace Combat 6, Xbox 360 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Canonical Ghidra project: `ghidra-projects/ace-combat-6`; it was not opened
  or modified in this cycle.
- No Xenia/oracle pass, controller session, model service, retail extraction or
  retail-data redistribution.
- The pre-existing modified Ghidra preferences/logs and untracked Ghidra/parser
  scripts were left untouched.

## Baseline actually replayed

The existing Release tree was reconfigured with `AC6_BUILD_PLATFORM=ON`, then
built with `-j16`. Qualified headless CTest with
`SDL_AUDIODRIVER=dummy xvfb-run -a` passed **61/61**. Both active JF contracts
passed and the Python tooling corpus passed **79/79** before the change.

The only baseline failure was the all-contract artefact audit. Its seven absent
paths all belonged to `mission01-native-gate.json`, whose own
`superseded_by` field names `mission01-native-gate-v2.json` and whose note says
those retired artefacts are intentionally no longer in the tree. This is the
debt recorded at cycle 1320, not a new product regression.

## Current audit scope

`contract_audit_scope.py` now partitions a supplied contract set before the
artefact, address and derivation audits run:

- a contract without `superseded_by` remains live and is audited unchanged;
- a superseded contract is printed as `contract_scope=historical` and its old
  evidence does not enter current counts;
- the replacement path must exist, parse as a JSON object and differ from the
  historical contract itself.

This is not an unchecked skip flag. Three controls cover a valid replacement,
a missing replacement and self-supersession. The last two fail closed. The v1
contract remains committed as the historical record; no evidence or hash was
rewritten to make it look current.

`CLAUDE.md` records this rule beside the three canonical all-contract commands.
The global goal and compact handoff now focus AC6 Mission 01 JV+JP; Pharaoh,
Ski Park Manager and AC5 remain present and explicitly frozen.

## Gates

```text
fresh configure/build                       pass
qualified headless CTest                    61/61
tools/tests                                 82/82
mission01-final-gate-v3 --require JF        pass
mission01-playable-gate-v1 --require JF     pass
contract_artifacts                          pass, 5 live / 1 historical, 146/146
contract_addresses                          pass, 321/321
contract_derivations                        pass, 52 behaviours, 0 gaps
```

## Not established

- JV and JP are not passed by this cycle.
- No `RetailContentImporter`, content store, persistent frontend or
  `VulkanSceneRenderer` exists yet.
- The 15-mission catalog is an existing provenance input, not yet a cache-import
  coverage matrix.
- No human action is requested; `JP-human-controller` stays reserved until all
  automated gates are closed.

## Next

Build the bounded importer/store foundation: qualify `default.xex`, `DATA.TBL`
and PAC identities, seal an atomic content-addressed cache, expose negative
controls, and turn the campaign catalog into a durable 15-payload coverage
matrix before routing `RetailSession` through it.
