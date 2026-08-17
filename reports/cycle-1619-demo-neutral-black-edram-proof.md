# Cycle 1619 — preuve EDRAM noire du premier frame neutral

## Résultat

Le contenu du premier frame neutral est maintenant qualifié sémantiquement
comme uniformément noir, sans supposer que l'EDRAM est initialisée à zéro.

Les 24 point draws bootstrap ont `RB_COLOR_MASK=0` et
`RB_DEPTHCONTROL=0`; ils ne modifient ni color, ni depth, ni stencil. Le draw
normal à l'offset 239 écrit ensuite RGBA `(0,0,0,0)` avec :

- rectangle 640×360 ;
- surface pitch 640, 4x MSAA (`RB_SURFACE_INFO=0x0A020280`) ;
- color mask complet, blend source one/destination zero/add ;
- depth et stencil activés mais fonctions `always` ;
- RT0 base tile 0, format `k_8_8_8_8`.

En 4x MSAA Xenos, les samples sont disposés 2×2. Le rectangle 640×360 couvre
donc une grille de 1280×720 samples. `GetSurfacePitchTiles` donne 16 tiles par
ligne et 45 lignes, soit exactement les 720 tiles que le copy draw réinterprète
ensuite comme une surface 1280×720 1x MSAA, même base tile 0 et même pitch 16
tiles. L'état initial de ces tiles est entièrement écrasé avant la copie.

Le resolve sélectionne génériquement
`ResolveCopyShaderIndex::kFast32bpp1x2xMSAA`, groupes 20×90. Le format RT brut
0 et le format destination brut 6 sont bitwise-equivalent ; `copy_dest_swap=1`
applique BGRA, sans effet sur zéro. Le résultat linéaire RGBA8 attendu est donc
3 686 400 octets nuls, SHA-256
`0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f`.

## Garde

Cette preuve est issue des bytes PM4/shaders démo et de règles génériques
Xenia/ReXGlue. Le runtime Vulkan n'a pas encore exécuté le draw/resolve et aucun
fichier image n'est publié. Le statut est `semantic_frame_qualified`, pas
`runtime_readback_produced`. START reste non promu.

Le receipt durable est
`analysis/demo/ac6-demo-neutral-edram-knownness-v1.json`. Le test tiling remplit
les gaps non-pixel avec `0xA5`, écrit seulement les offsets pixel calculés,
untile, puis exige le SHA noir : les trous de l'étendue tiled ne sont donc pas
implicitement supposés initialisés.

## Validation

- raster CPU : 640×360 pixels × 4 samples = 921 600 samples connus ;
- tiling : 921 600 offsets uniques, SHA linéaire noir exact ;
- tests raster, tiling et receipts Python : PASS ;
- CTest headless `SDL_AUDIODRIVER=dummy` : 16/16 PASS ;
- audit source, complexité et `git diff --check` : PASS ;
- aucun fallback `play`, shader généré, microcode ou image suivi.

## Frontier

Le premier état visuel neutral est noir et guest-owned sur preuves, mais il
reste à produire deux readbacks runtime byte-identiques via le chemin Vulkan
atteint. Après cette validation seulement, neutral pourra être comparé à START
et une transition visuelle causale recherchée.
