# AC6 retail NTSC-U/J — r492 — tentative de capture oracle malgré la contention hôte : un run atteint `type28=30` mais sans arrêt propre (aucun cache persistant), un second reste orphelin au-delà du timeout externe — aucune donnée fusionnable, mais nouvelle preuve de fuite de processus

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (décision utilisateur explicite : tenter malgré la
contention plutôt qu'attendre indéfiniment).

## Contexte

r489 avait mesuré une contention hôte externe (charge 40-52 sur
32 cœurs, deux jobs sans rapport) expliquant la friabilité des
captures. Après plus d'une heure sans amélioration significative
(charge encore ~38-45 au lancement de ce cycle), l'utilisateur a
demandé de continuer plutôt que d'attendre — ce cycle traite cela comme
« essayer et rapporter le résultat réel » plutôt que supposer un échec.

## Établi — tentative 1 : progression complète mais arrêt non propre, aucune donnée exploitable

`run_gate.py --diagnostic-route routes/us-pretype28-startup.steps
--mission-dump-shaders --display :241`, charge hôte 38-45 au
lancement. Résultat : **les 8/8 étapes de la route exécutées**,
capture `step-08-type28-30.png` produite (135 436 octets, non vide) —
la route a bien atteint son point cible, contrairement aux échecs
habituels sous charge. Mais `clean_shutdown=false`,
`game_status=-9` (signal), journal se terminant sur
`AudioRuntime: worker thread did not exit within 2s, terminating`.
Durée totale 92 s (raisonnable, pas le profil pathologique de 700 s
déjà documenté).

22 nuanceurs bruts dans `shader-dump/`, mais **aucun fichier
`.xsh`/`.xpso`** sous `storage-root/` — `parse_rexglue_cache.py`
nécessite les deux (`--xsh`/`--xpso`) en plus du dump brut pour
produire un manifeste de traduction ; sans cache persistant, la
traduction est bloquée par construction (comportement fail-closed déjà
documenté par r481/r486, reconfirmé ici indépendamment).

## Établi — tentative 2 : fuite de processus au-delà du timeout externe, nouvelle preuve directe

Même invocation, `--display :242`. Le bloc `timeout 240 ... ; echo
"exit=$?"` s'est terminé (capturé par la notification de tâche de
fond), mais son fichier de sortie shell (`r492-b-console.log`) est
resté à **0 octet** — signe que `timeout` a tué le wrapper Python
AVANT tout flush de sortie utile. Vérification directe (`pgrep`) **4
minutes après cette fin apparente** : `Xvfb :242` et `ac6recomp`
étaient toujours actifs, `ac6recomp.log` toujours en croissance
(11 Mo), journal montrant une activité continue (PRESENT, cycles
movie-worker) — **le processus n'était ni gelé ni terminé, juste
détaché du groupe de processus tué par `timeout`** (même mécanisme
déjà noté par r482 pour `Xvfb`, ici confirmé s'étendre à `ac6recomp`
lui-même). Terminé manuellement (`kill -9` sur les deux PID,
verrou X11 retiré) ; aucune fuite résiduelle après vérification.

Aucune capture `type28-30` atteinte dans cette fenêtre étendue
(~4 min au-delà du timeout nominal de 240 s) malgré une activité
continue — cohérent avec le profil de contention déjà caractérisé par
r489 (le processus reste actif, juste privé de temps CPU réel par
l'ordonnanceur), pas un nouveau mécanisme de blocage.

## Non établi

- Si un arrêt propre (`xdotool windowclose`, comme r480 l'avait
  utilisé) aurait permis à la tentative 1 de produire un cache
  persistant malgré la charge — non testé ce cycle (limite de 2
  tentatives respectée).
- Si la charge hôte redescendra suffisamment pour rendre les captures
  fiables à nouveau — dépend de processus externes à ce dépôt.

## Décisions prises

- **Aucune fusion dans le registre épinglé** — ni l'une ni l'autre
  tentative n'a produit de données exploitables (tentative 1 : dump
  brut sans cache persistant, traduction bloquée ; tentative 2 :
  aucune capture `type28-30` atteinte).
- **Limite de 2 tentatives respectée** — ne pas relancer indéfiniment
  sous contention confirmée.
- Nettoyage des processus orphelins effectué immédiatement à la
  découverte (pas laissé pour un cycle suivant).
- `native/` non touché — vérifié `git status` avant et après,
  identique aux cycles précédents (7 fichiers, propriété de la session
  concurrente).

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés — aucune source de contrat touchée. `ctest`
natif non relancé (`native/` non touché ce cycle).

## Named for r493

**La contention hôte persiste** (charge ~38-45 au moment de ce
rapport, inchangée depuis plus d'une heure) — ce n'est pas un problème
de ce dépôt. Reste ouvert :
1. Réessayer avec un arrêt propre explicite (`xdotool windowclose`,
   motif r480) plutôt que de compter sur l'arrêt interne de
   `run_gate.py`, pour voir si cela produit un cache persistant même
   sous charge partielle.
2. Attendre une fenêtre de charge plus basse avant de retenter (repère
   `uptime` sous ~10-15).
3. Considérer une garde supplémentaire dans `run_gate.py` lui-même
   pour tuer le groupe de processus complet (pas seulement le wrapper
   Python) en cas de timeout externe — corrigerait la fuite de
   processus observée ce cycle et par r482, à la source plutôt qu'au
   cas par cas.

## Files

Committé : ce rapport, `NEXT.md`. Aucun script réutilisable produit
(mesure/tentative directe). Non conservé (scratch,
`/fastdata/lavaulta/tmp/r492-*`) : dumps de nuanceurs bruts, journaux
complets, captures d'écran. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
