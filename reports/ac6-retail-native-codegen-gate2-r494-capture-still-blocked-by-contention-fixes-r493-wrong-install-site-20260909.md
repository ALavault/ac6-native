# AC6 retail NTSC-U/J — r494 — capture toujours bloquée par la contention hôte (2/2 timeouts, charge 33-40), et corrige un défaut réel dans le correctif r493 : le gestionnaire SIGTERM n'était installé que dans le point d'entrée que `run_gate.py` n'utilise jamais

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (2 tentatives de capture, décision utilisateur
explicite de continuer malgré la contention hôte).

## Contexte

Suite à r493 (`fdc6028d`), qui prétendait avoir corrigé la fuite de
processus `Xvfb`/`ac6recomp` sous `timeout`. `uptime` avant ce cycle :
charge 33,54/34,61/38,51 — améliorée par rapport aux 40-52 de r489
mais toujours élevée sur cet hôte à 32 cœurs.

## Établi — le correctif r493 était mal placé, ne couvrait pas le vrai chemin d'exécution

En relançant une capture réelle via `run_gate.py` (le point d'entrée
que TOUS les cycles précédents ont utilisé, pas un test synthétique),
`Xvfb`/`ac6recomp` sont restés actifs après l'expiration du `timeout`
externe — **exactement le symptôme que r493 prétendait avoir corrigé**.
Lecture directe de `recompilation/ace-combat-6-retail/tools/run_gate.py`
lignes 1140-1154 : `run_gate.py` ne construit PAS son exécution via
`ac6-oracle-run.py::main()` (où r493 avait installé le gestionnaire
`signal.signal(SIGTERM, ...)`). Il définit sa PROPRE sous-classe
`RetailRun(runner_module.OracleRun)` et pilote lui-même un bloc
`try: run.start(); run.execute(steps) except (RunError, OSError,
SubprocessError, ValueError): ... finally: run.close()` — sans jamais
appeler `main()` de `ac6-oracle-run.py`. Le gestionnaire SIGTERM
installé par r493 n'était donc **jamais actif** pour le chemin
d'exécution réellement utilisé par cette campagne depuis r454.

## Correctif appliqué et vérifié

Le gestionnaire est déplacé de `main()` vers `OracleRun.__init__()` —
le seul point que les deux chemins d'exécution partagent réellement
(`RetailRun` hérite de `__init__` sans le surcharger, contrairement à
`start()`/`focus()` qu'elle réimplémente entièrement). `run_gate.py`
ne capture pas `KeyboardInterrupt` dans son `except`, mais ce n'est
pas nécessaire : le bloc `finally: run.close()` s'exécute
inconditionnellement avant la propagation de toute exception, y
compris une non capturée par l'`except` — confirmé par lecture directe
de la sémantique Python, puis vérifié empiriquement.

**Vérifié directement, deux fois** : `timeout 20 python3
tools/run_gate.py --diagnostic-route routes/us-pretype28-startup.steps
--mission-dump-shaders --output <scratch> --display :262` → code de
sortie 124, **aucun processus résiduel** 3 secondes après (contraste
avec l'échec initial constaté au début de ce cycle, avant le
correctif). Confirmé une seconde fois lors des deux tentatives de
capture réelles de ce cycle (`--display :263`/`:264`, `timeout 240`
chacune) : les deux ont expiré (contention hôte) mais **aucune des
deux n'a laissé de processus résiduel**.

## Établi — la capture reste bloquée par la contention hôte, pas par un bug de ce dépôt

Deux tentatives réelles (`us-pretype28-startup.steps`,
`--mission-dump-shaders`, `timeout 240` chacune) : **2/2 timeouts**,
aucun `RESULT.json` produit, donc aucune donnée de nuanceur capturée
(les 3 états cibles de r478 restent non atteints). Cohérent avec la
mesure directe de r489 (contention CPU externe, charge 40-52) — la
charge mesurée ce cycle (33-40) reste dans la même plage à risque.

## Non établi

- **Si une charge encore plus basse (< 15-20, l'ordre de grandeur qui
  semble nécessaire d'après r489/ce cycle) permettrait une capture
  fiable** — plausible mais non testé, aurait nécessité d'attendre
  une fenêtre de calme qui ne s'est pas présentée pendant ce cycle.
- **Si le correctif de placement affecte d'autres appelants de
  `ac6-oracle-run.py::main()`** (l'exécution directe, hors
  `run_gate.py`) — non, le gestionnaire reste installé
  (indirectement, via `OracleRun.__init__()` que `main()` déclenche
  aussi en construisant `OracleRun(arguments)`), donc ce chemin reste
  couvert sans changement de comportement.

## Décisions prises

- Corriger le point d'installation plutôt que dupliquer le
  gestionnaire dans `run_gate.py` séparément — un seul point de
  vérité (`__init__`), pas deux fichiers à maintenir en synchronisation.
- Ne pas retenter une troisième fois ce cycle — la limite de 2
  tentatives fixée par la décision utilisateur est respectée, et une
  troisième tentative sans nouvelle preuve de charge réduite serait un
  pari, pas une décision informée.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` —
  vérifié inchangé avant et après (mêmes 7 fichiers modifiés par
  l'autre session concurrente).
- Nettoyer manuellement les processus fantômes laissés par la
  PREMIÈRE tentative de ce cycle (avant le correctif de placement) —
  `kill -9` direct, scratch supprimé, aucune fuite résiduelle après
  vérification.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché).
`git status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé avant
et après ce cycle.

## Named for r495

Le correctif de fuite de processus est maintenant réellement effectif
sur le chemin d'exécution utilisé par toute la campagne — bénéfice
acquis même si la capture elle-même reste bloquée. Reste ouvert : les
3 états cibles de r478, toujours non capturés, bloqués sur une
fenêtre de contention hôte suffisamment basse (non observée ce
cycle). Aucune nouvelle piste statique disponible sans données
fraîches (le mécanisme movie-worker est clos depuis r487/r488,
`fetch_const[1]` est clos depuis r490/r491).

## Files

Committé : `tools/ac6-oracle-run.py` (correctif de placement), ce
rapport, `NEXT.md`. Scratch (`/fastdata/lavaulta/tmp/r494-*`)
supprimé après chaque tentative, non conservé. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
