# Mission 01 asset closure comparison

Static evidence only; decoded payloads remain under `/tmp` and are not part of
the repository. The source identities are DATA.TBL SHA-256
`82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`, DATA00.PAC
SHA-256 `c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816`,
and DATA01.PAC SHA-256
`eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4`.

`build_ac6_asset_closure.py` passed for entries 9 and 119 together and for each
entry 119 through 133. Every closure has zero parser notes and zero invalid FHM
containers. `compare_ac6_asset_closures.py` was run with entry 119 as the base:

| candidate | shared exact nodes | base-only | candidate-only | same-shape/different-content signatures |
|---:|---:|---:|---:|---:|
| 120 | 21 | 396 | 264 | 32 |
| 121 | 19 | 398 | 116 | 12 |
| 122 | 20 | 397 | 87 | 18 |
| 123 | 20 | 397 | 217 | 32 |
| 124 | 20 | 397 | 212 | 24 |
| 125 | 21 | 396 | 40 | 8 |
| 126 | 20 | 397 | 329 | 41 |
| 127 | 20 | 397 | 54 | 12 |
| 128 | 21 | 396 | 143 | 19 |
| 129 | 19 | 398 | 200 | 29 |
| 130 | 17 | 400 | 120 | 15 |
| 131 | 27 | 390 | 388 | 118 |
| 132 | 26 | 391 | 402 | 106 |
| 133 | 19 | 398 | 65 | 13 |

The exact shared-node counts are structural evidence only. No node is assigned a
semantic name (terrain, sky, unit, effect, or table) from magic, size, order, or
FHM proximity. Mission 01's bounded associations currently close only the two
root identities and the six exact NDXR hashes listed in
`manifests/mission01/buffers.tsv`; transforms, materials, textures, and camera
remain explicitly open.
