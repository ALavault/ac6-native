# Cycle 1633 — provenance `rr` du new-press START

## Résultat

Un A/B START distinct est acquis sur deux stores neufs jusqu'au tick 254.
Les rapports directs/sous `rr` sont byte-identiques
(`3d8a3a1b…eeb`) et les RTPLY sont byte-identiques
(`a9cdedb3…cac`). Le seul outil utilisé est le `rr` local épinglé au commit
`7352eb80…dbfb0`.

Le guest écrit la valeur logique START `0x00000010` à `0x829D1550` au tick
252, thread 1 :

| Élément | Preuve PAL |
|---|---|
| fonction | `sub_822F6008` |
| PC | `0x822F6054` |
| bytes | `91 7F 00 14` |
| instruction | `stw r11,0x14(r31)` |
| LR | `0x822F617C` |

Le premier consumer est une copie guest de 64 octets par
`sub_82327D90`, appelée depuis `sub_822F5CE8`. L'instruction
`0x82327E34` (`ldu r7,8(r4)`) copie la valeur de `0x829D1550` vers
`0x7F0409F0`, toujours au tick 252/thread 1.

La première lecture après cette copie est `0x821A4E2C`, bytes PAL
`11 A0 F8 C3`, `lvx128 v13,r0,r31`, dans `sub_821A4C70`. Au watchpoint,
`r31=0x7F0409F0` et `r27=0xF`, ce qui joint exactement le chargement hôte
vectoriel à l'adresse guest alignée. Cette fonction effectue encore une copie
en bloc : aucun rôle fonctionnel ni branche de transition n'est inventé.

## Qualification

- `demo-qualified` : A/B START, writer, bytes, valeur, LR, thread/tick et
  première copie guest ;
- `demo-qualified` : première lecture de la copie à `0x821A4E2C` et registres
  d'adresse effectifs ;
- `unknown` : premier consumer sémantique et persistance sur deux ticks ;
- START et toute transition visuelle restent non promus.

Reçu : `analysis/demo/ac6-demo-start-newpress-rr-provenance-v1.json`.

## Prochain checkpoint

Suivre la destination de cette seconde copie jusqu'au premier test/branche
qui consomme la valeur. Exiger ensuite sa persistance ou sa propagation sur
deux ticks avant toute progression renderer START.
