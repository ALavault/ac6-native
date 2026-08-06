# Cycle 847 — écriture sauvegarde atomique

Date: 2026-08-04

## Résultat

`SaveStore::write_file` écrit désormais dans `<save>.tmp`, ferme et vérifie le
flux, puis renomme le temporaire sur le chemin final. Un échec de création ou
d'écriture nettoie le temporaire et ne publie pas de fichier partiel.

La fixture vérifie l'absence du temporaire après succès et après échec sur un
répertoire parent absent, en plus de la reprise des slots du cycle 846.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Le chemin utilisateur PAL et la sauvegarde déclenchée par le frontend ne sont
pas encore raccordés; ce cycle ferme seulement l'invariant d'écriture
interrompue du service.
