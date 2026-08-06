# Cycle 845 — replay file contract

Date: 2026-08-04

## Résultat

`ReplayLog` sait maintenant écrire et lire un format `AC6RPLY` version 1 :
en-tête fixe, version little-endian, nombre de frames borné à 1 000 000 et
champs `pitch/roll/yaw/throttle/buttons` sérialisés explicitement.

Le chargement construit un nouveau vecteur avant publication; un fichier
tronqué, corrompu, surdimensionné ou contenant des octets supplémentaires ne
modifie pas le replay courant. La fixture vérifie une reprise bit-à-bit et un
fichier invalide.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Ce contrat est local au service replay. La sauvegarde de progression, le
chemin utilisateur PAL, l'interruption/reprise et leur raccord frontend restent
à intégrer.
