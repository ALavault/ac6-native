# Cycle 1717 — sonde PAL neutral/START de l’adresse XMA

## Verdict

Une sonde strictement opt-in et read-only joint les valeurs dynamiques au
calcul PAL sans ajouter de mapping MMIO. Neutral et START lisent la même valeur
à `0x7FEA1800`, écrivent le même global, passent le même pointeur physique à
`MmGetPhysicalAddress`, puis tentent le même store `stwbrx` à
`0x7FEA1A80` avec la valeur wire `0x01000000`. Le trap survient avant effet,
au tick 1048/thread 21, après 911 PRESENT. La route par défaut conserve le
trap ordinal 548 et son RTPLY de référence.

Cette sonde qualifie `P/G/A/V` et leur causalité PAL; elle ne qualifie ni le
nom ou l’effet du registre, ni un packet XMA, le PCM, l’audio ou les pixels.

## Identité et garde

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire sonde | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` |
| binaire SHA-256 | `30429ddecb4154d2c09f4f68055bc69f937f38850afcfd8cb3212043313ed2bb` |
| variables | `AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1`, `AC6_DEMO_WATCH_XMA_ADDRESS=1` |
| backend/borne | `probe`, headless, `max_ticks=1050` |
| source de la sonde | `src/guest_bridge.cpp`, `src/guest_bridge/lifecycle.hpp`, `src/guest_bridge/audio_memory_dispatch.hpp`, `src/guest_bridge/xma_import_trace.hpp` |
| route de production | inchangée; `XMACreateContext` reste fail-closed |

Les temporaires et sorties complètes restent sous `/fastdata/lavaulta/tmp` et
ne sont pas suivis.

## Observations directes

| étape | neutral | START (`0x10` au tick 252) |
|---|---|---|
| lecture `0x7FEA1800` | tick 106, wire `0x0000802E`, logique `0x2E800000` | identique |
| store global `0x829DA52C` | tick 106, valeur `0x2E800000`, LR `0x8234F078`, `sub_82356510` | identique |
| `MmGetPhysicalAddress` du contexte | tick 1048, entrée `0x2E800000`, sortie `P=0x2E800000`, LR `0x823572AC` | identique |
| store MMIO PAL | tick 1048/thread 21, `A=0x7FEA1A80`, wire `0x01000000`, LR `0x823572AC` | identique |
| résultat | trap avant effet, 911 PRESENT | identique |

Les appels de préparation `MmGetPhysicalAddress` à `0x17360180`,
`0x17361F80`, `0x17362180`, `0x17363F80`, `0x17364180` et `0x17365F80`
ont également été observés au tick 1048, avant la création du premier
contexte; ils ne sont pas interprétés comme des adresses XMA.

Le store dynamique est joint aux bytes PAL déjà qualifiés :
`0x823572D8 = 7D60552C` (`stwbrx r11,0,r10`) puis
`0x823572DC = 7C0006AC` (`eieio`). La valeur wire observée correspond au
calcul logique `V=1` avec l’inversion big-endian du chemin `stwbrx`.

## Sorties et hashes

| route | RTPLY | rapport | stderr |
|---|---|---|---|
| neutral opt-in | `ab54c75ebccfdf3a41a840ab691a728e1d120a7616cca98a5f6ff42fa804fc43` | `e4c33682b9c834b1a08380c35581adc865bfb090207d96962d3d6848eaf399e0` | `3b8df9bf480ce529a1ddadc63c9a9091ca5260a04bceb7b359810af6928c6003` |
| START opt-in | `2ce0a717f8d3df56aa87644d7fc9624f5e02b1020a0b5eeaf0992f6877db417f` | `54ba6694f6fcc5cfdbd94627896cd299a119a803916d70f92e5bf2cbc490e8` | `3b8df9bf480ce529a1ddadc63c9a9091ca5260a04bceb7b359810af6928c6003` |
| neutral sans expérimentation | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `7c2555e8ada7d3c5b65525ef57024fc6af404a3224f4d65e832b1ed1fe3ae131` | `9fa0f47bab02839f61641f25b7e67789ad350a50a90f8cad8b6c7627692456c8` |

Les RTPLY/rapports opt-in restent ceux du cycle 1715; les nouveaux stderr
incluent seulement les lignes de sonde. Le contrôle par défaut conserve le
RTPLY baseline et piège sur `xboxkrnl.exe` ordinal 548 au tick 1048.

## Classification

- **demo-qualified** : lecture `0x7FEA1800`, global `0x829DA52C`,
  `P=0x2E800000`, `A=0x7FEA1A80`, valeur wire `0x01000000`, PC/LR/tick/thread,
  égalité neutral/START et garde par défaut.
- **demo-observed** : tentative de store et trap avant effet; 911 PRESENT.
- **xenia-generic** : aucune sémantique importée par cette sonde.
- **unknown** : effet matériel de `A/V`, index de registre, état XMA, packets,
  timestamps, volume, PCM, consumer audio et pixels.

## Prochain checkpoint

Ne pas transformer `0x7FEA1A80` en registre nommé et ne pas laisser la sonde
dans `play` ou `replay`. Le prochain test autorisé est une jointure statique
des autres écritures XMA de la même tranche (`0x82357310` et les stores
`0x7FEA1804/0x7FEA1AC0`) puis, seulement avec une preuve indépendante de leur
effet, une nouvelle expérience opt-in. Sans cette preuve, conserver le trap
ordinal 548 et ne pas décoder via FFmpeg/vgmstream.

Capsule : `analysis/demo/ac6-demo-xma-address-trace-v1.json`.
