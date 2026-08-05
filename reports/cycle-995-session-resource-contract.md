# Cycle 995 — persistance du contrat resource dans AC6SESS

Le format `AC6SESS` passe en version 7 pour sérialiser, avec chaque identité
d'asset de checkpoint, la taille déclarée et la liste d'identifiants de
dépendances. Les versions 1 à 6 restent acceptées; elles relisent les
identités historiques sans inventer de taille ou de dépendances. La
validation refuse les dépendances nulles, auto-référentes ou dupliquées.

Le round-trip de sauvegarde couvre maintenant un checkpoint portant un asset
de taille 7 et une dépendance `119`, puis compare le checkpoint relu à
l'original. Les invariants de restauration continuent de comparer l'identité
complète au manifeste courant et échouent fermement en cas de divergence.

Validation:

```text
cmake --build build -j2                         pass
SDL_AUDIODRIVER=dummy xvfb-run -a ctest ...     5/5 pass
```
