# Cycle 827 - tailles de streams NDXR bornees

`NativeGeometryMetadata` expose maintenant :
- `vertex_stride`;
- `index_size`;
- `vertex_byte_size`;
- `index_byte_size`.

Le parser NDXR borne exige un marqueur `DATA` apres les sections `VTX`, `IDX`
et `POLY`. Les bytes apres ce marqueur constituent le payload de stream borne.
Le chargement refuse une slice dont le payload ne couvre pas au minimum
`vertex_count * vertex_stride + index_count * index_size`.

Les controles ajoutes :
- `vertex_stride` non nul;
- `index_size` limite a 2 ou 4;
- tailles derivees non nulles;
- payload suffisant;
- `draw_world_geometry` reverifie les tailles avant readback.

Le test couvre :
- tailles derivees pour le terrain `021/010_NDXR`;
- rejet d'une section `VTX` incoherente;
- rejet d'une slice trop courte;
- soumission positive et readback deterministe avec payload borne.

Limite explicite : ce cycle verifie les tailles de streams, mais ne decode pas
encore les vertices, index reels, descriptors polygones complets ni NTXR.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- `ctest --test-dir reconstruction/ace-combat-6/build-debug --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1` en Release et Debug. Le scan `strings` ne retourne aucun
marqueur Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
