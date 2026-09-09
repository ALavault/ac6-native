# AC6 retail NTSC-U/J — r486 — tentative du merge différé de r481 (piste A libérée par décision utilisateur) : la fiabilité de capture s'est dégradée depuis r481, aucun merge forcé sur des données douteuses

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (décision utilisateur explicite : traiter `native/`
comme libre et tenter le merge différé nommé par r485).

## Contexte

Nommé par r485, candidat 2 : une fois `native/` confirmé stablement
libre, envisager le merge différé des traductions de r481 (12 paires,
aucune des 3 cibles de r478, un nuanceur réellement nouveau
`17dbf9b1e3d6ae04`) dans le registre épinglé. `native/` calme depuis
2026-09-09 05:07:55 jusqu'à au moins 09:13 (plus de 4h, bien au-delà
du cadencement de l'autre chaîne), aucune session propriétaire
identifiable via `ListAgents` — l'utilisateur a explicitement autorisé
ce cycle à traiter `native/` comme libre.

## Établi — le pipeline de traduction fonctionne, une fois le bon répertoire de travail utilisé

`tools/rexglue_shader_translate/build.sh` échouait initialement
(« ReXGlue SDK working copy not found ») parce que le chemin par
défaut du script est relatif à la RACINE du dépôt, pas à
`recompilation/ace-combat-6-retail/` — une erreur d'invocation de ce
cycle, pas un bug du script. Corrigé en lançant depuis la racine avec
le chemin SDK explicite
(`recompilation/ace-combat-6-retail/build/ntsc-uj/source/thirdparty/rexglue-sdk`).
Le binaire `rxshader-translate` compile et lie proprement une fois
cette correction faite.

## Établi — la fiabilité de capture de `routes/us-pretype28-startup.steps` s'est dégradée depuis r481 : 0 capture propre sur 4 tentatives ce cycle

r481 rapportait cette route comme fiable (`executed_steps=8/8`,
`clean_shutdown=true`, 56.4 s). Ce cycle, 4 tentatives :

1. Timeout complet (240 s), aucun `RESULT.json` produit, journal
   arrêté net ~7 s après le lancement (bien avant l'étape
   `type28`/mouvement). Fuite de processus détachés (`Xvfb`,
   `ac6recomp`) laissée par le `timeout` externe (même motif que
   r482), nettoyée manuellement.
2. `executed_steps=8/8` mais `clean_shutdown=false`
   (`game_status=-9`) — un thread `AudioRuntime` n'a pas terminé sous
   2 s, forçant l'arrêt. 22 fichiers de dump produits (11 nuanceurs
   uniques, cohérent avec le compte de r481), MAIS
   `parse_rexglue_cache.py` a refusé de continuer (fail-closed
   correct) : `xsh shader set differs from the dump (xsh-only=0
   dump-only=2)` — le cache `.xsh` persistant n'avait probablement pas
   fini de s'écrire au moment de l'arrêt forcé, laissant 2 nuanceurs
   présents dans le dump brut mais absents du cache validé.
3. Une course a été découverte : le processus détaché de la tentative
   1 (jamais correctement tué par un premier `pkill` trop tôt/mal
   ciblé) tournait ENCORE en arrière-plan (`ELAPSED=210s`) au moment
   du lancement de la tentative 3, écrivant dans le MÊME répertoire de
   sortie `/fastdata/lavaulta/tmp/r486-merge/` que la tentative 3 —
   corrompant son `RESULT.json` (JSON vide/invalide). Identifié via
   `ps -p <pid> -o ... cmd`, tué explicitement (`kill -9`), vérifié
   absent avant de continuer.
4. Après nettoyage complet vérifié (`pgrep` vide avant lancement),
   nouveau timeout complet (240 s), même motif que la tentative 1.

**Aucune des 4 tentatives n'a produit un jeu de données cohérent et
validable.** Ceci est un motif RÉEL et nouveau — r481 ne rapportait
aucun échec sur cette route spécifique — mais reste dans la même
famille de variance déjà caractérisée par r482 (« aléa dépendant de la
charge de l'hôte, non mesuré directement ») et r484
(`us-menu-navigation-probe.steps` confirmé flaky à 2/3). L'hypothèse
la plus simple est une dégradation de charge hôte au fil de la
session (ce cycle survient après plusieurs heures d'activité continue
sur cette machine, dont un build complet de `reconstruction/ace-combat-6`
juste avant) — **non mesurée directement, pas affirmée comme prouvée**.

## Décisions prises

- **Ne PAS forcer de merge sur les données de la tentative 2** — le
  refus fail-closed de `parse_rexglue_cache.py` est correct et
  respecté : un jeu de nuanceurs incomplet/incohérent ne doit pas
  entrer dans le registre épinglé même si un merge était par ailleurs
  autorisé ce cycle.
- **Ne pas retenter une 5e fois sans nouvelle preuve motivant un
  changement d'approche** — 4 échecs consécutifs sur une route
  documentée comme fiable par r481 est un signal de rendement
  décroissant, même raisonnement que les pauses de r480/r485.
- `native/` reste dans l'état exact hérité de l'autre session
  (7 fichiers modifiés, aucun changement introduit par ce cycle) —
  **aucun merge n'a eu lieu**, malgré l'autorisation utilisateur de ce
  cycle, parce qu'aucune capture propre n'a été obtenue pour le
  nourrir.
- Nettoyage systématique des fuites de processus à chaque tentative,
  vérifié par `pgrep` avant et après.

## Gate

Aucune source de contrat touchée, gates inchangés (non ré-exécutés
inutilement ce cycle — aucun changement à vérifier). `native/` ctest
non relancé (aucun changement natif à tester). `git status --porcelain`
sous `recompilation/ace-combat-6-retail/native/` confirmé identique à
avant ce cycle.

## Named for r487

**Le merge différé de r481 reste à faire, sans nouvelle donnée.**
Candidats :
1. Réessayer la capture à un moment de charge hôte plus faible
   (aucune mesure de charge prise ce cycle pour confirmer/infirmer
   cette hypothèse — instrumenter avant de réessayer, pas deviner).
2. Accepter le faible bénéfice (1 seul nuanceur nouveau au-delà du
   registre existant, aucune des 3 cibles) et abandonner cette piste
   spécifique plutôt que de continuer à dépenser du budget oracle
   dessus.
3. Revenir à la piste Ghidra sur l'attente « movie worker » (candidat
   1 de r485), qui n'a pas cette dépendance à un lancement
   oracle-hybride complet et n'est donc pas exposée à cette même
   variance de charge.

## Files

Aucun fichier committé ce cycle (aucun merge réussi). Scratch
(`/fastdata/lavaulta/tmp/r486-merge*`, `/fastdata/lavaulta/tmp/r486-translate/`)
nettoyé, non conservé. `recompilation/ace-combat-6-retail/tools/rexglue_shader_translate/build.sh`
non modifié — l'erreur d'invocation initiale de ce cycle était une
erreur d'utilisation (mauvais répertoire de travail), pas un bug du
script.
