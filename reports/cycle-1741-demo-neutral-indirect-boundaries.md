# Cycle 1741 — trois frontières indirectes neutral fermées

## Résultat

Un probe neutral frais, avec les deux expériences XMA déjà bornées, a dépassé
le frontier précédent de tick 1100 et atteint tick 2126. Trois entrées callable
manquantes ont été prouvées par leur cible dynamique, les bytes PAL du basefile
et leurs terminaisons de contrôle, puis ajoutées uniquement à
`confirmed-chunks.toml`. Le prochain trap fail-closed est
`0x8223FF70 -> 0x822CD118`, toujours à tick 2126, thread 1. START n'a pas été
injecté et aucun frontend n'est qualifié.

| entrée fermée | taille | SHA-256 bytes PAL | LR appelant |
|---|---:|---|---|
| `0x82277768` | `0x40` | `f6a88529…74d0` | `0x8223D110` |
| `0x822CCCB8` | `0x3C` | `ddcbba9f…a38f` | `0x8223D798` |
| `0x822CC3B8` | `0x1C` | `b065d7eb…bb3b` | `0x8223E304` |

Les trois entrées sont des sous-entrées de chunks Ghidra existants ; aucune
borne Ghidra, aucun C++ généré et aucun checkout externe n'ont été modifiés.
Chaque replay a conservé le même RTPLY neutral SHA-256
`1fda8336…5ed7`. Les rapports successifs sont scellés dans la capsule
`analysis/demo/ac6-demo-neutral-boundary-batch-1741-v1.json`.

## Atlas et shaders statiques

L'atlas statique a été régénéré deux fois avec des sorties byte-identiques :
12 870 fonctions, 112 frontières confirmées nettes, 3 041 220/3 041 220 bytes
`.text` classés et SHA-256 `37480d8c…604f`. L'ancien atlas est sauvegardé sous
`/fastdata/lavaulta/tmp/ac6-demo-static-decomp-atlas-v1.pre1741.json`.

Le handoff `ac6-demo-static-pac-shader-main-handoff.md` est intégré : les
quatre microcodes du premier frame sont des plages `.rdata` exactes du
basefile PAL et passent traduction plus `spirv-val` 4/4. Leur absence du scan
des 1 891 microcodes PAC uniques reste une preuve négative, pas une preuve de
synthèse dynamique. Cette qualification n'établit toujours aucun pixel non
noir.

## Validations

- codegen Release frais : 12 870 fonctions, zéro diagnostic de frontière,
  zéro instruction non supportée ;
- atlas frais A/B : byte-identique, couverture complète ;
- tests statiques : 77/77 ;
- sources shaders qualifiées : 4/4, `spirv-val` 4/4 ;
- pytest schéma + provenance shaders : 2 passés, 64 désélectionnés ;
- CTest codegen-OFF : 18/18 ;
- CTest codegen-ON Release : 17/17.

Deux fixtures `ac6-demo-xenos-command-tests.cpp` utilisent désormais des
vecteurs explicites avant conversion en `std::span`, ce qui ferme le build GCC
15 sans modifier le runtime ni les commandes testées.

## Prochain checkpoint

Traiter uniquement `0x822CD118` : cible dynamique, bytes, bornes et replay
neutral minimal. Ne pas injecter START avant un état frontend guest-owned, et
ne pas produire de screencap tant qu'un readback EDRAM non noir reproductible
n'est pas joint au guest.
