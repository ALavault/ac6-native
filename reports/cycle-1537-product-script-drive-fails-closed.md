# Cycle 1537 — la progression de scénario produit échoue fermée

Date : 2026-08-12

## Résultat

La route publique `play`/`replay` ne fabrique plus le signal `-2` du
scheduler retail à chaque tick. `RetailSessionConfig` expose désormais trois
politiques nommées :

- `ExternalProbe`, valeur produit par défaut, n'avance jamais le curseur sans
  appel explicite à `advance_script()` ;
- `QualifiedRuntime`, réservé au futur port des gardes retail, est refusé par
  les deux points d'entrée ;
- `DiagnosticFixedTick`, qui reproduit l'ancienne cadence synthétique, est
  accepté uniquement par le lecteur de payload de test et refusé par toute
  session ouverte depuis le cache scellé.

Une trace de replay ne change plus cette politique. Le rapport public porte
`script_drive=external_probe`, `script_advance_each_tick=false` et
`forced_progression=false` avec ou sans `--trace`.

Cela corrige la promotion prématurée de la cadence décrite au cycle 1519 et
la distinction spéciale `--trace` du checkpoint 4l. Ces rapports restent des
traces historiques ; le présent invariant les remplace pour la route produit.

## Contrôle de 3 600 ticks

Le test store-backed M01 exécute une fenêtre contrôlée de 3 600 ticks : un
parcours continu, une continuation restaurée au tick 1 800 et un rejeu complet
des mêmes entrées. À chaque tick, les trois parcours restent en `Gameplay`,
au sous-scénario 0 / pas 0, avec script non terminé, résultat
`InProgress` et zéro objectif réussi ou échoué. Les 339 compteurs valent zéro
avant et après la fenêtre. Les digests finaux du parcours continu, restauré et
rejoué sont identiques.

Le corpus PAL qualifié (index
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`)
repasse aussi le lecteur partagé M01–M15 : après huit ticks par mission,
`executed=1`, `ended=0` et l'état reste `Gameplay`. Cela constitue une garde
de lecteur seulement ; les sémantiques M02–M15 restent différées.

## Frontière conservée

M01-C n'est pas fermé. Il manque encore le join statique qui choisit la
première cible, son activation, sa durabilité/arme retail, puis relie sa mort
au producteur de compteur et aux trois gardes qui autorisent le signal `-2`.
Le runtime n'avancera pas la mission tant que cette chaîne n'est pas qualifiée.

## Validation

- build `ac6-retail-session-tests`, `ac6-retail-session-replay-tests` et
  `ac6-native` : réussi ;
- CTest `ac6-retail-session` et `ac6-retail-session-replay-tests` : 2/2 ;
- exécution manuelle avec le cache PAL qualifié : M01–M15, 15/15 ;
- `git diff --check` : réussi.
