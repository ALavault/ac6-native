# Passe qualité épisodique du dépôt — 2026-08-08

> **Instantané daté, conservé tel quel.** Ce rapport décrit un run du 8 août
> 2026 et ses chiffres sont ceux de ce run, pas de HEAD : il annonce `CTest
> 26/26`, la suite en compte **27** depuis. Il est commité parce que ses
> configurations — `tools/quality/ac6-agent-semgrep.yml`,
> `mull-campaign-progression.yml`, `mull-native-repo.yml` — sont citées par sa
> section « Reproduction » et n'étaient elles-mêmes pas suivies : un rapport
> dont on ne peut pas rejouer les commandes n'est pas une mesure. Les chiffres
> se relisent comme un état à cette date, pas comme une garantie courante.

## Périmètre

Les analyses couvrent les 86 fichiers C/C++ suivis du dépôt. Les extraits de
décompilation sous `reports/code/` sont analysés comme signal, mais jamais
modifiés. Le build qualifié couvre les sources natives manuscrites, leurs
outils et leurs tests sous `reconstruction/ace-combat-6`.

## Résultats

### Build, tests et analyseur Clang

- build Clang 21 complet : 79 étapes réussies ;
- CTest : 26/26 réussis, dont un skip qualifié pour Vulkan ;
- `scan-build-21 --status-bugs` : aucun défaut ;
- `clang-tidy-21` complet avec `clang-analyzer-*`, `bugprone-*`,
  `performance-*` et `portability-*` : 519 diagnostics bruts, très dupliqués
  par les inclusions d'en-têtes. Les catégories dominantes sont la politique
  `#pragma once` (281), les accès à `optional` dans les tests (148), les
  paramètres facilement inversables (39) et les affectations dans les
  conditions (26).

Les diagnostics pertinents ont conduit à :

- séparer validation et transfert de propriété dans sept chargeurs de
  manifeste ;
- supprimer un lookup de cible de rendu mort ;
- vérifier l'`optional` du descripteur NTXR avant usage ;
- rendre explicites deux conversions dimension entière vers `float` ;
- remplacer cinq bornes `offset + taille` par des preuves sans débordement ;
- ajouter un test synthétique au bord exact et tronqué du répertoire de modèles.

La contre-passe ciblée ne conserve que deux décisions non corrigées : la taille
de l'enum `NtxrRefusal` (changement ABI injustifié) et un paramètre passé par
valeur pour permettre son déplacement dans la base.

### Formatage

Le dépôt ne possède pas de `.clang-format`. Le style LLVM implicite augmente
artificiellement la taille de certaines fonctions et faisait échouer le budget
de complexité. Le bruit mécanique a été retiré ; seules les zones modifiées ont
été alignées sur le style existant. Une configuration de format explicite doit
être qualifiée avant toute réécriture globale.

### Semgrep 1.172.0

Les règles AC6 de `tools/quality/ac6-agent-semgrep.yml` contrôlent les additions
de bornes, les conversions adresse invitée/pointeur hôte, les lectures
multi-octets dépendantes de l'hôte et les layouts liés à la largeur des
pointeurs.

- avant correction : 7 résultats, dont 5 dans le code natif ;
- après correction : 2 résultats, tous deux dans les extraits en lecture seule
  de `reports/code/` ;
- erreurs de parsing finales : 0.

Il ne reste donc aucun résultat Semgrep dans le code natif modifiable.

### PMD CPD 7.26.0

| Mode | Avant | Après | Signal long après |
|---|---:|---:|---:|
| exact, 80 jetons | 39 groupes | 35 groupes | 5 >= 150, 0 >= 240 |
| identifiants/littéraux normalisés, 120 jetons | 87 groupes | 86 groupes | 12 >= 240, 1 >= 400 |

Les parseurs lexicaux strictement identiques sont maintenant centralisés dans
`src/text_parse.h`, avec appels explicitement qualifiés `detail::parse_*` et
sans déclaration `using`. Les règles de refus propres à chaque format restent
locales. Les clones restants les plus importants sont les chargeurs de
manifestes et une ancienne copie partielle de
`tests/campaign_progression_tests.cpp` à la racine.

### Mull 0.34.0 / LLVM 21.1.8

Paquet vérifié : SHA-256
`6b942ec5727f8ab5e06d7885d130a502dd0ad41c742814e6cbfdac3658f9a080`.
Le build instrumenté complet et les 25 tests exécutables réussissent ; Vulkan
est ignoré. Durée CTest instrumentée : 143,59 s.

La campagne bornée sur 19 exécutables rapides agrège 1 916 mutants uniques :
1 275 tués, 634 survivants et 7 timeouts. Les scores source les plus faibles
sont `native_hud.cpp` (20 %), `retail_model_directory.cpp` (47 %),
`mission01_compare.cpp` (58 %) et `product_runtime.cpp` (60 %). Dix sources ne
sont pas atteintes par ces cibles rapides, notamment `ntxr_texture.cpp`,
`native_geometry_raster.cpp`, `native_renderer.cpp` et `retail_session.cpp`.

`tools/quality/mull-native-repo.yml` fournit le filtre dépôt ;
`mull-campaign-progression.yml` conserve le filtre mono-source. Mull reste une
campagne épisodique par exécutable, jamais une voie de build normale.

## Reproduction concise

```sh
semgrep scan --metrics=off --config tools/quality/ac6-agent-semgrep.yml \
  $(git ls-files '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp')

cpd --minimum-tokens 80 --language cpp --format csv_with_linecount_per_file \
  --no-fail-on-violation --file-list <liste-des-fichiers-suivis>

scan-build-21 --status-bugs cmake --build <build-clang-propre> -j2

run-clang-tidy-21 -p <build-avec-compile-commands> -j2 \
  -checks='-*,clang-analyzer-*,bugprone-*,performance-*,portability-*'
```

## Risques résiduels

- couverture mutation faible sur plusieurs frontières natives ;
- dix sources sans cible Mull rapide dédiée ;
- clone suivi mais non compilé du test de progression à la racine ;
- format global non reproductible tant qu'un `.clang-format` n'est pas adopté ;
- smoke test Vulkan non exécuté dans cet environnement.
