# Mission 01 retail family closure boundary — cycle 1067

Date: 2026-08-06

## Scope

This slice used the bounded PAC extractor and closure builder on DATA.TBL
entries 9 through 23, then compared entry 9 against each of the other mission
family roots. The retail payloads remain external; no decompressed payload, FHM,
texture, or buffer was added to git.

Inputs:

- `game-files/DATA.TBL`, SHA-256
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`;
- extractor mode `--decompress`, with one bounded output per entry;
- closure builder output root `/tmp/ac6-mission-closures-9-23.YZby4v`;
- comparison schema `ac6.asset-closure-comparison.v1`.

All fifteen roots decoded as `mode1_pi_xor_raw_deflate`. Every closure has
`parser_note_count=0`; no FHM parser failure was observed.

## Root and closure identities

| DATA.TBL | decompressed bytes | root SHA-256 | unique nodes | occurrences | shared nodes |
|---:|---:|---|---:|---:|---:|
| 9 | 42,446,032 | `cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05` | 966 | 1112 | 28 |
| 10 | 10,232,304 | `e894db3b36956562fa888511c3a0ce352cd690186f96eeb0d7a772f3b1a59675` | 90 | 113 | 7 |
| 11 | 18,162,160 | `9ad51f392aa291aa9975cfc97db0c1133e862f611752af3020c1e0cda34b321e` | 90 | 113 | 7 |
| 12 | 21,238,256 | `51de2c162e2b3fbfe0af0cd3fa39dbe78be3f571e37b45c5b4d02cb57d75c291` | 90 | 113 | 7 |
| 13 | 37,163,504 | `43804307b77e68e085a83f8d2838b6b112b15e54a10c01e6debf98604ae87921` | 90 | 113 | 7 |
| 14 | 27,320,816 | `1c386251c8b8505da5b8a667395edad233349f4092a6a4443403b07691b37a09` | 89 | 113 | 8 |
| 15 | 30,120,272 | `d503092155a2d8c4f4bdcc86971e607e720d94abfa51461629a0b37b750c3947` | 503 | 729 | 115 |
| 16 | 32,293,360 | `e667df8084e710d2a9db957062bc0f6f78ac455fc749bf42831b7ed5e5ea20a1` | 90 | 113 | 7 |
| 17 | 36,000,976 | `f7448daf41808679e647b1477c7ae7100229dccb6b1aabd2274d8fa6d7be6980` | 956 | 1053 | 27 |
| 18 | 28,144,112 | `27251d6f29155cb9d1004e96fbbb825870eadb3fd90f2da63147e1dc2bc27fc7` | 89 | 113 | 8 |
| 19 | 29,290,992 | `36e8c0c27cc4a99a84186953f2a210529d8d60a59ea867d0c6bb9063627845e2` | 91 | 113 | 6 |
| 20 | 12,653,040 | `eaecae03f33ba2f6215fbbdff9c48c8967015644f81d8a234507d43ab698e199` | 87 | 113 | 10 |
| 21 | 48,476,816 | `16c35b83f5f4018c888c3ae80a51eae6dfcd69a49ab7e363c8ea4d5f054b74fa` | 716 | 838 | 30 |
| 22 | 13,681,136 | `dc320871b064c65a620c08c855b8ce3cc08b3d63765e44172473ee2dc8e9fc64` | 90 | 113 | 7 |
| 23 | 55,944,256 | `1a733dd9db6a4ab82666b8452814583a12e53ec47b120f945f5b188e576667f9` | 1703 | 1931 | 45 |

## Comparison result

Entry 9 compared with entries 10–23 yielded respectively:

```text
candidate  base-only  candidate-only  exact-shared  changed-shape-signatures
10         951        75              15            13
11         952        76              14            12
12         952        76              14            14
13         954        78              12            10
14         954        77              12             8
15         949       486              17            34
16         952        76              14            10
17         945       935              21            67
18         952        75              14            12
19         954        79              12            10
20         952        73              14            10
21         950       700              16            79
22         952        76              14             8
23         943      1680              23            95
```

The fifteen roots share the same twenty-six top-level slot shape, including
the geometry/MDLP/PLAD and graphics families. Entries 9, 15, 17, 21 and 23
also contain extra NFIC/Scen cutscene families. The ordinary roots do not
contain those extra scene slots. The common slots `0014` and `0015` remain
binary candidates only: their size, magic, string content, or slot position is
not enough to assign wave, unit, order, or objective semantics.

The entry-9 `0015` leaf is independently hashable and contains generic event
keys plus `OHAYASI_M01_1ST_A` through `_D`. This qualifies radio/event
identities only; it does not qualify a Mission 01 scenario table. No exact
`SubMisTbl`, `ComTbl`, `Maneuver`, wave publication, faction binding, or
objective condition was recovered from this closure comparison.

## Gate consequence

The static closure is useful for asset identity and dependency work, but it
does not close `units_and_waves` or `retail_objectives`. No native manifest is
generated from filename order, FHM proximity, magic plausibility, or shape
alone. The next dynamic boundary remains selector/owner/post-DPL registry
ownership; the native runtime continues to reject unqualified scenario data.
