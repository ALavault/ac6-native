# Cycle 643 — targeted asset worker bundle integration

Date: 2026-08-03

Bundle SHA-256:
`55e966a4cee0613bfaa0f93ce7fa3f66cd4005e997658ed4b042cffa7f86a974`.
Its 17-file internal manifest and ZIP integrity verify. It contains derived
reports and source tools only; no complete retail PAC or media pack.

## Qualified results

- All 126 `DATA.TBL` rows marked raw still use the index-derived XOR pad.
  Descrambling produces 126/126 `FHM ` containers; stored bytes produce none.
- Decoded row 9 is Mission 1 content. Its strings include `mapobj_m01_*`,
  `OHAYASI_M01_*` and `Scene/dd01_01a/...`; its top-level FHM has 26 children.
- The cycle-638 trace maps all 830 PAC reads uniquely to 55 table rows. The
  mission-transition tail is `199 -> 9 -> 119 -> 165 -> 210`.
- This strongly corroborates DPL id 9 -> table row 9, but does not close the
  direct registry-result join. Keep that distinction explicit.
- BGM, demo and voice packs are concatenated, `0x800`-aligned RIFF/WAVE/XMA
  records. `moviepack.bin` is ASF-like, not Bink.

## Integrated changes

- Raw PAC rows are descrambled by default in `tools/extract_ac6_pac.py`.
- `--preserve-raw-storage` retains exact on-disc bytes when required.
- Manifests publish `descrambled_count`, per-entry `descrambled`, and the
  preservation mode.
- The missing source-tree `tools/ac6_fhm.py` parser is restored with the APIs
  consumed by both FHM extractors.
- Unit coverage includes decoded/preserved raw rows and source-tree CLI import.

## Validation

- synthetic asset-tool suite: 4/4 pass;
- real PAL corpus: 126 extracted, 126 descrambled, 126 `FHM ` heads, zero
  decode failures;
- no retail payload committed.

## Remaining boundary

The asset evidence does not explain the native `MISSION 01 / STANDBY`
readiness/capability publication. The next asset-side experiment is the direct
DPL/CRC/registry-to-table-row join; the active executable frontier remains the
first missing producer of manager readiness/status or selected-aircraft data.
