# AC6 retail NTSC-U/J — r493 — corrige la fuite de groupe de processus signalée par r492 : `timeout` ne tuait ni `Xvfb` ni `ac6recomp`, faute d'un gestionnaire SIGTERM propageant le nettoyage existant

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé pour le correctif lui-même (lecture de code +
test direct de la fuite/du correctif) ; un test `timeout 15` a
relancé le binaire oracle brièvement pour vérifier le correctif.

## Contexte

Nommé par r492 : deux cycles indépendants (r482, r492) ont trouvé
`Xvfb`/`ac6recomp` toujours actifs plusieurs minutes après qu'un
`timeout N` externe a rendu la main, nécessitant un nettoyage manuel
à chaque fois. r482 avait attribué cela à `Xvfb` seul
(`start_new_session=True`) ; r492 a étendu l'observation à
`ac6recomp` lui-même. Aucun des deux cycles n'avait lu le mécanisme
exact.

## Établi — cause exacte, lue directement dans `tools/ac6-oracle-run.py`

- `OracleRun.start()` lance `Xvfb` et le jeu avec
  `start_new_session=True` (lignes ~619 et ~656) — chacun devient le
  leader d'un nouveau groupe de processus, détaché de celui du
  wrapper Python.
- `OracleRun.close()` (ligne 661) fait le nettoyage correct : il
  appelle `terminate_owned()` (ligne 283), qui envoie
  `SIGINT`/`SIGTERM`/`SIGKILL` via `os.killpg(process.pid, ...)` —
  donc au GROUPE entier, pas juste au PID direct. Ce mécanisme
  fonctionne et est correctement écrit.
- Le problème est l'appel : `main()` (ligne ~811) a
  `try: runner.start(); runner.execute(steps) except KeyboardInterrupt:
  ... finally: runner.close()`. **Seul `KeyboardInterrupt` (SIGINT,
  Ctrl-C) est intercepté.** Un `SIGTERM` externe (ce que `timeout`
  envoie par défaut) n'a AUCUNE disposition Python personnalisée
  avant ce correctif — le comportement par défaut de Python pour
  `SIGTERM` est de terminer le processus immédiatement, **sans
  dérouler la pile, donc sans exécuter le bloc `finally`**. `close()`
  n'était donc jamais appelé sur un `timeout`, et `Xvfb`/le jeu
  restaient orphelins dans leur propre groupe, hors de portée du
  `SIGTERM` que `timeout` envoie au seul processus wrapper.

## Correctif appliqué et vérifié

Un gestionnaire `signal.signal(signal.SIGTERM,
_raise_keyboard_interrupt_on_sigterm)` installé au tout début de
`main()`, avant tout lancement de sous-processus. Il transforme un
`SIGTERM` reçu en `KeyboardInterrupt` Python — réutilisant
EXACTEMENT le chemin de nettoyage déjà existant et correct pour
Ctrl-C, sans dupliquer de logique.

**Vérifié directement** : `timeout 15 python3 tools/run_gate.py
--diagnostic-route routes/us-pretype28-startup.steps
--mission-dump-shaders --output <scratch> --display :251` (le jeu
met largement plus de 15 s à atteindre `type28=30`, donc `timeout`
intervient à coup sûr) → code de sortie 124 (timeout atteint, comme
attendu) → **aucun processus `Xvfb`/`ac6recomp` résiduel** trois
secondes après (`pgrep -af "Xvfb :251|ac6recomp.*r493-timeout-test"`
vide). Avant le correctif, ce scénario exact laissait les deux
processus actifs (constaté indépendamment par r482 et r492).

## Non établi

- **Si ce correctif change quoi que ce soit à la contention hôte
  elle-même** (charge externe mesurée par r489) — non lié, cause
  différente ; ce correctif élimine seulement l'accumulation de
  processus fantômes que les tentatives échouées laissaient
  derrière elles.
- **Si d'autres signaux externes** (ex. `SIGKILL` direct, déjà
  documenté comme inévitable par nature — r479) restent hors de
  portée de ce correctif — oui, par construction : `SIGKILL` ne peut
  jamais être intercepté par aucun processus. Seul `SIGTERM` (ce que
  `timeout` envoie par défaut) est couvert.

## Décisions prises

- Corriger `tools/ac6-oracle-run.py` (racine du dépôt, partagé par
  `run_gate.py` et l'exécution oracle directe) plutôt que
  d'ajouter un correctif localisé à chaque appelant — un seul point
  d'installation du gestionnaire couvre tous les usages futurs de ce
  runner.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` — hors
  périmètre, aucun rapport avec ce correctif.
- Réutiliser le chemin `KeyboardInterrupt` existant plutôt
  qu'écrire un nouveau bloc de nettoyage dupliqué — plus sûr, moins
  de code à maintenir.

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

## Named for r494

Ce correctif rend les futures tentatives de capture oracle plus
sûres à répéter (plus besoin de nettoyage manuel après un
`timeout`), mais ne change rien à la contention hôte elle-même
(r489) ni à la friabilité de la capture qui en découle. Reste
ouvert : les 3 états `(nuanceur, primitive_type)` de r478, toujours
non capturés, bloqués sur une fenêtre de capture propre.

## Files

Committé : `tools/ac6-oracle-run.py` (correctif), ce rapport,
`NEXT.md`. Scratch de test (`/fastdata/lavaulta/tmp/r493-timeout-test/`)
supprimé après vérification, non conservé.
