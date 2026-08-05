# Cycle 1035 — frontière bridge des objets gameplay

Date : 2026-08-06.

## Provenance

- Lane : bridge, instrumentée; cette preuve ne peut pas valider un gate natif.
- XEX PAL : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Source bridge : commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, arbre externe
  dirty; l’instrumentation `ac6-gameplay-object-vtable` n’est pas copiée dans
  le dépôt natif.
- Exécutable instrumenté : SHA-256
  `e1a3be5398119c1fa5fabbecc14b1c1f3952ae024e989d3bea4bbff871969bc6`.
- Log borné : `/tmp/ac6-cycle-1035-bridge-object-vtables/ac6recomp-follow.log`,
  SHA-256
  `bc74e35b7b8e9e34ea74e3e02d8633ef224f240dd149801d5f9016a79025831f`.

## Census au premier HUD gameplay

Le run frais a suivi la route qualifiée briefing → loadout → cinématique →
HUD, puis a été arrêté après les captures de pilotage. Les échantillons
`0x822707C8` montrent, entre les frames 16312 et 16920 :

- `object_count=230` constant;
- joueur `0xB2470000`, vtable `0x820568D4`;
- `other_player_count=0`;
- un enfant actif `0xB2470100`, vtable `0x82007A10`;
- histogramme de la liste : `0x820568D4:1`, `0x82009AB0:1`,
  `0x82009440:228`.

Les champs bornés autour des objets sont sentinellés (`word4/8/12=0xFEFEFEFE`
et `word16=0x82054D94`) et ne portent pas d’identité d’unité dans cette
acquisition. Le vtable `0x82009440` est donc laissé anonyme : il peut décrire
des objets de scène, mais ne qualifie ni une vague, ni une faction, ni une
cible Mission 01. Aucun objectif ou transition de scénario n’est promu.

## Décision

Cette expérience ferme la mauvaise association « tous les objets de la liste
sont des unités ». La prochaine arête utile doit capturer le créateur ou le
registre d’unités au moment où une vague est effectivement publiée; relancer
le même census statique ne fournirait pas d’information nouvelle.
