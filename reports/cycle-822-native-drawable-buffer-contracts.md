# Cycle 822 - contrats buffer par drawable

`MissionDrawable` exige maintenant un contrat buffer :
- `buffer_id`;
- `vertex_count`;
- `index_count`;
- `content_hash`.

Le format TSV strict devient :
`mission_id<TAB>stable_id<TAB>kind<TAB>asset_id<TAB>primitive_count<TAB>buffer_id<TAB>vertex_count<TAB>index_count<TAB>content_hash`.

Le loader rejette tout drawable sans contrat complet, et
`NativeRenderTarget::draw_world_asset` verifie aussi cette condition avant de
marquer la cible couleur/profondeur.

Les fixtures de test pointent vers les preuves deja disponibles :
- `mapobj_m01_l_brg1_n` -> `GIDX268439850`, cycle 760;
- `mapparts_m01_l_034_010` -> `021/010_NDXR`, index count 41600, hash
  `7209F3DEB7BD097D`, cycle 774;
- `entry119_022_skycloud` -> `entry119/022_FHM/005_FHM`, cycle 774.

Limite explicite : ces champs sont des contrats de qualification. Ce cycle ne
charge pas encore les bytes retail, ne parse pas NDXR/NTXR, et ne ferme pas le
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
