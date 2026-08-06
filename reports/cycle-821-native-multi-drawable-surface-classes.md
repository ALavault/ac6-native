# Cycle 821 - classes de surfaces multi-drawable natives

`MissionDrawable` porte maintenant :
- `mission_id`;
- `stable_id`;
- `kind`;
- `asset`;
- `primitive_count`.

Le format TSV strict devient :
`mission_id<TAB>stable_id<TAB>kind<TAB>asset_id<TAB>primitive_count`.

`MissionDrawableDatabase` accepte plusieurs drawables pour un meme asset. Le
renderer exige au moins un drawable pour chaque asset liste dans
`MissionRenderDefinition`, puis soumet tous les drawables de cet asset dans la
cible couleur/profondeur.

Le test couvre une forme representative de Mission 01 :
- `mapobj_m01_l_brg1_n`, kind `mapobj`, asset 9;
- `mapparts_m01_l_034_010`, kind `terrain`, asset 119;
- `entry119_022_skycloud`, kind `sky_cloud`, asset 119.

La couverture readback attendue est 15 primitives declarees. La base incomplete
avec seulement le mapobj est rejetee avant soumission.

Limite explicite : les noms et classes suivent les preuves cycles 760/774, mais
ce cycle ne charge pas encore les buffers NDXR/NTXR retail et ne ferme pas le
rendu monde Mission 01.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
