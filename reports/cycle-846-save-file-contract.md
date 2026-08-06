# Cycle 846 — save progression file contract

Date: 2026-08-04

## Résultat

`SaveStore` persiste maintenant les slots dans un format `AC6SAVE` version 1,
avec ordre de slots déterministe, entiers little-endian et bits IEEE-754
explicitement sérialisés pour le snapshot (`tick`, position XYZ).

Le chargement est atomique et borné à 1 024 slots. Il rejette slot zéro,
snapshot invalide, doublon, troncature, magic/version inconnus et octets
supplémentaires. La fixture vérifie deux slots, leur reprise exacte et un
fichier corrompu.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Le fichier n'est pas encore raccordé au chemin utilisateur PAL, aux paramètres
de plateforme ni à la reprise naturelle du frontend SDL3. Le format ne contient
pas encore la progression campagne complète ou la localisation.
