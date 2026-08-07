# Cycle 1089 — micro-exécution P-code des parseurs de scénario

Date : 2026-08-08.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Aucun émulateur de console, aucun bridge, aucune exécution du produit
  natif.** Une fonction, un tas synthétique, quelques milliers de pas.
- Artefacts : `scripts/MicroExecuteScenarioParser.java`,
  `tools/emit_ac6_native_snapshot.py`, `analysis/microexec/`.

## Pourquoi pas le P-code exporté

`exports/*.json` contient du **high P-code** : forme SSA, nœuds `MULTIEQUAL`.
Il n'est pas exécutable linéairement. `EmulatorHelper` exécute le P-code **brut**
de chaque instruction, ce qui est à la fois plus simple et plus fidèle — ce sont
les sémantiques d'instruction réelles, pas une IR normalisée.

## Le montage

Mémoire choisie hors de l'image du programme, pour qu'une écriture égarée soit
visible plutôt que silencieuse :

```
payload  0xB0000000   le graphe de nœuds décodé, chargé verbatim
record   0xB4000000   l'enregistrement destination, pré-rempli de 0xCD
buffer   0xB5000000   le tampon de sous-enregistrements, pré-rempli de 0xCD
stack    0xC0001000
LR       0x00DEAD00   sentinelle d'arrêt
```

Le pré-remplissage par poison fait que « ce que la fonction a écrit » se lit
directement : toute plage non-`0xCD` après le retour est une écriture réelle.

Les appels au printeur d'erreur `0x823828B8` sont **interceptés et enregistrés**
plutôt qu'exécutés — quel chemin d'échec fermé se déclenche est précisément ce
que l'instantané doit capter. Les appels aux autres parseurs s'exécutent
réellement.

## L'autre côté de la comparaison

`tools/emit_ac6_native_snapshot.py` réimplémente les mêmes parseurs **depuis
`analysis/scenario-schema/`, pas depuis le code machine**, et écrit dans le même
espace d'adressage synthétique. Si les deux coïncident octet pour octet, le
schéma ne se contente pas d'être cohérent avec la charge utile : il reproduit le
comportement exact du parseur retail.

Les champs comparés ont été rendus comparables des deux côtés :

- `memory_writes` — les plages écrites, en ordre d'adresse ;
- `calls` — **uniquement** les appels au printeur d'erreur, qui sont un
  observable sémantique ; l'entrée dans un sous-parseur est un détail
  d'implémentation du code machine et part en provenance, non comparée ;
- `registers` — vide. Pour ces lecteurs `void`, `r3` au retour est un registre
  de travail écrasé par le dernier sous-appel (`0xB4000000`, `0xB5000000`,
  `0x00000008`, `0x00000448` selon le cas) ; le comparer reviendrait à comparer
  de l'allocation de registres. La valeur brute reste en provenance.

`tools/compare_ac6_function_snapshots.py` exigeait trois voies. Plutôt que de
fabriquer un troisième instantané, un mode **deux voies explicite** a été
ajouté : `mode: pair`, vocabulaire de classification propre
(`pair_equal` / `pair_diverges`), et une mention en clair dans la politique du
rapport qu'aucun instantané généré n'est présent. Il ne peut pas être confondu
avec un succès à trois voies.

## Ce que la largeur a révélé

Les quatre premiers cas — un nœud par classe — sont passés du premier coup.
C'était une anecdote. En passant à **24 nœuds `ObjBin`** répartis dans la charge
utile, **9 sur 24 ont divergé**, toujours d'une dérive de curseur de `0x60`.

La cause est une troisième asymétrie lecteur/dimensionneur, dans
`0x82330A30`, le dimensionneur du bloc de manœuvres :

```
lVar2 = 0x60;                                   // huit enregistrements de 0x0C
if (slot0 présent)  lVar2 = taille0 + 0x6C;     // ÉCRASE la base, n'ajoute pas
if (slot1 présent)  lVar2 += taille1 + 0x0C;
...                                              // slots 2..7 idem
```

