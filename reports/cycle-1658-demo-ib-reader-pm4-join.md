# Cycle 1658 — lecture de l’IB principal et jonction PM4

## Résultat

Un hook opt-in `AC6_DEMO_WATCH_IB_READERS` consigne les lectures effectuées par
la frontière native `GuestBridge::apply_xenos_mmio_write` après la publication
guest du write pointer. Neutral et START (tick 252, store neuf, codegen ON,
300 ticks) produisent exactement les mêmes 9 lectures ciblées, digest
`d5dc151c…d226e13`, et le même flux writer (`b96745e4…a39e3`).

## Lecture observée

La séquence lue est :

| région | adresse | offset | valeur |
|---|---:|---:|---:|
| ring publication | `0x126CA058` | 0 | `C0013F00` |
| ring publication | `0x126CA05C` | 1 | `0x1274A000` |
| ring publication | `0x126CA060` | 2 | `0xBD5` |
| IB principal | `0x1274A000` | 0 | `0x00000D02` |
| IB principal | `0x1274A3BC` | 239 | `C0003601` |
| IB principal | `0x1274A60C` | 387 | `C0003600` |
| IB principal | `0x1274A660` | 408 | `0x00054800` |
| IB principal | `0x1274A67C` | 415 | `C0036400` |
| IB principal | `0x1274CF50` | 3028 | `0x00000005` |

La publication est attachée au contexte guest thread 1, tick 0, LR
`0x821B9C80`, fonction `__imp__sub_821B9BC8`. Le chemin lit les mots ring aux
lignes 418–439 de `graphics_ring.hpp`, capture l’IB exact aux lignes 452–485,
puis transmet le flux borné à `XenosCommandProcessor::process_batch`.

## Jonction structurale

La plage lue est `[0x1274A000,0x1274CF54)`, 3 029 dwords, SHA
`d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`. Les
offsets 239 et 387 sont les deux `PM4_DRAW_INDX_2`; 415 est `PM4_XE_SWAP`;
3028 est le dernier payload de `PM4_EVENT_WRITE_SHD` commencé à 3025. Le type-0
à 408 est conservé comme écriture registre `0x4800`, sans nouvelle
interprétation sémantique.

La capture prouve un consommateur hôte exact du flux publié. Elle ne prétend
pas qu’un PC guest distinct lit l’IB : le hardware Xenos n’est pas une fonction
guest recompilée, donc `guest_reader_pc` reste explicitement inconnu.

## Qualification et limites

- `demo-qualified` : égalité A/B, valeurs lues, publication, bornes/hash IB,
  offsets PM4 et chemin transactionnel vers `process_batch`.
- `demo-observed` : le reader est un load hôte instrumenté ; la ligne générée
  `808` est un contexte d’instrumentation, pas un PC guest.
- `xenia-generic` : aucune preuve utilisée.
- `unknown` : PC guest matériel, sémantique du registre `0x4800`, pixels,
  frontend, mission et résultat.

Le hook est désactivé par défaut et n’altère aucun état. Les stores, Ghidra,
Xenia/ReXGlue, microcodes et actifs propriétaires restent inchangés.

## Validation

- A/B codegen ON : deux rapports identiques (`a11feac6…2a4340` /
  `c74ca0ea…3012593`), traces connues, stderr identiques
  `4edea680…7b57597`.
- Chaque route : 9 lectures, 4 958 stores main IB et 3 stores publication,
  163 PRESENT, aucun frontend/mission/terminal.
- Build démo/codegen : `cmake --build` passe ; CTest démo `18/18` et codegen
  `17/17`, pytest ciblé `52 passed, 4 subtests`, audits source et complexité
  passés. Le test scelle le digest de lecture et l’absence de PC guest
  inventé.

Capsule : `analysis/demo/ac6-demo-ib-reader-pm4-join-v1.json`.
