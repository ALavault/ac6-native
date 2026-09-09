# AC6 retail NTSC-U/J — r495 — troisième cycle consécutif d'essais bornés (r492, r494, r495) échoue à capturer les 3 états cibles ; deux causes d'échec distinctes observées, aucune donnée fusionnable, recommande une pause plus longue plutôt qu'un quatrième essai immédiat

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (deux tentatives bornées, plafond respecté).

## Contexte

Nommé par r494, avec un délai supplémentaire de 30 minutes décidé par
la session parente avant de relancer (plutôt que de répéter
immédiatement sous une charge inchangée). Charge hôte au lancement de
ce cycle : `load average: 33,58 33,54 33,88` — la lecture la plus
basse de toute cette investigation (r489 : 40-52 ; r492/r494 : 33-45),
mais toujours élevée sur cet hôte à 32 cœurs.

## Établi — deux tentatives bornées, deux causes d'échec DIFFÉRENTES, aucune donnée fusionnable

**Tentative 1** (`--display :271`) : timeout `240s` atteint sans
jamais satisfaire le `wait-pulse` en cours — trace complète de la
pile Python confirme l'interruption au milieu de
`OracleRun.wait_log()`/`sleep()`, exactement le chemin `SIGTERM`
corrigé par r493/r494. **Aucun processus résiduel après** (`pgrep`
vide) — le correctif tient, vérifié une troisième fois en conditions
réelles.

**Tentative 2** (`--display :272`) : **atteint la cible avec succès**
(8/8 étapes exécutées, `step-08-type28-30.png` produit, 22 nuanceurs
dumpés — même compte que r481/r483 réussis). Mais
`clean_shutdown=False`, `status=fail`. Le journal `ac6recomp.log`
montre la cause précise, DIFFÉRENTE de tout ce qui a été corrigé
aujourd'hui : `AudioRuntime: worker thread did not exit within 2s,
terminating` — un blocage d'arrêt interne au thread audio de
`ac6recomp` lui-même, pas un problème de nettoyage du wrapper Python
(`terminate_owned()`/`SIGTERM`, déjà vérifié fonctionnel dans cette
même tentative). Le dump de nuanceurs produit est dans un format
(`*.ucode.bin.{vert,frag}`/`*.ucode.{vert,frag}`) qui ne correspond
pas aux entrées `--xsh`/`--xpso` attendues par
`tools/rexglue_shader_translate/parse_rexglue_cache.py` — cohérent
avec la garde fail-closed déjà documentée par r492 sur un arrêt
incomplet, confirmée à nouveau ici sans forcer de traduction.

Aucun processus résiduel après cette tentative non plus.

## Non établi

- **Si le blocage du thread audio est lié à la contention hôte** (un
  thread privé de temps CPU au moment précis de l'arrêt pourrait
  dépasser un délai de 2s codé en dur) ou **un bug indépendant** du
  runtime — non déterminé ce cycle, nécessiterait une lecture du code
  source de l'arrêt du thread audio (`AudioRuntime`), hors périmètre
  d'un cycle de capture borné.
- **Si une charge hôte plus basse (< 33) résoudrait ce blocage audio
  spécifique** — non testé, la charge n'est jamais descendue sous 33
  durant cette investigation.

## Décisions prises

- **Ne PAS committer de fusion** — aucune des deux tentatives n'a
  produit de donnée exploitable (l'une n'a rien capturé, l'autre a un
  dump dans un format incompatible avec la traduction, à raison).
- **Ne PAS tenter une quatrième reprise immédiate.** C'est le
  troisième cycle consécutif (r492, r494, r495) à plafonner à 2
  tentatives sans résultat exploitable, sous une charge hôte qui reste
  élevée (33-52 selon les cycles) malgré une légère amélioration.
  Répéter une quatrième fois sous des conditions à peine différentes
  n'apporterait probablement pas d'information nouvelle — même
  raisonnement de rendement décroissant qui a motivé les pauses de
  r480 et r485.
- **Recommandation explicite à la session parente** : soit attendre
  une baisse de charge nettement plus marquée (repère : sous ~15-20
  sur cet hôte à 32 cœurs, pas seulement sous 35) avant une nouvelle
  tentative, soit investiguer le blocage du thread audio comme piste
  distincte (lecture de code, pas un nouvel essai de capture), soit
  mettre cette piste en pause pour de bon en attendant une décision
  utilisateur différente.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés — aucune source de contrat touchée. `ctest`
natif non relancé (`native/` non touché). `git status --porcelain`
sous `recompilation/ace-combat-6-retail/native/` confirmé inchangé
(7 fichiers modifiés par l'autre session, aucun changement introduit
ici) avant et après ce cycle.

## Named for r496

**Deux pistes distinctes, aucune tentée ce cycle par manque de
budget** :
1. Lire le code source de l'arrêt du thread `AudioRuntime` pour
   comprendre le délai de 2s codé en dur et s'il peut être fiabilisé
   (pas nécessairement lié à la contention hôte).
2. Attendre une baisse de charge nettement plus marquée avant toute
   nouvelle tentative de capture — le seuil observé jusqu'ici (33-52)
   n'a jamais permis un cycle complet propre.

Reste ouvert sinon : les 3 états cibles de r478, toujours non
capturés.

## Files

Committé : ce rapport, `NEXT.md`. Aucun changement sous
`recompilation/ace-combat-6-retail/native/`. Non conservé (scratch,
`/fastdata/lavaulta/tmp/r495-*`) : dumps de nuanceurs des deux
tentatives, journaux complets, captures d'écran — supprimés après
extraction des données ci-dessus.
