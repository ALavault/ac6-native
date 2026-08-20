# Démo PAL — le premier pixel shader produit réellement le noir

Le premier rectangle Vulkan atteint ne manque pas de texture, de constante ou
de couverture. Son pixel shader PAL exact (`0x82013E80..0x82013EA4`,
`4913603d…c98e25`) exécute seulement `max oC0, r0, r0` après allocation de la
couleur. Le traducteur initialise `r0` à zéro : la sortie RGBA est donc zéro.

Le reçu [ac6-demo-bootstrap-pixel-zero-v1.json](../analysis/demo/ac6-demo-bootstrap-pixel-zero-v1.json)
joint cette sémantique au readback réel 640×360 : les 921 600 échantillons MSAA
passent et les 230 400 pixels résolus sont noirs. Il ne s'agit pas d'une absence
de rasterisation ou d'un descripteur Vulkan manquant.

La prochaine frontière visuelle est le premier lot PM4 atteint qui charge un
pixel shader autre que ce bootstrap. Aucun pixel synthétique ni writeback guest
non qualifié n'est autorisé.
