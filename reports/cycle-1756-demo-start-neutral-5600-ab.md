# Cycle 1756 — A/B neutral et entrée buttons=16 à 5600 ticks

## Résultat

Deux exécutions froides PAL du même XEX (`de917873…5da8`), sous Vulkan avec
`max_ticks=5600`, atteignent 5463 PRESENT. Les deux restent sans frontend,
mission ou terminal; les graphiques sont identiques et l’ordonnanceur termine
avec 23 threads bloqués et 0 runnable. Le stderr XMA est byte-identique
(`abd259b9…6717`).

La première divergence est la trace séquence 760, tick 252: neutral porte
`buttons=0`, l’autre exécution porte `buttons=16`. Cette valeur d’entrée est
rapportée comme telle, sans lui attribuer un nom de bouton. Les seuls compteurs
différents sont les agrégats qualifiés `RtlEnterCriticalSection` ordinal 293
(`6290753` contre `6290585`) et `RtlLeaveCriticalSection` ordinal 304
(`6285405` contre `6285237`). Aucune divergence milestone, graphics ou
scheduler n’est observée.

Rapports, traces et manifestes sont consignés dans le reçu
[`ac6-demo-start-neutral-5600-ab-v1.json`](../analysis/demo/ac6-demo-start-neutral-5600-ab-v1.json).

## Limite

Cette A/B confirme uniquement une divergence d’entrée et de compteurs runtime;
elle ne qualifie ni frontend, ni mission, ni terminal, et ne constitue pas une
preuve native dérivée de Xenia.
