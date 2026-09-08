# AC6 retail NTSC-U/J — r418 — `presented_frames=0` est du câblage mort déjà documenté (r292) ; sur le binaire corrigé par r414, l'anneau GPU ne se tait plus après une seule salve (r293 réfuté par le nouveau comportement) mais n'émet toujours AUCUN paquet de présentation ; un nouveau blocage d'arrêt apparaît avec `AC6_NATIVE_VD_TRACE=1`

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r417 (nommé pour r418) : lire `sub_821D7AE0`/`sub_821D7CD0`
pour localiser où `presented_frames` devrait s'incrémenter et pourquoi
il reste `0`.

## Correction méthodologique avant d'aller plus loin

Avant de lire le PPC invité, une recherche côté hôte (`grep
presented_frames`) a directement répondu à la question — et révélé
qu'elle avait déjà été répondue par un cycle antérieur au fil r399
(**r292**, `artifacts/retail-us-native-r292-gpu-drain-check/`) :
**`diagnostics_.presented_frames` n'est écrit QUE par
`NativeRuntime::submit_ring()`, et le seul appelant de cette fonction
dans tout l'arbre est `native/tests/native_runtime_tests.cpp`.** Le
runtime réel ne l'appelle jamais. Ce diagnostic est du câblage mort,
confirmé indépendamment ce cycle par la même recherche. Lire
`sub_821D7AE0`/`sub_821D7CD0` n'aurait rien changé à cette conclusion
— nommé par r417 sur une fausse piste, corrigé ici avant d'investir
plus de budget dans une lecture PPC qui n'était pas nécessaire.

## Établi

### Le vrai chemin GPU (déjà identifié par r292) montre un changement net depuis le correctif de r414

`NativeGuestVdService::drain_locked()` (`native/src/native_guest_vd.cpp`)
est le chemin réel, piloté par un sondeur en arrière-plan à 1ms. r293
avait établi que ce chemin s'arrêtait après une brève salve initiale
("the mechanism working exactly as designed against a guest that has
stopped publishing" — PAS un bug hôte, le jeu invité arrêtait
lui-même de publier). r294 avait mesuré cette salve : **5** « vd drain
accepted », dont un burst de rendu réel (28 tirages cumulés), mais
**`present=0` dans chaque salve**.

Rejoué ce cycle (`AC6_NATIVE_VD_TRACE=1`, fenêtre `25000ms`, binaire
reconstruit par r414) :

```
vd drain accepted : 11 occurrences (contre 5 avant le correctif)
vd swap entry/check/publish : 5 occurrences, write_index en
  progression continue (25 -> 43 -> 49 -> 55 -> 67 -> 73)
present=0 dans les 11 salves, sans exception
```

**L'anneau ne se tait plus après une seule salve** — il continue de
publier et d'être drainé pendant toute la fenêtre observée, une
augmentation nette (11 contre 5) cohérente avec le boot qui progresse
désormais bien plus loin (r416/r417). **Mais aucun paquet `Present`
n'est jamais émis**, à travers 11 salves : `presented_frames` resterait
à `0` même si le câblage mort de r292 était réparé.

### Nouveau blocage observé : le processus ne se termine plus dans un délai raisonnable avec `AC6_NATIVE_VD_TRACE=1` activé

Le message `ac6recomp: generated entry terminated its own thread`
(le mécanisme d'arrêt propre confirmé légitime par r417) s'affiche
normalement en fin de trace — **mais le PROCESSUS lui-même continue de
consommer du CPU (`171%`, multi-thread) pendant encore ~100-150
secondes au-delà de la fenêtre de sonde configurée (`25000ms`)**, un
`timeout 40` (secondes) n'ayant PAS suffi à l'arrêter (le processus a
ignoré ou survécu au signal envoyé par `timeout`). Terminé de force
(`kill -9`) après constat. **Ceci n'a pas été observé lors des runs
sans `AC6_NATIVE_VD_TRACE`** (r414-r417, sorties `Inferior ... exited
normally` en quelques secondes après la fenêtre) — la corrélation avec
ce drapeau de trace spécifiquement n'est PAS confirmée par une capture
de contrôle isolant cette seule variable (non fait ce cycle, budget de
cycle épuisé sur les deux constats précédents).

## Ce que ceci établit

**`presented_frames=0` n'est PAS un blocage de la chaîne r399-r417**,
c'est du câblage mort déjà nommé par r292, sans rapport avec
l'allocateur ou le boot. Le chemin GPU réel (`NativeGuestVdService`)
montre une amélioration mesurable et cohérente avec r414
(l'anneau continue d'être alimenté au lieu de se taire), mais reste
bloqué avant tout paquet de présentation, pour une raison non
identifiée par ce cycle. **Un nouveau symptôme, sans rapport en
apparence avec tout ce qui précède, est observé** : un arrêt de
processus anormalement long avec le drapeau de trace GPU activé.

## Non établi

- **Pourquoi aucun paquet `Present` n'est jamais émis** — ni le code
  invité qui produirait ce paquet, ni la raison de son absence, n'ont
  été examinés ce cycle.
- **Si le nouveau blocage d'arrêt observé avec `AC6_NATIVE_VD_TRACE=1`
  est réellement provoqué par ce drapeau**, par le volume de trace
  plus élevé qu'avant (câblage plus actif), ou par autre chose —
  aucune capture de contrôle isolant la variable n'a été faite.
- **Si ce blocage d'arrêt existait déjà avant le correctif de r414**
  (jamais testé avec `AC6_NATIVE_VD_TRACE=1` sur l'ancien binaire dans
  ce cycle) — non comparé.
- Aucun correctif proposé ni appliqué ce cycle.

## Décisions prises

- Corriger sa propre commande nommée par r417 (lire le PPC invité)
  avant de l'exécuter, dès qu'une recherche côté hôte bien moins
  coûteuse a répondu à la question et révélé qu'elle était déjà
  répondue par r292 — éviter un travail redondant plutôt que de
  suivre la lettre de la piste nommée sans la revérifier d'abord
  (précédent CLAUDE.md : corriger soi-même, par nom et numéro de
  cycle).
- Terminer de force le processus bloqué plutôt que d'attendre
  indéfiniment ou de relancer une capture de contrôle ce cycle — noter
  le fait, ne pas le creuser sans un cycle dédié (précédent
  r1111/r1113 : ne pas deviner sans preuve).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r419

Deux forks indépendants, le moins cher d'abord :
1. Isoler si le nouvel arrêt de processus long est spécifique à
   `AC6_NATIVE_VD_TRACE=1` (rejouer SANS ce drapeau, avec une fenêtre
   identique, et mesurer le temps de sortie du processus) avant de le
   qualifier de blocage réel.
2. Si confirmé indépendant du drapeau de trace : tracer où le paquet
   `Present` invité serait censé être émis (probablement dans
   `sub_821D7AE0`/`sub_821D7CD0`, jamais lus) et pourquoi il ne l'est
   jamais dans la fenêtre observée.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r418-vd-trace-recheck/r418_vd_trace.log`.
