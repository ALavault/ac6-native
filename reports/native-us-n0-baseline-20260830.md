# AC6 natif NTSC-U/J — baseline N0 qualifiée (2026-08-30)

## Décision

N0 est vert. Le produit actif est `reconstruction/ace-combat-6`; son build,
ses 88 CTest, son cache NTSC-U/J complet et ses frontières source, binaire et
paquet sont qualifiés. N1a free-flight 1 800 ticks peut commencer.

R0 reste fermé négativement. Ce reçu ne promeut aucune preuve runtime ReXGlue,
aucune sémantique d'objectif et aucune éligibilité JV.

## Contrats NTSC-U/J séparés

Le test `ac6-ntsc-uj-static-contract` remplace le contrat oracle PAL dans la
suite produit sans changer son cardinal : 88 tests. Il relie uniquement :

- l'identité média des dix fichiers NTSC-U/J ;
- la chaîne statique US selector → DPL → `DATA.TBL` dans le projet
  `ghidra-projects/ac6-us` ;
- les 15 payloads campagne structurels qualifiés par lectures ISO bornées.

L'audit dédié rejette les valeurs PAL et interdit de transformer ces faits
statiques en preuve runtime.

## Cache qualifié

- ISO source : `204c5e64…43c98c`, 7 835 492 352 octets.
- Import final : 926/926 entrées, 5 409 550 519 octets décodés.
- Index : `d70719285296f3944c9a5e66ac21efc43298efc7497e08eed8b9ff2c1a34df5b`.
- Audit : 926 blobs, 15 payloads campagne, 15 mondes campagne, entrées M01
  caméra et monde exactes.
- Reçu : `reports/ac6-ntsc-uj-native-baseline-contract.json`, SHA-256
  `74cd688c07cb8102d17af3a81fa53a2a717d0865eab81c65d93fe3dbbde62315`.

Le premier essai a échoué avant décodage sur `bgmpack.bin` : l'importeur
exigeait correctement les six packs média, tandis que le README n'en citait
que quatre fichiers. Les six packs NTSC-U/J qualifiés ont été fournis et la
documentation a été corrigée. L'échec reste dans `import.log`; le succès est
dans `import-complete.log`.

## Build, tests et paquet

- Build complet : vert.
- Binaire : `c1e7ddb5246a507e741be3d51031bba9942e0823e51a81fb22b5828099f65edf`.
- CTest : 88/88, zéro échec, 19 skips explicites pour fixtures retail PAL
  locales absentes.
- Audit complexité : vert, 313 fichiers. Les arbres externes/générés sont
  exclus et la commande import a été déplacée sans changement de comportement
  vers son propriétaire `retail_commands.cpp`.
- Frontière produit : 260 sources et un ELF, vert.
- Paquet : 96 entrées, vert ; aucun ReXGlue, Xenia, C++ généré ou octet retail.
- Archive : `ac6-native-0.1.0-Linux.tar.gz`, SHA-256
  `2d008a259259d93506c3d32d1eac691e067a894bc72126dfa9e2dcedf62a403b`.

## Artefacts

Tous les logs complets et marqueurs de sortie sont sous
`artifacts/native-us-n0-baseline-20260830/` :

- `build-final.log`, statut 0 ;
- `ctest-final.log`, statut 0 ;
- `cache-audit.log`, statut 0 ;
- `package-final.log`, statut 0 ;
- `audits-final.log`, statut 0.

## Frontière suivante

N1a doit relier input/vol à `RetailSession`, publier pose joueur et caméra live,
puis rendre terrain, cité, eau, F-16 et HUD par Vulkan. L'acceptation exige deux
runs identiques de 1 800 ticks, cinq contrôles visibles, sans marqueur, caméra
diagnostique, intégrateur générique ni raster CPU interactif.
