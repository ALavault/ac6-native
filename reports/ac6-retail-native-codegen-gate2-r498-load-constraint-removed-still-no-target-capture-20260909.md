# AC6 retail NTSC-U/J — r498 — contrainte de charge levée sur décision utilisateur explicite ; 3 tentatives directes, toujours aucune capture des 3 cibles, RAM confirmée saine tout du long

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (poursuite directe de la piste A).

## Contexte

Décision utilisateur explicite reçue par la session parente pendant
r497 : abandonner la contrainte de charge CPU/GPU comme condition de
lancement (« Enlève la contrainte de charge » / « La seule limite est
sur la RAM »). RAM disponible vérifiée saine avant ce cycle (`free -m`
→ **105 Go disponibles sur 124 Go total**, colonne « disponible » qui
tient compte du cache récupérable — pas la colonne « libre », trompeuse
sur Linux). Ce cycle tente directement la capture des 3 états cibles
de r478 (`09dd1c7cddae1141`→primitive_type=0x01,
`57b8e5f14b93cff4`→primitive_type=0x08, `4dd456c4ea0923c1` absent),
sans mesure de contention préalable.

## Établi — 3 tentatives, toutes échouées, RAM inchangée tout du long

`python3 tools/run_gate.py --diagnostic-route
routes/us-pretype28-startup.steps --mission-dump-shaders --output
<scratch> --display <N>`, sous `timeout 240`, trois fois de suite
(affichages `:293`/`:294`/`:295`) :

1. **Tentative 1** : timeout complet, aucun fichier de nuanceur dumpé.
2. **Tentative 2** : timeout complet, aucun fichier de nuanceur dumpé.
3. **Tentative 3** : timeout atteint pendant un `sleep(4)` interne
   (`wait_log` → `sleep`, `tools/ac6-oracle-run.py:535/458`) — **avant**
   d'atteindre `type28=30`. 5 nuanceurs dumpés
   (`0A6D1DD7767FDF27`, `2E372EA28CC404B7`, `472913F460D4B446`,
   `8F1C48BA92C8E43E`, `C049A8C9E556F129`) — 3 déjà connus depuis r479
   (lancement direct sans entrée), 2 jamais vus auparavant dans cette
   campagne, mais **aucun des 3 ne correspond aux cibles**. Aucune
   correspondance avec `parse_capsule()` nécessaire — les digests ne
   correspondent à aucune des trois cibles avant même de tenter une
   traduction.

**Le correctif de nettoyage r493/r494 est confirmé fonctionnel une
nouvelle fois** : `pgrep -af "ac6recomp|Xvfb"` vide après chacune des
trois tentatives — aucun processus résiduel, malgré le `timeout 240`
qui a interrompu chacune. Une trace Python complète du
`KeyboardInterrupt` levé par `_raise_keyboard_interrupt_on_sigterm`
confirme le chemin exact.

**Un incident opérationnel corrigé en cours de cycle** : la première
tentative de lancement a été mal exécutée (double mise en arrière-plan
par erreur — un `&` interne combiné avec `run_in_background`), ce qui
a orphelinisé un premier processus qui a continué à tourner de façon
non suivie pendant plusieurs minutes avant d'être détecté et nettoyé
(répertoire de sortie partiellement rempli, sans `RESULT.json`).
Corrigé en relançant proprement (un seul niveau de mise en arrière-plan
par tentative) ; le répertoire orphelin a été supprimé sans être
utilisé comme donnée.

## Non établi

- **Si l'absence de contrainte de charge a changé quoi que ce soit** —
  les 3 tentatives ont échoué de la même manière (timeout avant
  `type28=30`) que les cycles précédents sous charge CPU/GPU élevée
  (r492/r494/r495/r496). Aucune preuve que retirer la contrainte a
  amélioré ou dégradé le taux de réussite sur cet échantillon de 3.
- **Le bug `AudioRuntime` de r495** — non revu ce cycle (aucune
  tentative n'a atteint ce point).
- **La cause réelle de la variance de démarrage** — toujours non
  identifiée avec certitude ; r489 (charge CPU), r497 (contention GPU)
  et maintenant ce cycle (RAM saine, charge non contrôlée) ont chacun
  mesuré une dimension sans obtenir une preuve causale complète.

## Décisions prises

- Respecter la nouvelle instruction utilisateur : ne plus gate les
  tentatives sur la charge CPU/GPU, vérifier seulement la RAM
  disponible avant de lancer.
- Ne pas fusionner les 5 nuanceurs dumpés par la tentative 3 — aucun
  ne correspond aux cibles, bénéfice nul.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` — vérifié
  inchangé avant et après ce cycle.
- Ne pas tenter une 4e capture ce cycle — la limite de 3 tentatives
  autorisées par la directive est atteinte ; nouvelle tentative
  laissée au cycle suivant sur décision de la session parente.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché).
`git status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé.

## Named for r499

Retirer la contrainte de charge n'a pas, sur cet échantillon de 3,
changé le résultat. Candidats pour la suite :
1. Répéter avec un échantillon plus large (5-10 tentatives) pour
   distinguer un effet réel d'une simple variance déjà documentée
   (r482/r489) — coût en temps mais pas en RAM (confirmée abondante).
2. Reprendre la piste `AudioRuntime` de r495 (lecture statique du code
   de fermeture du thread audio) — indépendante de toute tentative de
   capture.
3. Lire directement le chemin de code autour du `wait_log`/`sleep(4)`
   où la tentative 3 a expiré, pour vérifier si le blocage à cette
   étape précise est nouveau ou déjà documenté sous un autre nom par
   un cycle antérieur.

## Files

Committé : ce rapport, `NEXT.md`. Scratch
(`/fastdata/lavaulta/tmp/r498-*`, incluant le répertoire orphelin de
l'incident opérationnel) nettoyé, non conservé. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
