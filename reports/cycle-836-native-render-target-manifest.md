# Cycle 836 — native render target manifest

Date: 2026-08-04

## Resultat

La voie geometry-driven consomme maintenant une definition de render target par
mission.

Nouveau manifeste externe :

```text
mission_id<TAB>width<TAB>height<TAB>color_format<TAB>depth_format<TAB>depth_enabled
```

`MissionRenderTargetDatabase` fournit :

- `add`;
- `load_manifest`;
- `find(mission_id)`.

## Guards ajoutees

Une definition RT est refusee si :

- `mission_id == 0` ;
- largeur/hauteur nulles ;
- dimensions superieures a 4096 ;
- surface superieure a 16M pixels ;
- `color_format` n'est pas `rgba8` ou `rgba16f` ;
- `depth_format` n'est pas `none` ou `d24s8` ;
- `depth_enabled` ne correspond pas a `depth_format`;
- la mission est dupliquee.

Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
une definition RT pour la mission.

## Effet render

`NativeRenderTarget::draw_world_geometry` verifie maintenant :

- la taille effective du target contre le manifeste ;
- le format couleur du target contre le format RT du shader ;
- la compatibilite des flags material `depth_test/depth_write` avec la surface
  depth declaree.

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

- chargement d'un manifeste RT Mission 01 `64x32 rgba8 d24s8`;
- rejet d'un depth format incoherent avec `depth_enabled`;
- rejet du rendu geometry sans RT definition ;
- rejet d'une cible native aux mauvaises dimensions.

## Limites

Cette definition RT est encore contractuelle. Elle ne prouve pas les formats
Xenos reels ni les render targets intermediaires retail. Elle ferme la
frontiere produit manquante : une frame geometry-driven doit maintenant
declarer et verifier sa surface couleur/depth avant de produire des pixels.
