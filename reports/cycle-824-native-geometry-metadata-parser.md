# Cycle 824 - parser metadata geometrique native

Ajout de `NativeGeometryMetadata` et `NativeGeometryDatabase`.

Le parser lit uniquement des buffers externes deja verifies par
`QualifiedBufferDatabase`. Le format borne de cette etape est :
`AC6GEO1<TAB>vertex_count<TAB>index_count<TAB>primitive_count`.

Le chargement exige que les trois compteurs correspondent au
`MissionDrawable` associe. `NativeRenderTarget::draw_world_geometry` reverifie
la coherence avant de marquer la cible couleur/profondeur. Le renderer peut
recevoir une base `NativeGeometryDatabase`; si elle est presente, tout drawable
soumis doit avoir sa metadata geometrique chargee.

Le test couvre :
- trois buffers externes verifies par taille/FNV64;
- chargement metadata pour mapobj, terrain et sky/cloud;
- rejet renderer avec base geometrique incomplete;
- soumission positive et readback deterministe quand tous les buffers et
  metadata sont presents.

Limite explicite : `AC6GEO1` est un format de slice borne pour fermer la
frontiere produit. Ce cycle ne parse pas encore les structures retail NDXR/NTXR
et ne ferme pas le rendu monde Mission 01.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
