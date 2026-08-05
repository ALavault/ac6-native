# Cycle 991 — contrat atomique des manifestes d’assets qualifiés

Le runtime natif accepte maintenant les lignes d’assets legacy à trois
colonnes et les lignes étendues
`asset_id<TAB>relative_path<TAB>sha256<TAB>byte_size<TAB>dependencies`.
Une ligne étendue exige une taille non nulle; `-` représente une liste de
dépendances vide. En mode étendu, chaque fichier est résolu relativement au
manifeste, les chemins absolus ou contenant `..` sont refusés, puis la taille
et le SHA-256 sont vérifiés avant publication. Les identifiants de dépendance
doivent exister, ne peuvent pas s'auto-référencer et leur graphe doit être
acyclique.

Le chargement travaille dans une base temporaire. Toute erreur de syntaxe,
d'intégrité, de chemin ou de graphe laisse la base précédemment publiée
inchangée. Le générateur Mission 01 émet désormais les colonnes étendues avec
la taille calculée et `-`; aucun payload retail n'est copié dans ce contrat.

Tests ajoutés pour la validation positive, la compatibilité legacy, la taille
incorrecte, le hash incorrect, la dépendance absente et le cycle. Validation:

```text
cmake --build build -j2                         pass
SDL_AUDIODRIVER=dummy xvfb-run -a ctest ...     5/5 pass
```

La qualification retail des dépendances sémantiques reste inchangée: le
catalogue des missions 3–15 demeure `partial`.
