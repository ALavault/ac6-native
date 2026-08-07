# Cycle 1098 — le produit natif lit le conteneur de scénario lui-même

Date : 2026-08-08. Première tranche de décompilation native après la fermeture
de J1 : sortir le pipeline de Python et faire lire au produit la structure
retail directement.

## Pourquoi

J1 a été fermée avec des manifestes TSV produits par
`tools/emit_mission01_retail_manifests.py`. Le produit natif consommait donc du
contenu retail **pré-cuit par un script**. Tant que la seule implémentation du
conteneur vit en Python, la porte repose sur un outil hors du produit.

Cette tranche écrit la seconde implémentation, en C++, dans le produit.

## Ce qui est porté

`include/ac6/retail_scenario.h`, `src/retail_scenario.cpp` :

- **la primitive de conteneur** dérivée au cycle 1084 —
  `nœud = {u32 data_off, u32 table_off}`, `table = {s32 count, u32 child[]}`,
  tout décalage relatif à sa propre base, plus la règle de présence
  (`count > i` et les deux premiers mots de l'enfant non nuls tous les deux) ;
- **les trois vues** que les consommateurs qualifiés aux cycles 1096 et 1097
  lisent : les enregistrements d'unités du slot 0 (octet de classe `+0x08`,
  octet de faction `+0x0D`, liste `Obj` et ses vecteurs), la table de factions
  du slot 5 (`+0x28`, `+0x2C`), les sous-missions du slot 2 et les étiquettes de
  leur script ;
- **la projection en manifeste**, qui doit produire exactement les lignes du
  générateur Python.

Le tout **échoue fermé** : la charge utile est une entrée non fiable, chaque
décalage est validé contre la taille du tampon, un compte négatif ne devient
pas un immense non signé, et un octet de classe hors du `switch` que
`0x820A7F48` implémente rend `nullopt` au lieu d'une catégorie par défaut.

## La preuve

`tests/retail_scenario_parser_tests.cpp` a deux moitiés.

**Synthétique, toujours exécutée.** Un conteneur de dix slots construit octet
par octet dans le test — aucune donnée retail — qui fixe la primitive : la
règle de présence, le signe du compte, les bornes, la remontée d'un octet de
classe inconnu, et la projection de ligne sur des valeurs choisies.

**Retail, exécutée quand la charge utile décodée est présente sur la machine**
(elle n'est jamais versionnée ; sinon le test sort en 77 et `ctest` le marque
`Skipped`). Elle vérifie le recensement — 230 enregistrements, 434 `Obj`,
4 factions, 4 sous-missions, 6 pas — puis exige que **les lignes émises par le
lecteur C++ soient identiques à celles des manifestes versionnés**, produits par
un générateur Python indépendant.

Résultat : **230 lignes identiques**, et 4 lignes d'objectifs identiques.

C'est un différentiel à trois voies, sans oracle :

| implémentation | dérivée de | vérifiée contre |
| --- | --- | --- |
| micro-exécution p-code | le binaire retail lui-même | — |
| parseur Python | `analysis/scenario-schema/` | 138/138 `pair_equal` |
| lecteur C++ (ce cycle) | la primitive du cycle 1084 | 230 lignes identiques |

## Ce que cela ne change pas

- Les trois placeholders déclarés (santé, santé max, rayon) restent des
  placeholders : le lecteur C++ les écrit parce que l'enregistrement analysé
  n'a rien à mettre à leur place, exactement comme le générateur Python.
- Les dix lecteurs `*Bin` eux-mêmes — ceux qui produisent l'image mémoire
  octet pour octet — ne sont **pas** portés ici. Le produit lit le graphe de
  nœuds, pas le tampon analysé. Ce port reste à faire ; sa vérité de terrain
  existe déjà (138 comparaisons).
- Rien de la sémantique n'est ajouté : ce cycle déplace une implémentation, il
  n'établit aucun fait nouveau sur le retail.

## Suite

Porter les **comportements** consommateurs, pas seulement la lecture : la
classification et l'insertion de `0x820A7070` dans une table de 256
emplacements, et le séquenceur de sous-missions de `0x8226E158` avec son index
de pas, son horodatage et sa condition à trois opérateurs. C'est là que la
décompilation devient du code natif qui *fait* ce que fait le retail.
