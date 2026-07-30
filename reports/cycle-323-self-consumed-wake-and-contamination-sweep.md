# Cycle 323 — le signal est reconsommé par celui qui le pose ; et balayage de contamination

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC **big-endian**, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Base image : `0x82000000`
- Exécutable natif avant ce cycle :
  `bb7eeccf4deadc22854aeaa0b0ee0e79fd2db7b7a1f3abe87c20856189440306`
- Exécutable natif après ce cycle :
  `13a9f256ff3400d8c4d17a471b020a49f2fc6709e27b50ee2f284804f0a752a9`
- Route : `dynamic` pour la mesure invité, `deterministic-fast-path` pour la
  régression et l'analyse statique

## 0. Résumé

Le front demandé par le cycle 322 était une seule lecture. Elle a été faite, et
elle **renverse** les cycles 321 et 322 :

| | cycle 320 | cycle 321 | cycle 322 | **cycle 323** |
|---|---|---|---|---|
| valeur partagée | `0` (moitié haute seule) | — | supposée `>= 2` | **`0` sur les deux moitiés, mesuré** |
| anomalie « waiter éligible endormi » | signalée | — | déclarée inexistante | **réelle** |
| vol de signal | cause candidate | **réfuté** | sans objet | **confirmé, 3/3 déterministe** |
| voleur | waiter tardif | — | — | **le thread qui pose le signal** |

Et le dépôt contenait, non signalée, la correction SDK que le cycle 321 avait
interdit d'écrire — **déjà compilée dans le binaire**.

## 1. La mesure que le cycle 322 réclamait

Lecture hors débogueur, non perturbante, sur le fichier de mémoire invité
`/dev/shm/xenia_memory_*` (dont les offsets sont les adresses virtuelles
invité), via `scripts/ac6_read_guest_memory.py`. L'outil refuse de rapporter
quoi que ce soit avant d'avoir **prouvé** la correspondance : il relit l'ancre
`0x82346140` et exige le mot big-endian `0xE97F0010`, soit exactement
`ld r11,16(r31)`, l'instruction de chargement 64 bits du protocole.

Ancre qualifiée. Résultat, en déclarant largeur et moitié comme le cycle 322
l'impose :

```text
0x82870818  f80000a4   mutant          (= handle du cycle 320)
0x8287081C  f80000a8   event           (= handle du cycle 320)
0x82870820  00000001
0x82870824  00000000
0x82870828  00000000   <- MOITIÉ HAUTE
0x8287082C  00000000   <- MOITIÉ BASSE
```

**Valeur 64 bits = `0`.** Stable sur 18 échantillons couvrant 180 s, puis
6 échantillons sur une seconde exécution.

Le cycle 322 avait raison sur la mécanique — la moitié basse est bien à
`0x8287082C` — et tort sur la conclusion : il a corrigé une ambiguïté de mesure
puis **supposé** la valeur au lieu de la mesurer. C'est la troisième fois de la
campagne qu'une conclusion tirée du code seul tombe devant la mesure, après
`0x821CCBE0` (cycle 312) et le verrou MMIO `0x7FC86544` (cycle 317 §5).

Corroboration statique indépendante : `sub_8233A730` initialise ce champ à `0`
par `std r11,168(r3)` avec `r11 = 0`. La valeur n'a jamais été `>= 2`.

L'anomalie du cycle 320 est donc **réelle** : le thread principal attend `0`, la
valeur *est* `0`, et il dort.

## 2. Le protocole complet, producteur inclus

Le cycle 322 cherchait le producteur parmi les fonctions touchant
`0x82870780`. Il l'a manqué : c'est `sub_823460B0`, le **setter** de ce même
objet, appelé par les deux côtés.

```text
sub_823460B0(obj, v):            // setter
    NtWaitForSingleObject(obj+0)        // prendre le mutant
    std v, 16(obj)                      // 64 bits
    NtReleaseMutant(obj+0)
    NtSetEvent(obj+4)                   // <- unique annonce

sub_82346108(obj, wanted):       // waiter (cycle 322, confirmé)
    boucle: prendre mutant ; si load64(obj+16) == wanted -> relâcher, rendre
            sinon NtSignalAndWaitForSingleObjectEx(mutant, event, -1)
```

Les deux extrémités, lues dans la sortie recompilée :

| thread | fonction | attend | puis pose |
|---|---|---:|---:|
| principal | `sub_8233BA78` via `sub_8233AB00` / `sub_8233AAF0` | `0` | **`1`** |
| worker | `sub_8233AD70` | `1` | **`0`** |

C'est un **ping-pong strict** : chaque changement d'état est annoncé
**exactement une fois**, et les deux waiters attendent des valeurs
**différentes** sur le **même** événement auto-reset.

