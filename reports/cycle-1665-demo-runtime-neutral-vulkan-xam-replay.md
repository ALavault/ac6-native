# Cycle 1665 — replay Vulkan/XAM déterministe à 600 ticks

## Résultat

Une route neutral PAL a été enregistrée avec le build codegen ON courant,
backend Vulkan, depuis un store neuf, jusqu’à 600 ticks. Le probe a produit un
RTPLY-v4 et un movie XAM, puis `replay` a rejoué le RTPLY depuis un second store
neuf avec le movie XAM en lecture stricte.

| artefact | valeur |
|---|---|
| cible | `Default.xex`, SHA `de917873…5da8` |
| ticks | 600 |
| probe | `max_ticks`, code retour 4 attendu |
| replay | `deterministic=true`, code retour 0 |
| événements replay | 2151 |
| movie XAM | 68 973 octets, SHA `64a53fadbaf306a5b7cd6f4d67dc676f8b1de7f5a7418315faa580fe2d9854ed` |
| RTPLY-v4 | 5 946 400 octets, SHA `6c34827cc3f9962a7f4042610d69aeb54bbd0165fd5a1c830d341efad07970c7` |

Le replay valide la trace complète byte-à-byte avant suppression de sa trace
temporaire. Le renderer produit les mêmes valeurs aux deux passages :

- 5 shader loads, 26 draws, 1 present, 1 normal draw, 1 neutral resolve ;
- readback normal 640×360 :
  `0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366` ;
- resolve 1280×720 :
  `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f`.

## Qualification

- `demo-qualified` : identité XEX, movie XAM scellé, RTPLY-v4 et replay
  Vulkan déterministe depuis deux stores neufs ; aucune divergence XAM ou trace.
- `demo-observed` : 463 notifications PRESENT, renderer atteint au tick 600.
- `unknown` : EDRAM guest-owned, destination guest-owned, pixels non noirs,
  frontend, mission et résultat.

Le replay n’utilise aucun HID. START n’est pas impliqué dans cette route
neutral et aucune screencap n’est produite. Le resolve reste explicitement
fail-closed sur la projection neutral noire ; ce reçu ne ferme donc pas le lane
visuel ni le frontend.

## Artefacts temporaires et politique

Les fichiers sont sous `/fastdata/lavaulta/tmp/ac6-demo-vulkan-cycle1665-QLxyCT/`.
Les deux stores contiennent le même `Default.xex` PAL et les mêmes hashes de
conteneurs ; aucun actif n’est copié dans le projet.

Aucune modification retail, Xenia/ReXGlue, Ghidra, C++ généré, microcode ou
actif propriétaire n’est impliquée.

Capsule : `analysis/demo/ac6-demo-runtime-neutral-vulkan-xam-replay-v1.json`.
