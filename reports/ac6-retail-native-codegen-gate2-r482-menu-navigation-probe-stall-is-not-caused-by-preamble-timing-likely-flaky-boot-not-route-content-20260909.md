# AC6 retail NTSC-U/J — r482 — le blocage « movie worker » de `us-menu-navigation-probe.steps` (r481) N'EST PAS causé par un manque de temps de stabilisation avant le premier `wait-pulse` : avec un préambule rendu BYTE-POUR-BYTE identique à `us-pretype28-startup.steps` (route qui réussit), le blocage se reproduit à l'identique — cause probablement un aléa de démarrage hôte, pas un défaut de la route

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (poursuite directe de la piste A, décision utilisateur
déjà actée par la reprise de r481).

## Contexte

Nommé par r481, piste 1 : déboguer le blocage « movie worker » de
`routes/us-menu-navigation-probe.steps` avant toute nouvelle tentative
de route. Contrainte de coordination toujours en vigueur ce cycle,
respectée intégralement : la session concurrente non identifiée
modifiant `native/` sans commit (dernière écriture 2026-09-09
05:06:56/05:07:55) — vérifié calme avant ET après ce cycle (`git
status --porcelain` sous `native/` inchangé, mêmes 7 fichiers modifiés
par cette autre session, aucun ajout). Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` touché ce cycle.

## Établi — le mécanisme « AC6 movie worker » est une vraie primitive noyau, pas une invention diagnostique

`upstream/AC6_recomp/thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_threading.cpp:100-114` :
trois adresses constantes (`kAc6MovieWorkerSetEvent=0x82916E3C`,
`kAc6MovieWorkerWaitEvent0=0x82916E2C`,
`kAc6MovieWorkerWaitEvent1=0x82916E08`) et une fonction
`IsAc6MovieWorkerWait()` qui détecte un `KeWaitForMultipleObjects`
portant exactement ces deux handles d'attente. Le code aux lignes
557-570 et 979-1016 ne fait QUE journaliser (`REXKRNL_DEBUG`) quand ce
motif est détecté — **aucun contournement, aucun forçage de
signalement** n'existe dans ce chemin. L'attente est donc réelle :
si le jeu invité n'appelle jamais `KeSetEvent` sur
`kAc6MovieWorkerSetEvent`, l'attente ne se termine jamais par elle-même
(seul un timeout côté objet peut la débloquer, selon la configuration
du wait).

## Établi — hypothèse du préambule insuffisamment stabilisé, TESTÉE et RÉFUTÉE

`routes/us-menu-navigation-probe.steps` (créée par r481) différait de
`routes/us-pretype28-startup.steps` (route qui réussit de façon
répétée selon r481) sur exactement 3 lignes avant le premier
`wait-pulse type28=30` : il manquait `capture post-escape`, `sleep 4`
et `capture post-space`. Hypothèse motivée : un `space` envoyé trop
tôt (avant `sleep 4`) pourrait manquer la fenêtre d'acceptation
d'entrée du jeu invité et le faire bifurquer vers un état différent
qui ne satisfait jamais `type28=30`.

**Correctif appliqué** : les 3 lignes manquantes ajoutées à
`us-menu-navigation-probe.steps`, rendant son préambule (les 8
premières lignes non-commentaires, jusqu'à `capture type28-30` compris)
**strictement identique octet pour octet** à `us-pretype28-startup.steps`
(vérifié par `diff` direct des deux séquences non commentées — aucune
différence).

**Résultat** : `python3 tools/run_gate.py --diagnostic-route
routes/us-menu-navigation-probe.steps --mission-dump-shaders --output
<scratch> --display :192`, sous `timeout 240`. Deux captures produites
(`step-03-post-escape.png`, `step-06-post-space.png`, tailles non
nulles — donc les deux premières entrées `Escape`/`space` ont bien été
délivrées et la fenêtre a bien répondu au moins jusqu'à ces
captures). Le journal `ac6recomp.log` s'arrête net à
`2026-09-09 05:32:27.839` (juste après la configuration du format de
texture Vulkan `k_Y1_Cr_Y0_Cb_REP`) et **ne produit plus aucune ligne
ensuite** — 0 occurrence de `type28`/`selector44` dans tout le journal.
Le processus `ac6recomp` est resté vivant, consommant **121% CPU en
continu** (mesuré par `ps`, `ELAPSED=241s`), jusqu'à l'expiration du
`timeout 240` externe — **le même profil exact que r479/r480**
(processus actif, non gelé, mais journal muet).

**L'hypothèse du préambule est donc réfutée** : avec un préambule
strictement identique à une route qui réussit de façon reproductible,
le blocage se produit quand même. La cause n'est pas dans le contenu
de la route.

## Nettoyage effectué

Le `timeout 240` externe a tué le processus wrapper `run_gate.py` (code
retour 143) mais **pas** le sous-processus `ac6recomp`/`Xvfb` qu'il
avait lancé (`Xvfb` est démarré avec `start_new_session=True`,
détachant son groupe de processus). Détecté après coup via `pgrep` (le
binaire tournait toujours à 121% CPU, ~9 minutes après l'arrêt apparent
du journal) et terminé manuellement (`kill -9` sur les deux PID,
`/tmp/.X11-unix/X192` retiré). Aucune fuite résiduelle après
vérification (`pgrep -af "ac6recomp|Xvfb :192"` vide).

## Non établi — cause réelle du blocage

- **Si c'est un aléa dépendant de la charge de l'hôte** (une session
  concurrente distincte exécute activement, sans commit, des lancements
  GPU/Vulkan répétés dans `native/` ce cycle même — contention
  plausible sur le périphérique Vulkan/l'affichage X, non mesurée
  directement ce cycle) — hypothèse motivée par le contexte mais non
  testée (aucune mesure de contention GPU prise).
- **Si `us-pretype28-startup.steps` échouerait aussi de façon
  intermittente sous la même charge** — non retesté ce cycle (l'aurait
  fait sortir du périmètre : cela aurait consommé un budget oracle
  supplémentaire pour reproduire un négatif potentiellement déjà connu).
- **Le point de blocage exact dans le code invité** — aucune lecture
  directe du désassemblage autour de `0x82916E2C`/`0x82916E3C`/
  `0x82916E08` n'a été faite ce cycle (aurait nécessité une session
  Ghidra, hors périmètre d'un cycle oracle).

## Décisions prises

- Ne pas retenter une troisième fois avec une variation supplémentaire
  du minutage d'entrée — le facteur testé (temps de stabilisation du
  préambule) est maintenant réfuté avec une preuve directe (préambule
  identique, échec identique), et une nouvelle variation de minutage
  serait une nouvelle supposition sans nouvelle preuve qui la motive.
  Rendement décroissant, même motif que celui qui a fait recommander la
  pause de la piste A par r480.
- Conserver `routes/us-menu-navigation-probe.steps` dans son état
  actuel (préambule maintenant correct/identique à la route qui
  fonctionne) — utile pour une future tentative avec une vraie
  correction si la cause du blocage est un jour identifiée par lecture
  directe du code invité autour du « movie worker ».
- Ne PAS toucher `native/` ce cycle — vérifié avant et après.
- Ne pas merger dans le registre épinglé ce cycle (aucune nouvelle
  capture de nuanceur obtenue de toute façon — le run a échoué avant
  toute capture utile au-delà de ce que r481 avait déjà).

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, inchangés (aucune source de contrat touchée). `ctest`
natif délibérément NON relancé (arbre `native/` mid-edit par la
session concurrente — relancer produirait un résultat qui n'appartient
à aucune des deux sessions, même raisonnement que r481). `git status
--porcelain` sous `recompilation/ace-combat-6-retail/native/` confirmé
identique avant et après ce cycle (7 fichiers modifiés par l'autre
session, aucun changement introduit ici).

## Named for r483

**Piste A reste ouverte, sans nouvel élément permettant un merge** :
les 3 états cibles de r478 restent non capturés (inchangé depuis
r477/r478/r481). Le blocage « movie worker » de la route
d'extension-menu reste sans cause déterminée, mais l'hypothèse la plus
simple (minutage de préambule) est maintenant éliminée par une preuve
directe plutôt que par supposition. Candidats pour la suite, aucun
tenté ce cycle :
1. Lire directement le désassemblage invité autour des adresses
   `0x82916E2C`/`0x82916E3C`/`0x82916E08` (nécessite une session
   Ghidra) pour comprendre ce qui doit se produire pour que le movie
   worker signale son événement — pas une nouvelle supposition de
   minutage.
2. Retester `us-pretype28-startup.steps` (la route qui réussit) une
   deuxième fois maintenant, pour vérifier si elle échoue aussi de
   façon intermittente sous la charge hôte actuelle — déterminerait si
   la cause est un aléa de charge générique plutôt que quelque chose de
   spécifique à la route d'extension-menu.
3. Une fois l'arbre `native/` de la session concurrente confirmé calme
   (commité ou en pause stable), fusionner les 12 traductions déjà
   obtenues par r481 (aucune des 3 cibles, mais 1 nuanceur réellement
   nouveau) — toujours en attente, bénéfice marginal.
4. Considérer une pause de la piste A similaire à celle recommandée par
   r480 : 2 cycles consécutifs (r481, r482) de rendements décroissants
   sur ce sous-problème précis, sans nouvelle capture utile depuis r477.

## Files

Committé : `recompilation/ace-combat-6-retail/routes/us-menu-navigation-probe.steps`
(préambule corrigé), ce rapport, `NEXT.md` (nouvelle entrée ajoutée
sans perturber le pointeur actif de la session concurrente ni la
section historique restaurée par r481). `reports/handoff/CURRENT.json`
délibérément non touché (pointeur actif de l'autre session, même
raisonnement que r481). Non conservé (scratch,
`/fastdata/lavaulta/tmp/r482-scratch/`) : dump de nuanceurs, journal
complet, captures d'écran. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.
