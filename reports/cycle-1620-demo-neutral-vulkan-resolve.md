# Cycle 1620 — resolve Vulkan neutre test-only

## Résultat

Le resolve Xenos atteint est exécuté par un pipeline compute Vulkan autonome,
strictement borné au frame neutre qualifié. Deux processus frais produisent le
même readback linéaire noir 1280×720, SHA-256
`0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f`.

Le test utilise le SPIR-V générique ReXGlue
`resolve_fast_32bpp_1x2xmsaa_cs`, 8 380 octets, SHA-256
`e8cfb0d6…87b32`, validé avec le `spirv-val` épinglé pour Vulkan 1.1. Son ABI
est deux SSBO et cinq dwords de push constants. Le packing atteint corrigé est
`10 91400 01000300 5C28 1374A000`; le harness lie la destination à la base du
descripteur et transmet donc une base relative nulle.

La validation compare tout le buffer tiled à l'oracle CPU, conserve les gaps
internes à `0xA5`, vérifie des gardes avant et après `0x398000`, puis untile
avant de calculer le SHA linéaire. Un motif uniforme asymétrique confirme le
swap R/B et produit `66dde082…0dc8`.

## Qualification

- registres, dimensions et contenu EDRAM neutre : `demo-qualified` ;
- interprétation resolve et shader : `xenia-generic` ;
- exécution : `test-only-observed` ;
- readback du runtime produit : toujours `unknown`.

Xenia/ReXGlue ne sont pas une preuve AC6. Aucun shader, microcode, actif ou
sortie générée n'est suivi. Le raster CPU et ce pipeline ne sont jamais un
fallback de `play`.

## Validation

- build incrémental avec `ccache` : PASS ;
- `spirv-val --target-env vulkan1.1` : PASS ;
- deux processus Vulkan frais : identiques ;
- CTest sous Xvfb avec `SDL_AUDIODRIVER=dummy` : 17/17 PASS ;
- audit source, complexité et `git diff --check` : PASS.

Le receipt durable est
`analysis/demo/ac6-demo-neutral-vulkan-resolve-v1.json`.

## Frontier

Le prochain checkpoint est de brancher les mêmes constantes et le même kernel
au backend produit transactionnel après le draw EDRAM atteint, puis d'exiger
deux replays neutral frais au même hash. START ne sera comparé qu'ensuite,
depuis le même checkpoint.
