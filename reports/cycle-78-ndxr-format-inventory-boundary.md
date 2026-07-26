# Cycle 78 — AC6 NDXR raw-format inventory boundary

## Change

`ac6-asset-manifest` now records the raw `vertex_format` and `uv_format`
bytes of every bounded NDXR polygon descriptor, together with the total
descriptor count. The inventory deliberately remains below
`parse_ndxr_model`: it does not assign a vertex, bone, colour, or UV semantic
to an observed byte and can therefore describe a retail layout which the
higher-level decoder still refuses.

The descriptor walk uses the same bounded 0x30-byte object/polygon records as
`parse_ndxr_model`: object polygon counts come from `+0x2a`, then raw format
bytes from polygon descriptor `+0x0e` and `+0x0f`.

## Corpus attempt and boundary

The bounded full-archive read was attempted with the supplied PAL
`DATA.TBL`/`DATA00.PAC`/`DATA01.PAC`, with stdout discarded because the normal
manifest is intentionally large. It stopped at entry 0, path 0 before any
NDXR member:

```text
error: invalid NTXR at entry 0 path 0: NTXR entry has invalid eXt/GIDX framing
```

This failure predates the new NDXR descriptor walk: it occurs in the existing
NTXR validation branch before a later NDXR payload can be reached. It therefore
does not establish a new NDXR format, nor justify relaxing either parser. The
next evidence task is to reconcile this supplied archive with the known-good
NTXR corpus/provenance, then rerun the manifest.

## Validation

```bash
cmake --build .build/ace-combat-6/native -j16 --target ac6-asset-manifest ac6-ndxr-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure -R '^ac6-ndxr-tests$'
timeout 55s .build/ace-combat-6/native/ac6-asset-manifest \
  workspaces/ace-combat-6/game-files/DATA.TBL \
  workspaces/ace-combat-6/game-files/DATA00.PAC \
  workspaces/ace-combat-6/game-files/DATA01.PAC >/dev/null
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

- targeted NDXR test: **1/1 passed**;
- full AC6 CTest: **41/41 passed**;
- the full-retail manifest command exits non-zero at the existing NTXR gate,
  as recorded above;
- root installation retained `bin/` without a nested `bin/bin`.