L'événement est bien auto-reset côté invité, ce n'est pas un défaut de
traduction : `sub_82346010` construit l'objet et crée l'événement par
`rex_sub_821F4130(0, 0, 0, 0)`, dont la signature est
`CreateEvent(attrs, bManualReset, bInitialState, name)` ; `bManualReset = 0`
donne `EventType = 1`, `SynchronizationEvent`. Le `manual_reset_ = false`
observé au cycle 320 côté hôte est **fidèle**.

## 3. La régression, sur le protocole réel

Le banc du cycle 321 gelait la valeur partagée à `0` et injectait 200 signaux
indépendants. Sous ce modèle un signal volé ne peut que coûter du débit,
puisqu'un autre signal arrive toujours : **le banc ne pouvait pas exprimer le
défaut**, et son `REFUTES_THEFT` ne dit rien du protocole réel.

`scripts/ac6_condition_pingpong_regression.cpp` réplique verbatim
`PosixConditionBase::Wait`, `PosixCondition<Event>`, `PosixCondition<Mutant>` et
`SignalAndWait` du SDK, puis rejoue le ping-pong ci-dessus. Chaque essai tourne
dans son **propre processus** : un interblocage réussi laisse par construction
deux threads endormis pour toujours, et les récupérer dans le processus
perturberait précisément la propriété testée.

```text
rexglue HEAD auto-reset event
  essai 1: main=1/20000 worker=1 value_at_stop=0 STALLED
  essai 2: main=1/20000 worker=1 value_at_stop=0 STALLED
  essai 3: main=1/20000 worker=1 value_at_stop=0 STALLED

libération ordonnée NT (candidat)
  essai 1: main=20000/20000 worker=19999 value_at_stop=1 COMPLETED
  essai 2: main=20000/20000 worker=19999 value_at_stop=1 COMPLETED
  essai 3: main=20000/20000 worker=19999 value_at_stop=1 COMPLETED

AC6_CONDITION_PINGPONG_REGRESSION CONFIRMS_SELF_CONSUMED_WAKE   (exit 0)
```

Ce n'est pas une course rare : l'interblocage tombe **dès l'itération 1, 3/3**,
et l'état terminal du banc — valeur `0`, deux threads endormis — est
**exactement** l'état mesuré au §1.

### Le mécanisme

Le voleur n'est pas un waiter tardif. C'est **le thread qui vient de poser le
signal** :

1. le worker pose `0` puis appelle `NtSetEvent` ; `signal_ = true` ;
2. le worker enchaîne immédiatement sur `sub_82346108(obj, 1)`, ne trouve pas
   `1`, et entre dans `Wait` ;
3. `Wait` voit `signal_ == true`, le consomme, `post_execution()` remet
   `signal_ = false`, et rend `kSuccess` ;
4. le thread principal, réveillé par `notify_all` mais pas encore revenu sur
   `mutex_`, retrouve `signal_ == false` et se rendort ;
5. plus personne ne posera de signal. Valeur `0`, deux dormeurs, définitif.

Sous NT c'est impossible : `SetEvent` sur un événement de synchronisation
relâche un waiter **de la file d'attente**, et le thread qui signale n'y est pas
au moment du `SetEvent`. L'invariant manquant s'énonce sans invoquer l'ordre
FIFO : *un thread qui n'était pas en attente au moment du `Signal()` ne peut pas
réclamer ce signal.* La correction retenue l'implémente en attribuant le jeton
au premier waiter déjà en file, et ne retombe sur le booléen partagé que s'il
n'y a aucun waiter — donc aucun signal n'est perdu quand personne n'attend.

Confiance : `confirmed` pour le protocole, l'arithmétique, la valeur mesurée et
la reproduction de l'interblocage ; `cross-match` pour l'ordre FIFO exact comme
sémantique NT.

### Limite connue, non corrigée

`PosixConditionBase::WaitMultiple` continue de lire `signaled()` et d'appeler
`post_execution()` directement, sans passer par `Wait`. Un `Signal()` qui remet
son jeton à un waiter `Wait` ne réveillera donc pas un waiter `WaitMultiple` sur
le même événement, et réciproquement. Le cas mixte est une divergence ouverte ;
elle n'affecte pas le protocole ci-dessus, qui n'utilise que
`NtSignalAndWaitForSingleObjectEx`.

## 4. Effet runtime : mesuré, et il ne suffit pas

Correction appliquée (`patches/rexglue-auto-reset-event-nt-ordered-release-20260730.patch`),
reconstruction complète, binaire `13a9f256…a752a9`, sonde
`tools/ac6-frame-loop-probe.sh` :

| | avant (`bb7eeccf`) | après (`13a9f256`) |
|---|---|---|
| valeur invité `0x82870828` (64 bits) | `0`, stable 180 s | `0`, stable 60 s |
| `REX_FATAL` / branche non résolue | 0 | **0** |
| threads vivants | 33 | 33 |
| threads invités consommant du CPU | oui | **oui** |

