# Cycle 1579 — extraction PAL M01 pour le renderer natif

Le cône d'assets PAL de Mission 01 est maintenant extrait hors dépôt, par
lectures bornées et fermeture FHM content-addressed. Les conteneurs retail,
les payloads décodés et les fichiers de suivi ne sont pas versionnés.

## Source et racines

- XEX PAL : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- `DATA.TBL` : `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
- Racines bornées : entry 9 (scénario, 42 446 032 octets décodés) et entry
  119 (terrain, 165 892 096 octets décodés).
- Fermeture : 1 382 nœuds uniques, 1 615 occurrences, 142 FHM, 1 240
  feuilles, 28 nœuds partagés, zéro note de parser.

## Slices destinées au natif

- 5 NDXR F-16 (LOD 1–4 et `r_f16c_dd`).
- 6 NDXR de map parts (010, 059, 063, 144, 163, 169).
- 7 NTXR d'atlas terrain (6 pages 4096² et une page 4096×1024).

Chaque slice est contrôlée par taille, structure NDXR/NTXR et SHA-256. Les
hashes et les emplacements de cache externe sont dans
[`analysis/assets/mission01-pal-native-extraction.v1.json`](../analysis/assets/mission01-pal-native-extraction.v1.json).

## Reproduction et garde

```text
python3 tools/extract_ac6_pac.py game-files --indices 9 119 --decompress \
  --output $AC6_ASSET_CACHE/ac6-pal-m01-extract-20260813-v1
python3 tools/build_ac6_asset_closure.py \
  $AC6_ASSET_CACHE/ac6-pal-m01-extract-20260813-v1/manifest.json \
  --output $AC6_ASSET_CACHE/ac6-pal-m01-closure-20260813-v1
python3 tools/extract_ndxr_native_slices.py <MDLP> \
  $AC6_ASSET_CACHE/ac6-pal-m01-f16-slices-20260813-v1 \
  --names 'f16|o_f16c|r_f16c'
python3 tools/extract_ndxr_native_slices.py <entry119/021_FHM/014_FHM> \
  $AC6_ASSET_CACHE/ac6-pal-m01-terrain-slices-20260813-v4 \
  --names '(010_NDXR\\.ndxr|059_NDXR\\.ndxr|063_NDXR\\.ndxr|144_NDXR\\.ndxr|163_NDXR\\.ndxr|169_NDXR\\.ndxr)$'
python3 tools/extract_ntxr_native_slices.py <entry119/021_FHM/016_FHM> \
  $AC6_ASSET_CACHE/ac6-pal-m01-terrain-textures-20260813-v1 --names NTXR
```

La politique d'extraction refuse toute promotion de chemin contenant
`tracker`, `tracking` ou `telemetry`; seules les slices bornées et leurs
manifests/hash sont consommées par la suite native.

Validation effectuée : `json.tool` du manifeste, `extract_ac6_pac` (2/2
décodées, 2/2 FHM), `build_ac6_asset_closure` (PASS), extraction NDXR (5 + 6)
et NTXR (7). Le statut JV reste ouvert : cette extraction ne qualifie ni
association matériau/texture complète ni caméra retail.
