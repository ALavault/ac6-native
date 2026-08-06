# Cycle 900 — contrat d'en-tête NTXR

Les lignes texture peuvent maintenant ajouter `width`, `height` et `format`
après le chemin et la taille :

```text
mission stable texture sampler address fnv64 path size width height format
```

Le loader vérifie le magic `NTXR`, le FNV-64, la taille, puis les dimensions
et le format big-endian de l'en-tête (`0x24/0x26`, format `0x04`). Une tranche
entry 119 512×512 format 524 passe le manifeste ; les champs historiques à six
colonnes restent compatibles.

Le décodeur texel BC1/BC3 et le join MATE/GIDX ne sont pas encore activés dans
le rasterizer. Cette étape ferme uniquement l'identité et le contrat de
ressource externe, sans prétendre à la parité couleur.

Validations : build et manifeste réel acceptés ; CTest normal/sanitizer à
relancer après la prochaine modification du pipeline texture.
