# Cycles 637--638 — targeted assets and guest file-I/O ranges

Date: 2026-08-03

The source-only tooling produced a local 99 MiB targeted bundle containing all
126 raw PAC rows (549,700 bytes), stored and decoded DATA.TBL row 9, six
64-KiB-block media profiles, 4 MiB head/tail samples and ENG/JPN comparisons.
No complete retail pack is copied into the repository.

Cycle 638 enabled the read-only `AC6_TRACE_FILE_IO` kernel boundary and replayed
the corrected save/menu path through the rendered campaign introduction. It
recorded 930 ordered open/read operations across 16 paths. The replay then hit
the already known `live-single-member-wrapper` timeout; force readiness and
launch remained false.

Key media result before that divergence:

```text
bgmpack.bin       4 ranges   4,435,968 bytes
demopack_eng.bin  1 range      905,216 bytes
moviepack.bin     2 ranges   2,883,628 bytes
voicepack_eng     not opened
voicepack_jpn     not opened
demopack_jpn      not opened
```

PAC coverage in the same replay was 13 coalesced DATA00 ranges (150,138,880
bytes) and 41 DATA01 ranges (59,136,000 bytes). DATA00 includes
`0x01028000..0x01CC7800`, the padded read covering table row 9. This temporal
correlation strengthens the case for inspecting row 9 but still does not prove
that DPL resource id 9 uses table index 9.

Evidence:

- runtime log SHA-256:
  `5bc5739674bef884f5be86da2670b6304d0476beb843bb9ce4d1ed95a72a92c5`;
- range summary SHA-256:
  `1dc9432654105cd7820906fa49ae063f7efa1d66b3e7c914807a428757e8ad55`;
- raw-only archive SHA-256:
  `71be88c97f7ab2aa70774a8b6c46e710cd047dd20d3fa36d8562d3fd374127c0`.

The trace establishes that BGM, English demo and movie packs participate before
the current divergence; voice packs do not participate in this bounded replay.
