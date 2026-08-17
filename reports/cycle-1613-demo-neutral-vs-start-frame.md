# Cycle 1613 — frame neutral contre START

## Décision

START au tick 252 n'est pas promu comme transition visuelle. Dans les deux
replays historiques construits avec le même manifeste codegen
`b6fb4890…fc201` et le même boundary config `1f9e3878…45c8`, le trace neutral
porte `buttons=0` au tick 252 et le trace START porte `buttons=16`, avec
`logical_pressed=16`. Malgré cette différence d'entrée prouvée, les états PM4
neutral tick 253 et START tick 254 sont byte-identiques : même ring, mêmes IB,
mêmes packets, mêmes shaders, mêmes draws, mêmes effets, même `XE_SWAP` et
même compteur de 115 PRESENT. Aucune conséquence visuelle causale de START
n'est donc établie à ce checkpoint.

Le replay neutral frais, effectué depuis un store neuf avec le runtime
fail-closed courant, s'arrête au tick 0 sur le registre Xenos non qualifié
`0x0A02`. Il ne produit aucun `XE_SWAP`, aucun PRESENT et aucun readback. Son
état visuel guest est explicitement **inconnu** ; aucune screencap n'est
publiée.

## Neutral frais

Rapport source :
`/fastdata/lavaulta/tmp/ac6-demo-neutral-frame.KIIJGE/report.json`.
Inventaire durable :
`analysis/demo/ac6-demo-neutral-first-pm4-inventory-v1.json`, SHA-256
`085ff9984699b68177afd4d815b01dfb44ea30e2969c9a91db3f2688782350ac`.

| IB | Dwords | SHA-256 | Contenu structurel utile |
|---|---:|---|---|
| `0x1686A040` | 11 | `332312da…f699` | wait et référence |
| `0x1685A000` | 64 | `325e40b0…15d5` | 23 packets, trois IB imbriqués |
| `0x16ADF000` | 74 | `5714cd0f…1c3d` | deux `IM_LOAD_IMMEDIATE` |
| `0x16ADFD40` | 13 | `7faaea1a…84a9` | écritures de registres |
| `0x16AE0980` | 48 | `4f2445f9…dbfc` | 24 `DRAW_INDX_2` point |

Total : 72 packets et 235 dwords décodés structurellement. Le premier champ
inconnu est `0x0A02`, packet type 0 à l'offset dword 2 de `0x1685A000`.

Les deux microcodes bootstrap incorporés sont :

| Stage | Packet | Taille | SHA-256 microcode |
|---|---|---:|---|
| vertex | `0x16ADF000` +2 dwords (`0x16ADF008`) | 24 dwords / 96 bytes | `099625f3…e4e3` |
| pixel | `0x16ADF000` +29 dwords (`0x16ADF074`) | 9 dwords / 36 bytes | `4913603d…8e25` |

## Capture neutral/START historique commune

Les deux replays qualifient exactement :

- ring `0x126CA000`, deux soumissions et 25 dwords ;
- IB `0x127CA0C0`, 11 dwords, `ef7ab6e4…d2b0` ;
- IB `0x1274A000`, 3029 dwords, `d121c8d8…358d6` ;
- 877 packets : 340 type 0, 252 type 2, 285 type 3 ;
- 26 draws cumulés : 24 points bootstrap et deux rectangles ;
- un `XE_SWAP` à l'offset dword 415 du main IB.

Les trois `IM_LOAD_IMMEDIATE` du main IB sont :

| Stage | Offset / adresse packet | Taille | SHA-256 microcode |
|---|---|---:|---|
| vertex | 146 / `0x1274A248` | 27 dwords / 108 bytes | `93488cb9…402b` |
| pixel | 176 / `0x1274A2C0` | 9 dwords / 36 bytes | `4913603d…8e25` |
| vertex | 333 / `0x1274A534` | 15 dwords / 60 bytes | `586168ec…3cc0` |

Le shader pixel est identique au bootstrap. Son microcode correspond exactement
à deux containers de la démo, dans les wrappers NSXR `0x8264B390` et
`0x8264B790`, chaque fois au décalage `+0x280`. Les deux slices complètes de
160 bytes sont byte-identiques (SHA-256
`cd0be9a9accf9e9aa8e86d10069c684d3dcaa1909232e468b8e94406d3977e8c`) et
contiennent le microcode de 36 bytes SHA-256 `4913603d…8e25`. XenosRecomp
épinglé au commit `990d03b28a27b50277ee5d8d942e1c5f873869d1` accepte les deux containers et
produit, hors dépôt, deux sorties byte-identiques SHA-256
`e5ef8c7082aa80108c533d41b047a08ac6a12a32b501d3f4d9347ae53366d6ef`.
Cette preuve qualifie le décodage mais pas encore une instance productrice
unique. Les trois hashes vertex n'ont pas encore de correspondance exacte dans
les wrappers démo inspectés. Aucun nom, container ou adresse retail n'est
importé.

## Swap, copy/resolve et readback

Le packet final prouve seulement le fetch/swap suivant : ressource hashée
`5a192a0a…93e0`, adresse `0x1374A000`, format brut 6, tiled, 1280×720. Les
écritures candidates `RB_COPY_CONTROL=0` et `RB_COPY_DEST_INFO=0` apparaissent
aux offsets 404 et 406, mais ne suffisent pas à établir l'effet du dernier
copy/resolve. La source, la destination de resolve, le pitch et l'endian sont
inconnus. Les dimensions/format/tiling du fetch de présentation ne sont pas
promus comme paramètres de la surface source.

Le readback fail-closed s'arrête donc à `0x0A02`, avant tout effet. Sans source,
destination, pitch, endian et tiling de resolve tous qualifiés, produire une
image serait un fallback visuel interdit. Les containers temporaires et les
sorties XenosRecomp restent sous `/fastdata/lavaulta/tmp` et ne sont pas suivis.

## Prochain test

Qualifier `0x0A02..0x0A05` depuis une autorité Xenos primaire ou une preuve
dynamique démo exacte, puis rejouer neutral et START avec le même binaire,
store et checkpoint. Scanner ensuite hors ligne les seuls containers démo par
hash exact des quatre microcodes. START ne pourra être promu qu'après une
écriture guest différente, son consumer visuel et un readback reproductible.
