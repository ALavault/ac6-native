# Cycle 1682 — jonction des wrappers de section critique

## Verdict

Le seul écart neutral/START qui subsistait dans l'atlas frais jusqu'au tick
800 est maintenant joint aux bytes PAL exacts : `0x822E1DF0` est un wrapper
de branche vers l'import `RtlEnterCriticalSection`, et `0x822E1DF8` un wrapper
de branche vers `RtlLeaveCriticalSection`. Cette qualification porte sur le
contrôle de flux et la cible d'import, pas sur une transition frontend ou un
effet pixel.

Les deux routes gardent le même ensemble de fonctions, d'arêtes indirectes,
d'imports et d'IB. Le bouton START au tick 252 ne peut donc toujours pas être
promu comme cause d'une transition visuelle.

## Identité et preuves statiques

| élément | valeur |
|---|---|
| cible | `ac6-demo-xbox360-pal`, `Default.xex` |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon PPC big-endian / Xenos |
| basefile utilisé pour le contrôle des bytes | `.build/ac6-demo-codegen-xenon-38/xex-basefile.bin` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| atlas statique | `analysis/demo/ac6-demo-static-decomp-atlas-v1.json`, SHA `7ee1e677…714e2d` |
| sémantique statique | `analysis/demo/ac6-demo-static-semantics-v1.json`, SHA `e723930a…ba7962` |
| appelgraph | `analysis/demo/ac6-demo-sdk-callgraph.json`, SHA `fc051713…a82f99` |

Le mapping adresse→offset de ce basefile est `offset = guest - 0x82000000`.
Les quatre bytes relus directement sont :

| wrapper guest | offset basefile | bytes PAL | cible de branche | sens qualifié |
|---|---:|---|---|---|
| `0x822E1DF0` | `0x002E1DF0` | `48 09 3D F4` | `0x82375BE4` | `RtlEnterCriticalSection` |
| `0x822E1DF8` | `0x002E1DF8` | `48 09 3D DC` | `0x82375BD4` | `RtlLeaveCriticalSection` |

Les bornes Ghidra du projet démo sont respectivement `[0x822E1DF0,
0x822E1DF3]` et `[0x822E1DF8,0x822E1DFB]`; leurs hashes de bytes sont
`39fda93031a36353924736edccdee43a6e8ce169b3718e5004fde714051dec3d` et
`dc7b05359e80ad587b45aa660d0fd056fffa69d91f92ba1a149d8d65c2c86395`.
La fonction appelante `0x820FF8D8` couvre `[0x820FF8D8,0x820FF987]`, hash de
bytes `d83eea89ef1ed6e33d23ebe88b02578284ca0b24f959bd04fad58e89b86684f5`,
et possède les deux appels directs dans le registre statique.

Le C++ généré n'est cité qu'en cross-match de contrôle de flux, jamais comme
source de sémantique : `sub_822E1DF0` branche sur l'import à `0x82375BE4` et
`sub_822E1DF8` sur celui à `0x82375BD4` (`.build/ac6-demo-codegen-atomic-1`,
fichier `generated/ppc_recomp.36.cpp`).

## Comparaison dynamique fraîche

Les deux probes process-fresh sont celles du cycle 1681, stores neufs,
codegen ON, backend headless, jusqu'à 800 ticks, avec START `buttons=0x10` au
tick 252.

| observation | neutral | START |
|---|---:|---:|
| fonctions atteintes | 2 456 | 2 456 |
| arêtes indirectes | 1 078 | 1 078 |
| imports | 581 | 581 |
| `0x820FF8D8` | 548 | 548 |
| `0x822E1DF0` | 478 087 | 477 919 |
| `0x822E1DF8` | 478 087 | 477 919 |
| xboxkrnl ordinal 293, LR `0x820FF91C` | 476 443 | 476 275 |
| xboxkrnl ordinal 304, LR `0x820FF93C` | 475 895 | 475 727 |
| PRESENT | 116 | 116 |

Les quatre dernières lignes varient toutes de `-168` sous START. Les atlas
gardent les mêmes `first_tick=252`, `last_tick=799` pour les deux wrappers et
les mêmes IB : intermédiaire `ef7ab6e4…d2b0`, principal
`d121c8d8…358d6`. Aucun hook guest de lecteur/écrivain frontbuffer ne produit
de ligne; `frontend`, `mission` et `terminal` restent faux.

## Qualification

- `demo-qualified` : bytes, bornes, hashes et cibles d'import des deux
  wrappers; cadence A/B exacte et limitée aux appels de section critique.
- `demo-observed` : baisse `-168` des quatre compteurs après l'entrée START.
- `xenia-generic` : aucun élément utilisé pour cette jonction.
- `unknown` : état de la section critique, frontière frontend, consumer
  guest-owned, pixels, audio, mission, résultat et screencap.

Aucune écriture d'état synthétique n'a été ajoutée. Aucun projet Ghidra,
Xenia/ReXGlue, microcode, C++ généré ou actif propriétaire n'a été modifié ou
suivi.

## Prochain checkpoint

Instrumenter la frontière guest qui consomme effectivement la file de rendu
ou l'état XAM après le tick 252, en conservant le même A/B process-fresh. Une
variation de cadence des critical sections seule ne justifie ni START
causal, ni readback, ni screencap.
