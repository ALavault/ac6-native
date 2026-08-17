# Cycle 1681 — reachability neutral/START jusqu’au tick 800

## Résultat

Deux probes fraîches, stores neufs, codegen ON et backend headless, produisent
un atlas de reachability `v1` jusqu’au tick 800. Neutral et START (bouton
`0x10` au tick 252) atteignent exactement les mêmes fonctions et arêtes :
2 456 fonctions, 1 078 arêtes indirectes et 581 imports. Aucun nouveau
producteur guest de frontend ou de pixel n’apparaît dans START.

Les seules divergences de compteurs sont des répétitions dans
`xboxkrnl.exe` : ordinals 293 et 304, appelés depuis `0x820FF91C` et
`0x820FF93C`. Elles diminuent légèrement sous START, sans changer l’ensemble
des cibles. Les deux fonctions `0x822E1DF0` et `0x822E1DF8` ont la même fenêtre
`252..799` et un compteur inférieur de 168 sous START. Ce sont des différences
de cadence observées, pas une transition sémantique qualifiée.

## Identité et artefacts

| élément | neutral | START |
|---|---|---|
| cible | `Default.xex` PAL, SHA `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` | identique |
| atlas SHA-256 | `1ab042eb6ec9f0d73edf2f7233b3ee089b2ecedfedfb936491e16b7e5bf272e8` | `41d14bfd476a5b8892e7f4efc709eb9e92ab4e992bbb04db06a9776b8b1af859` |
| rapport SHA-256 | `4b27f553f3b50b93325834272c010f30a0595e9953e256d766e8298f4312090f` | `15cd5574232531f1f94b4dc239f4eb92d2912827bd63c6c8128d716d42c6f4ab` |
| RTPLY SHA-256 | `dbde578cbaaa543458bb3071657fb6625c314c333ed89be415345a731166fb25` | `df272431ba560497a35822c46f27976a5204382ff17c4bcefd68783a886523d2` |
| movie XAM SHA-256 | `ea359f36338102513a2f483af103dc38dc30528108569cceb9e7c70b112d2a66` | `cb3160ec6491e78ed6b31595a676583bf2f20025b7aef42cf62fdfc64b0f2bb7` |
| outcome | `max_ticks`, 800 | `max_ticks`, 800 |

Les atlas et runs restent sous
`/fastdata/lavaulta/tmp/ac6-demo-atomic-reachability-800.19008/`; aucun
artefact temporaire n’est copié dans le produit.

## Comparaison déterministe

| corpus | neutral | START | comparaison |
|---|---:|---:|---|
| fonctions | 2 456 | 2 456 | ensembles identiques |
| arêtes indirectes | 1 078 | 1 078 | enregistrements identiques |
| imports | 581 | 581 | deux compteurs différents |
| `0x822E1DF0` / `0x822E1DF8` | 478 087 | 477 919 | −168 chacun |
| xboxkrnl ordinal 293, LR `0x820FF91C` | 476 443 | 476 275 | −168 |
| xboxkrnl ordinal 304, LR `0x820FF93C` | 475 895 | 475 727 | −168 |

Toutes les fonctions communes gardent `first_tick`/`last_tick` identiques,
sauf les deux fonctions de cadence ci-dessus. La paire d’IB reste
`0x127CA0C0/11` SHA `ef7ab6e4…d2b0` et `0x1274A000/3029` SHA
`d121c8d8…358d6`; chaque route observe 116 PRESENT et
`frontend=false`, `mission=false`, `terminal=false`. Les hooks guest
frontbuffer readers/writers restent silencieux.

## Qualification

- `demo-qualified` : identité PAL, atlas de reachability borné, égalité des
  ensembles de fonctions/arêtes, différences de compteurs exactes et A/B
  reproductible.
- `demo-observed` : variation −168 des quatre compteurs associés à la boucle
  scheduler/import.
- `xenia-generic` : aucun.
- `unknown` : transition visuelle, consumer guest-owned, pixels, frontend,
  mission, résultat et screencap.

START n’est donc pas promu comme transition causale à tick 252. Le prochain
checkpoint doit viser une frontière guest qui différencie réellement les
routes, ou prolonger la qualification des imports sans injecter d’état.

Politique : aucune preuve retail fusionnée; aucun Xenia/ReXGlue/Ghidra, C++
généré, microcode ou actif propriétaire modifié ou suivi.
