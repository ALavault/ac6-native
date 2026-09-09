# AC6 retail NTSC-U/J — r489 — la charge hôte mesurée explique directement la « flakiness » de capture : 2/2 tentatives échouent sous une charge système de 44-52 sur 32 cœurs, dominée par deux jobs sans rapport avec ce dépôt

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (deux lancements diagnostiques, aucune modification du
jeu ni du produit natif).

## Contexte

Nommé par r488, piste 1 (la plus directe une fois la piste « movie
worker » définitivement close) : mesurer directement la contention de
charge hôte pendant un cycle de capture, plutôt que de continuer à
deviner sur le minutage des routes.

## Établi — la charge système est massivement sursouscrite, par des processus sans rapport avec ce dépôt

`nproc` = 32 cœurs. `uptime` au moment du lancement du premier run :
`load average: 49.76, 49.60, 48.86` — soit ~1,5× le nombre de cœurs
physiques, en régime soutenu (moyennes 1/5/15 min quasi identiques,
donc pas un pic transitoire).

`ps aux --sort=-%cpu` identifie la source : deux processus
**totalement indépendants de ce dépôt** dominent :
- PID 1957322 : `.venvs/blackwell_torch_cu128/bin/python
  scripts/generated_image_extract_frozen_features.py` — **2261 % CPU**
  (~22,6 cœurs), actif depuis 08:45, déjà 1636 minutes de temps CPU
  cumulé.
- PID 1953956 : `.venvs/blackwell_torch_cu128/bin/python -m
  scripts.signalshield_v3_s23_cal2_freeze` — **533 % CPU** (~5,3
  cœurs), actif depuis 08:41.

Ces deux processus consomment à eux seuls ~28 des 32 cœurs. Un
troisième, plus ancien (PID 3208666, `neural_amp/.venv/.../
run_teacher_v10_confirmation.py`, actif depuis le 7 septembre) ajoute
~1,1 cœur. Rien de tout cela n'appartient à la campagne AC6 ni à la
session concurrente `native/` (r488+) déjà identifiée — ce sont des
charges de travail sans rapport, partageant la même machine physique.

## Établi — corrélation directe entre charge et échec de capture

Deux lancements de `routes/us-pretype28-startup.steps` (la route que
r481 avait rapportée fiable, ~56 s, dans un contexte de charge non
mesuré à l'époque) :

| run | charge moyenne pendant l'exécution | issue |
|---|---|---|
| 1 | 44-52 (échantillonné à 8s, 15s, 30s d'intervalle sur toute la durée) | **timeout à 240s, aucun `RESULT.json`** |
| 2 | 44-52 (même plage) | **timeout à 240s, aucun `RESULT.json`** |

**2/2 échecs**, sous une charge quasi constante bien au-dessus de la
capacité du host. `ac6recomp` lui-même n'est pas figé — mesuré à
103-132 % CPU en continu tout le long du run 1 (donc actif, pas
bloqué en attente) — mais progresse lentement car il doit partager le
CPU avec ~28 cœurs d'activité concurrente sans rapport, sur seulement
32 disponibles. C'est cohérent avec le fait, déjà noté par r480, que
le processus reste actif (145-238 % CPU) sans jamais produire de
contenu exploitable en 700+ secondes réelles.

`run_gate.py` lui-même prend plus de 50 s avant même de lancer le
binaire `ac6recomp` (mesuré : le sous-processus `ac6recomp`
n'apparaît dans `ps` qu'après ~54 s d'attente sur le wrapper Python) —
un délai de lancement anormalement long pour un script qui ne fait
rien de coûteux avant d'appeler `subprocess`, cohérent avec la
contention de l'ordonnanceur CPU plutôt qu'un traitement interne.

## Non établi

- **Le comportement exact sous charge FAIBLE** — aucun run n'a été
  tenté dans une fenêtre de charge basse ce cycle (les deux jobs
  externes sont restés actifs en continu pendant toute la durée de
  l'investigation). L'hypothèse que la route redevient fiable une fois
  la charge externe retombée n'est donc pas directement vérifiée ici,
  seulement rendue plausible par corrélation et par le contraste avec
  le rapport original de r481 (56 s, contexte de charge non mesuré
  mais présumé plus faible).
- **Si la contention seule explique TOUT** le spectre observé
  depuis r479 (échecs francs à 240s, mais aussi le run de 700+
  secondes de r480 qui reste actif sans jamais aboutir) — plausible
  mais pas prouvé formellement distinct d'un éventuel second facteur.

## Décisions prises

- Ne pas relancer d'autre tentative de capture ce cycle — les deux
  runs sont déjà un échantillon suffisant pour établir la corrélation
  qualitative demandée par r488 (charge très élevée + 2/2 échecs),
  et chaque tentative supplémentaire sous cette même charge
  n'apporterait pas d'information nouvelle.
- Ne pas toucher aux deux processus externes (`generated_image_extract_frozen_features.py`,
  `signalshield_v3_s23_cal2_freeze`) — ils appartiennent à un autre
  travail de l'utilisateur sur cette machine partagée, hors périmètre
  de ce dépôt et de cette campagne.
- Nettoyage des processus `ac6recomp`/`Xvfb` orphelins après chaque
  timeout (même fuite que r482/r486 : `timeout` ne tue pas les
  enfants détachés de `Xvfb`) — `pgrep -af "ac6recomp|Xvfb"` confirmé
  vide après nettoyage, aucun résidu.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés. `native/` non touché ce cycle (mesure
diagnostique uniquement, aucun besoin de build natif).

## Named for r490

**La contention de charge hôte est maintenant établie comme un
facteur direct et mesuré**, pas une hypothèse. Pistes ouvertes :
1. Réessayer une capture après que les deux jobs externes identifiés
   se terminent ou libèrent des cœurs — vérifierait directement si la
   fiabilité originale de r481 (~56 s) revient une fois la contention
   levée. Non actionnable unilatéralement (dépend du travail d'un
   autre processus utilisateur, hors du contrôle de cette campagne).
2. Si la contention persiste, envisager d'augmenter le `timeout` des
   routes diagnostiques (actuellement 240s) pour absorber le
   ralentissement plutôt que d'échouer — un contournement, pas une
   correction, mais utile pour continuer à capturer des données utiles
   sous charge.
3. Piste `fetch_const` du HUD de vol (r475), toujours indépendante et
   jamais suivie — n'est pas affectée par ce facteur de charge de la
   même manière (analyse statique, pas de lancement oracle).

## Files

Committé : ce rapport, `NEXT.md`. Aucun script réutilisable produit
ce cycle (mesure shell directe, pas de nouvel outil). Non conservé
(scratch, `/fastdata/lavaulta/tmp/r489-load/`) : logs de console des
deux runs, supprimés après extraction des données ci-dessus. Aucun
fichier sous `recompilation/ace-combat-6-retail/native/` modifié.
