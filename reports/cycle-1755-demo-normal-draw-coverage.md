# Cycle 1755 — couverture complète du draw RT0 neutral

## Résultat

Une requête d’occlusion Vulkan opt-in précise (`occlusionQueryPrecise`, avec
`VK_QUERY_CONTROL_PRECISE_BIT`), exécutée sur deux stores froids avec le
binaire codegen ON, compte exactement 921 600 échantillons passés pour la
surface 640×360 en MSAA 4×. Le readback contient 230 400 pixels RGBA nuls,
zéro pixel du clear magenta et zéro autre pixel.

Le digest reste `0b150fd3…ec58366`. Le noir neutral provient donc d’un draw à
couverture complète qui écrit zéro, pas d’une absence de rasterisation, d’un
scissor vide ou d’un resolve sans source. Trace, rapport et stderr sont
byte-identiques 2/2 (`c5357c6d…c5794`, `c8b53c54…b740b`,
`9144cb36…522bd`).

Le reçu durable est
[`ac6-demo-normal-draw-coverage-v1.json`](../analysis/demo/ac6-demo-normal-draw-coverage-v1.json).

## Limite et suite

Ce résultat ferme le consommateur Vulkan neutral mais ne constitue pas un
readback frontend positif. START peut désormais être étudié après la chaîne
writer→PM4→draw→resolve fermée, sans promouvoir `frontend` tant qu’une
transition guest persistante et un readback non noir ne sont pas joints.

La sonde est désactivée par défaut, ne modifie ni shader ni pixels et n’utilise
ni Xenia ni donnée retail.
