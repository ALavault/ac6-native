# Cycle 835 — native shader permutation manifest

Date: 2026-08-04

## Resultat

La voie geometry-driven consomme maintenant un catalogue de permutations shader.

Nouveau manifeste externe :

```text
shader_permutation<TAB>vertex_layout<TAB>texture_fetches<TAB>constant_count<TAB>render_target_format
```

`ShaderPermutationDatabase` fournit :

- `add`;
- `load_manifest`;
- `find(shader_permutation)`.

## Guards ajoutees

Une permutation est refusee si :

- `shader_permutation` est vide ;
- `vertex_layout` est vide ;
- `texture_fetches == 0` ;
- `constant_count == 0` ;
- `render_target_format` n'est pas `rgba8`, `rgba16f` ou `d24s8` ;
- l'ID de permutation est duplique.

Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
que chaque `MissionMaterial::shader_permutation` resolve dans ce catalogue.

## Effet render

`NativeRenderTarget::draw_world_geometry` incorpore maintenant dans le shading :

- l'ID de permutation ;
- le vertex layout ;
- le nombre de texture fetches ;
- le nombre de constantes ;
- le format render target.

La permutation shader n'est donc plus une simple chaîne ignoree par le rendu
natif.

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

- chargement d'un manifeste shader complet pour mapobj, terrain, sky/cloud ;
- rejet d'un format RT inconnu ;
- rejet du rendu geometry avec textures mais sans shaders ;
- hashes reproductibles avec shaders identiques ;
- hash couleur different quand layouts/fetches/constants changent.

## Limites

Ce catalogue ne decode pas encore les microcodes Xenos ni les vrais layouts de
fetch. Il ferme le contrat produit qui manquait : une permutation shader doit
exister, etre validee et influer sur la voie render native avant tout rendu
geometry-driven.
