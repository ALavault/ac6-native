# AC6 retail NTSC-U/J — r501 — sixième cycle de capture (3 tentatives), toujours aucune donnée fusionnable ; le correctif `AudioRuntime` de r500 n'a pas pu être testé (aucune tentative n'a atteint l'arrêt propre) ; nouveau bug trouvé : un second `SIGTERM` pendant `close()` interrompt le nettoyage lui-même

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (3 tentatives de capture directes, aucune contrainte de
charge — décision utilisateur de r498, RAM vérifiée saine avant
départ : 105 Go disponibles sur 124 Go).

## Contexte

Nommé par r500 : vérifier par une capture réelle si le correctif
`AudioRuntime` (revérification de `worker_running_` entre clients,
commit `2e79f3f4` dans le sous-module `AC6_recomp`) réduit le blocage
observé une fois par r495. Aucune contrainte de charge appliquée
(décision utilisateur de r498, toujours en vigueur).

## Établi — 3 tentatives, aucune n'atteint l'arrêt propre, le correctif `AudioRuntime` n'a donc pas pu être testé

- **Tentative 1** (`--display :301`) : timeout complet (240s), aucun
  nuanceur capturé, aucun `RESULT.json`. Trace : interruption au même
  point exact que r498 — `wait_log()` →
  `self.sleep(4)` (`tools/ac6-oracle-run.py:535` puis `:458`) →
  `KeyboardInterrupt` levée par le gestionnaire `SIGTERM` installé en
  r493/r494. Aucun processus résiduel (nettoyage propre confirmé).
- **Tentative 2** (`--display :302`) : **NOUVEAU** — la trace montre
  le `KeyboardInterrupt` cette fois levé DANS `terminate_owned()`
  lui-même, à la ligne `process.wait(timeout=timeout)` de
  `tools/ac6-oracle-run.py:294` (appelé depuis `close()`, ligne 686,
  pour `self.game`). Un DEUXIÈME `SIGTERM` est donc arrivé pendant que
  le nettoyage normal (`close()`, déclenché par le PREMIER `SIGTERM`)
  était déjà en cours d'exécution — et comme le gestionnaire réinstallé
  par r493/r494 lève `KeyboardInterrupt` sans condition, cette
  deuxième levée a interrompu `close()` avant qu'il ait fini de tuer
  `self.game`, et AVANT même d'atteindre `terminate_owned(self.xvfb)`.
  **Résultat : `Xvfb` et `ac6recomp` sont restés actifs après la fin
  du wrapper `run_gate.py`**, nécessitant un nettoyage manuel
  (`kill -9`) — un vrai régression par rapport au comportement attendu
  de r493/r494, dans un cas que ces cycles n'avaient pas envisagé (une
  interruption PENDANT le nettoyage, pas seulement pendant l'exécution
  normale). 2 nuanceurs capturés avant l'interruption
  (`C049A8C9E556F129`, déjà connu depuis r479 — pas une des 3 cibles).
- **Tentative 3** (`--display :303`) : timeout complet, même point
  d'interruption exact que la tentative 1 (`sleep(4)` →
  `KeyboardInterrupt`), aucun processus résiduel cette fois. 9
  nuanceurs uniques capturés (`0A6D1DD7767FDF27`, `1899F02DC6758D8F`,
  `25ED986FB3F8E797`, `2E372EA28CC404B7`, `472913F460D4B446`,
  `8F1C48BA92C8E43E`, `BBAADA3605B82C5A`, `C049A8C9E556F129`,
  `EA41C0069AE03769`) — tous déjà connus des cycles précédents
  (r479/r481), **aucun ne correspond aux 3 cibles de r478**
  (`09dd1c7cddae1141`, `57b8e5f14b93cff4`, `4dd456c4ea0923c1`).

**Aucune des 3 tentatives n'a atteint `type28=30` ni l'arrêt propre** —
le correctif `AudioRuntime` de r500 reste donc non vérifié par une
capture réelle, faute d'avoir atteint le point où il s'appliquerait.

## Non établi

- **Si le correctif `AudioRuntime` fonctionne** — toujours non testé,
  aucune tentative de ce cycle n'a atteint l'arrêt propre.
- **La cause du deuxième `SIGTERM`** observé en tentative 2 — un seul
  `timeout 240` a été utilisé par tentative, qui n'envoie qu'un seul
  `SIGTERM` par défaut (pas de `--kill-after`). Le second signal
  pourrait venir d'ailleurs (le harnais parent, un signal du groupe de
  processus lui-même, ou une relance du `SIGTERM` par un mécanisme non
  identifié) — non déterminé ce cycle.

## Décisions prises

- Nettoyer manuellement les processus résiduels de la tentative 2
  (`kill -9`) plutôt que les laisser tourner.
- Ne PAS tenter de corriger le bug de réentrance `SIGTERM`/`close()`
  ce cycle — c'est une découverte, pas encore caractérisée
  suffisamment pour un correctif sûr (une seule observation, cause du
  second signal inconnue). Nommé pour la suite plutôt que corrigé à la
  hâte.
- Ne PAS fusionner dans le registre épinglé — aucun nuanceur cible
  capturé par aucune des 3 tentatives.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/`.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché). `git
status --porcelain` sous `recompilation/ace-combat-6-retail/native/`
confirmé inchangé avant et après ce cycle (mêmes 7 fichiers modifiés
par la session concurrente, aucun changement introduit ici).

## Named for r502

**Six cycles de capture (r492, r494, r495, r496, r498, r501), environ
15 tentatives individuelles, toujours aucune capture des 3 états
cibles de r478.** Reste ouvert :
1. Le bug de réentrance `SIGTERM` pendant `close()` (nouveau, trouvé
   ce cycle) — mériterait une garde simple (ex. ignorer les
   `SIGTERM` reçus une fois `close()` déjà entamé) avant d'être
   considéré fiable pour de futures tentatives, mais nécessite d'abord
   de comprendre la source du second signal.
2. Le correctif `AudioRuntime` de r500 reste non vérifié — nécessite
   une tentative qui atteint réellement l'arrêt propre.
3. Le blocage systématique à `sleep(4)`/`wait_log()` (2 tentatives sur
   3 ce cycle, cohérent avec r498) reste sans cause identifiée malgré
   six dimensions de contention/minutage explorées.

## Files

Committé : ce rapport, `NEXT.md`. Scratch
(`/fastdata/lavaulta/tmp/r501-*`) nettoyé, non conservé. Aucun fichier
sous `recompilation/ace-combat-6-retail/native/` modifié.
