# Cycle 825 - parser header NDXR borne

`NativeGeometryDatabase` n'accepte plus le format temporaire `AC6GEO1`.
Le header borne retenu est maintenant :
`NDXR<TAB>1<TAB>vertex_count<TAB>index_count<TAB>primitive_count`.

Le chargement exige :
- magic `NDXR`;
- version `1`;
- compteurs vertex/index/primitive identiques au `MissionDrawable`;
- buffer externe deja verifie par `QualifiedBufferDatabase`.

Le test couvre :
- trois slices factices NDXR pour mapobj, terrain et sky/cloud;
- metadata `source_format=NDXR`;
- rejet explicite de l'ancien header `AC6GEO1`;
- rejet renderer avec base geometrique incomplete;
- soumission positive et readback deterministe avec tous les buffers et
  metadata presents.

Limite explicite : ce cycle ferme seulement un header NDXR borne. Il ne decode
pas encore les structures retail completes, les streams de vertices, les index,
les descriptors polygones ni les textures NTXR.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
