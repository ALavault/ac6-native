# Cycle 1518 — replay retail versionné et déterministe

## Delivered

Le fichier `RetailSessionReplay` passe en version 3. L’identité du cache,
la difficulté, le loadout, la graine, le nombre de ticks, le digest final et
des checkpoints sont désormais sérialisés dans le même fichier avant le
remplacement atomique. Les checkpoints sont bornés à 4096, strictement
croissants et portent le SHA-256 du préfixe d’entrées correspondant.

Le digest final est volontairement défini et documenté comme le digest du
flux d’entrées canonique (pitch/roll/yaw little-endian sur 16 bits, throttle,
buttons : 9 octets par frame), pas comme un digest de l’état de monde. Les
formats v1 et v2 sont lus puis migrés en mémoire avec une graine stable et un
digest d’entrée calculé ; le format écrit reste v3.

`retail play --replay` pose un checkpoint tous les 600 ticks et clôt les
métadonnées au dernier tick. `retail replay` refuse une identité de cache ou
des métadonnées incohérentes et produit maintenant le schéma
`ac6.native-retail-replay.v3` avec graine, tick final, nombre de checkpoints,
digest d’entrée et digest final.

## Validation

- `cmake --build reconstruction/ace-combat-6/build -j16` : passe.
- CTest : 70/70 réussis, avec les deux skips d’environnement attendus
  (`ac6-retail-frontend-resources`, `ac6-vulkan-surface-smoke`).
- Tests Python : 91 réussis, 14 sous-tests réussis.
- Audits JF/contrats/adresses/dérivations/ladder : tous passés ; les trois
  copies actives du contrat Mission 01 ont été rafraîchies avec le hash et la
  taille actuels de `retail_session.cpp`.
- Audit du cache qualifié : `missions=15`, `blobs=926`,
  `bytes=5424368676`, index
  `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.
- Installation staging : `cmake --install ...` passe et `bin/bin` est absent.

## Boundary retained

La graine est persistée comme contrat de replay mais le runtime de mission ne
consomme pas encore un générateur aléatoire injecté ; la simulation actuelle
reste déterministe sans cette source. Le digest de monde final, le rejeu
humain JP et la parité image/audio JG restent à fermer avant la verticale
complète et la généralisation aux missions 02–15.
