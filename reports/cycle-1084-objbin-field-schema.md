# Cycle 1084 — schéma de champ de `ObjBin::read`

Date : 2026-08-07. Suite du cycle 1083.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  `reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin`,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- Dérivé de la décompilation seule. Aucun émulateur, aucun bridge, aucune
  exécution du produit natif.
- Artefacts : `analysis/scenario-schema/ObjBin.json`,
  `tools/validate_ac6_scenario_schema.py`, tests dans
  `tools/tests/test_ac6_static_tooling.py`.

## Préalable : deux feuilles que `.pdata` n'enregistre pas

Deux des trois sous-parseurs de `ObjBin` — `0x82330F98` et `0x82330F28` —
n'apparaissaient ni comme début `.pdata` ni dans aucun extent, et le décompilateur
les rendait en `func_0x82330f98(...)` sans corps. `.pdata` ne liste que les
fonctions nécessitant un déroulage ; les feuilles qui ne touchent aucun registre
non volatil et n'allouent pas de cadre en sont absentes.

Un `bl` direct est pourtant une preuve non ambiguë de début de fonction.
`scripts/CreateLeafCallTargetFunctions.java` crée une fonction à chaque cible
d'appel direct qui n'en a pas, en ignorant les appels indirects et sans jamais
découper un corps existant :

```
AC6_CALL_TARGETS distinct=7392
AC6_LEAF_FUNCTIONS already_present=6589 inside_existing_body=10 created=738 failed=55
```

## Chemin d'accès, dérivé du graphe d'appel

```
racine slot 0                       parseur 0x82309D20
  -> 0x8232CCA0   noeud par entrée : slot0 -> SetBin::read, slot1 -> 0x8232F380
     -> 0x8232F380  compteur u8 dans data ; alloue count*0x08 et count*0x20
        -> 0x8232F198  slot0 -> ObjBin::read dans l'enregistrement de 0x20, slot1 -> SetBin::read
```

`0x8232F380` alloue `count * 0x20` avant de boucler : **la taille de
l'enregistrement `ObjBin` est 0x20**, indépendamment de la lecture des champs.
Elle est ensuite confirmée champ par champ par `ObjBin::read`, qui écrit
exactement huit `u32`.

## Primitive de conteneur

Partagée par toute la famille `*Bin`. Tous les offsets sont relatifs à leur
propre base, jamais absolus.

```
noeud  : { u32 data_off ; u32 table_off }      0 = absent
table  : { s32 count ; u32 child_off[count] }  child_off relatif à la table
enfant présent  <=>  count > i  ET  ( u32[enfant] != 0  OU  u32[enfant+4] != 0 )
```

Le prédicat de présence est le test exact que `ObjBin::read` exécute avant
chaque slot.

## Enregistrement `ObjBin` — 0x20 octets, 8 × `u32`

| offset | champ | source | sous-enregistrement | taille | preuve d'identité |
| --- | --- | --- | --- | ---: | --- |
| +0x00 | `data` | `node.data_off` | — | — | `0x82010150` *ObjBin::read() / data empty!* |
| +0x04 | `param` | enfant[0] | variante étiquetée | 0x10 | lecteur `0x82330F98`, dimensionneur `0x82330F28` |
| +0x08 | `maneuvers` | enfant[1] | bloc manœuvres | 0x24 | lecteur `0x82330C58`, dimensionneur `0x82330A30` |
| +0x0C | `durable` | enfant[2] | pointeur | 0x10 | `0x8201009C` *DurableBin::read() / data empty!* |
| +0x10 | `weapon_0` | enfant[3] | pointeur | 0x10 | `0x820100C8` *WeaponBin::read() / data empty!* |
| +0x14 | `weapon_1` | enfant[4] | pointeur | 0x10 | idem |
| +0x18 | `weapon_2` | enfant[5] | pointeur | 0x10 | idem |
| +0x1C | `tail` | enfant[6] | pointeur | 0x04 | rôle inconnu |

L'identité de chaque champ vient de la chaîne d'erreur que **cette branche
précise** matérialise, pas d'une hypothèse sur l'adresse. Les trois emplacements
`weapon_*` partagent la même chaîne parce que le compilateur a inliné trois
appels à `WeaponBin::read`.

