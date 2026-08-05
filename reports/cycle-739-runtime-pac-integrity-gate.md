# Cycle 739 — runtime PAC integrity gate

Date: 2026-08-04 (Europe/Paris)

## Result

The Mission 01 asset gate passes for entries `9`, `119`, `165`, `199` and
`210`. Each runtime decode is owned by the frozen PAL decoder callsite, joined
uniquely to `DATA00.PAC` through the full
`(source_offset, compressed_size, decompressed_size)` tuple, and byte-for-byte
equal to the independent offline decode.

| entry | stored SHA-256 | decoded bytes | runtime/offline SHA-256 | result |
| ---: | --- | ---: | --- | --- |
| 9 | `95a7382b1e94dacf669837ced26f787cc6cddff0261d7565bf099d9e6b0eab54` | 42,446,032 | `cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05` | PASS |
| 119 | `c33dc3d9abd45293f3a1635534a7de099f84d7946d23d61e846dfa625bc1d142` | 165,892,096 | `e57cbeeb8f97a7a607ee1315b11a822b6af2d32581dcb7cbd557f1a6280e6dbd` | PASS |
| 165 | `4ed672f5498c9b626367409c03eed6f771f0e1d48aaf2bd89ddfe0c9cb53e106` | 2,422,384 | `2a469f0d9236cd3a5e463e42e6ae8bb02cb7f0689427683a689a58c8a73b9bfa` | PASS |
| 199 | `23e5713a146107ab5c494b8b99cea9db70dd1e89485792d4d7401ccb81690b25` | 4,075,520 | `ff5f0b58198feab5c40e3ef9486ee901330a57dd6489b7bb0d7499e289acac7a` | PASS |
| 210 | `1626a3b08f8ebc8010c593ed69609a20a7f6487a55c86d355d897e38449c225d` | 7,471,288 | `c79943600f931fa4696885e7d246c7bdd5b8d025521eea7d933f9c6ce907415d` | PASS |

The normalized recursive FHM trees also match for every runtime/offline pair.
Offline counts are:

| entry | top children | recursive nodes | FHM | NTXR | NDXR | MDLP | literal MATE magic |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 9 | 26 | 1,111 | 133 | 28 | 0 | 1 | 0 |
| 119 | 23 | 588 | 7 | 192 | 178 | 0 | 0 |
| 165 | 11 | 11 | 0 | 10 | 0 | 0 | 0 |
| 199 | 1 | 16 | 1 | 14 | 0 | 0 | 0 |
| 210 | 6 | 131 | 5 | 102 | 1 | 0 | 0 |

`MATE=0` is only a first-four-byte classification; it does not claim that the
MDLP material regions are absent. Material ownership still needs the existing
typed MATE/MDLP parser and the PAL MATE runtime latch.

## Runtime ownership

The hook reconstructs the 16-byte decoder record from the PAL streamer and
record table, then resolves it through `FindUniqueDecoded`. Recorded entries
use record indexes equal to their DATA.TBL rows and output valid `FHM ` heads.
No `r10 & 0xffff` identity is used.

Cycle 738 exposed an instrumentation defect: the decoded entry 199 was
rewritten on every cache-flush-loop hit, delaying the campaign transition until
the 300-second bound. `Ac6DumpPacDecodedEntry` now publishes each selected
entry once per process after a successful write. With this sole change,
Cycle 739 crossed the same transition in 8 seconds and emitted exactly five
selected decoded dumps.

## Identity and configuration

```text
workspace commit / tracked diff: 442c6dbcd5188fb84b056293a3ce7a000bd20669 / 05118e4cd1659dc7695c6abe11235e288819736dd446095df071c1da8f335b6c
runtime commit / tracked diff:   b8b03c7a89dc7f23bcd7844d15aa5080d480bf11 / 9d3581aed99f87a313eb98326706f6daf0d51f61b15f9a837ebe002d25dda3ae
RexGlue SDK provenance:          31a36d69f796cddd6b3ce545f6c6c332544ab294
generated tree SHA-256:          f42fa2c4c1ec3bfb061003ef7074f73881e968ef2719f7f78e59190d1c5af73d
executable SHA-256:              71bc891befa54b29786c370477e8a78a1e2292af41cc521f300ce0c826574c16
TOML SHA-256:                    fed716e3ff77b50e4866e2a67c5a183f21651f6cf29fdae930091c3fdf1c85b0
default.xex:                     acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
DATA.TBL:                        82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5
DATA00.PAC:                      c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816
DATA01.PAC:                      eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4
```

Effective runtime marker: performance off, debug logging,
`ac6_unlock_fps=false`, render capture and backend signature diagnostics on,
signature log and D3D trace off, all scales 1, direct host resolve off, native
2x MSAA on, invalid fetch constants disallowed, depth float24 overrides off and
deswizzle fix on. Vulkan selected NVIDIA RTX PRO 4000 Blackwell, API 1.4.329,
driver 595.84, vendor `10de`, device `2c34`.

## Validation

```text
PAC tuple test:       included in AC6 CTest
AC6 CTest PAL:        8/8 passed
Vulkan bridge build:  passed; no guest code generation
bounded route:        completed; owned timeout after 426 seconds
selected joins:       5/5 unique
decoded comparisons: 5/5 byte-identical
FHM comparisons:      5/5 normalized trees identical
follow log SHA-256:   c1b934d0489c34429a78371b368cf8fcbf90e68f5ce8fcc7957dbfb290ca9
runtime log SHA-256:  93203df90708d9ca01e023cf3a9f0e4c3fb5e128b7d4e3a053c5374038c95386
```

No Xenia run was used. The harness owned and cleaned only Xvfb `:97` and its
AC6 process. Shared Ollama PID `3585823` on `127.0.0.1:11435` was untouched.
Decoded retail payloads remain local under the ignored cycle log directory.

## Next boundary

GPU causal work may resume. The next checkpoint must identify one exact white
aircraft draw and a textured hangar positive control at the actual Vulkan
binding decision, then classify the first black stage of the gameplay render
graph. No asset, decoder or renderer compensation is justified by this gate.
