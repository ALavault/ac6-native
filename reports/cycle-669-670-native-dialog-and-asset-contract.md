# Cycles 669–670 — stock dialog boundary and native asset contract

Date: 2026-08-03

## Stock runtime result

Cycle 669 used the signed-view-corrected Vulkan binary, an isolated fresh
profile, no force flags and experiment lane `stock`. The state-gated route
reached file selector state 4/dialog type 6. A fresh confirm was observed as
`edge=0x1000`, serial 33, but `[screen+12]` remained zero and the selector
stayed at type 6. The run stopped at its bounded predicate rather than applying
blind input.

This sharpens the acceptance boundary: previous runs that traversed types
6/8/10 without loadout force flags still depended on the experimental save
response bridge. They are valid downstream graphics/loadout observations, but
not proof of a stock save-dialog producer.

## Portable replacement

The final native product now has an address-free `DialogFlow` contract:

- semantic actions instead of XInput masks;
- acknowledge and binary-choice dialogs;
- binary choice defaults to the negative branch when requested;
- a monotonically increasing input serial prevents the opening action from
  accepting the newly opened dialog;
- duplicate delivery of one input epoch is inert.

This is the portable form of the cycle-631 stale-edge finding. It contains no
RexGlue, XAM, guest pointer or PAL address dependency.

Files:

```text
reconstruction/ace-combat-6/include/ac6/dialog_flow.h
reconstruction/ace-combat-6/src/dialog_flow.cpp
reconstruction/ace-combat-6/tests/dialog_flow_tests.cpp
```

Validation:

```text
ac6-dialog-flow-tests: PASS
native corpus: 44/45 PASS
ac6-xenia-shader-cache-catalog-retail: TIMEOUT at its pre-existing 60 s limit
```

The timed-out test is an external retail shader-cache catalog operation and
does not exercise or link the dialog flow. Its isolated retry reached the same
fixed 60-second timeout; it remains a separate validation-infrastructure
boundary, not a dialog regression.

## Entry 9 extraction and metadata

Mission 1 `DATA.TBL` row 9 was re-extracted by an exact bounded PAC read and
decoded successfully. The retail binary remains local and ignored.

```text
DATA.TBL SHA-256:
82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5

DATA00.PAC SHA-256:
c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816

row:                 9
group:               0x00010000
source range:        DATA00.PAC 0x01028000 + 0x00C9F03A
stored SHA-256:      95a7382b1e94dacf669837ced26f787cc6cddff0261d7565bf099d9e6b0eab54
decoded size:        42,446,032
decoded SHA-256:     cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05
decoded signature:   FHM 
```

Local result:

```text
reports/local-assets/entry-0009/files/DATA00/compressed/0009.decompressed.bin
reports/local-assets/entry-0009/manifest.json
```

Reproduction:

```bash
python3 tools/extract_ac6_pac.py "$AC6_ASSET_ROOT" \
  --entry 9 --decompress \
  --output reports/local-assets/entry-0009
```

The extractor manifest now preserves, for every selected resource:

- qualified XEX, table and PAC identities;
- table row, group, archive, offset, length and exclusive end;
- stored size, SHA-256, first 32 bytes and four-byte magic;
- emitted representation (`stored` or `decoded`);
- decoded size, SHA-256, first 32 bytes and four-byte magic;
- exact local relative path.

The synthetic extractor suite remains 6/6 passing.

## Next boundary

Connect platform input actions to the native `DialogFlow` in the portable
shell. For the evidence runtime, retain the bridge only in its explicitly
experimental lane to reach Mission 1 and collect renderer/gameplay contracts;
do not promote it to stock or final-product architecture.
