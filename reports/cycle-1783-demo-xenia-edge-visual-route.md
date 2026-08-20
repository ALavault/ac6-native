# AC6 PAL démo — route visuelle Xenia Edge, cycle 1783

Verdict : **ORACLE MENU + MISSION VISIBLE / NATIVE FRONTEND-NO-GO**,
`supported=false`.

Deux cold boots Xenia Edge natif `60ff861` utilisent chacun une racine de
profil neuve, refusent la création de profil et lient le clavier SDL au slot
invité 0 avant le titre. Une seule impulsion START pendant `PRESS START`
produit le menu `NORMAL / NOVICE / EXIT` en moins de 750 ms dans les deux
exécutions. Les captures menu à 750 ms ont un RMSE normalisé de `0.0144738`.

Un contrôle tardif du second run coïncide avec l'expiration du titre et ouvre
l'attract mode ESRB. Il n'est pas promu comme transition menu. La seconde
transition positive attend donc un nouveau match visuel strict du titre avant
d'injecter START.

Une route oracle séparée confirme ensuite : `NORMAL` + A → chargement → écran
des contrôles + A → cinématique F-16 → START → HUD `MISSION START` et premières
cibles. Toutes les images sélectionnées contiennent du RGB non nul. Elles
restent des pixels oracle, jamais un readback du runtime natif.

Le contre-test natif ferme l'hypothèse « second bouton manquant » : START puis
START, et START puis A au tick 3010, atteignent tous deux 3020 ticks/2883
PRESENT sans nouveau lookup SWG après la rafale du tick 3001. START puis A
reste sans frontend à 4200 ticks/4063 PRESENT. La divergence native est donc
dans l'état/dispatch post-START, pas dans une confirmation utilisateur
supplémentaire.

Preuves :

- [`manifest.json`](../analysis/oracle/ac6-demo-xenia-edge-visual-route-v1/manifest.json)
- [`run-b-title.png`](../analysis/oracle/ac6-demo-xenia-edge-visual-route-v1/run-b-title.png)
- [`run-b-menu-0750ms.png`](../analysis/oracle/ac6-demo-xenia-edge-visual-route-v1/run-b-menu-0750ms.png)
- [`mission-hud-start.png`](../analysis/oracle/ac6-demo-xenia-edge-visual-route-v1/mission-hud-start.png)
- [`ac6-demo-title-followup-inputs-v1.json`](../analysis/demo/ac6-demo-title-followup-inputs-v1.json)

Risque restant : Edge n'expose toujours pas le transport synchrone
completed-present/XAM-poll. Cette observation borne la route visuelle, mais ne
fournit ni checkpoint exact ni movie guest frame-exact.
