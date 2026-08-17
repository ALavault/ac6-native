# Cycle 1725 — analyse de l’archive runtime Xenia/Linux

## Portée et intégrité

Analyse read-only de l’archive réellement présente à
`/fastdata/lavaulta/autoo-re-agent/workspaces/ace-combat-6/xenia-runtime-results-20260815-final.tar.zst`.
Le chemin `auto-re-agent` indiqué dans la demande n’existe pas; aucune copie ni
symlink n’a été créé.

| élément | valeur |
|---|---|
| archive SHA-256 | `0196ab3630a937118abea3d41e6d3dc663fcfdb04a3d7d2a843d572361578768` |
| `README.md` SHA-256 | `cc62c165f0dafb494da5d1fd3cfda1513c758de8ebe2cc000a88e4bc24526bc6` |
| `MANIFEST.md` SHA-256 | `14a49d1e3ec5bfe2fb4dc84248ac180f6acf49c1b4f5fa596b5c212d2e325620` |
| `SHA256SUMS` | 0 erreur (`sha256sum -c` sur toutes les entrées) |
| révision Xenia déclarée | `7010c86fb14f118ee598d3f76010dc0759b9502a` + patch local |
| cible déclarée | AC6 Demo, title ID `4E4D87E6`, version `0.0.1.2` |

Le manifeste ne porte pas le SHA-256 du `Default.xex` PAL
`de917873…5da8`. Les observations ci-dessous sont donc `xenia-oracle`/
`runtime-observed`, jamais `demo-qualified`; aucune preuve retail n’est jointe.
Les données originales restent dans l’archive; les seules sorties nouvelles sont
ce rapport et sa capsule JSON.

## Comparaison des exécutions

Les durées du flux binaire utilisent sa fréquence déclarée de 50 MHz. Les
compteurs de frames/audio sont ceux de `MANIFEST.md` ou du flux binaire, et ne
doivent pas être mélangés.

| session | mode | résultat observé | flux binaire | événements sémantiques |
|---|---|---|---:|---:|
| `20260815-145806` | native waits | gel; exit 137; un seul frame | 17 événements, 0,063 s, 1 `frame_swap` | 62 watchdog; threads invités 6 et 17 dans le même `SignalAndWait` |
| `20260815-150700` | `strace -f -qq -e trace=none` | atteint le gameplay, très ralenti | 17 784 événements, 79,058 s; 3 078 swaps, 14 683 submits audio | 599 lectures, 7 ouvertures; 52 562 `kernel_wait_multiple` |
| `20260815-151137` | bypass duplicate signal/wait | échec; gel noir; exit 137 | pas de progrès de phase | 338 watchdog; même paire de threads bloquée |
| `20260815-151937` | timeout 1 ms | échec; exit 137 | pas de progrès de phase | 68 watchdog; même paire bloquée |
| `20260815-152122` | timeout 16 ms | échec; exit 137 | pas de résolution | 119 watchdog; la paire de handles diffère dans le dernier échantillon |
| `20260815-152734` | native sans timeout | gel reproduit; exit 137 | pas de progrès de phase | 59 watchdog; même paire bloquée |
| `20260815-153303` | native + backtrace GDB | gel avec pile hôte capturée | — | pile `pthread_cond_wait` → `PosixConditionBase` → noyau Xenia |
| `20260815-162258` | native patchée | titre, menus, cinématique, gameplay soutenu; exit 137 dans l’archive | 72 488 événements, 328,963 s; 12 496 swaps, 59 969 submits audio | 790 lectures, 11 ouvertures; aucun événement sémantique de wait bloqué |

Le run `162258` est stable pendant l’intervalle annoncé de 30,539 secondes:
frames 8262→9173 (911, soit 29,83 fps) et submits audio 32416→38125. Le
manifeste avertit d’un ralentissement après environ 280 secondes invitées;
aucune conclusion de stabilité longue durée n’est retenue.

## Chronologie runtime

Faits observés dans `README.md`, `MANIFEST.md`, `runtime-events.bin`,
`semantic-events.jsonl` et `controller.jsonl`:

1. Le run patché crée 23 threads invités; premières créations à tick
   `61544948`, puis premier `frame_swap` à tick `66457543` (~1,329 s) et
   premier `audio_submit` à tick `100382210` (~2,008 s).
2. La séquence visuelle déclarée est titre → menus → cinématique pré-mission
   → gameplay, avec un checkpoint gameplay soutenu. Les quatre PNG restent
   des captures Xenia/oracle, pas des readbacks guest-owned de `ac6-demo-recomp`.
3. Le contrôleur injecté enregistre les transitions brutes (par exemple
   `buttons=0x10`, `0x400`, `0x1000` puis retour à zéro) avec PC de lecture
   `0x822F6168`/LR `0x822F616C`; aucun nom de bouton n’est inféré.
4. Le run sous `strace` produit des swaps et de l’audio, mais reste un oracle
   de scheduling: son dernier tick binaire est `0xF64D9C7C` (~82,646 s).
5. Les runs natifs négatifs restent dans le hand-off
   `NtSignalAndWaitForSingleObjectEx`; le patch POSIX est la différence
   contrôlée du run positif, sans preuve que toutes les sémantiques Windows
   sont déjà équivalentes.

## Cartographie thread → primitive → handles

