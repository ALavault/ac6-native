# Mission 01 retail briefing-package boundary — cycle 1072

Date: 2026-08-06

## Scope

This slice classifies the mission-specific DATA.TBL entries 210–224 after the
exhaustive CPU scan of the retail corpus. It is a static ownership experiment:
it does not promote any objective, wave, faction, unit, or target semantics to
the native runtime. No retail payload is committed.

Inputs and reproducibility identities:

- `DATA.TBL` SHA-256:
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`;
- family census JSON SHA-256:
  `80967c8584883457a262d7469c418943cf1250e73b5f26a096dd0209163a3fcc`;
- targeted decoded-text scan JSON SHA-256:
  `3248e16cf197563fbb8aa164df38c2196ab1fd4068990d0c8aecf610b17a39ea`;
- both scans used the bounded extractor with decompression and retained only
  manifests, hashes, and summaries outside the repository.

## Fifteen-entry family

Every entry decoded through `mode1_pi_xor_raw_deflate`. Each has one `BRDB`,
one `BMAP`, one `SWG`, and a graphics/audio FHM family. The `SWG` names are
`briefing_ms01` through `briefing_ms15`, matching the entry index. `BMAP` has
the same 263,200-byte shape in all fifteen packages; the `BRDB` and `SWG`
sizes vary by mission.

| entry | expanded bytes | expanded SHA-256 | BRDB bytes | BMAP bytes | SWG bytes |
|---:|---:|---|---:|---:|---:|
| 210 | 7,471,288 | `c79943600f931fa4696885e7d246c7bdd5b8d025521eea7d933f9c6ce907415d` | 2,912 | 263,200 | 1,160,124 |
| 211 | 8,204,632 | `3e4ad6c8cab3248d715cfb1ca2a9abe11afbb3c7c98827b70871df3e61c318f2` | 2,208 | 263,200 | 1,187,456 |
| 212 | 8,829,020 | `9f5a1a659a1c01edc8d272f2302502c6d59d7a0e37e977d83bea207ac89eadb5` | 3,008 | 263,200 | 1,348,200 |
| 213 | 8,937,672 | `1d5ee9d5380b2cad698b630a1d09c503094df93860b9e593325237acaedcb1eb` | 4,384 | 263,200 | 1,373,576 |
| 214 | 9,406,608 | `ef0db8e9f1780a579b55b7c34fa21e970719eb2cb52ae6b72d24af786bde484d` | 4,736 | 263,200 | 1,421,612 |
| 215 | 8,556,676 | `1102dd8a6b4a02833472152a6886787bf2f41fa3c608acfb1ed83645e51175b8` | 6,912 | 263,200 | 1,296,380 |
| 216 | 9,017,540 | `f401ce106d4d682efc92ef61172ff5763aefd941f9466a10706b7f97ceecd7b9` | 3,776 | 263,200 | 1,356,436 |
| 217 | 10,700,932 | `17503dbf44e50a611e0f2bfb2f49f91d5c32744de9ad5ea652f1255500d11a08` | 3,904 | 263,200 | 1,549,252 |
| 218 | 9,474,188 | `8308a8f4232d500b1c5582767dd607b9b3c6e2ec4a07d4e35cfa0220100eb9a9` | 864 | 263,200 | 1,425,868 |
| 219 | 9,060,508 | `7558aaf2a623391677cc25a7ac8da3128b94d7c01c1a189c78dd2285d3fa6da6` | 3,552 | 263,200 | 1,390,780 |
| 220 | 8,906,848 | `b21df19775c196ba0d56ec23986feb90c7f20d70ccdd850390115f70e63e466c` | 5,792 | 263,200 | 1,350,228 |
| 221 | 9,869,488 | `755739413ed374124cea53f3fbfd56d437921f3cb46b83f9e6a465978f6fd514` | 1,760 | 263,200 | 1,431,132 |
| 222 | 8,284,304 | `39c2a64d28a27ed105df86a37219898a9a550df5c872ede697ebd92bb683b9e7` | 6,368 | 263,200 | 1,316,860 |
| 223 | 7,678,264 | `0dcecee55fee1eb1d26aa635be60cc624d70fabc985df28a6e7260786d38f7ee` | 640 | 263,200 | 1,200,576 |
| 224 | 10,148,128 | `7552ce5ac48099076c88614515a21424f30a8a4c85dce76f4a96dc1e64a11e88` | 1,376 | 263,200 | 1,501,476 |

The family contains 128–178 parsed nodes per root, with one BRDB/BMAP/SWG
triplet in each package. Shape and mission-number correspondence are recorded
only as classification evidence; they are not semantic associations.

## Entry 210 bounded inspection

The entry-210 manifest identifies a compressed DATA00.PAC range at offset
`0x7D000000`, length `943180`, stored SHA-256
`1626a3b08f8ebc8010c593ed69609a20a7f6487a55c86d355d897e38449c225d`.
The expanded root is 7,471,288 bytes with SHA-256
`c79943600f931fa4696885e7d246c7bdd5b8d025521eea7d933f9c6ce907415d`.

The exact bounded leaves are:

- `BRDB`, offset 128, size 2,912, SHA-256
  `c46c824a25a7728976af5376adb05c193d2a11826aa3a3751daf61252eee5e9a`;
- `BMAP`, offset 3,040, size 263,200, SHA-256
  `c50823a12ab889585c692d65402e7715a2ee491d0a83e4e832347b28195dbb8f`;
- `SWG` leaf `briefing_ms01`, size 1,160,124, SHA-256
  `883501eb9afa568927fec3537c5690b0a093db58b3d6a885bfe922e7031f5cf4`.

The decoded SWG has 618 strings and 101 sibling texture references. Its
strings are widget/action identifiers; there are zero matches for `Aerial`,
`Defence`, `Gracemeria`, `objective`, `wave`, or `target`. BRDB decoding yields
numeric/map-record-shaped data without an objective condition, unit identity,
wave transition, or faction binding. BMAP is map/briefing visual data. The
manifest records one parser failure for an unclassified auxiliary leaf; this
is not promoted as a clean scenario closure.

## Corpus result and gate consequence

The 32-process decoded scan covered all 926 DATA.TBL entries and
5,424,368,676 expanded bytes. Generic UI, briefing/debrief, map/NDXR and
shader families account for the token hits. No owning `SubMisTbl`, `ComTbl`,
`Maneuver`, objective-condition table, or qualified unit/wave identity was
found. The entry-210 package is therefore qualified as a retail briefing/map
family, not as the Mission 01 gameplay scenario owner.

This closes the briefing-package misclassification boundary and leaves
`retail_objectives` open. The remaining causal edge is the owner/consumer of
the binary gameplay scenario records (or a bounded dynamic acquisition of that
owner); the native P6 fixture remains explicitly non-retail.