Le lecteur `0x82330C58` n'avance, lui, que de `0x0C` par slot présent. Les deux
diffèrent donc de **exactement `0x60`** dès que le slot 0 est présent — et c'est
le **dimensionneur** que suit `ObjBin::read` pour avancer son curseur.

Après avoir reproduit cette règle exactement : **24/24**.

C'est le troisième cas d'asymétrie relevé dans cette famille, après
`SetBin::getReadBuffSize` qui réserve `0x2C` là où `read` écrit `0x08`
(cycle 1088). Aucun n'est un défaut observable — tous vont dans le sens du
sur-provisionnement — mais tous changent la disposition du tampon, donc aucun
n'est ignorable.

## Résultats

| cas | nœuds | résultat |
| --- | ---: | --- |
| `ComBin::read` `0x82331E78` | 1 | `pair_equal` |
| `ComTblBin::read` `0x82331C10` | 1 | `pair_equal` |
| `ManeuverBin::read` `0x82331808` | 1 + **24** | `pair_equal` |
| `ObjBin::read` `0x82330158` | 1 + **24** | `pair_equal` |
| **total** | **52** | **52 `pair_equal`, 0 divergence** |

Détail des quatre cas de référence :

| classe | pas | entrées sous-parseur | appels d'erreur | record | buffer |
| --- | ---: | ---: | ---: | ---: | ---: |
| `ComBin` | 9 | 0 | **0** | 4 o | 0 o |
| `ComTblBin` | 95 | 10 | **0** | 8 o | 4 o |
| `ManeuverBin` | 2 120 | 137 | **0** | 12 o | 204 o |
| `ObjBin` | 14 972 | 823 | **0** | 12 o | 1 056 o |

**Aucun chemin d'échec fermé ne se déclenche** sur ces 52 exécutions. Le retail
parse ses propres données sans jamais toucher une de ses 30 chaînes d'erreur —
ce que le parseur natif reproduit, `calls: []` des deux côtés.

Les tailles confirment les schémas indépendamment : `ComBin` 4 octets,
`ComTblBin` 8, `ManeuverBin` 12, et pour `ObjBin` les 12 octets écrits
correspondent aux trois champs dont l'enfant est présent sur ce nœud — la
conditionnalité des huit champs, telle que le schéma la décrit.

Le tampon de `ManeuverBin` vaut 204 = `9 × 12` (les deux tableaux parallèles)
+ 96 (les sous-tampons `ComTblBin`), exactement l'arithmétique du schéma.

## Tests

Quatre cas de régression synthétiques gardent la règle `0x60`/`0x6C` que seule
la largeur avait révélée : bloc vide, slot 0 seul, slot postérieur seul, et
l'écart de `0x60` lui-même. Suite complète :
`python3 -m unittest discover -s tools/tests` → **47 tests, OK**.

## Portée

Ce cycle **établit** que les schémas `ObjBin`, `ManeuverBin`, `ComTblBin` et
`ComBin` reproduisent le comportement du parseur retail octet pour octet sur
52 nœuds réels de la Mission 01, sans aucun oracle.

Il **n'établit pas** :

- la sémantique des charges utiles derrière les pointeurs résolus — le parseur
  les recopie sans les interpréter, et la comparaison ne dit rien de plus ;
- quoi que ce soit sur `OrderBin`, `ActBin`, `SetBin`, `SubMisTblBin`,
  `SubMisBin` ou `RadioTblBin` : leurs schémas restent validés
  structurellement, pas micro-exécutés ;
- une identité de vague ou une condition d'objectif.
  `retail_units_and_waves` et `retail_objectives` restent **ouverts**.

## Prochaine tranche

Étendre la micro-exécution à `OrderBin::read` — l'union à dix étiquettes est le
lecteur dont le comportement varie le plus, donc celui où une comparaison large
a le plus de valeur — puis à `ActBin` et `SetBin`. Ensuite seulement, la
génération des manifestes natifs et la porte J1.