| contexte | PC/LR invité | primitive | handles | statut |
|---|---|---|---|---|
| threads 6 et 17, native négative | `0x821A69C8` / `0x821A69CC` | `NtSignalAndWaitForSingleObjectEx` | signal `0xF8000088`, wait `0xF800008C` | tous deux bloqués dans les snapshots |
| thread 17, run `strace` | même PC/LR | même primitive | même paire | thread 17 reste dans l’attente; thread 6 continue |
| run GDB, hôte Thread 20 | PC invité visible dans la pile trampoline | `XObject::SignalAndWait` | mapping hôte→invité non imprimé dans ce fichier | `pthread_cond_wait` → `PosixConditionBase::Wait` |
| run GDB, hôte Thread 7 | trampoline invité | `XObject::SignalAndWait` | mapping hôte→invité non imprimé | même chaîne POSIX |
| run GDB, hôte Threads 6/17 | trampoline invité | `XObject::Wait` / `NtWaitForSingleObjectEx` | handles non exposés par la pile seule | `pthread_cond_wait` |

La pile hôte prouve la primitive POSIX et les symboles Xenia, mais ne permet pas
de déduire seule l’identité d’un thread invité. La jointure thread/handles vient
des snapshots et du manifeste, pas d’un nom hôte supposé.

## Adresses et fonctions prioritaires

### Décompilation guest (priorité immédiate)

- `0x821A69C8` / `0x821A69CC`: caller PC/LR de la paire
  `NtSignalAndWaitForSingleObjectEx`.
- `0x821A6A78` / `0x821A6A7C`: site observé par l’instrumentation du
  `kernel_wait_multiple` dans le run `strace` (fonction exacte à qualifier sur
  les bytes PAL, nom non promu).
- `0x821C5A1C` / `0x821C5A20`: `frame_swap` observé, args bruts
  `[0xBA9A0000, 0x1A9A0000, 1280, 720, 6]`.
- `0x8234D360` / `0x8234D364`: `audio_submit` observé, args bruts
  `[0, 0x40020700, 0, 0, 0]`; ce n’est pas une preuve XMA.
- `0x821A8CFC` / `0x821A8D00`: site de création de threads observé.

### Noyau hôte Xenia (cross-match générique seulement)

Les symboles suivants sont directement présents dans la backtrace GDB et dans
le patch archivé: `xe::kernel::XObject::SignalAndWait`,
`xe::kernel::XObject::Wait`, `xe::threading::PosixConditionBase::Wait`,
`xe::kernel::xboxkrnl::NtSignalAndWaitForSingleObjectEx_entry`,
`xe::kernel::xboxkrnl::NtWaitForSingleObjectEx_entry`,
`PosixCondition<Event>::Signal` et `PosixEvent`. Ils orientent l’instrumentation
mais ne remplacent pas la qualification du XEX PAL.

## Faits contre hypothèses

### Faits

- Le gel natif est reproductible avec les threads 6 et 17 sur la même paire
  signal/wait; le run `strace` progresse mais paie une forte pénalité.
- Le bypass de handles dupliqués, les timeouts 1 ms/16 ms, un sleep/yield et
  l’affinité n’ont pas produit un run correct et stable (manifest corrigé).
- Le patch positif rend `SignalAndWait` transactionnel côté POSIX et réserve le
  réveil à un waiter sélectionné; les tests archivés annoncent 712 assertions
  `[wait]` et 25 `[event]`.
- Le ralentissement tardif du run positif est réel dans le manifeste, mais sa
  cause n’est pas isolée.

### Hypothèses vérifiables

1. **Perte ou mauvaise attribution d’un réveil auto-reset** dans la fenêtre
   signal→wait: hypothèse la mieux soutenue par le contraste natif/`strace` et
   par la correction du choix de waiter, mais pas encore prouvée par un numéro
   de séquence POSIX.
2. **Suspension d’un thread alors qu’il tient une condition ou une section
   noyau**: compatible avec le retour positif sous perturbation de scheduling;
   à tester, pas à appliquer comme correctif AC6.
3. **Défaut distinct de performance tardive**: le seuil ~280 s ne doit pas être
   attribué au deadlock sans mesure de files, allocations et cadence audio.

## Instrumentation recommandée (signal/coût)

1. Ajouter un journal borné autour de `NtSignalAndWaitForSingleObjectEx`:
   thread invité, PC/LR, handles, type d’objet, état signalé avant/après,
   résultat, timeout et TID hôte.
2. Dans `PosixConditionBase::SignalAndWait`, enregistrer un compteur monotone
   par condition, l’instant d’enqueue/dequeue du waiter, le waiter choisi et
   chaque `notify_one`; cela détecte directement une perte de réveil sans
   perturber l’ordonnancement comme `strace`.
3. Corréler `XEvent` create/set/clear/pulse/reset, suspension/reprise et
   sections critiques au même compteur; conserver les handles bruts.
4. Rejouer un script neutral identique sur trois routes (native, patchée,
   `strace`) avec une fenêtre courte puis une fenêtre ~300 s; comparer seulement
   l’ordre des événements, pas les adresses hôte.
5. Continuer à compter swaps/audio et à marquer les phases, sans capturer ni
   suivre de shader, microcode, XMA décodé ou actif propriétaire.

## Verdict AC6

L’archive est valorisée et intégralement contrôlée. Elle fournit un oracle utile
pour prioriser `xboxkrnl`/scheduler et la défaillance POSIX, ainsi qu’une preuve
Xenia de titre → gameplay. Elle ne ferme aucune lane de la démo PAL native:
pas de SHA `Default.xex` qualifié dans l’archive, pas de pixel guest-owned,
pas d’audio XMA qualifié, pas de mission endogène. Le checkpoint renderer
cycle 1724 reste donc inchangé: readback guest-owned noir reproductible,
source EDRAM non nulle et trois readbacks non noirs encore requis.
