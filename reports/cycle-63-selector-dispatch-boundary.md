# AC6 — correction de frontière de dispatch `0x82227378`

Date : 2026-07-16

## Statut : supersédé pour le call-site `0x8226ECB0`

Cette analyse dépend du corps `ace-combat-6-corrected`, lequel est maintenant
connu incompatible avec le corps PPC retail à `0x8226ECB0`. Elle reste une
description bornée de ce corps corrigé, mais ne définit plus le contrat du
callback du parcours retail. La relecture du projet `ace-combat-6` établit le
snapshot de registres au call-site : `r4 = objet + 0x24f8`, `r5 = objet +
0x254c`, `f1 = delta`; elle ne prétend pas en déduire l'ABI interne de
`0x82227378`. Voir
`cycle-94-traversal-project-provenance-recheck.md`.

## Preuves

- XEX AC6 PAL `default.xex`, SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Inspection Ghidra headless corrigée :
  `.build/ac6-ghidra-cycle-63/inspect.log` et
  `.build/ac6-ghidra-cycle-63/continuation.log`.

Le corps réel commence par copier `r3/r4/r5` dans `r29/r28/r26`. Il compare
`r4` à la plage entière inclusive `0x80..0x83`, puis utilise `r6 == -1` pour
balayer dix pointeurs à partir de `r3 +0x90`; sinon il sélectionne un pointeur
par index depuis `r3 + (r6 + 0x24) * 4`. Pour chaque table non nulle à
`child +0x1c`, il appelle `0x822C67E8`, vérifie l'index contre le compte
`table +0x08`, forme `table +0x0c + index*0x30`, puis appelle le helper
`0x822272D8`.

Ces faits qualifient la forme des arguments internes, pas la préparation de
registres au call-site `0x8226ECB0` ni le rôle des objets.

## Correction native

Le callback hôte ne prétend plus recevoir `(frame_delta, 0x24f8, 0x254c)`.
Il reçoit un `Function82227378OpaqueArguments` : sélecteur `r4`, valeur `r5`
et index signé `r6`. La branche directe de traversal exige
`qualified=true`; sinon elle s'arrête et incrémente
`direct_context_rejected`. Cela empêche de transformer une valeur flottante
de frame en sélecteur entier fictif.

Les scénarios de test peuvent fournir des arguments synthétiques explicitement
qualifiés. Cela teste la mécanique native, pas le contexte retail.

## Limites

Le helper `0x822272D8`, le mappage `0x822C67E8`, les tables et le call-site
PPC restent `needs-dynamic-evidence`. Aucun lien avec une mission, un avion,
un spawn ou un comportement de vol n'est déduit.
