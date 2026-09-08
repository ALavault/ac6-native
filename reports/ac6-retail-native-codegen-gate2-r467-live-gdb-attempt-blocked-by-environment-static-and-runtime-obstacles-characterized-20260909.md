# AC6 retail NTSC-U/J — r467 — une session gdb en direct sur le « movie worker » a été TENTÉE mais n'a pas abouti : trois obstacles d'environnement distincts caractérisés, un vrai crash sous débogueur découvert en chemin, cause racine toujours NON établie — prochaine étape resserrée

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle décisif ce cycle (tentatives multiples, aucune n'a abouti
jusqu'à la lecture d'état recherchée) ; aucun budget de session oracle
« complète » consommé au sens où aucune route de gameplay n'a été
menée à son terme utile.

## Contexte

Nommé par r466 : une session gdb en direct sur le binaire oracle
(profil `rexglue-oracle`, buildable depuis r465) — point d'arrêt au
retour de `KeWaitForMultipleObjects` pour la paire d'objets du
« movie worker » (`0x82916E2C`/`0x82916E08`), lecture de l'état
interne (`manual_reset_`/`signal_`) au moment précis où
`result=0` (déjà établi par r466 : systématiquement, sur les 159 644
lignes du journal r465).

## Établi — la voie d'attachement à un processus déjà lancé est fermée dans ce bac à sable

`/proc/sys/kernel/yama/ptrace_scope = 1` (« restricted ») et aucun
accès `sudo` (`sudo -n true` échoue, authentification interactive
requise). `gdb -p <pid>` sur le processus lancé par
`run_gate.py` (dont le parent réel est l'interpréteur Python de
`run_gate.py`, pas cette session) échoue immédiatement :
« Could not attach to process… check ptrace_scope ». Sans root et
sans être le parent direct, l'attachement post-lancement est
structurellement impossible ici.

## Établi — un lancement direct sous gdb (`gdb --args ./ac6recomp …`) contourne l'attachement, mais révèle un VRAI crash spécifique au débogueur

Lancer le binaire (`install/ntsc-uj/bin/ac6recomp`, produit par le
rebuild `rexglue-oracle` de r465/ce cycle) directement sous
`gdb --args` (donc gdb devient le parent direct — la seule voie
d'attachement possible sans root) fait système : **un SIGSEGV réel et
reproductible** dans `rex_sub_821E4378` (code PPC recompilé réel du
jeu), sur le « Main XThread », survenant tôt (avant même d'atteindre
le point d'arrêt visé), et ce **avec le jeu de drapeaux minimal ET
avec le jeu de drapeaux complet identique à celui de r463-r466**
(reproduit deux fois, indépendamment du contenu des arguments). Ceci
n'apparaît JAMAIS lors des lancements normaux via `run_gate.py` sans
débogueur (r463-r466). C'est cohérent avec un moteur
(ReXGlue/XenonRecomp) qui utilise potentiellement des pages gardées ou
une gestion de signal SIGSEGV pour un mécanisme interne (accès
mémoire invité non aligné, pagination paresseuse) — un débogueur qui
intercepte le signal AVANT le propre gestionnaire de l'application le
casse. Ajouter `handle SIGSEGV nostop noprint pass` (et `SIGBUS`)
supprime l'arrêt gdb sur ce signal et laisse le jeu continuer, mais au
prix d'un ralentissement massif (chaque signal passe quand même par un
aller-retour noyau→gdb→relivraison), et le jeu démarre alors un très
grand nombre de threads (largement > 80, contre un rythme de
démarrage normal sous `run_gate.py`) sans jamais atteindre le point
d'arrêt visé dans le temps observé.

## Établi — un troisième obstacle : la limite de temps externe du harnais tue la tâche d'arrière-plan avant que le jeu (ralenti par gdb) n'atteigne le point d'arrêt

Une tentative avec `handle SIGSEGV pass` a tourné plusieurs minutes
(largement au-delà de ce qu'un lancement normal `run_gate.py` met pour
atteindre la phase cinématique, cf. r465/r466 : quelques dizaines de
secondes), puis a été terminée par un signal externe (code de sortie
`137` = `SIGKILL`) avant d'atteindre le point d'arrêt — le processus
`ac6recomp` orphelin a continué à tourner seul un moment après (gdb
tué, `ptrace` relâché), preuve que le lancement tournait bien mais
beaucoup trop lentement sous débogueur pour ce budget de temps.

## Établi — l'analyse statique du code généré ne trouve pas les adresses (attendu, pas une preuve négative)

Recherche des trois adresses (`0x82916E2C`/`08`/`3C`) dans l'arbre
généré `build/ntsc-uj/source/generated/ac6recomp_recomp.*.cpp` (le PPC
recompilé) : **zéro occurrence**. Ce n'est PAS une preuve qu'elles
n'existent pas — XenonRecomp/ReXGlue calcule la plupart des adresses
de données via arithmétique de registres à l'exécution (`lis`/`addi`
résolus en opérations sur `ctx.rN`, pas en littéraux figés dans le
C++ généré), donc une adresse de donnée arbitraire n'apparaît
généralement PAS comme chaîne littérale dans le code généré même si
elle est bien référencée. Trouver le site de création réel de ces
deux `XEvent` nécessiterait soit une passe Ghidra (recherche de
xrefs de données, hors périmètre outillé de ce cycle sans accès
Ghidra headless vérifié depuis cette tâche), soit un watchpoint en
direct sur l'écriture initiale (la même limitation d'attachement que
ci-dessus s'applique).

