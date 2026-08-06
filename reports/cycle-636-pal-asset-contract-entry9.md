# Cycle 636 — PAL asset contract and bounded entry 9

Date: 2026-08-03

The local source names are `DATA.TBL`, `dummy.bin` and `dummy30.bin`; upload
suffixes are not retained. Retail files remain ignored and untracked.

The canonical PAL contract now qualifies the XEX, 926-row/two-pack table, both
PAC identities, the four allowed groups, archive distributions, `0x8000`
alignment, exact padded progression and unique archive/offset/size keys.
Dummy files are optional extraction/VFS canaries only.

The selective extractor read only table row 9's `DATA00.PAC` range
`0x01028000+0x00C9F03A`. Mode-1 decoding succeeded:

```text
decoded size:   42,446,032 bytes (0x0287ACD0)
decoded SHA256: cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05
magic:          FHM 
manifest SHA256: 325179b23b2ac1dbaf86a1b85cd058294845ddfd9cc20bb4e4e7b8f72275d98f
```

This classifies the table payload but does not join the DPL namespace to table
row indices. `DPL resource id 9 == DATA.TBL entry 9` remains unproven.

Validation: Python asset tests 3/3, `ac6_pac_index` pass,
`ac6_real_asset_contract` pass, binary/table/PAC/canary verifier pass, and
`git diff --check` pass. The downstream recursive FHM command is currently
blocked by the pre-existing missing local module `tools/ac6_fhm.py`; no claim
is made from that failed stage.
