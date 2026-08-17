# Cycle 1634 — consumer sémantique et non-persistance de START

## Résultat

Le watchpoint `rr` du gate START cycle 1633 est poursuivi jusqu'au premier
consumer sémantique. Après les deux copies en bloc, `sub_82198BF0` lit la
valeur à `0x7F040910` au tick 252/thread 1 :

| Étape | PC | Bytes PAL | Effet |
|---|---|---|---|
| lecture | `0x82198D0C` | `81 61 00 78` | `lwz r11,120(r1)` |
| extraction START | `0x82198D68` | `55 7C 35 6A` | bit `0x10` → bit `0x400` |
| publication | `0x82198DD0` | `91 7F 00 08` | écrit `0x400` à `0x827B37E0` |

Le prochain appel input au tick 253 montre `0x827B37E0 = 0`. La sortie
new-press est donc une impulsion d'un tick et ne persiste pas deux ticks.
Cette conclusion ne qualifie pas encore un changement de tâche, de frontend
ou de pixels : START reste non promu visuellement.

## Qualification

- `demo-qualified` : A/B direct/rr, chaîne writer → copies → lecture →
  extraction → publication, PC/bytes/LR/thread/tick et valeur au tick 253 ;
- `demo-qualified` : réfutation de la persistance de la sortie new-press sur
  deux ticks ;
- `unknown` : éventuel consumer du niveau courant, construction des tâches
  frontend et transition visuelle.

Reçu : `analysis/demo/ac6-demo-start-newpress-rr-provenance-v1.json`.

Politique inchangée : rr local commit `7352eb80…dbfb0`, aucune preuve retail,
aucune mutation Xenia/ReXGlue/Ghidra/C++ généré/microcode, aucun actif suivi.

## Prochain checkpoint

Comparer cette publication `0x827B37E0` au champ de niveau courant et aux
dispatches guest des ticks 252–254, en conservant un watchpoint exact. Ne
promouvoir START que si une tâche/frontend et une écriture guest causale sont
observées.
