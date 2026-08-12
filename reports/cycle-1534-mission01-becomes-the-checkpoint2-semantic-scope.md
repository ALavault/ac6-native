# Cycle 1534 — Mission 01 devient le scope sémantique du checkpoint 2

## Décision

La preview communautaire est désormais construite verticalement sur Mission 01
avant d'étendre la sémantique aux quatorze autres missions. Les six lanes du
checkpoint 2 doivent donc fermer le cône de dépendances effectivement exécuté
par M01, pas l'ensemble des variantes de campagne avant que M01 soit jouable.

Cette décision réconcilie le checkpoint avec la politique déjà portée par
`analysis/mission01-execution-spine.json` : `defer_other_missions=true` et
`active_scope=mission01-executed-dependency-cone`.

## Ce qui ne change pas

Un lecteur partagé reste testé sur les quinze payloads campagne. Cette
obligation détecte les régressions de format, de bornes et de rejet, mais ne
transforme pas une variante M02–M15 non exécutée par la preview en blocker
sémantique de M01.

La preview exige toujours les six lanes M01, puis M01-B à M01-F et les quatre
gates JF/JV/JP/JG. Aucun comportement, compteur, asset, flux média ou octet
retail ne peut être synthétisé pour accélérer cette fermeture.

## Ordre de livraison

```text
checkpoint 2 : six lanes du cône exécuté M01
checkpoint 3 : M01-B..F et JF/JV/JP/JG
preview TGZ  : frontend honnêtement limité à M01, sans octet retail
ensuite      : extension M02..M15 et campagne complète
```

Le produit 1.0 PAL hors-ligne reste une cible ultérieure. Le changement porte
sur l'ordre des preuves et de livraison, pas sur l'autorisation de déclarer les
autres missions supportées.
