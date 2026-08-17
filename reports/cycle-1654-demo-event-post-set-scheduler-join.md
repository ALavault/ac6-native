# Cycle 1654 — `NtSetEvent` vers la prochaine activation scheduler

## Résultat

Un hook opt-in et read-only capture la première activation de thread après
chaque `NtSetEvent(E000004C)`. Le hook est désactivé par défaut et ne touche ni
le guest state ni le renderer. Le run A/B frais couvre 300 ticks avec des
stores neufs et conserve les deux traces RTPLY déjà qualifiées.

Chaîne observée, sans extrapolation sémantique :

```text
payload writer 0x822EEE3C
  → lecture E000004C à 0x822EEE44
  → NtSetEvent, caller LR 0x821A6AC4
  → event_wake
  → première activation scheduler du même tick
```

## A/B

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 300 | 300 |
| `set_enter` E000004C | 351 | 351 |
| `event_wake` E000004C | 351 | 351 |
| activations post-set | 351 | 351 |
| delta tick `resume_tick-set_tick` | 0 (351/351) | 0 (351/351) |
| digest lignes post-set | `ebf80c87…92537a` | `ebf80c87…92537a` |
| SHA stderr | `3a082f48…cf94c9a` | `3a082f48…cf94c9a` |
| PRESENT | 163 | 163 |
| frontend / mission / terminal | non / non / non | non / non / non |

Les traces RTPLY et les rapports restent respectivement
`2e49ae67…ad9c115` / `179db68a…235345b` et
`a11feac6…2a4340` / `c74ca0ea…3012593`.

## Activations exactes

| entry guest | thread repris | startup | parameter | occurrences neutral=START | hash bytes PAL |
|---|---:|---|---|---:|---|
| `0x821A7160` | 1 | `0x00000000` | `0x00000000` | 53 | `7840b8e8…31b175b5` |
| `0x821C4970` | 18 | `0x821A93F8` | `0x100446D4` | 149 | `1322ae24…a6da3ee2b` |
| `0x82320560` | 21 | `0x821A93F8` | `0x00000000` | 97 | `8bc388e6…dbaffdb0` |
| `0x822EE158` | 14 | `0x821A93F8` | `0x82933F78` | 39 | `b74b5e20…434ab37` |
| `0x821A1D10` | 9 | `0x821A93F8` | `0x00000001` | 13 | `3d8036a5…98acb046` |

Les cinq frontières sont présentes dans les sémantiques statiques du projet
`ace-combat-6-demo` et leurs bytes sont lus dans le basefile PAL
`b98a9ac1…4218`. Leurs rôles restent `unknown`; aucun nom généré n’est promu
comme preuve de comportement.

Le thread 12 produit 299 `NtSetEvent` et le thread 1 en produit 52. Les
threads repris sont 1 (53), 18 (149), 21 (97), 14 (39) et 9 (13). Les tuples
`event_wake` → activation scheduler concordent 351/351 dans chaque route;
la valeur `granted_thread` et l’état post-publication sont ceux du wake, pas
une interprétation de l’objet.

## Qualification

- `demo-qualified` : handle, LR, tick, thread, wake, entry/startup/parameter,
  bytes PAL, ordre et égalité A/B.
- `demo-observed` : cinq familles d’activation scheduler et leurs comptes.
- `unknown` : fonction appelée après l’entrée, rôle des paramètres, transition
  scheduler→queue PM4, frontend, mission et pixels.
- `xenia-generic` : aucune preuve Xenia fusionnée.

Le résultat ne justifie toujours ni START comme transition frontend, ni
screencap. La prochaine frontière est le premier store/lecture guest effectué
par ces activations, puis son lien exact avec la queue Xenos déjà capturée.

## Validation

- build codegen ON/OFF réussis après ajout du hook ;
- CTest codegen ON `17/17` et OFF `18/18` ; pytest build corpus `48 passed`,
  4 sous-tests ; audit sources et complexité `pass` ;
- le hook est borné à 8 192 lignes et désactivé sans
  `AC6_DEMO_WATCH_EVENT_POST_SET` ;
- neutral/START frais : 351/351 wakes et activations, digests identiques ;
- aucune modification du C++ généré, de Ghidra, de Xenia/ReXGlue, des
  microcodes ou d’un actif propriétaire.

Capsule SHA-256 :
`7b419adf84f7d7ec83afac7ade49c018a574bfc7bf654022539508c955ffa78d`.
