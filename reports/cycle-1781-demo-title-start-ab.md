# AC6 PAL démo — START title A/B, cycle 1781

Verdict : **TITLE-ENTRY-REACHED / FRONTEND-NO-GO**, `supported=false`.

Deux imports froids PAL rejouent 3 200 ticks avec movie XAM et atlas. START
au tick 3000 diverge au tick 3001 : thread 1, LR `0x820DC224`, vers les
entrées `0x820D32D0` et `0x820D3AC8`. L’atlas ajoute 133 fonctions et 91
arêtes indirectes par rapport au neutral ; la route confirme donc le cône
title attendu.

Les deux routes conservent pourtant `PRESENT=3063`,
`frontend=false`, `mission=false`, `terminal=false`, aucune requête
persistante du mode manager, aucun propriétaire menu, et aucun writeback
guest. Cette exécution headless est une capsule de contrôle-flow, non une
validation visuelle.

Les digests et le premier point de divergence sont dans
[`ac6-demo-title-start-ab-v1.json`](../analysis/demo/ac6-demo-title-start-ab-v1.json).
