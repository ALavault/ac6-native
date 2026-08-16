# Cycle 1754 — writer guest du draw RT0 joint au `RB_COPY`

## Résultat

Une sonde opt-in neutral, codegen ON et Vulkan joint le paquet qui produit la
source EDRAM au paquet `RB_COPY` suivant. L’EDRAM n’étant pas de la RAM Xenon,
le writer guest qualifiable est le store du paquet PM4, pas un store CPU dans
les échantillons EDRAM.

Le draw RT0 est le paquet `DRAW_INDX_2` prédicat à l’adresse `0x1274A3BC`
(main IB offset 239, header `C0003601`, initiator `00030088`). Son store guest
est `0x821B5840`, bytes PAL `95 4B 00 04` (`stwu r10,4(r11)`), dans la frontière
Ghidra/pdata `0x821B55C0..0x821B58AC`, thread 1, tick 0.

Le `RB_COPY` est le second `DRAW_INDX_2`, à `0x1274A60C` (offset 387, header
`C0003600`). Son store guest est `0x821B7C04`, mêmes bytes PAL, dans
`0x821B6FD0..0x821B7E14`, thread 1, tick 0. Les deux sont consommés par la
publication du main IB depuis LR `0x821B9C80`.

Deux stores froids reproduisent byte-identiquement trace, rapport et stderr de
sonde. Les SHA-256 sont respectivement `c5357c6d…c5794`,
`c8b53c54…b740b` et `061104e7…68f2`; le stdout normalisé vaut
`386ad33a…55b6`. Le reçu durable est
[`ac6-demo-edram-source-command-join-v1.json`](../analysis/demo/ac6-demo-edram-source-command-join-v1.json).

## Limite

La chaîne writer guest → PM4 draw → `RB_COPY` est fermée, mais le contenu
EDRAM reste noir/non qualifié (`0b150fd3…ec58366` avant resolve et
`0c660f2b…a4913a5f` après). Aucun frontend, mission, pixel non noir ou support
produit n’est promu. La suite doit instrumenter le consommateur Vulkan du draw
et établir un readback positif avant d’ouvrir START.
