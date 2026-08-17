# Cycle 1739 — sonde des 24 `PointList` bootstrap

## Résultat

Une exécution neutral fraîche, store neuf, codegen ON, Vulkan/Xvfb et 253
ticks a activé uniquement `AC6_DEMO_WATCH_POINT_DRAWS=1`. Les 24 lignes sont
au tick 0/thread 1, avec le même VS `099625f3…e4e3`, le même PS
`4913603d…8e25` et exactement le même état brut.

Les mots observés sont `RB_SURFACE_INFO=0`, `RB_COLOR_MASK=0`,
`RB_DEPTH_INFO=0`, `RB_DEPTH_CONTROL=0`, `RB_MODECONTROL=0x1000000E`,
`TEXTURE_FETCH_0=0` et `TEXTURE_FETCH_1=0`. Aucun fetch de vertex n’est donc
capturé pour ces points; la sonde n’attribue pas de sémantique Xenos à ces
valeurs et ne les traite pas comme un writer EDRAM.

Le renderer termine avec les mêmes readbacks noirs (`0b150fd3…ec58366`,
`0c660f2b…a4913a5f`). Le résultat durable est
[`ac6-demo-point-draw-source-probe-v1.json`](../analysis/demo/ac6-demo-point-draw-source-probe-v1.json).

## Limite et suite

Ce résultat exclut seulement les 24 `PointList` comme candidat qualifiable sur
la base des registres/fetch observés; il ne prouve pas l’absence de tout effet
interne Xenos. Le prochain test reste l’instrumentation du producteur de la
surface/EDRAM au draw rectangle, ou un writer guest borné avant `RB_COPY`, avec
PC/LR/thread/tick et trap fail-closed. Aucun pixel, fallback ou screencap n’est
promu.
