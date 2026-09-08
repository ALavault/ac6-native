# AC6 retail NTSC-U/J — r467 — deux tentatives gdb en direct échouent sur des obstacles d'environnement distincts ; un troisième lancement, SANS gdb, révèle que le motif « movie worker » lu par r466 comme le symptôme du blocage tourne en fait dès la première seconde après le boot — correction de r466, cause racine toujours ouverte

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle tenté (lancements directs du binaire oracle `hybrid_backend_fixes`), non concluant sur la cause racine.

## Contexte

Nommé par r466 : une session gdb en direct pour déterminer lequel des
deux objets d'attente du « movie worker » (`0x82916E2C`/`0x82916E08`)
reste en permanence signalé, et pourquoi — la seule pièce manquante
pour clore l'investigation du blocage de transition monde/campagne qui
bloque la campagne de couverture du registre de nuanceurs épinglé
(r454-r466).

## Établi — deux tentatives gdb séparées, deux obstacles distincts, aucune n'atteint la cause racine

**Tentative 1** : point d'arrêt CONDITIONNEL sur `is_ac6_movie_worker_wait`
à la ligne exacte de `KeWaitForMultipleObjects`
(`thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_threading.cpp:1005`).
Lancée en arrière-plan, tuée après 12+ minutes sans jamais toucher le
point d'arrêt. Le site est appelé en boucle serrée (r466 : 159 644 fois
sur le run précédent) ; l'évaluation de la condition côté débogueur à
CHAQUE appel — même quand elle est fausse — impose un arrêt/reprise
`ptrace` par appel, rendant l'approche impraticable à cette fréquence.

**Tentative 2** (préparée par le fork précédent avant cette reprise,
jamais committée) : deux obstacles supplémentaires caractérisés —
`ptrace_scope=1` sans privilège root ferme l'attachement à un processus
lancé normalement par `run_gate.py` ; un lancement DIRECT sous gdb
(seule voie d'attachement sans root) révèle un VRAI `SIGSEGV`
reproductible dans `rex_sub_821E4378` (code PPC recompilé réel), jamais
observé en lancement normal — cohérent avec un mécanisme de signal
interne au moteur que l'interception gdb du `SIGSEGV` casse ;
`handle SIGSEGV nostop noprint pass` contourne le crash mais ralentit
l'exécution au point de dépasser le budget de temps d'une tâche
d'arrière-plan du harnais, qui tue le processus avant d'atteindre le
point d'arrêt cible.

## Établi — une CORRECTION de r466, trouvée en contournant plutôt qu'en résolvant les deux obstacles ci-dessus

Troisième lancement : `ac6recomp` exécuté DIRECTEMENT, sans aucun
gdb attaché, sur le même serveur Xvfb déjà en place. Le journal
(`ac6recomp.log`, flush immédiat) montre, DÈS `00:44:50` — la toute
première fraction de seconde après le boot, avant tout chargement de
monde ou toute cinématique — le motif exact que r466 avait lu comme
LE symptôme du blocage :
```
[KeSetEvent] AC6 movie worker event ptr=82916E3C increment=1 wait=0
[KeWaitForMultipleObjects] AC6 movie worker wait result=0 objects=[82916E2C 82916E08] wait_type=1 reason=3 mode=1 alertable=0 timeout=0000000000000000
```
répété en boucle serrée, à un rythme de l'ordre de la milliseconde,
**identique en forme aux 159 644 occurrences que r466 avait
mesurées sur l'ensemble d'un run bloqué**.

**Ceci corrige r466** : cette boucle n'est PAS un symptôme spécifique
à l'état bloqué — elle tourne en continu depuis le tout début du
processus, dans un état où le jeu n'a même pas encore chargé quoi que
ce soit. Un thread réellement « coincé » n'expliquerait pas sa
présence identique dès l'instant zéro. Le motif ressemble beaucoup
plus à un sondage normal par tick (le movie worker vérifie s'il a du
travail à chaque frame, trouve l'événement déjà signalé, et
recommence) qu'à un thread bloqué sur un objet qui ne se libère
jamais. **La piste « movie worker figé » que r466 avait mise en avant
est donc affaiblie par cette preuve directe.**

## Non établi

- **La cause racine réelle du blocage de transition monde/campagne**
  (pourquoi `[ac6-visual-phase] world=1` n'est jamais atteint après la
  cutscene) — toujours inconnue. La boucle du movie worker n'étant
  probablement pas la bonne piste, la prochaine investigation doit
  regarder ailleurs (ce qui gouverne cette activation elle-même).
- **La cause du `SIGSEGV` spécifique à gdb dans `rex_sub_821E4378`**
  (tentative 2) — caractérisée mais non expliquée ; potentiellement
  un mécanisme de signal interne au moteur (ex. un handler SEH-like
  ou une détection d'exception vectorielle) qui suppose l'absence
  d'un débogueur observateur.

## Décisions prises

- **Ne pas retenter l'approche par point d'arrêt conditionnel** sur ce
  site — le coût par appel la rend impraticable à cette fréquence
  d'invocation ; toute future tentative gdb devrait cibler un site
  moins fréquemment appelé (par exemple là où `world=1` DEVRAIT être
  positionné, plutôt que la boucle de sondage elle-même).
- **Nettoyage complet effectué** : tous les processus `ac6recomp`/
  `gdb`/`Xvfb` orphelins des trois tentatives ont été tués, les
  répertoires temporaires (`/fastdata/lavaulta/tmp/ac6-r467-gdb-*`)
  supprimés.
- **Aucune reconstruction ce cycle** : le profil `native` n'a été
  touché à aucun moment (les trois tentatives n'ont fait que lancer
  le binaire oracle déjà compilé) — pas de vérification `ctest`/sonde
  nécessaire pour ce cycle spécifique (état inchangé par construction).

## Gate

Aucune source de production modifiée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production touchée ce
cycle).

## Named for r468

La vraie cause du blocage de transition monde/campagne reste à
trouver ailleurs que dans la boucle du movie worker : regarder ce qui
gouverne l'activation `[ac6-visual-phase] world=1` elle-même (site
d'écriture de ce flag, ou de la structure d'état dont il dérive)
plutôt que ce site de wait précis, à travers une recherche statique
(xrefs Ghidra sur le projet `ac6-us`) plutôt qu'un nouveau lancement
gdb long sur un site à haute fréquence. Reste ouvert, non bloquant :
(1) le trou de couverture du registre épinglé (r456) reste la piste
principale une fois ce blocage résolu ; (2) une fois le câblage
`PinnedShaderRuntime` jugé mûr, reconsidérer le committage groupé de
l'arriéré natif (r433/r434/r438/r454-r462) ; (3) mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut du
profil natif (confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau conservé (répertoires temporaires
sous `/fastdata/lavaulta/tmp/ac6-r467-gdb-*`, supprimés).
