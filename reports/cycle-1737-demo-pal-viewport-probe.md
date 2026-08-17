# Cycle 1737 — probe viewport PAL : effet réel, qualification refusée

## Résultat

Une exécution neutral fraîche, store neuf, codegen ON, Vulkan et 253 ticks a
activé uniquement `AC6_DEMO_EXPERIMENTAL_PAL_VIEWPORT=1`. Le probe applique
`VkViewport {x=0, y=360, width=640, height=-360}` à partir de la scale/offset
PAL observée. Le renderer piège volontairement car le digest ne correspond
plus à l’oracle noir : `1dea584c…` avec 750 pixels noirs et 229 600 pixels du
clear magenta sur 230 400.

Un contrôle frais avec le même binaire et la variable absente termine avec
`return_code=0`, stderr vide, le digest normal
`0b150fd3…ec58366` et le digest guest linéaire
`0c660f2b…a4913a5f`. La garde expérimentale n’altère donc pas le chemin par
défaut.

Le reçu durable est
[`ac6-demo-pal-viewport-probe-v1.json`](../analysis/demo/ac6-demo-pal-viewport-probe-v1.json).

## Interprétation bornée

Le résultat prouve seulement que le viewport maximal hôte n’est pas la seule
explication possible du noir : la transformation PAL modifie effectivement la
couverture. Il ne prouve ni un écran valide, ni la taille réelle de `RB_SURFACE_INFO`,
ni le contenu EDRAM. Le probe est donc classé `demo-observed`, sans promotion
`demo-qualified`.

L’hypothèse « remplacer le viewport hôte suffit » est rejetée. Le prochain test
doit joindre les dimensions et l’état de RT0 depuis les registres PAL avant de
modifier la surface Vulkan; une source EDRAM synthétique ou un fallback de
présentation reste interdit.

## Garde

La variable est désactivée par défaut; le chemin production garde son viewport
borné et son trap fail-closed. Aucun checkout Xenia/ReXGlue, Ghidra, microcode
ou C++ généré n’a été modifié.
