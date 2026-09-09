# AC6 retail NTSC-U/J — r502 — corrige le bug de nettoyage SIGTERM ré-entrant trouvé par r501 : un second SIGTERM pendant le nettoyage abandonnait `terminate_owned()` en cours de route

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé pour le correctif lui-même ; un test direct par
envoi de plusieurs `SIGTERM` réels au processus wrapper a été exécuté
pour vérifier l'absence de régression.

## Contexte

Nommé par r501 : la tentative 3 de son cycle a reçu un second
`SIGTERM` alors que `close()` était déjà en cours de nettoyage suite
à un premier `SIGTERM` — le gestionnaire installé par r493/r494
relance inconditionnellement `KeyboardInterrupt` à CHAQUE `SIGTERM`
reçu, y compris pendant que `terminate_owned()` (qui envoie déjà sa
propre séquence `SIGINT`/`SIGTERM`/`SIGKILL` via `os.killpg`) est en
plein milieu de son propre `process.wait(timeout=...)`. La nouvelle
`KeyboardInterrupt` relancée interrompt cette attente, `close()` ne
finit jamais d'atteindre `terminate_owned(self.xvfb)` (appelé après
`terminate_owned(self.game)` dans le code), laissant `Xvfb` orphelin.

## Établi — cause exacte

`_raise_keyboard_interrupt_on_sigterm()` (ligne ~730) ne modifiait
jamais sa propre disposition de signal après son premier
déclenchement — chaque `SIGTERM` reçu, y compris pendant le
déroulement du `finally: runner.close()` qu'il a lui-même déclenché,
relance une NOUVELLE `KeyboardInterrupt`, qui remonte la pile
d'exécution EN COURS (potentiellement au milieu de
`terminate_owned()`), abandonnant le nettoyage à mi-chemin.

## Correctif appliqué et vérifié

Le gestionnaire installe `signal.signal(signal.SIGTERM,
signal.SIG_IGN)` comme toute première action, avant de relancer
`KeyboardInterrupt`. Ceci est sûr : `terminate_owned()` a déjà sa
propre escalade bornée
(`SIGINT`/`SIGTERM`/`SIGKILL`, avec timeouts 3s/2s/2s par étape,
lignes ~283-296) — ignorer les `SIGTERM` externes supplémentaires
pendant cette fenêtre ne bloque rien, ça laisse simplement cette
escalade déjà bornée se terminer sans interruption externe.

**Vérifié** : lancement réel de `tools/run_gate.py` (route
`us-pretype28-startup.steps`), confirmation qu'`ac6recomp` tournait
bien avant d'envoyer 3 `SIGTERM` successifs au processus wrapper
(à ~1s d'intervalle). Aucun processus résiduel après (`pgrep -af
"Xvfb|ac6recomp"` vide). **Non établi** : le test manuel n'a pas
réussi à forcer la fenêtre de course exacte (le nettoyage se termine
trop vite dans cet environnement pour que le second signal arrive
authentiquement pendant un `process.wait()` en cours — chaque envoi
successif de `SIGTERM` a trouvé le processus déjà terminé). La
correction repose donc sur un raisonnement direct du mécanisme
(`SIG_IGN` posé de façon atomique avant toute relance
d'exception — la sémantique POSIX/Python garantit qu'aucun signal
ultérieur ne peut ré-entrer le gestionnaire une fois la disposition
changée) plutôt que sur une reproduction empirique de la race exacte
que r501 a rencontrée — cohérent avec la discipline de ce dépôt
(citer le mécanisme, pas seulement observer un résultat) mais à
noter explicitement comme preuve indirecte.

## Décisions prises

- Corriger `tools/ac6-oracle-run.py` (même fichier que r493/r494,
  seul point d'installation partagé par les deux points d'entrée).
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/`.
- Documenter honnêtement que le test manuel n'a pas reproduit la
  race exacte plutôt que de prétendre une vérification empirique
  complète.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché). `git
status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé.

## Named for r503

Ce correctif ferme le troisième bug trouvé dans la chaîne de
nettoyage `SIGTERM` de ce cycle de campagne (r493 : jamais appelé du
tout ; r494 : installé au mauvais endroit ; r502 : ré-entrant). Les 3
états cibles de r478 restent non capturés après 6 cycles/~15
tentatives (r492-r498, r501). Reste ouvert, sans reprise décidée.

## Files

Committé : `tools/ac6-oracle-run.py` (correctif), ce rapport,
`NEXT.md`. Scratch de test
(`/fastdata/lavaulta/tmp/r502-doubleterm-test*`) supprimé après
vérification, non conservé.