**La correction ne débloque pas la présentation d'images.** Elle corrige un
défaut prouvé, elle ne referme pas le front. Aucune revendication de
jouabilité, aucune de parité.

Et la comparaison la plus utile — le nombre de présentations — **n'a pas pu être
faite** : voir §5.3.

## 5. Balayage de contamination

Cinq contaminations trouvées pendant ce cycle. Elles sont listées avec ce qui a
été fait, parce qu'une contamination signalée mais non tracée revient.

### 5.1 La correction interdite était dans l'arbre, et dans le binaire — RETIRÉE puis REQUALIFIÉE

`thirdparty/rexglue-sdk/src/core/threading_posix.cpp` portait, non commis et non
mentionné dans aucun rapport, exactement la variante à file d'attente que le
cycle 321 avait conclu qu'il « ne doit pas [être] écrit[e] ».

Chronologie, par horodatage des fichiers :

```text
12:53  rapport cycle 319
13:38  rapport cycle 320  (binaire 2f95bf26, sain)
13:44  threading_posix.cpp modifié      <- la correction proposée par 320
13:45  binaire relié -> bb7eeccf         <- la correction est dedans
23:33  rapport cycle 321  «ne pas écrire cette correction»
23:41  rapport cycle 322
```

Le cycle 321 a lu `HEAD` et décrit l'implémentation d'origine, sans voir que
l'arbre de travail et le binaire ne correspondaient plus. Les cycles 321 et 322
ont donc raisonné sur un SDK différent de celui qui tournait. Preuve que la
correction était bien liée : `nm` sur `bb7eeccf` montre
`PosixCondition<Event>::Wait` comme symbole propre, alors qu'à `HEAD` seul
`PosixConditionBase::Wait` existe — aucune autre spécialisation ne surcharge
`Wait`.

Fait : diff archivé, arbre remis à `HEAD`, reconstruit et remesuré ; puis la
correction **requalifiée** par la régression du §3 et réappliquée sous le nom
`rexglue-auto-reset-event-nt-ordered-release-20260730.patch`.

### 5.2 Fuite de mémoire invité en `/dev/shm` — CORRIGÉE ET OUTILLÉE

rexglue réserve la mémoire invité dans `/dev/shm/xenia_memory_<tick>`
(4 831 838 207 octets) et ne la délie que dans `Memory::~Memory`, donc
uniquement sur arrêt propre. **Toutes** les sondes AC6 se terminent par timeout
(124) ou abort (134) : chacune fuit sa réservation. `/dev/shm` est du tmpfs,
donc ce sont des pages RAM hôte.

Trouvé : **28 orphelins, 9,8 Gio résidents**, datés du 27 juillet, sur un
`/dev/shm` de 61 Gio. `tools/ac6-advance-loop.sh` fait jusqu'à deux exécutions
par itération sur 12 itérations : ~24 fuites par invocation.

Fait : orphelins supprimés après avoir vérifié qu'aucun processus vivant ne les
mappe ; garde `tools/ac6-clean-runtime-leaks.sh` écrite, qui ne supprime qu'un
fichier mappé par aucun processus vivant, et appelée en début et fin de sonde.
Elle a déjà récupéré 431 Mio à la fin de la sonde de ce cycle.

### 5.3 L'appareil de mesure des cycles 316/317 n'est pas dans l'arbre — SIGNALÉ

Les chiffres qui structurent les cycles 316 et 317 — 2 appels `VdSwap`, 2
présentations, 4 250 interruptions vblank, 4 EOP, 4 250 appels à
`sub_821EFBA0`, 47 lectures `DATA00.PAC` — viennent de compteurs instrumentés.
Ces correctifs sont archivés dans `patches/` et **ne sont pas appliqués** :

```text
rexglue-instrument-vdswap-and-gpu-interrupt-20260727.patch   s'applique proprement
rexglue-instrument-interrupt-sources-20260727.patch          s'applique proprement
rexglue-instrument-apc-counters-20260727.patch               s'applique proprement
```

Une exécution nue ne journalise donc rien de tout cela : la sonde de ce cycle
rapporte `PRESENT 0 / VdSwap 0 / DATA00.PAC 0` avec 5 lignes de sortie, non
parce que rien ne s'est produit, mais parce que **rien ne le compte**. Les
compteurs `g_guest_present_count` / `g_guest_swaps_issued` qui existent dans
`graphics_system.cpp` servent au cadencement et ne sont jamais émis.

C'est un piège de mesure : un futur cycle qui compare « présentations avant /
après » lira `0` des deux côtés et pourra y voir une régression. Aucun fichier
de l'arbre n'enregistrait cette dépendance avant ce rapport.

