# Cycle 1088 — la famille `*Bin` est close

Date : 2026-08-07. Dernière tranche des cycles 1084 à 1087 : les six classes
restantes, et le passage d'un validateur par classe à un validateur piloté par le
schéma.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- Décompilation seule. Aucun émulateur, aucun bridge, aucune exécution du
  produit natif.

## Les six classes restantes

### `SubMisTblBin` — racine slot 2

Compteur `u8`, foulée `0x10`, éléments `SubMisBin`. Six chaînes d'identité,
trois pour `read` et trois pour `getReadBuffSize`.

### `SubMisBin` — 0x0C, 3 × `u32`

| offset | champ | contenu |
| --- | --- | --- |
| +0x00 | `data` | peut légitimement être absent — pas de chemin d'échec |
| +0x04 | `mapmask` | enfant[0], enregistrement pointeur `0x10`, échec fermé `0x8200FE38` **`MapmaskBin`** |
| +0x08 | `block` | enfant[1], enregistrement `0x08`, lecteur `0x8232ED10`, liste d'éléments de `0x28` |

`MapmaskBin` **n'a pas de fonction propre** : inliné ici, sa seule trace est sa
chaîne d'erreur. La liste d'éléments de `0x28` derrière `0x8232ED10` n'a **aucune
chaîne** dans tout son sous-arbre ; son rôle reste inconnu.

### `SetBin` — 0x08, en-tête de liste

Déjà lu comme conteneur au cycle 1086 ; son schéma propre est maintenant écrit.

> **Écart relevé dans le binaire.** `SetBin::read` écrit `0x08` par acte, mais
> `SetBin::getReadBuffSize` réserve **`0x2C`** par acte. `0x2C` est la foulée
> d'un enregistrement `ActBin`… c'est-à-dire la constante de
> `ActBin::getReadBuffSize`, jamais rétrécie après copie. L'écart est un
> sur-provisionnement de `0x24` par acte : jamais un manque, donc sans effet
> observable à l'exécution. Consigné tel quel, sans le corriger.

### `RadioTblBin` — racine slot 3

| offset | champ | contenu |
| --- | --- | --- |
| +0x00 | `data` | son premier **demi-mot** est le compteur ; échec fermé `0x8200F574` |
| +0x04 | `entries` | `count` × `0x10` |

**Compteur `u16`** — troisième largeur de la famille. Chaque entrée est lue par
le petit branchement feuille `0x8232C7B0`, qui écrit **un seul `u32`** (le
pointeur résolu, ou 0) et rien d'autre ; son dimensionneur `0x822663A8` est un
`return 0`. Une entrée radio n'ajoute donc aucun sous-tampon.

### `ComTblBin` — 0x08, compteur `u8`, éléments `ComBin` de `0x04`

`getReadBuffSize` retourne `count << 2`, cohérent avec la foulée.

### `ComBin` — 0x04, la feuille

`read` `0x82331E78` résout un pointeur et échoue en fermé sur `0x820106BC`.
Pas de table enfant, pas de compteur, pas de dimensionneur. C'est le fond de
l'arbre.

## Un validateur piloté par le schéma

Quatre classes ne diffèrent que par leur position et la largeur de leur
compteur. Plutôt que de réécrire la même marche, le schéma porte désormais un
chemin `reach` et un bloc `list_header`, et le validateur les exécute :

```json
"list_header": {"count_type": "u16", "count_offset": 0, "element_stride": 16},
"reach": [{"op": "root"}, {"op": "child", "index": 3}]
```

Les trois largeurs de compteur de la famille sont ainsi lues depuis le schéma :

| largeur | classes |
| --- | --- |
| `u8` | `SetBin`, `ActBin`, `SubMisTblBin`, `ComTblBin` |
| `u16` | `RadioTblBin` |
| `s32` | `ManeuverBin` |

## Validation sur les octets réels

