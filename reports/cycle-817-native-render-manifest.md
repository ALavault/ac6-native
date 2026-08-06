# Cycle 817 - manifeste de rendu natif

Ajout de `MissionRenderDefinition` et `MissionRenderDatabase`. Le format TSV
strict est :
`mission_id<TAB>asset_ids`.

`WorldFrame` publie maintenant le `mission_id` produit par `MissionRuntime`.
`VulkanRenderer::RenderAssets` porte la base d'assets et la definition de rendu
associee; la soumission refuse une definition absente, vide, rattachee a une
autre mission ou dont un asset requis manque. Les IDs 9/119 ne sont donc plus
codes dans le renderer.

Le test produit couvre :
- manifeste de rendu valide pour missions 1 et 2;
- manifeste invalide avec asset duplique;
- frame non prete;
- base d'assets absente;
- definition de rendu absente;
- definition de mission incompatible;
- soumission positive quand frame, units, joueur, assets et definition
  correspondent.

Validation :
- `cmake --build reconstruction/ace-combat-6/build -j2`
- `ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure`
- `strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'`
- `ldd reconstruction/ace-combat-6/build/ac6-native`

CTest passe a `1/1`. Le scan `strings` ne retourne aucun marqueur
Xbox/oracle/PPC; `ldd` ne liste que les dependances Linux standard.
