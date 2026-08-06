# Cycle 829 — native geometry-driven target coverage

Date: 2026-08-04

## Resultat

La voie `VulkanRenderer` + `NativeGeometryDatabase` ne marque plus la cible
native via le fallback synthetique `primitive_count`.

Quand `RenderAssets::geometries` est fourni :

- le renderer exige une `NativeGeometryMetadata` pour chaque drawable ;
- le renderer exige aussi le `DecodedGeometry` associe ;
- `NativeRenderTarget::draw_world_geometry` marque couleur/depth depuis les
  samples vertex/index decodes.

Le fallback `draw_world_asset` reste disponible uniquement pour les chemins
sans base geometry.

## Guards ajoutees

`draw_world_geometry` refuse maintenant :

- target non initialisee ;
- frame non prete ;
- drawable incompatible avec la mission courante ;
- metadata non coherente avec le drawable ;
- samples vertex/index absents ;
- position non finie ;
- index sample hors `vertex_count`.

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

## Test de non-regression

Le test produit compare maintenant :

- fallback synthetique sans `geometries` : couverture attendue `15` ;
- voie geometry-driven avec `geometries` : couverture couleur/depth superieure
  au fallback et hashes differents.

Le harness `REQUIRE` affiche fichier, ligne et expression en echec pour eviter
les aborts opaques dans les cycles suivants.

## Limites

Ce cycle ne revendique toujours pas un rendu mesh retail Mission 01. La cible
native depend maintenant de samples geometry decodes, mais il manque encore le
decode complet des streams retail, les layouts vertex reels, les transforms
mesh, materiaux, textures, shaders, depth/blend et comparaison oracle frame.
