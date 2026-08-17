# Cycle 1617 — oracle vertex ReXGlue et raster CPU borné

## Résultat

Un CLI hors ligne test-only appelle directement `Shader`, `AnalyzeUcode` et
`SpirvShaderTranslator` du SDK ReXGlue générique. Son build vérifie le checkout
extérieur `dcd41b7457fcac8242f8ef40de83d1719390d5af`, l'arbre SDK
`741541d6035616dc406f7d74c2fe8f155913c77b` et l'archive graphics SHA-256
`564a728c…243a5`. Tous les fichiers d'entrée et de sortie sont imposés sous
`TMPDIR`; seules les quatre identités shader observées dans la démo PAL sont
admises. Une identité ou un stage différent trap avant écriture.

Les trois vertex microcodes immédiats atteints ont été extraits uniquement des
IB capturés, vérifiés puis traduits :

| microcode SHA-256 | dwords | `SQ_PROGRAM_CNTL` | GPR VS | SPIR-V bytes | SPIR-V SHA-256 |
|---|---:|---:|---:|---:|---|
| `099625f3…e4e3` | 24 | `0x1000000E` | 15 | 7 800 | `944fd752…ce6` |
| `93488cb9…402b` | 27 | `0x10010001` | 2 | 12 496 | `ba9b97cc…576` |
| `586168ec…3cc0` | 15 | `0x00010002` | 3 | 9 288 | `4913cadb…920` |

Les trois sorties passent le `spirv-val` épinglé, commit
`e39e5c5838bc4b4162c349f2a2e5f163efe5432f`, SHA-256 outil
`2cc19cdd…e3406`. Microcodes, désassemblages et SPIR-V restent temporaires. Le
receipt durable ne contient que provenance, tailles et hashes :
`analysis/demo/ac6-demo-rexglue-reached-vertex-spirv-v1.json`.

## Comparaison XenosRecomp / DXC

L'inventaire NSXR démo déterministe ne contient aucun des trois hashes vertex.
XenosRecomp exige un `ShaderContainer` complet ; fabriquer un container serait
une preuve synthétique interdite. La comparaison vertex est donc
`unknown-no-exact-demo-container` et s'arrête fail-closed.

Le pixel shader, lui, possède deux containers démo exacts. Sa sortie HLSL
XenosRecomp a été recompilée deux fois byte-identiquement avec le DXC installé
`v1.9.2602.24` (`d355aa83`, SHA-256 `db50584b…9940d`) : SPIR-V 1 448 bytes,
SHA-256 `5affe6f8…6306`, validé Vulkan 1.1 par `spirv-val`. Le receipt pixel a été
mis à jour ; aucun HLSL ou SPIR-V n'est suivi.

## Rasterizer CPU test-only

`ac6-demo-reached-raster-tests` couvre uniquement : viewport 1280×720,
rectangle normal observé 640×360, interpolation RGBA bornée, color mask
`0xFFFF`, rectangle resolve observé 1280×720 et copie logique RGBA8 avec pitch
1280/endian 0/destination tiled. Toute divergence trap. La copie est
explicitement pré-tiling : elle ne remplace pas le cache EDRAM Vulkan et n'est
reliée à aucune commande `play`.

## Build et validation

- ccache actif dans le build démo (`CMAKE_C_COMPILER_LAUNCHER` et
  `CMAKE_CXX_COMPILER_LAUNCHER` = `/usr/bin/ccache`) ;
- tests raster et shader ciblés : PASS ;
- CTest headless `SDL_AUDIODRIVER=dummy` : 15/15 PASS ;
- Python : 26/26 PASS ;
- complexité et audit source : PASS ;
- identité négative CLI : trap exit 2, aucune sortie créée ;
- checkouts ReXGlue/Xenia/XenosRecomp/SPIRV-Tools inchangés.

## Frontier

La traduction des trois VS et la couverture logique des deux rectangles sont
fermées. Un screencap reste interdit : l'état initial complet de l'EDRAM avant
le draw normal, la conversion/tile exacte du cache Vulkan et les 24 draws
bootstrap ne sont pas encore joints à des pixels. Prochain checkpoint : porter
seulement le chemin Xenia/ReXGlue EDRAM RGBA8 atteint, vérifier son tiling sur
les adresses capturées, puis comparer deux readbacks neutral frais avant START.
