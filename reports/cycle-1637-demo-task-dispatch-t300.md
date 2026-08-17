# Cycle 1637 — dispatcher de tâches START jusqu'au tick 300

## Résultat

Le replay `rr` local du même START XAM, depuis un store neuf et avec le
binaire démo PAL qualifié, observe le dispatcher guest `0x82259D10` à chaque
tick 252–299 (48 observations). La lecture de la mémoire guest est décodée en
big-endian et la liste est parcourue sans mutation ni resynchronisation.

| liste | nœud | tâche | vtable | slot 4 |
|---:|---:|---:|---:|---:|
| `+564` | — | — | — | — |
| `+580` | `0x18970424` | `0x2E7F0080` | `0x8201130C` (`CModeTaskStartUpDemoOffline`) | `0x8218A4A0` |
| `+596` | `0x18970404` | `0x18BA2BF4` | `0x8200F388` (`CTaskLoading`) | `0x8218CE20` |
| `+596` | `0x18970414` | `0x18980000` | `0x82011694` (`CTaskModeManager`) | `0x821929A8` |

Les vtables candidates `0x8200C904` / `0x8200E5C4` et leurs owners
`0x82170F58` / `0x82185198` ne sont ni dans ces listes ni atteints par le
probe de frontière. L'absence est rapportée comme observation bornée, pas
comme preuve de non-construction hors de cette fenêtre.

## Provenance et garde

- cible : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- `rr` : `.tools/rr-install/bin/rr`, commit
  `7352eb807ed75e3b51be85fa6a27f121235dbfb0` ;
- trace : `ac6-demo-start-rr-t300/trace` sous `TMPDIR` ;
- script GDB SHA-256 :
  `4dcc6e3ac6f9ee74170cf86aeb3a3ad690ebe73b9af2112105d7db9a199f2f29` ;
- log SHA-256 :
  `31f649f320cb722238e0261e0e25bc2da7493ac355230b857484f3f6d6047741` ;
- callsite slot 4 : `0x82259D74` ;
- les noms RTTI sont joints au catalogue statique démo qualifié ; les valeurs
  vtable/nœud/tâche et les ticks sont observés dynamiquement.

L'A/B frais direct/`rr` jusqu'au tick 300 reste byte-identique : RTPLY
`0b5ffbbd…85182`, rapport `e3f6b8e2…9785`, 163 PRESENT, frontend/mission faux.
START n'est donc pas promu et aucune mutation renderer n'est déduite.

## Classification

- `demo-qualified` : identité XEX, breakpoint/callsite, listes guest, nœuds,
  tâches, vtables, slot 4, ticks et hashes du script/log ;
- `unknown` : producteur qui construirait les deux tâches menu, transition
  visuelle et tout effet au-delà de tick 300 ;
- aucune preuve retail, Xenia/ReXGlue, Ghidra, C++ généré, microcode ou actif
  propriétaire n'est importée ou suivie.

## Prochain checkpoint

Instrumenter en lecture seule la file guest `0x82386CC0` et ses indices
`+0x60D0/+0x60D4`, autour des frontières primaire `0x820FF8D8` et worker
`0x820FFCA0`, afin d'identifier le premier producteur/lecteur qui pourrait
construire une tâche menu. Toute promotion exige un nouvel A/B frais et
conserve le renderer fail-closed.
