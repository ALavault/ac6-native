# AC6 retail NTSC-U/J — r496 — quatrième cycle consécutif de capture oracle échoue malgré une charge hôte mesurée plus basse (31-35) ; les deux tentatives échouent AVANT même d'atteindre le `wait-pulse` — recommandation de changer de stratégie plutôt que de retenter

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (2 tentatives bornées, aucune donnée exploitable).

## Contexte

Nommé par r495, après une pause d'une heure décidée par
l'utilisateur (charge hôte relevée à 31-35 au lancement de ce
cycle — la fourchette la plus basse et la plus stable observée
jusqu'ici, contre 33-52 pour r492/r494/r495).

## Établi — deux tentatives, échec plus précoce qu'aux cycles précédents

**Tentative 1** (`--display :281`) : `timeout 240s` atteint pendant
`OracleRun.wait_log()`/`sleep()` — même chemin `SIGTERM` que r493/r494,
correctement intercepté (`KeyboardInterrupt` levée,
`terminate_owned()` appelé). **Aucun processus résiduel** (`pgrep`
vide) — correctif de nettoyage vérifié une quatrième fois en
conditions réelles.

**Tentative 2** (`--display :282`) : `timeout 240s` de nouveau
atteint, toujours dans `wait_log()`/`sleep(4)` — mais cette fois la
trace de la pile Python montre l'interruption au step `sleep 4`
initial (ligne 535 de `ac6-oracle-run.py`), **avant même la première
attente `wait-pulse`**. Le journal `ac6recomp.log` s'arrête en
pleine création de threads invités (`thid 18-21`, fonction
`821F8008`) — le jeu n'a pas fini son initialisation de threads en
240 secondes. C'est un échec PLUS PRÉCOCE que celui de r495 tentative
1 (qui avait au moins atteint un `wait-pulse` en cours) et bien plus
précoce que r481/r483 (56s pour un run réussi complet). Aucun
processus résiduel non plus.

## Non établi

- **Pourquoi la moyenne de charge relevée (31-35, la plus basse de
  l'investigation) n'a pas produit de meilleur résultat que les
  cycles précédents** — soit la moyenne sur 1/5/15 min masque des
  pics instantanés bien plus élevés pendant les secondes critiques du
  boot, soit un troisième facteur indépendant de la charge CPU globale
  contribue (I/O disque, latence mémoire, autre ressource partagée
  non mesurée par `uptime`) — non testé, nécessiterait un profilage
  plus fin (`vmstat`/`iostat` en continu pendant le run, pas seulement
  un instantané avant lancement).

## Décisions prises

- **Ne PAS committer de fusion** — aucune capture utile.
- **Ne PAS tenter de cinquième cycle immédiat.** C'est le QUATRIÈME
  cycle consécutif (r492, r494, r495, r496) à échouer, sur 8
  tentatives bornées au total, malgré une amélioration mesurée de la
  charge hôte moyenne. Le fait que ce cycle échoue PLUS TÔT que les
  précédents malgré une charge plus basse contredit l'hypothèse simple
  « charge basse ⇒ capture réussie » — la relation causale que r489
  avait établie sur 2 échantillons ne se généralise pas proprement à
  un troisième facteur encore non identifié.

## Recommandation explicite à la session parente

**Changer de stratégie plutôt que de retenter une cinquième fois
sous des conditions à peine différentes.** Deux options concrètes,
aucune décidée ici :

1. **Mesurer, pas juste attendre.** La boucle actuelle (« attendre,
   vérifier `uptime`, retenter ») a maintenant produit 4 échecs
   consécutifs malgré une charge en baisse — l'hypothèse initiale de
   r489 (corrélation simple charge/succès) ne suffit plus à expliquer
   les résultats. Avant toute cinquième tentative de capture,
   profiler un run en continu (`vmstat 1`/`iostat 1`/`pidstat 1`
   pendant toute la durée d'un essai) pour identifier la ressource
   réellement contendue au moment précis où `ac6recomp` ralentit —
   pas seulement un instantané `uptime` avant le lancement.
2. **Abandonner temporairement la piste de capture** et revenir à de
   l'investigation statique (lecture de code, comme r487/r488/r490/r491
   l'ont fait avec succès ce même jour) — la piste `AudioRuntime`
   trouvée par r495 reste non lue, et pourrait révéler quelque chose
   d'indépendant de la contention hôte plutôt que de continuer à
   dépendre d'une fenêtre de capture qui n'est manifestement pas
   fiable même à charge modérée.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché).
`git status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé avant
et après ce cycle (mêmes 7 fichiers modifiés par l'autre session).

## Named for r497

Deux pistes concrètes, aucune tentée ce cycle (voir recommandation
ci-dessus) : profilage fin de la ressource contendue, ou lecture
statique du code d'arrêt `AudioRuntime` (r495). Reste ouvert sinon :
les 3 états cibles de r478, toujours non capturés après 8 tentatives
bornées au total.

## Files

Committé : ce rapport, `NEXT.md`. Scratch
(`/fastdata/lavaulta/tmp/r496-*`) supprimé après extraction des
données ci-dessus, non conservé. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
