# AC6 PAL démo — corridor frame-dispatch titre, cycle 1784

Deux probes frais et strictement identiques (START T3000, relâchement T3001)
ferment le dispatch du propriétaire `0x2E4035D0`. Avant START, il exécute par
tick `0x82323968` une fois, `0x82322A80` deux fois et `0x823239F0` une fois.
À T3001, il bascule vers **un seul** appel `0x823239F0` par tick jusqu'à T3019.

La transition est déterministe : les deux AC6RTPLY, rapports et sorties
diagnostiques ont les mêmes SHA-256. L'entrée de table concernée est
`0x2DD6A854 = {0x000B9620, 0x00000001}`. Le handler restant rejoint le
consommateur matriciel `0x820EB200` via `LR=0x82325ED4`; ce consommateur reste
atteint après START. Aucun frontend, objectif de mission ni terminal ne se
produit dans cette fenêtre.

Le nouveau traceur est uniquement opt-in, ne lit ni n'écrit la mémoire guest,
et ne trace que `LR=0x82323E4C` aux ticks 2990–3020. Il fournit le premier
point de divergence précis : le problème est après le dispatch de frame et le
consommateur matriciel, pas une entrée START absente.

Capsule : [`ac6-demo-title-frame-dispatch-ab-v1.json`](../analysis/demo/ac6-demo-title-frame-dispatch-ab-v1.json).
