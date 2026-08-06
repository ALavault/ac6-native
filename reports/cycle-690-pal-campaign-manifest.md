# Cycle 690 — PAL retail campaign manifest

Date: 2026-08-03 (Europe/Paris)

The local source-only corpus now has a durable hash/structure manifest at
`reports/ac6-pal-campaign-manifest.json`. It records the qualified PAL XEX,
DATA.TBL and PAC identities, the two-bank coverage bounds and the proven
selector-1 route.

## Corpus

```text
default.xex   7,483,392 bytes  acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
DATA.TBL         14,824 bytes  82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5
DATA00.PAC  2,267,086,848 bytes  c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816
DATA01.PAC    664,141,824 bytes  eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4
```

`DATA.TBL` has 926 entries, pack count 2, archive distribution 466/460 and
raw distribution 74/52. Its final required ends are `0x8720ca9f` and
`0x27958034`; the local PAC files reach the corresponding next `0x8000`
boundaries.

## Mission qualification

Mission 1 is recorded as selector 1 → DPL 9 → DATA.TBL entry 9, with the exact
bank, offset and stored/expanded sizes from the table. Mission 2 is present
only as an unresolved placeholder: no selector-2-to-retail-resource identity
or runtime completion is claimed. The manifest is metadata and hashes only;
the retail containers remain local and are not copied into the repository or
uploaded.
