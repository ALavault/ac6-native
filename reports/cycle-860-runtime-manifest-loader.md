# Cycle 860 — manifeste runtime externe

Date: 2026-08-04

## Résultat

`MissionManifestLoader` lit un manifeste de chemins TSV (`catalog`, `assets`,
`launches`), résout les chemins relatifs contre le répertoire du manifeste et
charge les trois bases dans des objets temporaires avant publication atomique.
Clés inconnues, doublons, chemins vides, fichiers absents ou bases invalides
sont rejetés.

La fixture crée un jeu de manifestes relatifs, charge mission 1/asset 9/
lancement 4097, puis supprime les fichiers temporaires. Aucun asset retail
n'est embarqué dans le binaire.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Le manifeste ne couvre pas encore les bases render/géométrie/matériaux et
`ac6-native` ne le consomme pas encore pour lancer Mission 01.
