# AC6 retail NTSC-U/J — r507 — une investigation antérieure (r279-r358) a déjà trouvé ET corrigé un vrai deadlock côté `native` pour ce même symptôme (un seul tirage puis blocage) ; le correctif ne se transpose PAS tel quel au produit `rexglue-oracle`, mais la méthode et la structure du problème sont directement pertinentes

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`
(ISO US, SHA-256 `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`).
Aucun oracle lancé ce cycle — lecture de code (Ghidra non nécessaire,
le mécanisme exact était déjà documenté par une campagne antérieure)
et croisement avec le code source ReXGlue déjà présent dans le dépôt.

## Contexte

Nommé par r506 : après une image présentée (~8s), le rendu s'arrête
totalement pendant 900+ secondes, un thread continuant à exécuter le
sondage non bloquant « movie worker » (r487/r488) sans jamais
progresser. r506 posait trois pistes ; celle-ci correspond à la
piste 1 (lire ce qui devrait se produire après la première image) et
à la piste 2 (comparer avec le produit natif), traitées ensemble car
elles se sont révélées liées à une découverte préexistante.

## Établi — une investigation antérieure massive (~80 cycles, r279-r358) a déjà caractérisé et corrigé EXACTEMENT ce symptôme, mais côté `native`, pas `rexglue-oracle`

En cherchant dans `recompilation/ace-combat-6-retail/artifacts/` toute
trace antérieure de « présentation unique puis blocage », une chaîne
de rapports `retail-us-native-r279` à `r358` (bien antérieure à la
chaîne r454+ de cette session, donc jamais consultée par r492-r506)
documente précisément ce symptôme pour le produit **natif** :
`presented_frames` restait à 0/1 indéfiniment. Séquence des faits
établis par cette chaîne (lue directement, pas résumée de mémoire) :

- **r301** : échantillonnage direct `/proc/$PID/task/*/{stat,wchan}`
  (pas de gdb) sur ~29 threads réels. ~15 threads tournent à ~93% CPU
  en sondage actif continu (motif « bounded polling », cohérent avec
  ce que r506 observe pour le thread « movie worker » côté
  oracle-hybride — CE N'EST PAS une anomalie, c'est le comportement
  normal de ce moteur). **Au moins un thread (celui qui compte
  réellement) est authentiquement parqué dans un `futex_do_wait` réel**
  — ni gelé au sens OS, ni en sondage.
- **r320-r326** : identification d'une section critique invité
  (`0x10000610`, le tas mémoire invité) tenue par le thread principal
  avec une profondeur de récursion anormale et constante
  (**359 943**, mesurée sur 8+ lancements indépendants) — le thread
  principal a acquis ce verrou pour son propre travail d'allocation
  ordinaire et ne l'a jamais relâché avant d'entrer dans une attente
  du signal d'achèvement d'un thread ouvrier — lequel a lui-même besoin
  de ce même verrou pour terminer et signaler. Deadlock réel, causé
  par un déséquilibre enter/leave côté thread principal, pas un
  deuxième verrou classique en AB-BA.
- **r327-r357** : traçage empirique (grep statique jugé intraitable —
  plus de 90 sites d'appel à l'allocateur dans le code généré,
  r327 point 2) jusqu'au site exact : `sub_821FA6F8`, chemin de sortie
  `loc_821FA94C`, auquel manquait l'appel conditionnel
  `RtlLeaveCriticalSection(*(r30+1408))` déjà présent correctement sur
  le chemin sœur `loc_821FA924`. **Cause racine exacte : un artefact de
  la génération de code XenonRecomp** — le code machine retail à cette
  adresse est PARTAGÉ par deux fonctions synthétiques adjacentes
  (`sub_821FA6F0` et `sub_821FA6F8`), et XenonRecomp duplique cet
  épilogue partagé dans les deux au lieu d'émettre un saut
  inter-fonction, perdant l'appel `Leave` sur l'une des deux copies.
- **r358** : correctif appliqué (`tools/apply_sub_821fa6f8_leave_fix.py`,
  patch du C++ généré, tracké, idempotent), **sur décision explicite de
  l'utilisateur** (choix du contournement natif plutôt que de la
  reproduction fidèle du bug). Vérifié : `recursion=4` au lieu de
  `359943`, le processus natif termine désormais normalement de
  lui-même en fin de fenêtre de sondage au lieu de devoir être
  interrompu de force. `presented_frames` restait à 0 DANS LA MÊME
  fenêtre de test (90s) — le deadlock spécifique est résolu, mais
  `r358` note explicitement (sans le trancher) qu'un autre goulot
  pourrait être la limite suivante. Cohérent avec l'état établi de
  cette session (r473-r475) : le produit natif atteint bien 86 tirages
  d'interface précoce dans sa fenêtre de sondage — donc un déblocage
  ultérieur a bien eu lieu, probablement lié à ce correctif ou à des
  cycles suivants non consultés ce tour (`ctest`/`pytest` mentionnés
  jusqu'à r358, la chaîne complète va possiblement plus loin, hors
  périmètre de cette lecture).

## Établi — ce correctif précis NE PEUT PAS s'appliquer tel quel au produit `rexglue-oracle`

Lu directement
`upstream/AC6_recomp/thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_rtl.cpp`
(lignes 382, 432, 625-630) : `RtlEnterCriticalSection`/
`RtlLeaveCriticalSection` sont des **hooks d'export XBOXKRNL**
(`XBOXKRNL_EXPORT` sur `RtlEnterCriticalSection_entry`/
`RtlLeaveCriticalSection_entry`) — ReXGlue exécute le code machine
PowerPC RÉEL du binaire retail (interprétation/JIT sur les
instructions authentiques), **pas une retraduction C++ statique par
fonction comme XenonRecomp**. Le bug exact de r356-r358 était un
artefact de la génération de code de XenonRecomp (une duplication
d'épilogue partagé entre deux fonctions synthétiques adjacentes) — un
concept qui n'existe pas pour un interpréteur qui exécute le binaire
retail tel quel. **Ce correctif ne se transpose donc pas.**

## Non établi

- **Si ReXGlue a un bug ANALOGUE mais distinct** dans sa propre
  implémentation des primitives `Rtl*CriticalSection`/
  `KeWaitForMultipleObjects`/allocateur de tas invité — non
  investigué ce cycle (nécessiterait de lire ces implémentations en
  détail, ou de reproduire la technique d'échantillonnage OS de r301
  directement sur le produit oracle-hybride en cours de blocage —
  hors périmètre de ce cycle statique, candidat direct pour la suite).
- **Si le blocage précis observé par r506 (une image puis rien)
  correspond au MÊME point du code invité** (`sub_8233B5A0`/
  `sub_82345CE0`/le tas `0x10000610`) que celui caractérisé par
  r279-r358, ou à un point différent** — plausible étant donné le
  même binaire XEX exécuté, mais non confirmé par une lecture Ghidra
  ciblée sur ces adresses précises dans le contexte du lancement
  oracle-hybride (candidat pour r508).
- **Ce qui s'est passé entre r358 et r473** (déblocage ultérieur du
  produit natif jusqu'à 86 tirages) — chaîne non entièrement lue ce
  cycle, pourrait contenir des indices supplémentaires pertinents.

## Correction du raisonnement de r487/r488 : PAS invalidé, mais recontextualisé par ce parallèle

r487/r488 ont établi que le sondage « movie worker » est non bloquant
par conception (`timeout=0`) et que sa condition de branchement est
un simple compteur de threads vivants — donc il ne peut PAS être la
cause du blocage. **Le parallèle natif (r301) confirme et renforce
cette conclusion plutôt que de la contredire** : dans l'investigation
native, ~15 des ~29 threads tournaient aussi en sondage actif
continu sans que cela indique un problème — c'est le comportement
normal du moteur. Le vrai coupable, dans le cas natif, était un thread
SPÉCIFIQUE et DIFFÉRENT (le thread principal, parqué dans un
`futex_do_wait` réel, pas dans le sondage movie-worker). Aucune
correction n'est donc apportée à r487/r488 ; leur conclusion reste
valide et ce cycle en trouve une confirmation indépendante par
analogie structurelle.

## Décisions prises

- Documenter cette investigation antérieure en détail plutôt que de
  la résumer superficiellement — sa valeur pour la suite dépend de
  connaître les adresses et le mécanisme exacts, pas seulement « ça a
  été trouvé un jour ».
- Ne PAS tenter d'appliquer le correctif natif au produit
  `rexglue-oracle` — il ne s'applique pas structurellement, l'appliquer
  serait une erreur de méthode, pas juste une perte de temps.
- Ne PAS toucher `recompilation/ace-combat-6-retail/native/` ni
  lancer de capture oracle ce cycle — conforme au périmètre statique
  demandé.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts. `ctest` natif non relancé (`native/` non touché). `git
status --porcelain` sous
`recompilation/ace-combat-6-retail/native/` confirmé inchangé.

## Named for r508

**Candidat direct et bien motivé** : reproduire la technique
d'échantillonnage OS de r301 (`/proc/$PID/task/*/{stat,wchan}`, sans
gdb, sans `timeout` externe) directement sur un lancement du binaire
`rexglue-oracle` (`install/ntsc-uj/bin/ac6recomp` en mode
`hybrid_backend_fixes`) pendant qu'il est bloqué après sa première
image, pour déterminer si un thread précis y est authentiquement
parqué (`futex_do_wait` ou équivalent) plutôt que de simplement
constater que le thread movie-worker tourne (déjà su, non concluant).
Si un tel thread est trouvé, tracer son gdb/backtrace pour identifier
la primitive exacte sur laquelle il attend — même méthode que
r301→r325, appliquée au nouveau produit. Les 3 états cibles de r478
restent non capturés ; ce blocage précoce est probablement leur vraie
cause commune avec le symptôme historique natif, mais ceci reste à
confirmer, pas à supposer.

## Files

Committé : ce rapport, `NEXT.md`. Aucun script Ghidra nécessaire ce
cycle (lecture directe de fichiers déjà trackés dans le dépôt et
d'artefacts de rapports existants). Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
