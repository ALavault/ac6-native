# Cycle 80 — AC6 NDXR model-layout coverage

## Full-corpus structural gate

After the NTXR framing fix, the manifest attempts `parse_ndxr_model` for every
bounded NDXR wrapper but records a rejected layout instead of aborting the
archive inventory. The initial PAL corpus completed within the 55-second bound:

```text
ndxr_models_parsed=2143
ndxr_models_rejected=85
ndxr_model_rejection_reasons=NDXR vertex records exceeds its payload:85
```

The first reproducible rejection was DATA.TBL entry 346, nested FHM path
`0.16`, a 68,054-byte NDXR payload. Its wrapper/header bounds remained valid;
the rejection occurred later while validating vertex-record extent.

## Consequence

The 2,143 accepted models established that the existing bounded parser reaches
positions, UVs, and indices for most observed layouts. The 85 rejected
wrappers were not silently truncated or treated as renderable. Their common
failure isolated a layout-recovery task, not a reason to weaken the bounds
check or claim a new Xenos format.

## Recovered `0x07` geometry stride

The rejection diagnostics showed all 85 failing polygons at raw vertex format
`0x07`: 84 with UV format `0x11`, one with `0x21`. Every affected payload had
space for a 44-byte combined record but not for the prior 52- or 60-byte
assumption. The observed layouts therefore establish a 36-byte `0x07`
geometry portion followed by the existing 8- or 16-byte UV tails.

`vertex_record_size(0x07)` now returns 36. A compact unit fixture deliberately
contains only one 44-byte `0x07`/`0x11` record, so it would fail under the old
52-byte interpretation. The renewed full bounded archive pass reports:

```text
ndxr_models_parsed=2228
ndxr_models_rejected=0
```

This is structural traversal evidence only: it does not establish material
semantics, Xenos draw submission, or Xenia parity.

The native scene shell and its established campaign-scene tests are unchanged.
This is neither Xenia parity nor mission-flight proof.

## Validation

- `ac6-ndxr-tests`: **1/1 passed**;
- full archive manifest: completed under the explicit 55-second timeout;
- existing AC6 test suite remains the release gate before promotion.
