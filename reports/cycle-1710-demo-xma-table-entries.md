# Cycle 1710 — entrées XMA PAL bornées

## Verdict

La sonde PAL lit, à l’entrée de `0x82357240`, les trois entrées de la table
runtime (3 × 96 octets), sans écrire ni appeler l’import. Neutral et START
produisent les mêmes 72 mots :

| index | adresse | mots non nuls observés (offset → valeur) |
|---:|---|---|
| 0 | `0x17360010` | `+0x00→0x07800000`, `+0x04→0xB8800000`, `+0x1C→0x17360180`, `+0x20→0x17361F80`, `+0x44→0x17360180`, `+0x48→0x17361F80` |
| 1 | `0x17360070` | `+0x00→0x07800000`, `+0x04→0xB8800000`, `+0x1C→0x17362180`, `+0x20→0x17363F80`, `+0x44→0x17362180`, `+0x48→0x17363F80` |
| 2 | `0x173600D0` | `+0x00→0x07800000`, `+0x04→0xB8800000`, `+0x1C→0x17364180`, `+0x20→0x17365F80`, `+0x44→0x17364180`, `+0x48→0x17365F80` |

Tous les autres mots de ces entrées, y compris `+0x40` (output slot), sont
zéro dans cette observation. Les valeurs sont conservées comme mots
big-endian démo ; aucune signification XMA n’est inférée depuis leurs formes.

## Identité et A/B

- Cible : `Default.xex` PAL démo, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Build : `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256
  `58c1b403795f75692491122bfbbd46a0ed59981f84d0811f16ac39fd2001660f`.
- `src/guest_bridge.cpp` SHA-256
  `862e9eea3083cb939f2af3c8fd6e3f140c987cced159201dce1ce1eaaf176834`.
- `src/guest_bridge/xma_import_trace.hpp` SHA-256
  `daa49656f6ca8d77cdc2158eb215dd40473eb5d98f8b85cdb0d9b3eaa6edaa4b`.
- Complexité : 1 443 lignes; CTest complet après cette capture : **17/17**.
- Bruts : `/fastdata/lavaulta/tmp/ac6-cycle1710.4WglKZ/`.

| mesure | neutral | START tick 252 |
|---|---|---|
| RTPLY SHA-256 | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `d53bf82d50724f6b0d771f30edfcdc17439b63fabba6a1e884b72a1ac2268c8d` |
| rapport SHA-256 | `7c2555e8ada7d3c5b65525ef57024fc6af404a3224f4d65e832b1ed1fe3ae131` | `238bd9e4f7e8ea150b7e5622101340145bc0dd9e7076d5c6472f4970634022cc` |
| stderr sonde | `8fd7688187f440a0e4cb242e7abda91edc42b018043845032a8fe9d0fff17ca8` | identique |
| PRESENT / import | 911 / ordinal 548 trap au tick 1048 | identique |

## Table et sélection

La même ligne runtime précède les entrées :

```text
table=0x17360000 count=0x00000003 flags=0x00030000 entries=0x17360010
```

Le code PAL joint au cycle 1703 utilise `entry = entries + index*0x60` et
`output_slot = entry + 0x40`. L’entrée 0, dont le slot est nul, est donc celle
qui déclenche `XMACreateContext(0x17360050)`. Les entrées 1 et 2 sont observées
mais ne sont pas appelées dans cette route parce que le wrapper s’arrête au
trap de l’entrée 0.

## Classification

- **demo-qualified** : adresse/count/flags/base; stride et offset issus des
  bytes PAL; les trois adresses d’entrée; tous les mots non nuls et les zéros
  de sortie; sélection de l’entrée 0; A/B exact.
- **demo-observed** : motifs et espacements des pointeurs guest dans les
  entrées 1/2.
- **xenia-generic** : aucun élément promu.
- **unknown** : sens des champs, format des buffers, ABI du contexte, valeur
  de retour, packets/timestamps/volume, PCM, sortie vidéo guest-owned et
  mission.

La capture est opt-in, bornée à 288 octets et strictement lecture seule.
L’ordinal 548 reste fail-closed; aucun audio n’est décodé et aucune screencap
n’est produite.

## Prochain checkpoint

Utiliser uniquement ces adresses/ranges comme cibles de stores/loads ciblés
dans une nouvelle exécution, puis qualifier le premier packet XMA ou l’absence
de consumer avant le trap. Ne pas donner de noms sémantiques aux offsets sans
une preuve PAL supplémentaire.
