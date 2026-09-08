# AC6 retail NTSC-U/J — r437 — le pointeur global `0x82935D98` n'est écrit NULLE PART pendant toute l'exécution (point d'arrêt matériel confirmé sur le run complet) — la recherche statique de l'écrivain manquant est bloquée : ce site n'a jamais été désassemblé dans le projet Ghidra existant

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r436 : tracer qui devrait peupler le pointeur global à
l'adresse invité `0x82935D98`, avant tout correctif sur
`sub_821D6C20`.

## Établi — capturé en direct (gdb, point d'arrêt matériel sur toute la durée du run)

### La base mémoire invité est stable et vérifiable via un symbole réel

Point d'arrêt sur le constructeur `ac6::native::GuestAddressSpace::
GuestAddressSpace()` (symbole réel du binaire hôte, pas une adresse
devinée), lecture du membre `base_` juste après construction :
`base_ = 0x00007ffef5800000`. **Identique à la valeur déduite des
registres du crash de r435/r436** — confirme que le placement mémoire
est stable dans cet environnement, pas un artefact d'un run
particulier.

### Un point d'arrêt matériel armé du tout début du process jusqu'au crash ne se déclenche JAMAIS

Adresse hôte cible calculée : `base_ + 0x82935D98 = 0x7fff78135d98`.
`watch *(unsigned int*)0x7fff78135d98` armé immédiatement après la
construction de `GuestAddressSpace` (donc avant tout code invité,
y compris `__xstart`), puis `continue` jusqu'à la fin du run.
**Résultat : le process progresse à travers tout le boot, plusieurs
cycles `vd drain`/`vd swap`, jusqu'au SIGSEGV déjà connu de
r435/r436 — le point d'arrêt matériel ne se déclenche PAS une seule
fois.**

**Ceci prouve, plutôt que suppose, que cette adresse n'est écrite par
AUCUN code exécuté pendant tout ce run** — ni par le code invité
recompilé, ni par un stub hôte, du tout premier octet de code invité
jusqu'au crash. Ce n'est pas une question d'ordre d'exécution
perturbé par un correctif antérieur (r413/r422/r430) : il n'existe
tout simplement **aucun écrivain exercé**, à aucun moment de
l'exécution observée.

## Recherche statique de l'écrivain — bloquée, documentée précisément

Tentative de localiser statiquement le site d'écriture qui devrait
exister dans le binaire retail (pas seulement dans le code exécuté).

**Ghidra headless est disponible** dans cet environnement — pas sous
`ace-combat-6/.tools/` (vide), mais réutilisable depuis l'installation
du workspace voisin `ace-combat-squadron-leader/.tools/
ghidra_12.1.2_PUBLIC/support/analyzeHeadless`, pointé en lecture seule
(`-readOnly -noanalysis`) sur le projet `ghidra-projects/ace-combat-6`
de CE dépôt (vérifié : `PROGRAM_EXECUTABLE` correspond au
`game-files/default.xex` de ce dépôt, `.text` couvre bien
`0x82090000-0x823d772b`, qui contient `0x821D6C20`).

- `FindReferencesToRange.java 0x82935d98 0x82935d9c` → **0
  référence** trouvée dans la base de données Ghidra existante.
- `FindPpcSplitAddressMaterialization.java 0x82935d98` (spécifiquement
  conçu pour retrouver les paires `lis`/`addi` séparées) → **0
  résultat**.
- `DumpFunction.java 0x821D6C20` → **aucune fonction définie à cette
  adresse dans ce projet**, et aucune instruction désassemblée dans
  toute la fenêtre `±0x40` autour — l'outil est retombé sur la
  prochaine instruction réellement désassemblée, **~0x6C4 octets plus
  loin**, une région sans rapport.

**Cause identifiée** : `sub_821D6C20` — et vraisemblablement une
bonne partie de la région autour — n'a **jamais été désassemblée ni
créée comme fonction** dans ce projet Ghidra stocké, cohérent avec le
fait établi par r346/r436 que ce site n'avait jamais été exercé avant
ce cycle de correctifs, donc jamais individuellement étudié. Les deux
scripts de recherche par adresse ne peuvent trouver que ce que Ghidra
a déjà désassemblé — sans une passe d'auto-analyse complète sur cette
région (non tentée ce cycle : coûteuse, et modifierait potentiellement
l'état stocké du projet au-delà de ce qu'un cycle de continuation
devrait décider seul), la recherche statique de l'écrivain manquant
reste bloquée.

## Ce que ceci établit

**Le défaut n'est pas un simple oubli d'appel qu'un correctif ciblé
pourrait contourner** : sur toute la fenêtre observée, aucun code —
ni invité ni hôte — n'écrit jamais cette adresse. Deux explications
restent possibles et ne sont pas encore départagées :
1. Le sous-système propriétaire de cet objet nécessite un stub hôte
   pour un service Xbox 360 non modélisé (XAM, réseau, un composant
   optionnel) dont l'appel réel (sur consoles) construit l'objet ; ce
   dépôt ne simule pas ce service, donc le chemin de construction
   n'est jamais atteint.
2. Un contrôle conditionnel légitime (capacité matérielle, disponibilité
   réseau, autre) évalue différemment dans cet environnement offline
   et saute la construction — comportement correct pour cet
   environnement, mais qui laisse `sub_821D6C20` face à un pointeur
   qu'il ne s'attend normalement jamais à voir nul.

## Non établi

- **Lequel des deux scénarios ci-dessus est le bon** — nécessiterait
  soit une auto-analyse Ghidra complète de la région pour trouver
  statiquement tous les appelants potentiels du constructeur (non
  tentée, coût/risque non qualifiés ce cycle), soit une lecture
  manuelle plus large du désassemblage autour de `0x82935D98` dans un
  autre outil.
- **Si ce même schéma (récepteur jamais construit) affecte d'autres
  adresses** dans le même voisinage de code — hors périmètre de ce
  cycle.

## Décisions prises

- **Ne pas lancer d'auto-analyse Ghidra complète ce cycle** — un
  changement d'état potentiellement long et significatif sur un
  projet stocké partagé entre cycles, pour une question qui a déjà
  une réponse suffisamment forte via la preuve en direct (aucune
  écriture, point final) pour continuer à documenter précisément sans
  ce risque.
- **Ne pas corriger `sub_821D6C20` ni tenter un contournement** —
  toujours vrai depuis r436 : corriger sans savoir lequel des deux
  scénarios ci-dessus s'applique serait une règle plausible sans
  contrôle.
- **Documenter la disponibilité de Ghidra headless via le workspace
  voisin** (`ace-combat-squadron-leader/.tools/`) comme référence
  utile pour de futurs cycles — utilisé ici en lecture seule
  uniquement, sans aucune modification de ce dépôt ni de l'autre
  workspace.

## Gate

Aucune source de production éditée ce cycle (investigation en direct
et via Ghidra en lecture seule uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées depuis r436 — aucune source
affectée)
`ctest` (racine et natif) inchangé depuis r436 (aucune source
modifiée).

## Named for r438

Deux pistes indépendantes, l'une ne bloquant pas l'autre :
1. Si l'utilisateur juge le coût acceptable, lancer une auto-analyse
   Ghidra ciblée (pas sur tout le binaire — juste la région autour de
   `0x821D6C20`/`0x82935D98`) pour permettre enfin une recherche
   statique fiable de l'écrivain manquant.
2. En parallèle, revenir à la ligne ouverte de r434/r435 sur le
   contenu visuel réel (le SSBO à 64 Mio de `PinnedShaderRuntime`) —
   indépendante de ce défaut, puisque ce défaut affecte une fonction
   différente (`sub_821D6C20`) sur un chemin d'exécution différent
   (thread d'entrée, pas le service `VdSwap`).

## Files

Ghidra headless réutilisé depuis
`/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-squadron-leader/.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless`
(lecture seule, `-readOnly -noanalysis`, aucune écriture dans l'autre
workspace ni dans celui-ci). Aucun artefact gitignoré nouveau (journaux
sous `/fastdata/tmp/...scratchpad/`, non conservés).
