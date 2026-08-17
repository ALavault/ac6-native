# Cycle 1694 — shim borné du chunk `0x820E7E08`

## Résultat

Après la jointure dynamique du cycle 1693, un shim fail-closed a été ajouté
pour l’entrée observée `0x820E7E08`. Il vérifie les 16 bytes PAL du chunk
`0x820E7E00..0x820E7E0F`, écrit seulement `r5=0` comme le stub exact, puis
appelle la fonction déjà qualifiée `0x820E1F78`. Il ne crée pas de frontière
Ghidra et ne modifie aucun état guest en dehors de l’effet littéral du stub.

Un A/B frais avec le même binaire montre que neutral et START atteignent tous
deux 600 ticks et 463 PRESENT. START ajoute uniquement les arêtes attendues
à tick 268; frontend, mission, terminal et pixels restent non qualifiés.

## Identité et implémentation

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile | SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| shim | `src/guest_bridge/qualified_thunk.hpp`, SHA `b3c16369db739fa8313da90a67834124e5592f7fb40800778635352dd6251c02` |
| dispatch table | `src/guest_bridge.cpp`, SHA `c880a12db076cd5d3c1992cf74ce20f08b0af49e9f3adb7398cb1b43d7796946` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA `0b56619b39900a755c7ddd2e8f05081f7ecb94fd943cc5a6c690178e0a5ced41` |
| vérification | `ac6-demo-frontier-tests`, `ac6-demo-python-tests`, `ac6-demo-status`, `ac6-demo-no-guest-cli` : 4/4 |

Le helper `dispatch_reached_chunk_entry` refuse toute adresse autre que
`0x820E7E08`, vérifie les mots BE
`38a000014bffa17438a000004bffa16c`, exige `lookup(0x820E1F78)`, puis exécute
`r5=0` et la fonction cible. Toute divergence trappe avant effet.

## A/B frais

| route | résultat | PRESENT | RTPLY SHA | rapport SHA | stderr SHA |
|---|---|---:|---|---|---|
| neutral, pas d’entrée, 600 ticks | `max_ticks` | 463 | `b724112495de5af96b395f93ba5d1dacd8a522bfae2aa1fcc69890b3c9aac9fb` | `dbe6484af6c1446db0e2e5017b34441d909cc07e81922959d8ad9d9719b16484` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| START tenu ticks 252–267, 600 ticks | `max_ticks` | 463 | `e1e99f68d7a3f04de87d8d4244eea137d9306265797b9f42ef0bc9c30185993e` | `dddbdc3d71169559449b5e69bc6dc5150a174fd3dcb4e0528b5089ce8e56d15b` | `ef14bba9caa3ffa9a41b6930be137d8a0d2cbe3c604ade589c28ee0f27e33d1c` |

Les deux rapports indiquent `frontend=false`, `mission=false` et
`terminal=false`. Le rapport START contient la dispatch virtuelle exacte
`object=0x2E3D3D14`, `vtable=0x820077AC`, `slot=78`.

## Différence contrôlée START

Par rapport à neutral, START ajoute à tick 268 :

| LR | cible | compteur | qualification |
|---|---|---:|---|
| `0x82321F34` | `0x820E7E08` | 1 | chunk stub, vtable/slot exacts |
| `0x82321F30` | `0x820D0D10` | 1 | cible statique connue, sémantique inconnue |
| `0x823231B8` | `0x820DEA08` | 3 | cible `.pdata` connue, sémantique inconnue |

Les fréquences des imports `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`
changent aussi, mais aucun état visuel ou PRESENT ne diverge. Les deux
nouvelles cibles restent des observations de contrôle de flux, pas des noms
de frontend.

## Classification et garde

- `demo-qualified` : bytes du chunk, sélection vtable slot 78, shim exact,
  A/B frais à 600 ticks, 463 PRESENT communs et tests 4/4.
- `demo-observed` : trois arêtes START ajoutées à tick 268 et variation des
  compteurs de sections critiques.
- `xenia-generic` : aucun élément.
- `unknown` : sémantique de `0x820E7E08`, `0x820D0D10`, `0x820DEA08`,
  frontend, pixels, audio, mission et terminal.

Le shim reste limité au runtime de la démo, sans fallback visuel. Le prochain
checkpoint doit comparer la transition guest autour des trois nouvelles
arêtes et rechercher une écriture guest causale; il ne doit pas promouvoir
START ni ajouter de rendu synthétique sur la seule égalité des PRESENT.

