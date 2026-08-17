# Cycle 1675 — état lu par le handler d’interruption graphique

## Verdict

Le hook opt-in `AC6_DEMO_WATCH_GRAPHICS_INTERRUPT_STATE=1` journalise les
`load32` du seul handler `0x821C5190`. Deux exécutions process-fresh (neutral
et START, 800 ticks, Vulkan) produisent exactement les mêmes 12 lignes d’état
au tick 1. La valeur chargée au site de `bctrl` interne est `r9=0`, donc
`0x821C528C` ne fait aucun appel indirect dans le corpus atteint.

Le handler n’est pas un consumer de pixels : les hooks scalaires et vectoriels
voient zéro lecture de `0x1374A000..0x13AE2000`. Le diagnostic ne modifie pas
GuestMemory, et la route normale reste sans ce hook.

## Identité et artefacts

- Cible : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, Xenon
  big-endian / Xenos.
- Neutral : RTPLY
  `bbc7ecb8fc20bfdbbdd1d70d24a4fdc78f6619dd5622c761e88541d776fd7068`, rapport
  `0d70fd74c24bd4205491c66c56bd4a5c31278dd99d6270a44248196990563ffa`.
- START : RTPLY
  `54a2860fccb21ab3be0595ae5532a7f9e0dbc3b7b971aaf334a7a087f5a427e1`, rapport
  `062e97c4087ec46b14979ecfd0ce3cef1e5f00531e27bfd899d7e6b7386c36fe`.
- Stderr des deux runs, incluant registration, 800 calls et 12 loads :
  SHA-256 `c0563a6e313861793ba51e165a65521b1ac10e2753575f144c5c7e170b6c1079`.
- Binaire vector diagnostic avec garde : SHA-256
  `686e311babdbc740967e790639339fd8f1e5233f44e8a2f0c6ee3532ed789ea4`.
- Binaire codegen normal : SHA-256
  `502c62a0fe19e9116a3ec557f1f4c62b506d4f842ab1eccc36f33261e1daea91`.

## Trace exacte (12/12 identique neutral/START)

| adresse | valeur | LR | ligne générée |
|---|---:|---:|---:|
| `0x82000608` | `0x000101BE` | `0x821C5198` | 28303 |
| `0x000101BE` | `0x10041A00` | `0x821C5198` | 28305 |
| `0x10045A9C` | `0x00000000` | `0x821C51B8` | 28314 |
| `0x10046E18` | `0x0000003C` | `0x821C51B8` | 28316 |
| `0x10045A90` | `0x00000000` | `0x821C51B8` | 28341 |
| `0x10045A94` | `0x00000000` | `0x821C51FC` | 28369 |
| `0x10045A8C` | `0x00000000` | `0x821C51FC` | 28371 |
| `0x10045A84` | `0x00000000` | `0x821C51FC` | 28392 |
| `0x10045A8C` | `0x00000000` | `0x821C51FC` | 28428 |
| `0x10045A9C` | `0x00000001` | `0x821C51FC` | 28436 |
| `0x10045AA0` | `0x00000000` | `0x821C51FC` | 28461 |
| `0x10044494` | `0x16AE2000` | `0x821C51FC` | 28482 |

Le load de `0x10045A84` correspond à `lwz r9,16516(r31)` dans le PAL généré
par le décompilateur épinglé. Sa valeur zéro explique l’absence de l’arête
interne après `bctrl` `0x821C528C`; aucune cible n’est inventée.

## Qualification

- `demo-qualified` : adresses, valeurs, LR, tick 1, thread 2 et répétition
  byte-identique neutral/START; `r9=0` au site computed-call; zéro lecture
  scalaire/vectorielle du frontbuffer; 663 `VdSwap` par route.
- `demo-observed` : état global `0x82000608 → 0x000101BE → 0x10041A00` et
  champs relatifs lus dans le handler.
- `xenia-generic` : aucune nouvelle preuve.
- `unknown` : sémantique des champs, cible future si `r9` devient non nul,
  lecture hôte de scanout, pixels, frontend, mission et screencap.

## Validation et garde

- CTest codegen : `17/17`.
- CTest vector diagnostic : `17/17`.
- `render_status.py --check` : `AC6_DEMO_STATUS_PASS`.
- `git diff --check` : pass.
- Hook activé uniquement par deux variables d’environnement, limité à 256
  lignes, sans écriture guest; le produit normal ne change pas de comportement.

Après activation de la garde d’adresses, deux probes process-fresh à 253 ticks
(`headless`, neutral puis START) passent sans trap :

| run | rapport | trace | stderr | loads | violations |
|---|---|---|---|---:|---:|
| neutral | `d96a9b68a90f389207ee725ea5f182ea935d798f15c341bc455c6208370b6a79` | `1d41d2e26003a631f8bec19534812a258ffc5d90466be885f6f7f3df797ebef7` | `84f4cb2905d5df8e6d4818dda12edce1729894b8393d9b613cca3200f80c80b8` | 12 | 0 |
| START | `0f66d089f0656be4c79f0cd9b101f194d27ba6cefcd91e49e60ca2dfa10d564e` | `31553733582cf7345375d8f197a28645621e488700f2a31b7e883b66109360b2` | identique | 12 | 0 |

La plage autorisée par la garde est exactement le global `0x82000608`, le
pointeur observé `0x000101BE`, les champs observés
`0x10045A84..0x10045AA0`, `0x10046E18` et `0x10044494`. Toute autre adresse
dans `0x821C5190` déclenche `RuntimeTrap` avant le load.

Le prochain changement doit rechercher le premier consumer guest-owned hors de
cette frontière. Il ne doit pas produire de readback ou de fallback visuel tant qu’un consumer
guest-owned n’est pas établi.

Aucun Xenia/ReXGlue, Ghidra, C++ généré, microcode, preuve retail ou actif
propriétaire n’a été modifié ou suivi.
