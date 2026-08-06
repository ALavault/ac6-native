# Cycle 837 — native render pass manifest

Date: 2026-08-04

## Resultat

La voie geometry-driven consomme maintenant une passe render explicite par
mission.

Nouveau manifeste externe :

```text
mission_id<TAB>pass_id<TAB>order<TAB>color_target<TAB>depth_target<TAB>clear_color<TAB>clear_depth
```

`MissionRenderPassDatabase` fournit :

- `add`;
- `load_manifest`;
- `find(mission_id, pass_id)`.

## Guards ajoutees

Une passe est refusee si :

- `mission_id == 0` ;
- `pass_id` est vide ;
- `order == 0` ;
- `color_target` n'est pas `main_color` ;
- `depth_target` n'est pas `main_depth` ou `none` ;
- `clear_depth` n'est pas fini ou sort de `[0,1]` ;
- le couple `(mission_id, pass_id)` est duplique.

Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
une passe `world`. `draw_world_geometry` verifie aussi que cette passe cible
`main_depth` quand la RT mission active depth, ou `none` sinon.

## Effet render

`draw_world_geometry` incorpore maintenant dans le shading :

- `pass_id` ;
- `order` ;
- `color_target` ;
- `depth_target` ;
- `clear_color`.

La passe render n'est donc pas seulement une garde; elle influence la sortie
deterministe.

## Validation

Commandes executees :

```text
cmake --build reconstruction/ace-combat-6/build -j2
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
```

Resultat : `1/1` test passe.

Audits :

```text
strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'
ldd reconstruction/ace-combat-6/build/ac6-native
rg -n 'mission_id_ == 1|mission_id == 1|assets\.has\(9\)|assets\.has\(119\)' reconstruction/ace-combat-6/include reconstruction/ace-combat-6/src
```

Resultat : aucun marqueur Xbox/oracle/PPC liste, dependances Linux standard,
aucun branchement produit Mission 01 ou asset 9/119 hardcode dans `include` ou
`src`.

## Tests ajoutes

- chargement d'une passe `world` Mission 01 ;
- rejet d'un `clear_depth` hors bornes ;
- rejet du rendu geometry avec RT mais sans passe ;
- hash couleur different quand l'ordre de passe change.

## Limites

Ce cycle ne modele pas encore les multiples RT intermediaires retail ni leurs
resolves. Il ferme la frontiere produit qui manquait : la passe monde est
declaree, validee et consommee par le rendu natif au lieu d'etre implicite.
