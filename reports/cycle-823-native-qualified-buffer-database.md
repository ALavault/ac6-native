# Cycle 823 - base de buffers qualifies externe

Ajout de `QualifiedBufferRecord` et `QualifiedBufferDatabase`.

Le format TSV strict est :
`buffer_id<TAB>path<TAB>byte_size<TAB>fnv64`.

La base charge uniquement des chemins externes. `verify(buffer_id)` lit le
fichier borne, calcule un FNV64 et valide simultanement taille et hash.
`VulkanRenderer::RenderAssets` peut recevoir cette base; si elle est presente,
tous les drawables soumis doivent pointer vers un buffer deja verifie.

Le test couvre :
- trois slices factices chargees depuis fichiers externes temporaires;
- verification positive par taille/hash;
- verification negative avec hash divergent;
- rejet renderer avec buffers charges mais non verifies;
- soumission positive apres verification des buffers.

Limite explicite : les slices de test ne sont pas des bytes retail. Ce cycle
cree la frontiere de verification externe; il ne parse pas encore NDXR/NTXR et
ne ferme pas le rendu monde Mission 01.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