| classe | résultat |
| --- | --- |
| `SubMisTblBin` | compteur `u8` = **4**, table = 4, 4 présents, **2 enfants chacun** — le `mapmask` et le bloc que `SubMisBin` lit |
| `RadioTblBin` | compteur `u16` = **54**, table = 54, 54 présents, **0 enfant chacun** — cohérent avec un lecteur d'un mot et un dimensionneur nul |

Zéro incohérence, zéro dépassement de table dans les deux cas.

Les quatre classes restantes ne sont pas laissées en erreur générique : leur
schéma déclare `validated_transitively_by`, et l'outil le dit explicitement avec
un code de sortie 3, distinct de l'échec (1) et du non-supporté (2).

```
SubMisBin  est validé transitivement par SubMisTblBin
SetBin     est validé transitivement par ActBin
ComTblBin  est validé transitivement par ManeuverBin
ComBin     est validé transitivement par ManeuverBin
```

## Tests

Cinq cas synthétiques couvrent le validateur générique : compteur `u8`, `u16`,
`s32`, compteur dépassant la table, et **lecture avec la mauvaise largeur de
compteur** — un `u16` valant 5 lu comme `u8` donne 0, et le test exige que
l'écart soit visible. Suite complète :
`python3 -m unittest discover -s tools/tests` → **43 tests, OK**.

## La famille, close

| classe | enregistrement | compteur | validation |
| --- | --- | --- | --- |
| `ObjBin` | 0x20, 8 × `u32` | — | 434 enregistrements |
| `OrderBin` | 0x2C, union à 10 étiquettes | `u8` étiquette | 2 975 |
| `ActBin` | 0x08, en-tête | `u8` | 492 |
| `SetBin` | 0x08, en-tête | `u8` | 230, transitif |
| `ManeuverBin` | 0x0C, deux tableaux | `s32` | 977 / 9 251 |
| `ComTblBin` | 0x08, en-tête | `u8` | 9 251, transitif |
| `ComBin` | 0x04, feuille | — | transitif |
| `SubMisTblBin` | 0x08, en-tête | `u8` | 4 |
| `SubMisBin` | 0x0C | — | 4, transitif |
| `RadioTblBin` | 0x08, en-tête | `u16` | 54 |

Classes **inlinées**, sans fonction propre, identifiées uniquement par la chaîne
d'erreur qu'elles matérialisent : `DurableBin`, `WeaponBin`, `ParamGroundBin`
(dans `ObjBin::read`), les six `Order*Bin` (dans `OrderBin::read`), `ComTblMBin`
(dans `ManeuverBin::read`), `MapmaskBin` (dans `SubMisBin::read`).

**Toutes les classes de la famille énumérée au cycle 1082 sont désormais
couvertes**, soit par un schéma propre, soit comme variante inlinée nommée.

## Ce que la famille n'établit toujours pas

Les dix schémas décrivent des **conteneurs et des dispositions
d'enregistrements**. Aucun ne dit ce que la charge utile derrière un pointeur
résolu signifie. En particulier :

- aucune identité de vague ;
- aucune condition d'objectif ;
- aucune jointure entre une entrée `RadioTblBin` et une clé radio ou un
  sous-titre ;
- le rôle des étiquettes `OrderBin` 0, 2, 7 et 9, et celui de la liste de `0x28`
  de `SubMisBin`, restent inconnus faute de chaîne d'erreur.

`retail_units_and_waves` et `retail_objectives` restent **ouverts** dans le
contrat v2. C'est la limite de la méthode « nommer par les chaînes d'erreur » :
elle donne la structure et l'identité des classes, pas la sémantique des
champs.

## Prochaine tranche

Micro-exécution P-code des lecteurs sur cette charge utile, comparée à un
parseur natif via `tools/compare_ac6_function_snapshots.py`. C'est la preuve
`microexec` que réclame la porte v2, et le premier pas qui dépasse la structure
pour toucher au comportement.
