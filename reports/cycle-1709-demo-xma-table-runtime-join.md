# Cycle 1709 — table XMA PAL jointe au runtime

## Verdict

La table passée au wrapper PAL `0x82357240` est maintenant observée
directement, en neutral et START, avant le trap fail-closed :

```text
table  = 0x17360000
count  = 0x00000003
flags  = 0x00030000
entries= 0x17360010
stride = 0x60       (preuve statique PAL cycle 1703)
index  = 0          (le premier slot nul déclenche l'appel)
slot   = entries + 0x40 = 0x17360050
```

Le mot `0x17360050` n’est donc plus seulement un candidat `entry+64` : son
adresse et son entrée 0 sont joints aux bytes PAL et au runtime frais. Cela ne
qualifie toujours pas l’ABI du contexte XMA, car `XMACreateContext` ordinal 548
trap avant son retour.

## Identité et A/B

- Cible : `Default.xex` démo PAL, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Build : `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256
  `84e3864a55282081507beb3cb22069c1668696c72f6ba1904db591ef0d6ce217`.
- `src/guest_bridge.cpp` SHA-256
  `862e9eea3083cb939f2af3c8fd6e3f140c987cced159201dce1ce1eaaf176834`.
- `src/guest_bridge/xma_import_trace.hpp` SHA-256
  `23b1bf94579d2b3aebb4afb5c67eb55e65fce5555e03f4bfcd7ef1ef8adb0f12`.
- Baseline de complexité : 1 443 lignes; audit pass.
- CTest complet après cycle précédent : **17/17**.
- Bruts : `/fastdata/lavaulta/tmp/ac6-cycle1709.vxxpib/`.

| mesure | neutral | START tick 252 |
|---|---|---|
| RTPLY SHA-256 | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `d53bf82d50724f6b0d771f30edfcdc17439b63fabba6a1e884b72a1ac2268c8d` |
| rapport SHA-256 | `7c2555e8ada7d3c5b65525ef57024fc6af404a3224f4d65e832b1ed1fe3ae131` | `238bd9e4f7e8ea150b7e5622101340145bc0dd9e7076d5c6472f4970634022cc` |
| stderr complet de la sonde | `eac2aecd1c20d7b408ec9f811af9c6f51f92239bda59005b648283f5634be83c` | identique |
| PRESENT / frontier | 911 / tick 1048, ordinal 548 | identique |

## Observation directe

Les deux routes produisent exactement :

```text
AC6_XMA_SLOT_ENTRY pc=0x82357240 ... tick=1048 thread=21
  r3=0x17360000 r4=0 r5=0x6180 r6=0 r7=1
AC6_XMA_TABLE mapped=1 tick=1048 thread=21 table=0x17360000
  count=0x00000003 flags=0x00030000 entries=0x17360010
AC6_XMA_SLOT_LOAD pc=0x82357240 lr=0x82357248 tick=1048 thread=21
  address=0x17360050 size=4 value=0
AC6_XMA_CREATE tick=1048 thread=21 lr=0x82357298
  r3=0x17360050 r4=0 r5=0x6180 r6=0 r7=1
```

Le store de préparation antérieur reste :

```text
AC6_XMA_SLOT_STORE pc=0x823273E0 lr=0x821A3E74 tick=2 thread=1
  address=0x17360050 size=4 value=0xFEFEFEFE
```

## Classification

- **demo-qualified** : `table`, `count`, `flags`, `entries`, `stride`, entrée
  0, slot `+0x40`, PC/LR/tick/thread, store/load et registres d’import;
  neutral/START byte-identiques sur la sonde; trap avant effet.
- **demo-observed** : flags `0x00030000` et les autres entrées non lues dans
  cette route; fenêtre bornée de 96 octets.
- **xenia-generic** : aucun élément promu dans cette jointure.
- **unknown** : ABI du contexte et valeur de retour, contenu audio des trois
  entrées, output pointer après import, packet/timestamps/volume, consumer PCM,
  screencap guest-owned et résultat de mission.

La sonde reste opt-in et en lecture seule. Aucun état guest n’est créé, aucun
packet XMA n’est décodé et aucun fichier propriétaire n’est suivi.

## Prochain checkpoint

Conserver l’import 548 en trap. Qualifier le contenu des entrées 0..2 par
lectures bornées déjà atteintes dans `0x82357240`, puis le retour/output pointer
seulement via une instrumentation qui ne remplace pas l’import. Ne pas lancer
`vgmstream-cli`/FFmpeg avant un packet XMA démo exact.
