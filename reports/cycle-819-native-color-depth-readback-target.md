# Cycle 819 - cible readback couleur/profondeur native

Ajout de `NativeRenderTarget` et `RenderReadback` dans `ac6_product_core`.
La cible fournit :
- `resize(width, height)` fail-closed sur dimensions nulles ou trop grandes;
- `clear(color, depth)`;
- `mark_world_asset(frame, asset, ordinal)`;
- `readback()` avec couverture couleur, couverture profondeur et hashes
  deterministes.

`VulkanRenderer::render` accepte un target optionnel. Si une frame passe les
gates existantes, chaque asset declare dans `MissionRenderDefinition` marque la
cible couleur/profondeur. Les compteurs de soumission monde restent synchrones
avec cette operation.

Limite explicite : ce cycle ne rend pas encore les meshes retail Mission 01.
Les pixels marques sont une surface de verification produit pour le futur
raccord Vulkan couleur/profondeur; ils ne remplacent pas la preuve attendue
terrain/sky/cloud/F-16 issue des assets PAL.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1`;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
