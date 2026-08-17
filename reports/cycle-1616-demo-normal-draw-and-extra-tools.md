# Cycle 1616 — draw normal, SPIR-V validé et oracles épinglés

## Outils qualifiés

Le manifeste portefeuille `ac6-extra-tools/v1` qualifie :

- SPIRV-Tools commit `e39e5c5838bc4b4162c349f2a2e5f163efe5432f`,
  `spirv-val` SHA-256 `2cc19cdd…e3406` ;
- vgmstream commit `5bf4bdf0de37710b412bd81649e513fb054cef85`,
  CLI SHA-256 `f88f81c5…0cab5`.

`spirv-val` est désormais obligatoire après DXC pour toute sortie SPIR-V issue
de XenosRecomp. `vgmstream-cli` ne lit encore aucun actif : il restera dormant
jusqu'au premier flux XMA/XWB atteint et sera alors comparé à FFmpeg sur
metadata, nombre de samples, canaux et PCM hashé. Aucun PCM ne sera suivi.

## Pixel shader atteint

La chaîne temporaire qualifiée est : container démo exact SHA-256
`cd0be9a9…77e8c` → XenosRecomp `990d03b…869d1` → HLSL SHA-256
`e5ef8c70…66d6ef` → DXC → SPIR-V 1448 bytes SHA-256
`23a2d50a…1b0fc` → `spirv-val` exit 0, sortie d'erreur vide.

Le receipt durable est
`analysis/demo/ac6-demo-pixel-spirv-validation-v1.json`. Le HLSL, le SPIR-V,
le container temporaire et le microcode ne sont pas suivis.

## Draw EDRAM normal

L'observer opt-in `AC6_DEMO_WATCH_RESOLVE` capture uniquement les fetchs vertex
bornés des rectangles atteints. Le draw normal neutral au tick 0 qualifie :

- `EdramMode=4` (`kColorDepth`, xenia-generic) ;
- vertex fetch `0x127CA03C`, 21 dwords, endian mode 2 ;
- trois enregistrements de 7 floats ; positions observées
  `(-0.5,-0.5)`, `(639.5,-0.5)`, `(639.5,359.5)` ;
- les cinq autres floats de chaque enregistrement sont zéro ;
- viewport X ±640 et Y ±360 ;
- vertex shader `93488cb9…402b`, pixel shader `4913603d…8e25` ;
- rectangle list, trois indices auto, draw prédicat vrai dans la capture.

Le pixel shader XenosRecomp validé sort son interpolant color 0. Il serait
incorrect d'en conclure que l'image est noire avant d'avoir traduit exactement
le vertex shader immédiat, établi ses exports/interpolateurs, et appliqué les
états blend/color-mask. Les trois vertex shaders immédiats n'ont de match
littéral ni dans le XEX ni dans les 121 payloads logiques `DATA.TBL`.

## Frontier

Adresse, pitch, format, endian, tiling et rectangle du resolve sont fermés. Le
premier champ renderer encore `unknown` est la sémantique exacte du vertex
shader `93488cb9…402b` et donc la valeur color 0 produite dans EDRAM. Le prochain
test utilise le compilateur/disassembleur shader Xenia sur le microcode brut
temporaire, puis valide toute sortie SPIR-V avec l'outil épinglé. Aucun readback
n'est publié avant cette fermeture.

## Validation

- `spirv-val` : PASS sur le pixel shader atteint ;
- test receipt SPIR-V et politique non-suivie : PASS ;
- test IB réel resolve offsets 326/387/415 : PASS ;
- test Xenos transactionnel : PASS ;
- audit source et `git diff --check` : PASS ;
- checkouts Xenia/XenosRecomp/SPIRV-Tools/vgmstream : inchangés.
