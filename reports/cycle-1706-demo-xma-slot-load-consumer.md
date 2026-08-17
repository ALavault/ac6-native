# Cycle 1706 — lecture du slot XMA et A/B frais

## Verdict

Une paire de stores neufs neutral/START, bâtie avec le même binaire démo,
confirme la chaîne guest suivante sans implémenter l’import :

```text
tick 2  thread 1  0x823273E0:0x821A3E74  store 0x17360050 = FEFEFEFE
tick 1048 thread 21 0x82357240:0x82357248 load 0x17360050 = 00000000
tick 1048 thread 21  LR 0x82357298  XMACreateContext ordinal 548 -> trap
```

Le résultat est identique entre neutral et START pour les écritures, les
lectures, les registres de l’import, le diagnostic et le nombre de PRESENT.
Le START modifie seulement les compteurs d’événements d’entrée dans le
rapport et le RTPLY; il ne produit aucune transition XMA qualifiée.

## Identité et artefacts

- Cible : `ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Build : `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256
  `55cb936000849012c7516fecb9002e7118db4471b440a26285e74291beef15bb`.
- Hook source `src/guest_bridge.cpp`, SHA-256
  `f947c64dd89be8c5c909a4540f18e03ec8f852f50cecc0dae5d73d4c1f1eceec`.
- Hook source `src/guest_bridge/xma_import_trace.hpp`, SHA-256
  `8ef408447f37650691fab009aa4f051bd9840ece06bfd4691db1e615d227b09a`.
- Baseline de complexité : `src/guest_bridge.cpp` 1 434 lignes; audit pass.
- CTest avec `TMPDIR=/fastdata/lavaulta/tmp`, `SDL_AUDIODRIVER=dummy` :
  **17/17**.
- Exécutions : `/fastdata/lavaulta/tmp/ac6-cycle1706.YcZlMG/` (artefacts
  bruts conservés hors du projet).

## A/B neutral / START

| élément | neutral | START tick 252 |
|---|---|---|
| codegen/ghidra/boundary | `ae57c868…a185` / `576fa31e…086c` / `398df897…d248` | identiques |
| RTPLY SHA-256 | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `d53bf82d50724f6b0d771f30edfcdc17439b63fabba6a1e884b72a1ac2268c8d` |
| rapport SHA-256 | `7c2555e8ada7d3c5b65525ef57024fc6af404a3224f4d65e832b1ed1fe3ae131` | `238bd9e4f7e8ea150b7e5622101340145bc0dd9e7076d5c6472f4970634022cc` |
| stderr de la sonde | `4f563ac6306859bc9b4beddbc9d4ddbd9eef73ecd123c34cb1c16696ac56ec80` | identique |
| PRESENT | 911 | 911 |
| frontier | tick 1048, thread 21, LR `0x82357298`, ordinal 548 | identique |
| VD_SWAP | 1280×720, format 6, frontbuffer `0x1374A000`, 911 appels | identique |

Les RTPLY diffèrent comme attendu par les événements d’entrée; la présence de
START n’est pas une preuve de transition visuelle ou audio.

## Preuves directes du slot

La sonde est opt-in (`AC6_DEMO_WATCH_XMA_SLOT=1`) et observe uniquement les
loads/stores guest réussis. Les deux runs produisent exactement ces deux
lignes :

```text
AC6_XMA_SLOT_STORE pc=0x823273E0 lr=0x821A3E74 tick=2 thread=1
  address=0x17360050 size=4 value=0x00000000FEFEFEFE
AC6_XMA_SLOT_LOAD pc=0x82357240 lr=0x82357248 tick=1048 thread=21
  address=0x17360050 size=4 value=0x0000000000000000
```

L’appel immédiatement suivant est :

```text
AC6_XMA_CREATE tick=1048 thread=21 lr=0x82357298
  r3=0x17360050 r4=0x00000000 r5=0x00006180 r6=0x00000000 r7=0x00000001
```

Le hook historique imprime 96 octets bornés à partir de `r3`; cette fenêtre
est une observation, pas un descripteur qualifié. Le rapprochement statique
PAL du cycle 1703 indique que `r3` est le slot de sortie et que la table
contient une base d’entrées de 96 octets; la valeur observée ici ne suffit
pas à sélectionner une entrée ni à produire un contexte XMA.

## Classification

- **demo-qualified** : identité XEX; A/B frais; store/load du mot
  `0x17360050`; PC/LR, tick et thread de ces accès; registres de l’import;
  911 PRESENT et `VD_SWAP` identiques; trap avant tout effet de l’ordinal 548.
- **demo-observed** : motif de remplissage `FEFEFEFE`; fenêtre de 96 octets
  imprimée par la sonde; différence de compteurs d’imports induite par START.
- **xenia-generic** : aucun élément utilisé pour qualifier ce slot.
- **unknown** : premier writer avant le remplissage; table/index réellement
  sélectionné; valeur de retour/output pointer après l’import; consumer et
  packets XMA; PCM, volume, timestamps et audio anglais/japonais.

## Garde et prochain checkpoint

L’ordinal `xboxkrnl.exe:548 (XMACreateContext)` reste fail-closed et trap à
`tick=1048`; aucun décodage, readback ou screencap n’est promu. Le prochain
test ciblé est une nouvelle paire neutral/START avec capture du TLS
tick/thread au store du helper `0x823273E0`, puis un watchpoint sur le premier
consumer du mot `[0x17360050,0x17360054)`. L’output pointer post-appel et le
premier packet doivent être établis avant toute utilisation de
`vgmstream-cli`/FFmpeg ou implémentation XMA.

L’archive Xenia valorisée au cycle 1657 reste une oracle séparée; elle ne
fournit aucune preuve `demo-qualified` pour ce slot.
