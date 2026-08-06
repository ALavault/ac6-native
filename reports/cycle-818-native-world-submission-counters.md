# Cycle 818 - compteurs de soumission monde natifs

`VulkanRenderer` publie maintenant deux compteurs produit :
- `last_world_asset_count`;
- `world_asset_submissions`.

Les compteurs sont mis a jour uniquement apres une soumission acceptee par les
gates existantes : frame prete, mission compatible, joueur non nul, unites
actives, base d'assets presente et `MissionRenderDefinition` valide. Les
soumissions rejetees laissent les compteurs a zero.

Cette preuve ne revendique pas encore un rendu pixels monde dans `ac6-native`.
Elle ajoute une surface observable hors HUD qui permettra au prochain raccord
Vulkan reel de prouver que les assets monde declares sont effectivement soumis,
puis de remplacer cette mesure par couleur/profondeur/couverture.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- scan source ciblé : aucun `assets.has(9)`, `assets.has(119)` ni
  `mission_id_ == 1` dans le renderer/runtime produit;
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