Piège adjacent, corrigé dans la sonde : `Ac6recompAppCreate` fait
inconditionnellement `REXCVAR_SET(log_file, "ac6recomp.log")` et ne monte le
niveau à `debug` que si `log_level` est resté par défaut. Passer
`--log_file` ou `--log_level` **réduit** donc silencieusement ce qui est
enregistré.

### 5.4 Un `gdb` de trois jours — TUÉ

Un `gdb -batch` du 27 juillet 09:55 tournait encore, 3 j 02 h 38, 124 Mio RSS,
avec un enfant `ac6recomp` zombie. Il venait d'une sonde de cycle antérieur
(`--vulkan_device=0`, expérience du cycle 316) qui n'a jamais rendu la main.
Tué, zombie récolté.

### 5.5 `timeout` ne bornait pas les sondes — CORRIGÉ

Le motif `timeout N xvfb-run ... ac6recomp` — utilisé par
`tools/ac6-advance-loop.sh` et repris par la première version de la sonde de ce
cycle — signale `xvfb-run`, un script shell, et **laisse le petit-fils
`ac6recomp` vivant**. La première sonde de ce cycle a ainsi tourné 180 s au lieu
de 90, et c'est le mécanisme le plus probable derrière le `gdb` du §5.4. La
sonde borne maintenant le processus invité directement par son pid.

## 6. Modifications

- `scripts/ac6_read_guest_memory.py` (nouveau) — lecture invité qualifiée par
  ancre, largeur et moitié déclarées, sans débogueur ni ptrace.
- `scripts/ac6_condition_pingpong_regression.cpp` (nouveau) — régression du
  protocole réel, isolée par processus.
- `tools/ac6-clean-runtime-leaks.sh` (nouveau) — garde de fuite `/dev/shm`.
- `tools/ac6-frame-loop-probe.sh` (nouveau) — sonde bornée du front.
- `patches/rexglue-auto-reset-event-nt-ordered-release-20260730.patch`
  (nouveau) — la correction, requalifiée, appliquée à l'arbre.
- `reports/cycle-320…md`, `cycle-321…md`, `cycle-322…md` — annotés en tête avec
  ce qui tombe et ce qui reste.
- Aucune sortie générée n'a été éditée. La configuration n'a pas changé.

## 7. Validation exécutée

```text
ancre 0x82346140 == 0xE97F0010 sur le fichier mémoire invité      PASS
valeur 64 bits 0x82870828, 18 + 6 échantillons                    0, stable
sub_82346010 / rex_sub_821F4130 -> CreateEvent(_,0,0,_)           lecture directe
sub_8233BA78 / sub_8233AD70 / sub_823460B0 ping-pong              lecture directe
g++ -std=c++20 -O2 -pthread régression                            PASS
régression HEAD                                    STALLED 3/3, valeur 0
régression libération ordonnée NT                  COMPLETED 3/3
reconstruction HEAD (revert)                                      rc=0
reconstruction avec correction -> 13a9f256…a752a9                 rc=0
sonde runtime 60 s, correction appliquée           0 REX_FATAL, 33 threads
bash -n sur les deux nouveaux scripts                             PASS
garde de fuite, exécution réelle                   431 Mio récupérés
```

Ni preuve de jouabilité, ni preuve de parité retail.
`recompiler-generated` n'est pas `verified`.

## 8. Front suivant

Le défaut de synchronisation est fermé comme défaut ; il n'était **pas** la
cause du gel de la boucle de trame. Le front revient donc là où le cycle 317 §5
l'avait laissé, avec un appareil de mesure à remonter d'abord :

1. **Remonter l'observabilité avant toute mesure.** Réappliquer les trois
   correctifs d'instrumentation du §5.3 depuis `patches/`, ou mieux, émettre
   `g_guest_swaps_issued` / `g_guest_present_count` dans l'arbre derrière
   `log_noisy` pour que le compte survive aux régénérations. Sans cela aucune
   comparaison de présentations n'est admissible.
2. Reprendre `sub_821EFBA0`, appelée aux 4 250 interruptions vblank sans jamais
   demander de présentation, et suivre la branche qu'elle prend : la machine à
   états tourne dans un mode qui ne dessine pas.
3. Refermer la limite `WaitMultiple` du §3 si un objet invité mélange les deux
   familles d'attente.
4. `sub_821F5828` porte toujours son `REX_FATAL("Unresolved branch from
   0x821F5880 to 0x821F5854")` sur le chemin alertable (`alertable != 0`,
   statut `257`), relevé au cycle 322 et non corrigé.

Règle ajoutée, dans la lignée du cycle 322 : **un banc de régression doit
reproduire la forme du protocole, pas une abstraction de celui-ci.** Un banc qui
ne peut pas exprimer le défaut ne le réfute pas — il ne le teste pas. Et une
correction posée dans l'arbre doit être ou bien qualifiée par un rapport, ou
bien retirée avant la fin du cycle.