### Sous-enregistrement `param` — variante étiquetée, 0x10

`0x82330F28` retourne `0x10` pour les étiquettes 0, 1 et 2, et `0` sinon.
`0x82330F98` lit `u8 tag = *(u8*)data` et n'écrit qu'un seul mot :

| étiquette | champ écrit | preuve |
| ---: | --- | --- |
| 0 | +0x04 | — |
| 1 | +0x08 | `0x82010294` *ParamGroundBin::read() / data empty!* |
| 2 | +0x0C | — |

### Sous-enregistrement `maneuvers` — 0x24

`{ u32 data ; u32 maneuver[8] }`, chaque entrée lue par
`ManeuverBin::read 0x82331808` en sous-enregistrements de `0x0C`, dimensionnés
par `ManeuverBin::getReadBuffSize 0x823316A0`.

## Validation sur les octets réels

`tools/validate_ac6_scenario_schema.py` parcourt la charge utile avec exactement
les règles ci-dessus et vérifie les invariants que le code garantit. Il recoupe
aussi ses résultats avec les compteurs que le schéma enregistre, pour qu'un
schéma qui diverge des octets échoue au lieu de survivre en prose.

```
$ python3 tools/validate_ac6_scenario_schema.py \
    analysis/scenario-schema/ObjBin.json \
    reports/logs/.../idx_0009/000_00_00_00_10.bin
```

| observable | valeur |
| --- | ---: |
| entrées de niveau 0 (slot 0) | 230 |
| enregistrements `ObjBin` atteints | **434** |
| incohérences | **0** |
| enfants par nœud `Obj` | **7 pour les 434** |
| étiquettes du bloc paramètre | 0 → 170, 1 → 202, 2 → 16 |
| emplacements d'arme remplis | 0 → 152, 1 → 189, 2 → 93 |
| manœuvres par `Obj` | 5 → 37, 6 → 132 |

Quatre concordances non triviales :

1. **Les 434 nœuds ont exactement 7 enfants** — précisément le nombre de slots
   que le code traite (`6 < count` est le dernier test). Pas un seul écart.
2. Aucune étiquette de paramètre ne sort de `0..2`, l'intervalle exact
   qu'implémentent le lecteur et le dimensionneur.
3. Aucun enregistrement ne remplit un quatrième emplacement d'arme ; il n'en
   existe que trois.
4. Aucun `Obj` ne dépasse 8 manœuvres, la borne du bloc `0x24`.

Cinq tests synthétiques couvrent le validateur sans donnée retail : cas
conforme, trop d'enfants, étiquette hors intervalle, empreinte de charge utile
fausse, et compteur enregistré contredit. Suite complète :
`python3 -m unittest discover -s tools/tests` → **29 tests, OK**.

## Précision sur la cardinalité 230

Le cycle 1083 rapprochait les 230 enfants du slot 0 des 230 objets du census
runtime du cycle 1080. Ce cycle précise le niveau : **230 est le nombre
d'entrées de niveau 0**, et chacune contient de 0 à plusieurs enregistrements
`ObjBin`, pour un total de **434**. La coïncidence numérique porte donc sur les
entrées de niveau 0, pas sur les enregistrements `Obj`. L'appariement un à un
reste non prouvé.

## Ce que ce cycle n'établit pas

- La **signification** des charges utiles visées par chaque pointeur résolu ;
  seule la disposition conteneur/enregistrement est lue.
- La sémantique interne de `DurableBin`, `WeaponBin` et `ParamGroundBin`.
- L'identité de vague et les conditions d'objectif. `retail_units_and_waves` et
  `retail_objectives` restent **ouverts**.

## Prochaine tranche

Même méthode sur `ActBin::read` `0x82330688` et `OrderBin::read` `0x82331208` —
`OrderBin` porte à lui seul les huit variantes `Order*`, donc son schéma est
celui qui porte les conditions. Puis micro-exécution P-code de ces trois
lecteurs sur cette charge utile, comparée au parseur natif via
`tools/compare_ac6_function_snapshots.py`.
