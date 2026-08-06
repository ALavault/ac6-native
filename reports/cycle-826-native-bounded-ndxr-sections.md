# Cycle 826 - sections NDXR bornees

Le parser NDXR borne lit maintenant trois sections obligatoires apres le
header :
- `VTX<TAB>vertex_count<TAB>stride`;
- `IDX<TAB>index_count<TAB>index_size`;
- `POLY<TAB>primitive_count<TAB>flags`.

Les sections sont strictes : aucune section inconnue, dupliquee, manquante ou
incoherente n'est acceptee. Les counts doivent correspondre au header NDXR et
au contrat `MissionDrawable`. `NativeGeometryMetadata` expose
`vertex_section_count`, `index_section_count` et `polygon_descriptor_count`;
`NativeRenderTarget::draw_world_geometry` reverifie ces valeurs avant de
marquer la cible couleur/profondeur.

Le test couvre :
- mapobj, terrain et sky/cloud avec sections valides;
- rejet de l'ancien `AC6GEO1`;
- rejet d'une section `VTX` dont le count ne correspond pas au drawable;
- soumission positive et readback deterministe avec metadata complete.

Limite explicite : ce cycle ne lit pas encore les bytes des streams vertices,
les index reels, les descriptors polygones complets ni les textures NTXR.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
