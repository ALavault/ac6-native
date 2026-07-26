# AC6 cycle 237 — persistent Xenia shader-cache correlation

Date: 2026-07-18

## Question

Can an already-qualified graphical Xenia run provide active Xenos shader
identities without another VNC session or the failed Xvfb `dump_shaders` path?

## Qualified inputs

- target: AC6 PAL `default.xex`, title ID `4E4D07D1`;
- XEX SHA-256:
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- Xenia Canary release: `16e1eb8`, executable SHA-256
  `c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a`;
- retained run log SHA-256:
  `80baac707556bafc53a249e412f9ef42230124f44ae4510a3e095ce489e69c93`;
- retained shareable shader cache `4E4D07D1.xsh`, SHA-256:
  `bbd197970e99135ac21e64fde7adae808f4ff8781069dc7ff4232b7c72fe4668`;
- qualified PAL `DATA.TBL`, `DATA00.PAC` and `DATA01.PAC` corpus.

The log and cache have the same final timestamp window. The log identifies the
PAL title, reports `Loaded 7 shaders from storage`, creates a Vulkan swapchain,
and later exits normally. The cache implementation in the pinned
Xenia/ReXGlue source is append-only for newly encountered hashes and skips a
shader already loaded from the current storage. Therefore records 0 through 6
are the pre-run baseline; the remaining records are newly persisted during
this run. A previously cached shader may also have been used during the run,
so the new-record set is a lower bound on active identities, not a complete
draw trace.

## Recovered cache contract

`parse_xenia_shader_cache()` now validates:

- the `XESH` file signature;
- canonical format version `0x20201219` stored byte-swapped;
- little-endian 64-bit runtime hashes;
- the packed 31-bit dword count and high-bit vertex/pixel stage;
- non-empty, complete and in-bounds ucode records.

`ac6-xenia-shader-cache-catalog` then recomputes unseeded XXH3-64 over every
ucode span and rejects the cache on the first mismatch. It scans the same
3,808 bounded retail ucode spans qualified in cycle 235, compares exact hashes
and also checks that the cached and retail stages agree.

## Result

```text
xenia_cache_format_version=0x20201219
xenia_cache_records=219
xenia_cache_unique_hashes=219
xenia_cache_vertex_records=105
xenia_cache_pixel_records=114
xenia_cache_hashes_validated=219
xenia_cache_baseline_records=7
xenia_cache_new_records=212
xenia_cache_new_unique_hashes=212
retail_static_shaders=3808
retail_bounded_ucode_spans=3808
cache_hashes_matched_to_retail=113
cache_hashes_unmatched=106
new_cache_hashes_matched_to_retail=111
new_cache_hashes_unmatched=101
retail_containers_matching_cache=221
cache_retail_stage_mismatches=0
cache_hashes_using_specular_parameter=18
new_cache_hashes_using_specular_parameter=18
retail_specular_containers_matching_cache=38
xenia_cache_status=PASS records=219 baseline=7 new=212 matched=113 new_matched=111 new_specular=18 stage_mismatches=0
```

All 18 cache hashes that map to retail permutations declaring
`ACE_vSpecularParam` are in the newly persisted suffix. They cover 38 retail
containers because identical ucode appears in multiple retained containers.
Examples include `psPhgCT`, `psPhgCT_SP`, `psCokCT_SP_ENV2`,
`psCstCT2_SP_IL_ENV2` and `psCokCT_NM_SP_AO_ENV2` variants.

The 106 unmatched cache hashes are retained explicitly. They may come from
XEX-embedded shaders, other retail resource stores, emulator-generated paths
or a different corpus boundary; they are not assigned to `DATA.TBL` by name.

## Confidence and remaining boundary

- **confirmed**: all 219 cache records are structurally valid and their stored
  XXH3-64 hashes match their exact ucode bytes;
- **dynamic**: 212 new unique shader hashes were appended during the qualified
  Xenia process after its seven-record startup baseline;
- **cross-match**: 111 of those new hashes match exact qualified retail DATA
  ucode, including 18 unique `ACE_vSpecularParam` permutations;
- **unknown**: the chronological draw order and the MATE object active for any
  one shader, because `.xsh` is a set-like persistent store rather than a draw
  trace;
- **unknown**: the source of the 101 newly persisted hashes absent from the
  current retail DATA corpus.

This closes the question of whether specular-capable retail permutations were
encountered by the existing runtime run. It does **not** close the specific
MATE-to-permutation relation and does not justify applying one equation to all
materials. That boundary remains `needs-dynamic-evidence` and requires a draw-
ordered capture, not another blind Xvfb launch.

## Validation

```bash
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .build/ace-combat-6 -j16 --target \
  ac6-xenos-shader-tests ac6-xenia-shader-cache-catalog
ctest --test-dir .build/ace-combat-6 --output-on-failure \
  -R '^(ac6-xenos-shader-tests|ac6-xenia-shader-cache-catalog-retail)$'
```

Results:

- focused parser and retail/cache correlation: **2/2 PASS**;
- complete GCC corpus: **44/44 PASS**;
- complete Clang corpus including four XenonRecomp probes: **48/48 PASS**;
- root install publishes `bin/ac6-xenia-shader-cache-catalog` without
  `bin/bin` nesting;
- the installed tool reproduces the exact `PASS records=219 ...` status;
- `clang-format --dry-run --Werror` and `git diff --check`: PASS.

No Xenia process, GUI, VNC, controller, keyboard or human action was used.
