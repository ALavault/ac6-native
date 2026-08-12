# AC6 static evidence tooling

These tools reduce dependence on long instrumented runtime runs. They do not
replace retail evidence for timing, rasterisation or perceptual parity; they
turn the XEX-derived routing and decoded asset corpus into deterministic,
content-addressed contracts.

## Decode selected DATA.TBL entries

`extract_ac6_pac.py` performs bounded `seek` reads and never loads a complete PAC
into memory.

```bash
python3 tools/extract_ac6_pac.py "$AC6_ASSET_ROOT" \
  --indices 9 119 \
  --decompress \
  --output /tmp/ac6-mission01-roots
```

With `--decompress`, compressed records are descrambled and inflated, while raw
records are descrambled without inflation. Use `--preserve-raw-storage` only for
forensic comparison with exact on-disc bytes.

## Build the Mission 01 asset closure

```bash
python3 tools/build_ac6_asset_closure.py \
  /tmp/ac6-mission01-roots/manifest.json \
  --output /tmp/ac6-mission01-closure
```

The builder verifies every payload size and SHA-256 before parsing. It writes:

- `closure.json`: unique content-addressed nodes, roots and child edges;
- `occurrences.tsv`: every root-relative occurrence;
- `shared_nodes.tsv`: duplicate resources, including cross-root reuse.

No payload bytes are copied into the closure. Unknown leaf magics are preserved;
invalid FHM containers, unsafe paths, hash mismatches, recursive cycles and
parser truncation notes fail closed by default.

To include independently extracted roots, pass multiple manifests. Duplicate
`(DATA.TBL SHA-256, entry index)` roots are rejected rather than silently merged.

## Compare campaign payloads structurally

Build one closure per payload or payload family, then run:

```bash
python3 tools/compare_ac6_asset_closures.py \
  /tmp/entry119/closure.json \
  /tmp/entry120/closure.json \
  --output /tmp/entry119-vs-120.json
```

The report distinguishes exact SHA-256 identity from a weaker `(magic, size)`
shape match. A shape match is a candidate for further reverse engineering, not
proof that two resources have the same semantics.

Use `--fail-on-difference` when equality is a regression gate.

## Compare PPC, generated and native function behaviour

The snapshot comparator supports the three-way method used for micro-executed
functions:

```bash
python3 tools/compare_ac6_function_snapshots.py \
  --ppc /tmp/capsule/ppc.json \
  --generated /tmp/capsule/generated.json \
  --native /tmp/capsule/native.json \
  --output /tmp/capsule/comparison.json
```

Minimal snapshot form:

```json
{
  "schema": "ac6.function-snapshot.v1",
  "identity": {
    "implementation": "ppc-pcode",
    "function": "0x821A16B8",
    "case": "view-2-context-valid"
  },
  "exit": {"kind": "return"},
  "registers": {"r3": 1},
  "special_registers": {"cr": "0x00000000"},
  "calls": [{"target": "0x82200000", "ordinal": 0}],
  "memory_writes": [
    {"address": "0xA0000100", "size": 4, "after_hex": "00000001"}
  ]
}
```

The comparator classifies:

- `all_equal`;
- `generated_diverges`;
- `native_diverges`;
- `ppc_or_microexec_diverges`;
- `all_diverge`.

Provenance metadata is ignored, while call order and memory-write order remain
significant. Floating-point tolerance is zero by default and must be enabled
explicitly with `--float-abs` or `--float-rel`.

## Audit the native J0/J1 acceptance gate

Audit the current contract against repository-relative or external artifacts:

```bash
python3 tools/audit_ac6_mission01_native_gate.py \
  analysis/contracts/mission01-native-gate-v2.json \
  --artifact-root . \
  --require J1
```

The fixed J0/J1 requirement list cannot be shortened. A passed requirement must
carry hashed native evidence; bridge or retail observations may support a claim
but cannot pass a native gate by themselves. World visibility and player
visibility require a native capture, deterministic behaviour requires a native
test, and retail objectives require both native execution and static,
micro-executed or differential semantic evidence.

## Validation

```bash
python3 -m unittest discover -s tools/tests -v
python3 -m py_compile \
  tools/extract_ac6_pac.py \
  tools/build_ac6_asset_closure.py \
  tools/compare_ac6_asset_closures.py \
  tools/compare_ac6_function_snapshots.py \
  tools/audit_ac6_mission01_native_gate.py
```

The synthetic tests cover raw-record descrambling, exact-storage preservation,
FHM deduplication, same-shape payload differences and three-way divergence
classification.
