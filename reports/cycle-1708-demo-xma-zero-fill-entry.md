# Cycle 1708 — entrée PAL du zero-fill XMA

## Verdict

Une nouvelle paire neutral/START, avec une sonde d’entrée de fonction
désactivée par défaut, joint le contexte guest du zero-fill et du wrapper XMA.
Les deux routes sont strictement identiques sur la chaîne utile :

```text
0x821A4B70 entry: tick 1048, thread 21, LR 0x82356610,
  r3=0x17360000 r4=0 r5=0x6180
0x82357240 entry: tick 1048, thread 21, LR 0x8235675C,
  r3=0x17360000 r4=0 r5=0x6180 r6=0 r7=1
load 0x17360050 = 0, then XMACreateContext(r3=0x17360050) -> trap
```

Le helper `0x821A4B70` exécute le chemin PAL `dcbzl` borné par la longueur
`0x6180`; la sonde ne transforme pas ce comportement et ne le nomme pas
« initialisation XMA ». Le premier consumer post-import reste inexistant dans
le run parce que l’ordinal 548 est toujours fail-closed.

## Identité et validation

- Cible : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Build : `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256
  `0ecb565b0346fe9b9404aa612ab04bd52670f8c2ee0031722a3bcccc689ff393`.
- `src/guest_bridge.cpp` SHA-256
  `9e8a30cb98c596bdb78184ae7c5c9801dc2c5135a783d3e70ac694404ae1771e`.
- `src/guest_bridge/xma_import_trace.hpp` SHA-256
  `115d6921256f80a1dc1c9c274001be32b4e4481793f27ac60dc6d944a33e24f2`.
- Complexité : `src/guest_bridge.cpp` 1 438 lignes, audit pass.
- CTest complet avec `TMPDIR=/fastdata/lavaulta/tmp` et audio dummy :
  **17/17** après ce cycle.
- Bruts hors projet : `/fastdata/lavaulta/tmp/ac6-cycle1708.Dso5wQ/`.
- Garde désactivée : une probe fraîche bornée à 1 tick sans variable XMA ne
  produit aucune ligne `AC6_XMA_*` (sortie max-ticks attendue).

## A/B frais

| mesure | neutral | START tick 252 |
|---|---|---|
| RTPLY | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `d53bf82d50724f6b0d771f30edfcdc17439b63fabba6a1e884b72a1ac2268c8d` |
| rapport | `7c2555e8ada7d3c5b65525ef57024fc6af404a3224f4d65e832b1ed1fe3ae131` | `238bd9e4f7e8ea150b7e5622101340145bc0dd9e7076d5c6472f4970634022cc` |
| stderr sonde | `dbfa5e8fcb8e7deaa83fcdc952eaf09c9f6774a5f1cb5a35712137f739f67a02` | identique |
| outcome/frontier | trap ordinal 548, tick 1048, thread 21 | identique |
| PRESENT | 911 | 911 |

Les deux RTPLY diffèrent uniquement par l’injection START et ses compteurs;
la chaîne XMA observée et le `VD_SWAP` restent identiques.

## Entrées et stores observés

Le filtre `AC6_DEMO_WATCH_XMA_SLOT=1` ne conserve que les entrées concernées
et les accès réussis au mot `[0x17360050,0x17360054)` :

```text
AC6_XMA_SLOT_ENTRY pc=0x821A3C30 lr=0x821A481C tick=0 thread=1
  r3=0 r4=0 r5=1 r6=7 r11=0x82774B00
AC6_XMA_SLOT_STORE pc=0x823273E0 lr=0x821A3E74 tick=2 thread=1
  address=0x17360050 size=4 value=0xFEFEFEFE
AC6_XMA_SLOT_ENTRY pc=0x82356528 lr=0x82351958 tick=1048 thread=21
  r3=3 r4=0x2EB408A0 r5=0 r6=0x2E8B5BA0 r7=0x108
AC6_XMA_SLOT_ENTRY pc=0x821A4B70 lr=0x82356610 tick=1048 thread=21
  r3=0x17360000 r4=0 r5=0x6180
AC6_XMA_SLOT_ENTRY pc=0x82357240 lr=0x8235675C tick=1048 thread=21
  r3=0x17360000 r4=0 r5=0x6180 r6=0 r7=1
AC6_XMA_SLOT_LOAD pc=0x82357240 lr=0x82357248 tick=1048 thread=21
  address=0x17360050 size=4 value=0
AC6_XMA_CREATE tick=1048 thread=21 lr=0x82357298
  r3=0x17360050 r4=0 r5=0x6180 r6=0 r7=1
```

Les sept observations utiles ci-dessus sont byte-identiques entre neutral et
START; le stderr complet de chaque route a le même SHA.

## Classification et garde

- **demo-qualified** : contexte tick/thread/LR du zero-fill et du wrapper;
  borne `r3=0x17360000`, `r5=0x6180`; store FE et load zéro du slot;
  registres d’appel et trap ordinal 548; 911 PRESENT A/B.
- **demo-observed** : interprétation de la boucle `dcbzl` comme remplissage
  borné, et entrée initiale `0x821A3C30`.
- **xenia-generic** : aucun élément utilisé.
- **unknown** : writer antérieur au FE, table/index effectivement choisi,
  valeur de retour/output pointer après import, premier consumer, packets XMA,
  timestamps, PCM et screencap guest-owned.

L’import reste sans effet : pas d’allocation, de décodage, de mutation audio,
de readback ou de screencap. La sonde d’entrée ne s’active qu’avec la variable
d’environnement explicite.

## Prochain checkpoint

Conserver l’ordinal 548 en trap. Qualifier d’abord le retour/output pointer
avec un arrêt juste après l’import uniquement dans une branche expérimentale
qui reste fail-closed, ou, si le trap empêche tout consumer, produire une
preuve statique complète de l’absence de consumer atteignable avant le trap.
Ne pas utiliser `vgmstream-cli` ou FFmpeg avant le premier packet XMA exact.
