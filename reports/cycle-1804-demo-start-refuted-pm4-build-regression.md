# Cycle 1804 — START réfuté comme cause de la disparition PM4

Le A/B qualifié conservé sous
`artifacts/goal-playable/post-start-pm4-ab-20260824/` ne change qu'une entrée :
START au tick 3000, relâché au tick 3001. Les deux runs atteignent 3002 ticks,
2894 `VdSwap`, 24 draws typés, mais zéro soumission ring, zéro dword et zéro
présentation renderer.

La capture positive historique a été inspectée : `old-logo-positive-control.png`
montre bien le logo Namco. Les deux runs courants n'ont produit aucun readback ;
les captures noires ne sont donc pas promues en validation visuelle.

La causalité START/SWG est réfutée. La frontière remonte à une régression du
chemin ring/MMIO du binaire reconstruit après le run positif.

