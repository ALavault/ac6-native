# Cycle 1086 — schéma de champ de `ActBin::read`

Date : 2026-08-07. Troisième application de la méthode des cycles 1084 et 1085.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- Dérivé de la décompilation seule. Aucun émulateur, aucun bridge, aucune
  exécution du produit natif.
- Artefacts : `analysis/scenario-schema/ActBin.json`,
  `tools/validate_ac6_scenario_schema.py`, tests dans
  `tools/tests/test_ac6_static_tooling.py`.

## Identité nommée deux fois

`ActBin` est la première classe de la famille dont **les deux fonctions** se
nomment indépendamment, chacune avec son propre triplet de chaînes d'échec :

| fonction | data | child | par élément |
| --- | --- | --- | --- |
| `read` `0x82330688` | `0x8201026C` | `0x82010244` | `0x82010218` *order empty! : %d* |
| `getReadBuffSize` `0x82330540` | `0x820101E4` | `0x820101B0` | `0x82010178` *order empty! : %d* |

Le conteneur au-dessus se lit de la même façon :

| fonction | data | child | par élément |
| --- | --- | --- | --- |
| `SetBin::read` `0x8232F5F8` | `0x82010074` | `0x8201004C` | `0x82010020` *act empty! : %d* |
| `SetBin::getReadBuffSize` `0x8232F4D0` | `0x8200FFEC` | `0x8200FFB8` | `0x8200FF80` *act empty! : %d* |

## Enregistrement `ActBin` — 0x08 octets, 2 × `u32`

`SetBin::read` alloue `count * 0x08` et met à zéro **deux `u32`** avant chaque
appel à `ActBin::read` : la taille est établie par l'appelant, indépendamment de
la lecture des champs.

| offset | champ | source | note |
| --- | --- | --- | --- |
| +0x00 | `data` | `node.data_off` résolu | son premier octet est le compteur `u8` d'ordres ; échec fermé `0x8201026C` |
| +0x04 | `orders` | curseur de tampon | tableau de `count` enregistrements `OrderBin` de `0x2C` ; écrit seulement si `count != 0` |

**`ActBin` est un pur en-tête de liste.** Il ne porte aucune variante et aucun
scalaire propre au-delà de l'octet de compteur. `SetBin` a exactement la même
forme, un niveau au-dessus.

Comptabilité du tampon, cohérente entre les deux fonctions :

- `read` réserve `count * 0x2C` puis avance de `OrderBin::getReadBuffSize` par
  élément ;
- `getReadBuffSize` retourne `0` si `count == 0`, sinon `count * 0x2C` plus la
  somme des `OrderBin::getReadBuffSize` ;
- `getReadBuffSize` met à zéro **onze `u32`** de brouillon par élément — le même
  `0x2C` que `read` alloue. Troisième confirmation indépendante de la taille de
  l'enregistrement `OrderBin`.

## Validation sur les octets réels

| observable | valeur |
| --- | ---: |
| nœuds `SetBin` | 230 |
| nœuds `ActBin` | 492 |
| actes déclarant zéro ordre | **0** |
| dépassements de compteur `Set` | **0** |
| dépassements de compteur `Act` | **0** |
| incohérences | **0** |

| actes par `Set` | 1 | 2 | 3 | 4 |
| --- | ---: | ---: | ---: | ---: |
| occurrences | 75 | 81 | 41 | 33 |

Ordres par acte : de **2 à 52**, avec une longue traîne
(209 actes en portent 2, puis 92 en portent 3, jusqu'à 4 actes de 52).

Aucun `Set` ni `Act` ne déclare plus d'éléments que sa table n'en contient : les
chemins `act empty! : %d` et `order empty! : %d` ne se déclenchent jamais sur
cette charge utile. Aucun acte ne déclare zéro ordre, donc la sortie anticipée
de `read` n'est jamais empruntée ici.

## Recoupements arithmétiques

Trois égalités que rien n'imposait, entre deux validateurs écrits séparément :

```
Σ acts_per_set    = 1×75 + 2×81 + 3×41 + 4×33          = 492  = act_nodes
Σ orders_per_act  = 2×209 + 3×92 + … + 52×4            = 2975 = enregistrements OrderBin du cycle 1085
Σ sets            = 75 + 81 + 41 + 33                  = 230  = set_nodes
```

Le total **2 975** est exactement celui que rapporte le validateur `OrderBin`,
qui parcourt l'arbre par un autre chemin de code. Les deux marches concordent au
niveau de l'unité.

## Tests

Trois cas synthétiques : compteur déclaré égal à la table, compteur déclaré
dépassant la table, acte déclarant zéro ordre. Suite complète :
`python3 -m unittest discover -s tools/tests` → **35 tests, OK**.
Les trois schémas revalident sur la charge utile réelle avec `exit=0`.

## Ce que ce cycle n'établit pas

- La signification des octets qui suivent le compteur dans le mot `data`.
- **Ce qu'un `Act` regroupe sémantiquement** ; seulement qu'il s'agit d'une liste
  ordonnée d'ordres.
- Toujours aucune condition d'objectif. `retail_units_and_waves` et
  `retail_objectives` restent ouverts.

## État de la famille

| classe | enregistrement | validé | reste |
| --- | --- | --- | --- |
| `ObjBin` | 0x20, 8 × `u32` | 434 enregistrements, 0 incohérence | charges utiles des pointeurs |
| `OrderBin` | 0x2C, union à 10 étiquettes | 2 975, 0 incohérence | 4 étiquettes sans nom |
| `ActBin` | 0x08, en-tête de liste | 492, 0 incohérence | sémantique du regroupement |
| `SetBin` | 0x08, en-tête de liste | 230, lu comme conteneur | schéma propre à formaliser |

## Prochaine tranche

`ManeuverBin` `0x82331808` et `ComTblBin` `0x82331C10`, atteints depuis
`ObjBin` par le bloc de manœuvres. Puis micro-exécution P-code de
`ObjBin::read`, `ActBin::read` et `OrderBin::read` sur cette charge utile,
comparée au parseur natif via `tools/compare_ac6_function_snapshots.py` — la
preuve `microexec` que réclame la porte v2.
