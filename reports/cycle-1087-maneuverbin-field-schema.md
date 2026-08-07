# Cycle 1087 — schéma de champ de `ManeuverBin::read`

Date : 2026-08-07. Quatrième application de la méthode des cycles 1084 à 1086.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- Dérivé de la décompilation seule. Aucun émulateur, aucun bridge, aucune
  exécution du produit natif.
- Artefacts : `analysis/scenario-schema/ManeuverBin.json`,
  `tools/validate_ac6_scenario_schema.py`, tests dans
  `tools/tests/test_ac6_static_tooling.py`.

## Sept chaînes d'identité

| adresse | message |
| --- | --- |
| `0x820105D4` | `ManeuverBin::read() / data empty!` |
| `0x820105A8` | `ManeuverBin::read() / child empty!` |
| `0x82010538` | `ManeuverBin::read() / comtblm empty! : %d` |
| `0x8201056C` | `ManeuverBin::read() / comtblm child empty! : %d` |
| `0x82010500` | `ManeuverBin::getReadBuffSize() / data empty!` |
| `0x820104C8` | `ManeuverBin::getReadBuffSize() / child empty!` |
| `0x8201048C` | `ManeuverBin::getReadBuffSize() / comtblm empty!` |

`ComTblMBin` **n'a pas de fonction propre** : il est inliné dans
`ManeuverBin::read`, et sa seule trace est la chaîne
`0x82010460 ComTblMBin::read() / p_data empty!`.

## Enregistrement `ManeuverBin` — 0x0C octets, 3 × `u32`

`0x82330C58` écrit l'enregistrement au curseur de tampon et passe
`curseur + 0x0C` comme sous-tampon pour chaque emplacement de manœuvre : la
taille vient de l'appelant.

| offset | champ | contenu |
| --- | --- | --- |
| +0x00 | `data` | son premier **mot** est le compteur ; échec fermé `0x820105D4` |
| +0x04 | `comtblm` | tableau de `count` × **4** — un pointeur résolu chacun, la charge utile `ComTblMBin` |
| +0x08 | `comtbl` | tableau de `count` × **8** — un enregistrement `ComTblBin` chacun |

**Particularité : le compteur est un `s32`**, pas un `u8`. `SetBin`, `ActBin` et
`ComTblBin` utilisent tous un `u8` ; `ManeuverBin` est le seul de la famille à
lire un mot entier.

`getReadBuffSize` confirme les deux tableaux par une expression différente :
`((count + count*2) & 0x3FFFFFFF) << 2`, soit `count × 12` — exactement
`4 + 8` par élément, plus la somme des `ComTblBin::getReadBuffSize`.

### Descente en deux sauts

`read` ne touche pas directement le nœud `ComTblBin` ; il descend :

```
élément  = table + child_off[i]              échec fermé 0x82010538
comtblm[i] = élément.data_off résolu         échec fermé 0x82010460  (ComTblMBin)
interne  = élément.table_off résolu          échec fermé 0x8201056C
noeud ComTblBin = premier enfant d'interne, exigé présent   échec fermé 0x8201056C
```

### Sous-enregistrement `ComTblBin` — 0x08

| offset | champ | contenu |
| --- | --- | --- |
| +0x00 | `data` | son premier **octet** est le compteur ; échec fermé `0x82010690` |
| +0x04 | `coms` | tableau de `count` × 4, lus par `ComBin::read 0x82331E78` |

`ComTblBin::getReadBuffSize` retourne `count << 2`, cohérent avec des éléments
`ComBin` de 4 octets. `count == 0` sort avant la table enfant.

## Validation sur les octets réels

| observable | valeur |
| --- | ---: |
| nœuds `ManeuverBin` | **977** |
| éléments de manœuvre | **9 251** |
| compteurs négatifs | **0** |
| dépassements de table | **0** |
| incohérences | **0** |
| éléments atteignant leur nœud `ComTblBin` | **9 251 / 9 251** |

Éléments par manœuvre : de 3 à 15, **sans jamais valoir 6**
(9 → 371, 12 → 142, 10 → 98, 5 → 94 …).
`com` par `ComTblBin` : 0 à 7, dont **18** à zéro — la sortie anticipée de
`ComTblBin::read`.

Le fait le plus net : **les 9 251 éléments franchissent tous la descente en deux
sauts**, sans un seul `p_data` absent, table absente ou premier enfant vide. La
règle de descente est donc lue correctement, sur 9 251 essais.

## Recoupements arithmétiques

```
Σ elements_per_maneuver  = 3×8 + 4×28 + … + 15×36   = 9251  = maneuver_elements
Σ noeuds                 = 8 + 28 + … + 36          = 977   = maneuver_nodes
Σ coms_per_comtbl        = 18 + 2130 + … + 228      = 9251  = un ComTblBin par élément
```

Et surtout, depuis un cycle antérieur et un validateur distinct :
`ObjBin` rapportait `maneuvers_per_obj = {5: 37, 6: 132}`, soit
**37 × 5 + 132 × 6 = 977** — exactement le nombre de nœuds `ManeuverBin`
atteints ici.

## Tests

Trois cas synthétiques : descente complète, compteur déclaré dépassant la table,
descente rompue classifiée au lieu d'être silencieusement ignorée. Suite
complète : `python3 -m unittest discover -s tools/tests` → **38 tests, OK**.
Les quatre schémas revalident sur la charge utile réelle avec `exit=0`.

## Ce que ce cycle n'établit pas

- La charge utile derrière chaque pointeur `ComTblM` résolu.
- **Ce qu'une manœuvre commande** ; seulement qu'il s'agit d'une liste de paires
  `ComTblM`/`ComTbl`.
- Le contenu d'un enregistrement `ComBin`.
- Toujours aucune condition d'objectif. `retail_units_and_waves` et
  `retail_objectives` restent ouverts.

## État de la famille

| classe | enregistrement | compteur | validé |
| --- | --- | --- | --- |
| `ObjBin` | 0x20, 8 × `u32` | — | 434, 0 incohérence |
| `OrderBin` | 0x2C, union à 10 étiquettes | `u8` étiquette | 2 975, 0 incohérence |
| `ActBin` | 0x08, en-tête de liste | `u8` | 492, 0 incohérence |
| `SetBin` | 0x08, en-tête de liste | `u8` | 230, lu comme conteneur |
| `ManeuverBin` | 0x0C, deux tableaux parallèles | **`s32`** | 977, 0 incohérence |
| `ComTblBin` | 0x08, en-tête de liste | `u8` | 9 251, lu comme sous-enregistrement |

## Prochaine tranche

`ComBin::read` `0x82331E78` clôt la famille en profondeur. Ensuite,
micro-exécution P-code de `ObjBin::read`, `ActBin::read`, `OrderBin::read` et
`ManeuverBin::read` sur cette charge utile, comparée au parseur natif via
`tools/compare_ac6_function_snapshots.py` — la preuve `microexec` que réclame la
porte v2.
