# Cycle 1628 — resolve neutral dans le runtime Vulkan

Date : 2026-08-15  
Cible : démo Xbox LIVE PAL `Default.xex`  
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Résultat

Le backend produit joint maintenant le draw normal neutral au rectangle copy
atteint, puis exécute le resolve compute générique ReXGlue avant `XE_SWAP`.
Cette route reste strictement bornée au frame neutral noir qualifié.

La jointure exige successivement :

1. readback du draw normal 4×MSAA : 640×360, 230 000 pixels noirs, zéro
   sentinelle, SHA-256
   `0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366` ;
2. second draw PAL exact : `RectangleList`, trois auto-indices, non prédicaté,
   VS `586168ec…3cc0`, PS `4913603d…8e25` ;
3. profil copy exact : RT/depth/viewport déjà scellés et bloc
   `0x2318..0x231B = {00100000,1374A000,02D00500,01000300}` ;
4. ABI compute `{00000010,00091400,01000300,00005C28,00000000}` avec base
   destination relative au descriptor ;
5. EDRAM 10 Mio nulle, déduite du resolve UNORM noir du draw 4×MSAA, puis
   destination tiled bornée à `0x398000` avec gardes avant/après ;
6. comparaison intégrale au tiling CPU, untile 1280×720 et digest linéaire
   `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f`.

Le rectangle copy est interprété comme l'opération de resolve Xenos, comme dans
le render-target cache Xenia/ReXGlue ; il n'est pas rasterisé comme une seconde
primitive couleur approximative. Toute divergence de commande, registre,
format, taille, garde, tiling ou digest trap avant publication.

## Qualification

- `demo-observed` : ordre des deux draws, snapshot copy et `XE_SWAP` ;
- `demo-qualified` : identités shader, registres, destination, dimensions,
  contenu neutral noir et digests ;
- `xenia-generic` : ABI EDRAM/copy, kernel compute et règle de tiling ;
- `unknown` : premier frame non noir, pixels START et formats futurs.

Xenia/ReXGlue ne sont pas une preuve AC6. Le SPIR-V resolve embarqué reste le
blob générique épinglé de 8 380 octets, SHA-256 `e8cfb0d6…87b32`, validé par
`spirv-val`. Aucun SPIR-V, microcode, désassemblage ou actif propriétaire n'est
suivi.

## Reproductibilité et validations

Deux runs neutral Vulkan depuis stores neufs donnent :

- RTPLY `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794` ;
- rapport `33b6c8b3759461fd7720ca3942258cee3fcd1ec4732746a1df8314e50b5685a7` ;
- `normal_draws=1`, `neutral_resolves=1` ;
- sorties byte-identiques 2/2.

Validation : CTest codegen OFF 18/18, codegen ON 17/17, audits source et
complexité PASS. `src/main.cpp` reste borné à 1 195 lignes.

## Prochain checkpoint

Publier un readback runtime durable uniquement sous `probe --milestone-report`,
joint au tick 252 et au digest PM4, sans collision avec trace/report/capsule.
Ensuite comparer neutral et START depuis le même checkpoint frais ; START ne
sera promu que si le premier changement de pixels est causal et guest-owned.
