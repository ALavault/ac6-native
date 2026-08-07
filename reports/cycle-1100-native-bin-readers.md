# Cycle 1100 — les dix lecteurs `*Bin` en code natif, vérifiés contre le retail

Date : 2026-08-08. Troisième tranche de décompilation native, et la dernière du
front « lecture » : l'image mémoire octet pour octet.

## Le point dur : contre quoi vérifier

Un port qui se compare au parseur Python ne prouve rien de plus qu'un accord
entre deux réécritures de la même idée. La référence forte existe déjà : les
**138 instantanés `*.ppc.json`** produits en exécutant les instructions retail
dans l'émulateur p-code de Ghidra (cycles 1089–1092).

Les lire depuis C++ demanderait un analyseur JSON dans le produit. À la place,
`tools/emit_ac6_reader_digests.py` les réduit à une ligne chacun :

```
class  node_offset  run_count  written_bytes  digest
```

où le digest est un FNV-1a 64 sur la sérialisation canonique des plages
écrites, `"{address:08x}:{size}:{after_hex}\n"` en ordre d'adresse. La
définition est écrite des deux côtés, et rien d'autre n'est partagé.

L'artefact `analysis/microexec/reader-digests.tsv` fait 138 lignes et couvre les
dix classes.

## Ce qui est porté

`include/ac6/retail_bin_readers.h`, `src/retail_bin_readers.cpp` : les dix
lecteurs et leurs dimensionneurs, dans l'espace d'adresses synthétique que
`MicroExecuteScenarioParser.java` utilise, avec un masque d'écriture explicite
plutôt qu'un diff contre un remplissage — un parseur peut écrire un octet égal
à n'importe quelle valeur de poison.

Les **quatre particularités** sont reproduites avec leur commentaire :

| # | où | règle |
| ---: | --- | --- |
| 1 | `ActBin::getReadBuffSize` | réserve `0x2C` par acte là où le lecteur écrit `0x08` |
| 2 | bloc de manœuvres `0x82330A30` | part de `0x60` et **écrase** par `taille0 + 0x6C` |
| 3 | dimensionneur d'ordres | déréférencerait un pointeur nul sur enfant absent — chemin mort |
| 4 | liste `0x8232EC08` | arrondit le total au multiple de 16, seul dimensionneur qui aligne |

Les deux descentes que la Mission 01 n'exerce pas — étiquette 2 d'`OrderBin`
(`0x82331D98`) et étiquette 2 de la liste `0x28` (`0x823308E0`) — **ne sont pas
devinées** : leur précondition est reproduite et le port échoue explicitement
si un nœud la satisfait, au lieu de passer en silence.

## Le résultat

```
retail_bin_readers cases=138 written_bytes=54840
```

**138 cas sur 138, du premier essai.** Nombre de plages, nombre d'octets écrits
et digest identiques à ce que la micro-exécution des instructions retail a
produit, sur les dix classes.

Un contrôle négatif est intégré : après un cas réussi, un seul mot écrit en
plus doit déplacer le digest. Sinon un accord pourrait être l'artefact d'un
digest insensible au contenu.

## L'état du chaînage de vérification

| implémentation | dérivée de | vérifiée contre |
| --- | --- | --- |
| micro-exécution p-code | les instructions retail | — |
| parseur Python | `analysis/scenario-schema/` | 138/138 `pair_equal` |
| lecteur de conteneur C++ (1098) | la primitive du cycle 1084 | 230 lignes identiques |
| lecteurs `*Bin` C++ (ce cycle) | le schéma et les cycles 1089–1092 | **138/138 digests** |

Aucun oracle nulle part : ni émulateur de console, ni bridge, ni exécution du
produit retail.

## Ce que cela n'établit pas

- Rien de sémantique, encore une fois. Ces lecteurs recopient des pointeurs
  sans les interpréter ; leur exactitude est une exactitude de disposition.
- Les deux descentes non modélisées le restent. Un autre niveau, ou une autre
  mission, peut les exercer — et le port le dira au lieu de mentir.
- Le produit ne *consomme* pas encore cette image : il lit le graphe de nœuds
  pour ses manifestes. Brancher l'image analysée sur le runtime est une autre
  tranche, qui demandera d'abord de savoir quels champs de l'image les
  consommateurs relisent.

## Suite

Deux directions, indépendantes :

1. **Descendre d'un cran dans la sémantique** — les 339 compteurs du slot 1,
   ceux que la condition d'étiquette 7 compare, n'ont pas de producteur connu.
   C'est la question qui rendrait le flux d'objectifs complet.
2. **Étendre hors de la Mission 01** — rejouer la chaîne sur une seconde
   mission testerait tout ce qui précède contre des données qui n'ont pas servi
   à le construire. Les descentes d'étiquette 2 y seraient peut-être exercées.
