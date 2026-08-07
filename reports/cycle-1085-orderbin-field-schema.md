# Cycle 1085 — schéma de champ de `OrderBin::read`

Date : 2026-08-07. Même méthode que le cycle 1084, appliquée au lecteur qui
porte les huit variantes `Order*`.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- Dérivé de la décompilation seule. Aucun émulateur, aucun bridge, aucune
  exécution du produit natif.
- Artefacts : `analysis/scenario-schema/OrderBin.json`,
  `tools/validate_ac6_scenario_schema.py`, tests dans
  `tools/tests/test_ac6_static_tooling.py`.

## Taille de l'enregistrement, prouvée par l'appelant

`ActBin::read` `0x82330688` alloue `count * 0x2C` et met à zéro **onze `u32`**
(`puVar7[0..10]`) avant chaque appel à `OrderBin::read`. La taille est donc
établie indépendamment de la lecture des champs — même schéma de preuve que
`0x8232F380` pour `ObjBin`.

Les deux conteneurs intermédiaires se lisent au passage :

| conteneur | compteur | foulée | élément | chaînes d'échec |
| --- | --- | ---: | --- | --- |
| `SetBin` `0x8232F5F8` | `u8` à `data` | 0x08 | `ActBin` | `0x82010074`, `0x8201004C`, `0x82010020` *act empty! : %d* |
| `ActBin` `0x82330688` | `u8` à `data` | **0x2C** | `OrderBin` | `0x8201026C`, `0x82010244`, `0x82010218` *order empty! : %d* |

## Enregistrement `OrderBin` — 0x2C octets, 11 × `u32`, union étiquetée

Discriminant : `u8 tag = *(u8*)(node.data_off résolu)`, échec fermé sur
`0x82010438` *OrderBin::read() / data empty!*. Exactement **un** emplacement est
écrit par enregistrement ; les dix autres restent nuls.

| étiquette | offset | champ | taille tampon | variante | preuve d'identité |
| ---: | --- | --- | ---: | --- | --- |
| — | +0x00 | `data` | — | — | porte l'étiquette |
| 0 | +0x04 | `tag_0` | 4 | non nommée | *aucune chaîne d'échec* |
| 1 | +0x08 | `disappear` | 4 | **OrderDisappearBin** | `0x820102C4` |
| 2 | +0x0C | `tag_2` | 8 + imbriqué | non nommée | lecteur `0x82331AD0`, dimensionneur `0x82331A38` |
| 3 | +0x10 | `stop` | 4 | **OrderStopBin** | `0x820102F8` |
| 4 | +0x14 | `lead` | 4 | **OrderLeadBin** | `0x82010324` |
| 5 | +0x18 | `jump` | 4 | **OrderJumpBin** | `0x8201037C` |
| 6 | +0x1C | `flag` | 4 | **OrderFlagBin** | `0x82010350` |
| 7 | +0x20 | `tag_7` | 4 | non nommée | *aucune chaîne d'échec* |
| 8 | +0x24 | `property` | 4 | **OrderPropertyBin** | `0x820103A8` |
| 9 | +0x28 | `tag_9` | 4 | non nommée | *aucune chaîne d'échec* |

`getReadBuffSize` `0x823310E8` confirme le même domaine par un autre chemin :
4 octets pour les étiquettes 0, 1, 3, 4, 5, 6, 7, 8, 9 ; `8 + imbriqué` pour
l'étiquette 2 ; **0 pour toute autre valeur**.

Six variantes sur dix sont nommées par la chaîne d'échec que **leur propre
branche** matérialise. Les quatre autres (0, 2, 7, 9) écrivent un pointeur
résolu sans chemin d'échec et restent donc **sans nom** — c'est une limite du
binaire, pas de la méthode.

## Validation sur les octets réels

```
$ python3 tools/validate_ac6_scenario_schema.py \
    analysis/scenario-schema/OrderBin.json \
    reports/logs/.../idx_0009/000_00_00_00_10.bin
```

| observable | valeur |
| --- | ---: |
| nœuds `SetBin` | 230 |
| nœuds `ActBin` | 492 |
| enregistrements `OrderBin` | **2 975** |
| incohérences | **0** |
| enfants par nœud `Order` | **1 pour les 2 975** |

Répartition des étiquettes :

| étiquette | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| occurrences | 233 | 206 | **890** | 442 | **0** | 686 | 232 | 53 | 233 | **0** |

Trois concordances non triviales :

1. **Aucune étiquette ne sort de `0..9`**, l'intervalle exact qu'implémentent le
   lecteur et le dimensionneur, sur 2 975 enregistrements.
2. **Les 2 975 nœuds portent exactement un enfant**, le seul que le lecteur
   cherche.
3. Les étiquettes **4 (`OrderLeadBin`) et 9 ne sont jamais employées** par la
   Mission 01 ; les huit autres le sont.

## Un résultat négatif, vérifié à la main

Les **890** enregistrements d'étiquette 2 s'arrêtent tous avant la liste
imbriquée. `0x82331AD0` exige que son nœud ait une donnée, une table et un
premier enfant *présent* ; ici la table contient exactement une entrée et cette
entrée est un nœud vide `{0, 0}`. Le lecteur écrit donc son en-tête de 8 octets
et s'arrête, sur son propre test de vacuité.

Vérifié à la main sur trois enregistrements, aux offsets `0xDD0`, `0x7B50` et
`0x7CA0` : `child[0]` présent avec `{0x10, 0x60}`, table à une entrée, entrée
`{0, 0}`.

**Le compteur d'éléments de la liste imbriquée n'est donc jamais exercé par
cette charge utile et reste non qualifié.** Une première lecture le plaçait à
`data+0x0C` d'après `0x82331D58` ; appliquée à la charge utile elle rend des
motifs de flottants (`0x442F0000` = 700.0, `0x44C80000` = 1600.0), ce qui
signifie que le nœud atteint n'était pas le bon. La descente est désormais
classifiée par la précondition que le lecteur impose, au lieu d'affirmer une
disposition non exercée.

## Tests

Trois cas synthétiques couvrent la marche `Set → Act → Order` sans donnée
retail : étiquette dans l'intervalle, étiquette hors intervalle, répartition
enregistrée contredite. Suite complète :
`python3 -m unittest discover -s tools/tests` → **32 tests, OK**.
Les deux schémas revalident sur la charge utile réelle avec `exit=0`.

## Ce que ce cycle n'établit pas

- La charge utile derrière chaque pointeur de variante résolu ; seules la
  disposition conteneur et l'union étiquetée sont lues.
- Le rôle des quatre étiquettes sans nom (0, 2, 7, 9).
- La signification de la liste imbriquée de l'étiquette 2.
- **Aucune condition d'objectif.** `retail_units_and_waves` et
  `retail_objectives` restent ouverts. Savoir qu'un ordre est un
  `OrderStopBin` ne dit pas ce qu'il arrête.

## Prochaine tranche

`ActBin::read` est déjà lu ici comme conteneur ; son propre enregistrement
reste à formaliser. Ensuite, micro-exécution P-code de `ObjBin::read`,
`ActBin::read` et `OrderBin::read` sur cette charge utile, comparée au parseur
natif via `tools/compare_ac6_function_snapshots.py` — la preuve `microexec` que
la porte v2 réclame.
