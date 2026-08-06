# Cycle 833 — native material pipeline manifest

Date: 2026-08-04

## Resultat

La voie geometry-driven consomme maintenant un materiau/pipeline explicite par
drawable.

Nouveau manifeste externe :

```text
mission_id<TAB>stable_id<TAB>shader_permutation<TAB>depth_test<TAB>depth_write<TAB>blend_mode<TAB>base_color
```

`MissionMaterialDatabase` fournit :

- `add`;
- `load_manifest`;
- `find(mission_id, stable_id)`.

## Guards ajoutees

Un material est refuse si :

- `mission_id == 0` ;
- `stable_id` ou `shader_permutation` est vide ;
- `depth_test` ou `depth_write` n'est pas `0`/`1` ;
- `blend_mode` n'est pas `opaque`, `alpha` ou `additive` ;
- `base_color` a un alpha nul ;
- le couple `(mission_id, stable_id)` est duplique.

Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
geometry, transform et material pour chaque drawable.

## Effet render

`NativeRenderTarget::draw_world_geometry` utilise maintenant le material pour :

- depth test ;
- depth write ;
- blend opaque ;
- blend alpha ;
- blend additive ;
- shading depuis `base_color`.

La couleur n'est plus directement codee depuis `asset_id` dans la voie
geometry-driven.

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

- chargement d'un manifeste material complet pour mapobj, terrain, sky/cloud ;
- rejet d'un blend mode inconnu ;
- rejet du rendu geometry sans base material ;
- hashes reproductibles avec materials identiques ;
- hashes differents quand les transforms changent ;
- hash couleur different quand les materials changent.

## Limites

Le champ `shader_permutation` est encore un identifiant contractuel, pas un
decode Xenos complet. Les prochains contrats utiles sont les layouts de shader,
textures/samplers, formats render target et depth/blend derives d'une preuve
retail qualifiee.
