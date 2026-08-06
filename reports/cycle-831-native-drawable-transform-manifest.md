# Cycle 831 — native drawable transform manifest

Date: 2026-08-04

## Resultat

La voie geometry-driven consomme maintenant des transforms natives explicites
par drawable.

Nouveau manifeste externe :

```text
mission_id<TAB>stable_id<TAB>tx<TAB>ty<TAB>tz<TAB>sx<TAB>sy<TAB>sz
```

`MissionTransformDatabase` fournit :

- `add`;
- `load_manifest`;
- `find(mission_id, stable_id)`.

## Guards ajoutees

Une transform est refusee si :

- `mission_id == 0` ;
- `stable_id` est vide ;
- une translation/scale n'est pas finie ;
- un scale est inferieur ou egal a zero ;
- le couple `(mission_id, stable_id)` est duplique.

Quand `RenderAssets::geometries` est fourni, `VulkanRenderer` exige maintenant
une transform pour chaque drawable. `draw_world_geometry` verifie aussi que la
transform correspond au drawable avant projection.

## Effet render

`NativeRenderTarget::draw_world_geometry` transforme maintenant les samples
locaux en world-space :

```text
world = local * scale + translate
```

Les samples vertex et les points derives de la bounds sont marques apres cette
transformation. Ce n'est pas encore une matrice retail complete, mais la
frontiere locale -> monde est explicite, externe et testee.

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

- chargement d'un manifeste transform complet pour mapobj, terrain, sky/cloud ;
- rejet d'un transform avec scale nul ;
- rejet du rendu geometry sans base transform ;
- rejet du rendu avec base geometry incomplete meme si transforms presentes ;
- hashes reproductibles avec transforms identiques ;
- hashes differents quand les transforms changent.

## Limites

Les transforms de ce cycle sont contractuelles et externes; elles ne sont pas
encore extraites d'un manifeste retail qualifie ni prouvees contre oracle. Le
prochain travail utile est de rattacher ces transforms a des sources retail
qualifiees et/ou de remplacer le scale/translate par une matrice native plus
proche des transforms executees.
