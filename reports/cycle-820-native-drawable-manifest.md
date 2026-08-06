# Cycle 820 - manifeste drawable natif

Ajout de `MissionDrawable`, `MissionDrawableDatabase` et
`NativeRenderTarget::draw_world_asset`.

Le format TSV strict est :
`mission_id<TAB>asset_id<TAB>primitive_count`.

Le renderer peut maintenant recevoir une base de drawables. Quand elle est
presente, chaque asset liste dans `MissionRenderDefinition` doit avoir un
drawable correspondant pour la mission courante. Une base incomplete est
rejetee avant toute soumission.

Le readback ne marque plus seulement un pixel par asset : chaque drawable
marque une couverture deterministe derivee de son `primitive_count`, bornee a
64 echantillons par asset. Le test couvre deux drawables mission 1 avec 3 et 5
primitives, soit 8 pixels couleur/depth couverts, et verifie les hashes sur une
deuxieme cible.

Limite explicite : les drawables restent des contrats declaratifs. Ils ne sont
pas encore les NDXR/NTXR retail de Mission 01 et ne ferment pas le rendu monde
terrain/sky/mapobj/F-16.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
