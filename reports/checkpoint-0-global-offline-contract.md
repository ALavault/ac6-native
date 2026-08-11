# Checkpoint 0 — global offline contract

Date: 2026-08-11

## Result

The native Linux product now has one global offline ladder, a fail-closed
machine-readable mission-gate template, and an audited capability matrix with
exactly M01 through M15. `MISSION01_LADDER.md` remains a detail ledger and points
to the global roadmap for scope and ordering.

Historical contract handling was already implemented by
`tools/contract_audit_scope.py`: a `superseded_by` contract is excluded from the
live set only when its replacement exists, parses, is not itself historical,
and is not self-referential. Its focused tests and all three contract-wide
audits pass; no replacement was needed.

## Baseline and validation

- `cmake --build reconstruction/ace-combat-6/build -j16`: pass.
- CTest: 69/69 pass; frontend retail resources and Vulkan surface smoke skip in
  the current environment as designed.
- Python baseline: 87/87 pass, plus 14 subtests.
- After adding the global ladder audit: focused 4/4 pass; the complete suite is
  rerun at checkpoint closure.
- Mission 01 final gate: JF pass, no open behaviour.
- Contract artefacts: 5 live, 1 historical, 146/146 match HEAD.
- Contract addresses: 321/321 supported.
- Contract derivations: 52 behaviours, zero gaps, zero multiple derivations.
- Global ladder audit: 15 missions, 4 gates, 8 checkpoints.

## Residual risks and next boundary

The matrix records only demonstrated support: M01 JF is passed; every JV, JP,
JG and all M02–M15 gates remain open. Checkpoint 1 begins with the qualified
926-row DATA.TBL inventory and an atomic RetailContentStore v2 design. No claim
is made yet that the cache contains the complete offline closure.
