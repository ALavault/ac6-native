# Cycle 838 — native render resolve manifest

Date: 2026-08-04

## Resultat

La voie geometry-driven consomme maintenant un resolve explicite de la passe
`world` vers la cible finale.

Nouveau manifeste externe :

```text
mission_id<TAB>source_pass<TAB>source_target<TAB>destination_target<TAB>mode
```

`MissionRenderResolveDatabase` fournit :

- `add`;
- `load_manifest`;
- `find(mission_id, source_pass)`.

## Guards ajoutees

Un resolve est refuse si :

- `mission_id == 0` ;
- `source_pass != world` ;
- `source_target != main_color` ;
- `destination_target != present` ;
- `mode` n'est pas `copy`, `tonemap` ou `linear` ;
- le couple `(mission_id, source_pass)` est duplique.

Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
une definition RT, une passe `world` et un resolve `world`.

## Effet render

`draw_world_geometry` verifie la coherence du resolve avec la passe active et
incorpore dans le shading :

- `source_target` ;
- `destination_target` ;
- `mode`.

Le resolve n'est donc pas une garde passive : il influence la sortie
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

- chargement d'un resolve `world/main_color -> present` ;
- rejet d'une destination inconnue ;
- rejet du rendu geometry avec RT+passe mais sans resolve ;
- hash couleur different quand le mode resolve change.

## Limites

Ce cycle ne modele pas encore les resolves Xenos reels, MSAA, copies de RT
intermediaires, ni tonemap retail. Il ferme la frontiere produit : la sortie
finale n'est plus implicite, elle est declaree et verifiee.