## Ce que ceci établit, et ce qui reste ouvert

**Établi (au-delà de r466)** : la voie « attacher gdb à un run
`run_gate.py` normal » est fermée dans ce bac à sable (pas de root,
pas de parenté directe). La voie « lancer directement sous gdb » est
ouverte en principe mais se heurte à deux problèmes cumulés
spécifiques au débogueur (un crash caché par le gestionnaire de
signal habituel de l'application, et un ralentissement qui dépasse le
budget de temps d'une tâche d'arrière-plan de ce harnais). Aucun des
deux n'invalide la caractérisation de r466 (boucle serrée, `result=0`
systématique) — ils empêchent seulement d'aller plus loin CE cycle.

**Non établi** : toujours pas lequel des deux objets reste signalé, ni
pourquoi. Le crash `rex_sub_821E4378` découvert sous débogueur est
lui-même un fait nouveau, potentiellement intéressant pour une
investigation séparée (pourquoi ce code se comporte différemment sous
ptrace), mais hors périmètre de cette piste.

## Décisions prises

- Ne PAS continuer à relancer des tentatives gdb à l'aveugle avec des
  budgets de temps croissants — le motif (démarrage massif de threads
  + tuerie externe) s'est répété deux fois de façon cohérente ; une
  troisième tentative identique ne changerait probablement rien sans
  changer d'approche.
- Ne PAS tenter de contourner `ptrace_scope` ou d'escalader les
  privilèges — hors périmètre d'une investigation de cycle, et ce
  n'est de toute façon pas une décision à prendre unilatéralement.
- Nettoyer intégralement les processus orphelins (`ac6recomp`, `gdb`,
  `Xvfb :188`) créés par ces tentatives.
- **Restaurer intégralement le profil `native`** : `manifest.json` et
  `static-validation.json` remis depuis la sauvegarde prise avant
  toute manipulation du profil `rexglue-oracle` (SHA-256 identiques
  vérifiés par comparaison directe) ; `ctest` natif **11/11** ; sonde
  `--probe-entry` en direct confirme `presented_frames=5 state=2` et
  capture `pixels=921600 non_black=0 distinct_colors=1` — identique à
  la ligne de base r454-r466, aucune régression.

## Gate

Aucune source de production modifiée ce cycle (seule une
investigation par lancement de processus et lecture, entièrement
annulée). Gates de dépôt exécutés pour clôturer le cycle normalement :
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine inchangé (seul l'échec préexistant connu
`ac6-cpp-complexity`).

## Named for r468

**Prochaine étape resserrée, différente de la précédente** : plutôt
qu'un lancement direct sous gdb avec passage de signaux (lent,
tué par le harnais), tenter soit (a) une session Ghidra headless
(projet `ac6-us`, XEX qualifié) pour trouver, par xrefs de données
statiques, où le code PPC retail écrit/lit `0x82916E2C`/`0x82916E08`
— une analyse purement statique, sans processus long à faire tourner
sous débogueur — soit (b) un lancement sous gdb avec un budget de
temps EXPLICITEMENT plus généreux qu'une tâche d'arrière-plan standard
(en gardant `handle SIGSEGV pass` mais en investiguant D'ABORD, sans
gdb, pourquoi `rex_sub_821E4378` ne crashe jamais en lancement normal
— cela pourrait accélérer un futur lancement sous gdb en évitant ce
signal parasite entièrement via un correctif ciblé si sa cause est
elle-même triviale). Reste ouvert, non bloquant : (1) le trou de
couverture du registre épinglé (r456) reste la piste principale une
fois ce blocage résolu ; (2) une fois le câblage `PinnedShaderRuntime`
jugé mûr, reconsidérer le committage groupé de l'arriéré natif
(r433/r434/r438/r454-r462) ; (3) mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut du
profil natif (confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau conservé. Répertoires temporaires
sous `/fastdata/lavaulta/tmp/ac6-r467-*` (logs gdb, sorties de
lancement, sauvegarde du manifeste natif) non committés, non
nettoyés (laissés pour inspection si utile au cycle suivant).
