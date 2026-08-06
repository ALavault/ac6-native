# Cycle 834 — native texture sampler manifest

Date: 2026-08-04

## Resultat

La voie geometry-driven consomme maintenant une texture/sampler explicite par
drawable.

Nouveau manifeste externe :

```text
mission_id<TAB>stable_id<TAB>texture_id<TAB>sampler_filter<TAB>sampler_address<TAB>content_hash
```

`MissionTextureDatabase` fournit :

- `add`;
- `load_manifest`;
- `find(mission_id, stable_id)`.

## Guards ajoutees

Une texture est refusee si :

- `mission_id == 0` ;
- `stable_id` ou `texture_id` est vide ;
- `sampler_filter` n'est pas `nearest` ou `linear` ;
- `sampler_address` n'est pas `wrap` ou `clamp` ;
- `content_hash == 0` ;
- le couple `(mission_id, stable_id)` est duplique.

Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
geometry, transform, material et texture pour chaque drawable.

## Effet render

`NativeRenderTarget::draw_world_geometry` incorpore maintenant :

- le hash texture ;
- le filtre sampler ;
- le mode address ;

dans le sel de shading. Les textures restent des contrats externes; aucun byte
retail n'est embarque.

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

- chargement d'un manifeste texture complet pour mapobj, terrain, sky/cloud ;
- rejet d'un sampler filter inconnu ;
- rejet du rendu geometry avec materials mais sans textures ;
- hashes reproductibles avec textures identiques ;
- hashes differents quand les transforms changent ;
- hash couleur different quand les materials changent ;
- hash couleur different quand les textures/samplers changent.

## Limites

Le contenu texture est encore un hash contractuel, pas un decode NTXR ni un
sampler Vulkan/Xenos complet. Les prochains contrats utiles sont les formats
texture, dimensions, mip levels, samplers reels et l'association shader-layout
qualifiee depuis les preuves retail.
