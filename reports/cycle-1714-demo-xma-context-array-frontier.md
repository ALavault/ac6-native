# Cycle 1714 — frontière XMA après lecture de la base de contexte

## Verdict

La lecture PAL observée du registre XMA `0x7FEA1800` a été reproduite dans une
expérience strictement opt-in. Elle alloue paresseusement une table de
contextes générique, respecte l'inversion de bytes de `lwbrx`, puis avance le
premier trap de `0x7FEA31E0` à `0x7FEA1A80`. Ce progrès est une frontière
dynamique, pas une qualification du registre, du contexte XMA ou de l'audio.

La route par défaut est inchangée : son RTPLY reste byte-identique à la
baseline cycle 1712 et l'import `XMACreateContext` ordinal 548 piège au tick
1048. Aucun lane du gate PAL n'est fermé.

## Identité et provenance

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire testé | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` |
| binaire SHA-256 | `d6ad26517ed9e746cf50b0a499db924daca5cade4f8129708ee213c5acde49d3` |
| expérience | `AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1` |
| borne | `probe`, headless, `max_ticks=1050`, stores neutral/START cycle 1712 |

La couture statique `sub_82356510` et ses bytes PAL restent ceux du reçu
cycle 1713 : `lwbrx` lit `0x7FEA1800`, puis le code écrit la valeur calculée
dans le global PAL avant l'appel de contexte. Aucun nom générique n'est
transplanté dans le binaire.

La fonction PAL `0x82357240..0x8235730B` est aussi jointe directement au
basefile (204 octets, SHA-256
`7436f8404267283916f2f2e64fdcda534788553fbf366daa330bc09fe9220ed9`). Ses
instructions calculent un index `((physical - 0x829DA52C) >> 6)`, le bornent à
16 bits, le sauvegardent à `context+0x50`, puis forment l'adresse d'écriture
avec le littéral PAL `0x1FFA86A0` et `index >> 5`, avant `stwbrx` et `eieio`.
Pour l'entrée observée, l'index vaut 1 et l'adresse résultante vaut
`0x7FEA1A80`; cette jointure qualifie l'arithmétique et l'ordre des effets,
mais ne donne aucun nom ou sémantique au registre matériel.

## Résultats A/B

| route | RTPLY SHA-256 | rapport SHA-256 | stderr SHA-256 | frontier | PRESENT |
|---|---|---|---|---|---:|
| neutral opt-in | `ab54c75ebccfdf3a41a840ab691a728e1d120a7616cca98a5f6ff42fa804fc43` | `e4c33682b9c834b1a08380c35581adc865bfb090207d96962d3d6848eaf399e0` | `18b1a2b66a11c5e63a3f778b2cd9e3a4294ee8635da3f2a70396056fa008bd7b` | tick 1048, write `0x7FEA1A80` | 911 |
| START opt-in (`0x10` au tick 252) | `2ce0a717f8d3df56aa87644d7fc9624f5e02b1020a0b5eeaf0992f6877db417f` | `54ba669894f6fcc5cfdbd94627896cd299a119a803916d70f92e5bf2cbc490e8` | `18b1a2b66a11c5e63a3f778b2cd9e3a4294ee8635da3f2a70396056fa008bd7b` | tick 1048, write `0x7FEA1A80` | 911 |
| default neutral | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `7c2555e8ada7d3c5b65525ef57024fc6af404a3224f4d65e832b1ed1fe3ae131` | `9fa0f47bab02839f61641f25b7e67789ad350a50a90f8cad8b6c7627692456c8` | ordinal 548, tick 1048 | 911 |

Les deux routes opt-in ont le même diagnostic, le même thread/tick, les mêmes
IB et le même stderr. Le RTPLY START diffère normalement par l'entrée au tick
252. Le RTPLY par défaut `6a759832…f20` est l'invariant de non-régression
attendu.

## Effet observé et limites

La lecture opt-in alloue `320 × 64` octets à `0x2E800000`, puis renvoie la
valeur wire-endian correspondant à cette base. Le premier store suivant piège
avant effet sur `0x7FEA1A80`, avec `LR=0x823572AC`, thread 21, tick 1048 et
valeur logique `1`. Cette adresse est le seul nouveau fait dynamique; sa
sémantique XMA, le bitmap, l'initialisation de contexte, les packets,
timestamps, volumes, PCM et langues restent `unknown`.

Les deux IB graphiques restent inchangés et complets : ring base `0x126CA000`,
25 dwords soumis, IB intermédiaire `0x127CA0C0/11` SHA
`ef7ab6e4…d2b0`, IB principal `0x1274A000/3029` SHA
`d121c8d8…358d6`. Les 911 PRESENT ne constituent pas un readback qualifié.

## Classification

- **demo-qualified** : identité XEX, bytes PAL de la lecture, égalité neutral/
  START des IB et de la frontière, invariance RTPLY de la route par défaut.
- **demo-observed** : allocation opt-in `0x2E800000`, tentative de store
  `0x7FEA1A80`, thread/tick/LR et trap avant effet.
- **xenia-generic** : taille `XMA_CONTEXT_DATA=64` et réserve de 320 entrées,
  utilisés comme forme de test seulement.
- **unknown** : registre `0x0C78`/`0x0680` selon le chemin, sémantique du store,
  indexation matérielle, premier paquet XMA, décodage audio et toute parité
  visuelle.

## Garde et prochain checkpoint

Le handler reste borné par l'environnement et les callsites PAL exacts;
`play`/`replay` ne l'activent pas. La route par défaut conserve l'import trap
et son RTPLY baseline. Ne pas mapper `0x7FEA1A80` par approximation : le
prochain test doit fournir une preuve PAL indépendante de lecture/écriture et
d'effet pour cette zone, ou revenir au trap ordinal 548. Aucun décodage XMA,
readback ou screencap n'est promu.

Sorties temporaires :
`/fastdata/lavaulta/tmp/ac6-cycle1714-final-neutral.yK4zVl/`,
`/fastdata/lavaulta/tmp/ac6-cycle1714-final-start.H0oBrn/` et
`/fastdata/lavaulta/tmp/ac6-cycle1714-default3.BshWxO/`.

Capsule : `analysis/demo/ac6-demo-xma-context-array-frontier-v1.json`.
