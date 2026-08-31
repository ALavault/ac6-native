# AC6 retail NTSC-U/J — r89 : le verrou de complétion confirme r87/r88, négatif borné (2026-08-31)

- TROUVÉ (statique, `FindStoresAtDisplacement.java 0x2abd`) :
  `sub_821E60A8` (0x821e60a8-0x821e61a0, corps complet vérifié) contient sa
  propre structure verrou/relecture, jusqu'ici non tracée : trois portes
  séquentielles (`object+0x2abc` bit 0x80 clair, un drapeau global non nul,
  `object+0x2abd` bit 0x2 clair) gardent un appel DIRECT à
  `sub_821E61A8(object, limite-2, 0)` — la même fonction d'attente que le
  thread bloqué — suivi d'un verrou de complétion à usage unique
  (`object+0x2abd` bit 0x2, posé en sortie).
- PROUVÉ (vivant, sonde mono-thread, lecture octet-par-octet à l'arrêt
  confirmé dans `__imp__sub_821E6AC8`) : `object+0x2abd` = `0x00` — le
  verrou n'a JAMAIS été posé pour cet objet. `object+0x540c` = tout zéro
  (rôle non établi ce cycle). `*(0x164e0000)+0x0` = octets bruts
  `05 00 00 00`, volontairement NON interprété numériquement — ce champ
  n'est écrit par aucun site connu de `native_guest_vd.cpp`
  (`drain_locked()` écrit `+0x3c`, pas `+0x0`), donc son sens grand-boutien
  vs. hôte reste ouvert plutôt que deviné.
- DÉCISION : ces lectures CONFIRMENT r87/r88 (le callback d'interruption ne
  se déclenche jamais → `sub_821E60A8` n'atteint jamais son verrou pour cet
  objet) par une preuve mémoire vivante indépendante, sans ouvrir de
  nouveau mécanisme de déblocage. Négatif borné pour ce sous-fil
  (`sub_821E6AC8`/`0x10001a00`) : cinq cycles (r85-r89) ont établi ce qui
  est prouvé et ce qui ne l'est pas; le passage statique sur les 76 sites
  d'appel de `sub_821E60A8` reste hors de portée pour l'instant. Retour à
  la liste scheduler/kernel/VFS/XAM plus large de NEXT.md au prochain
  cycle.
- HYGIÈNE SESSION : job cron ponctuel dupliqué `833b1b98` supprimé (restait
  actif en parallèle du job récurrent `1874bb39`); arrêt des rappels
  `ScheduleWakeup` en fin de cycle (mécanisme du mode dynamique, pas du
  mode à intervalle fixe) — les deux ensemble doublaient la cadence réelle
  de la boucle.

Preuve : `reports/ac6-retail-native-codegen-gate2-r89-latch-confirms-r87-bounded-negative-20260831.md`.

# AC6 retail NTSC-U/J — r88 : la chaîne r87 n'atteint pas l'écriture de déblocage (2026-08-31)

- CORRIGÉ : relecture précise de `sub_821E63F0` — l'offset `+0x10` du bloc
  `object+0x2a94` N'EST PAS un sentinel séparé, c'est le POINTEUR DE
  FONCTION du sous-callback lui-même (le test `0xBADF00D` est une garde
  anti-poison, pas une condition normale); `+0x14` est son contexte. Quand
  ce pointeur est NUL (confirmé : c'est le cas ici, le bloc est fraîchement
  `memset`é à zéro par `sub_821E65B0` et jamais réécrit — seul écrivain via
  déréférencement dans toute l'image : le handler lui-même, à l'offset
  +0x0, pas +0x10/+0x14), le gestionnaire d'interruption SAUTE
  intentionnellement l'appel du sous-callback.
- PROUVÉ (runtime, confirmation indépendante n°3 de l'identité d'objet) :
  `ctx.r3=0x821E63F0`, `ctx.r4=0x10001a00` au point d'entrée réel de
  `VdSetGraphicsInterruptCallback`.
- DÉCISION : même une correction du stub natif de cet import
  n'atteindrait PAS `sub_821E60A8`/`sub_821E5D60` par ce chemin — la
  chaîne causale de r87 ne tient pas telle que tracée. Rétractée comme
  mécanisme de correction (les faits bruts restent valables). Après quatre
  cycles à resserrer puis fermer des mécanismes spécifiques sans trouver
  la réponse finale, la prochaine étape doit reconsidérer la portée plutôt
  que proposer une cinquième hypothèse : soit documenter un négatif borné
  pour ce sous-fil et revenir à la liste plus large de NEXT.md
  (scheduler/kernel), soit s'engager dans un passage statique
  substantiellement plus coûteux (les 76 sites d'appel de
  `sub_821E60A8`) seulement si jugé utile.

Preuve : `reports/ac6-retail-native-codegen-gate2-r88-nested-callback-is-null-by-design-20260831.md`.

# AC6 retail NTSC-U/J — r87 : le callback d'interruption Vd retail n'est jamais invoqué (2026-08-31)

- PROUVÉ : `VdSetGraphicsInterruptCallback` a deux appelants statiques.
  `0x821f1220` enregistre réellement `callback=0x821E63F0, context=self`;
  `0x821f15fc` désenregistre (0,0), suivi d'un appel de fermeture à
  `sub_821E65B0` (chemin d'arrêt, sans rapport).
- PROUVÉ : `sub_821E63F0` est le vrai gestionnaire d'interruption Vd/CP —
  garde `source==1`, valide un sentinel de corruption (`0xBADF00D`, trap
  sinon), charge un SOUS-callback enregistré à `context+0x2a94+0x14` et
  l'invoque via `bctrl`, puis efface un bit de statut sous verrou.
- PROUVÉ : le stub natif `VdSetGraphicsInterruptCallback`
  (`materialize_native_import_stubs.py`) est un no-op complet — il
  n'enregistre rien et n'invoque jamais le callback. `sub_821E63F0` n'est
  donc JAMAIS appelé par ce runtime, en aucune circonstance.
- OUVERT : la chaîne du sous-callback (`context+0x2a94+0x14`) jusqu'à
  `sub_821E60A8`/`sub_821E5D60` (l'écriture de déblocage déjà identifiée)
  n'est pas encore tracée instruction par instruction — seule la forme est
  établie.
- DÉCISION : toujours aucune implémentation — tracer le sous-callback
  d'abord. Si confirmé, le correctif est de faire enregistrer et invoquer
  ce callback par le service Vd natif (via `PPC_LOOKUP_FUNC`, comme le
  stub `ExCreateThread`) lors d'une VRAIE progression d'anneau/IB observée
  dans `drain_locked()` — jamais sur un minuteur fixe ni inconditionnellement.

Preuve : `reports/ac6-retail-native-codegen-gate2-r87-interrupt-callback-never-fires-20260831.md`.

# AC6 retail NTSC-U/J — r86 : le pipeline PM4/Vd fonctionne; l'attente bloquée est un sous-allocateur adjacent (2026-08-31)

- PROUVÉ (trace `AC6_NATIVE_VD_TRACE=1`, diagnostic déjà existant dans
  `native_guest_vd.cpp`) : le pipeline PM4/Vd natif fonctionne — objet
  `0x10001a00` découvert, `PM4_ME_INIT` (19 dwords) puis le lot IB (12
  dwords) tous deux ACCEPTÉS. Rien n'est bloqué côté anneau graphique; ceci
  correspond exactement à ce qui était déjà établi (r56-r75, rapport r11).
- PROUVÉ : le readback réel écrit par `drain_locked()` est
  `object+0x2a90` (=`0x164e0000`) **+ 0x3c** = `0x164e003c`. Mais
  `sub_821E61A8` déréférence l'offset **+0x0** de ce même bloc — un champ
  différent. Séparément, `object+0x2a9c` (la "limite" comparée) est stable
  à **7**, sans rapport avec les indices d'écriture réels de l'anneau (19
  puis 31).
- REFORMULATION : l'attente générique `sub_821E61A8`/`sub_821E64A8`/
  `sub_821E65B0` (dont les faits de flot de contrôle de r77-r83 restent
  valables) est un **sous-allocateur séparé, adjacent** — probablement un
  tampon de mise en scène de liste de commandes partageant le même objet
  "périphérique graphique" que l'anneau Vd, mais drainé par un mécanisme
  différent, non encore identifié. Ce n'est probablement PAS un blocage du
  pipeline graphique lui-même.
- DÉCISION : ne rien implémenter. Prochaine étape : trouver quel code
  retail (pas le côté attente) écrit `object+0x2a90+0x0` directement (pas
  via le bloc readback `+0x3c`) pour comprendre ce compteur de mise en
  scène et son mécanisme de drainage réel.

Preuve : `reports/ac6-retail-native-codegen-gate2-r86-vd-pipeline-works-generic-wait-is-separate-20260831.md`.

# AC6 retail NTSC-U/J — r85 : CORRECTION, r77-r84 ont tracé le mauvais objet (2026-08-31)

- CORRIGÉ (majeur) : r78 a dérivé "l'objet bloqué" via `frame 1` + `print
  $rbp` pendant que GDB était arrêté dans `sub_821E6AC8` (frame 0) — une
  technique non fiable sur un binaire sans info de debug (le `%rbp` affiché
  reflétait probablement l'usage LOCAL de `sub_821E6AC8` lui-même, pas une
  valeur correctement dérouler pour la frame appelante). Cela a donné
  `0x1a0010`, jamais recoupé avant ce cycle.
- PROUVÉ : une méthode fiable (breakpoint à l'entrée BRUTE de
  `sub_821E64A8`, lecture directe de `ctx.r3` via `$rdi`, aucun changement
  de registre encore effectué) donne **`object = 0x10001a00`** — qui
  correspond EXACTEMENT à l'objet Vd/PM4 déjà établi dans
  `reports/ac6-retail-native-codegen-gate2-r11-20260831.md` (tranche r53,
  "l'objet à 0x10001a00, le ring 0x162d0000 et le readback"), une
  investigation totalement indépendante d'avant ce fil r77.
- PROUVÉ : avec l'objet correct, `+0x2a90` déréférence exactement l'adresse
  de readback déjà connue `0x164e0000`, et `+0x30` est un curseur proche de
  l'anneau déjà connu `0x162d0000`; `+0x2a9c=7`. Les trois champs sont
  STABLES sur 200 itérations de la boucle (sonde à thread unique,
  déterministe) — aucun des constats "objet null"/"contradiction" de
  r77-r84 ne s'applique à cet objet.
- DÉCISION : rétracter l'identité `0x1a0010` et tout ce qui en dépend
  directement (valeurs de champs, récits "allocation NULL"/"ordre
  d'init"/"contradiction de porte"). Les faits de flot de contrôle
  (structure de `sub_821E64A8`/`sub_821E61A8`/`sub_821E6AC8`/
  `sub_821E5D60`/`sub_821E6A08`) restent valables, lus depuis le
  désassemblage indépendamment de l'objet. Reconnecte l'investigation à
  l'item déjà ouvert r53 : "le consommateur PM4/Vd natif n'est pas encore
  relié" — prochaine étape : vérifier ce lien avec
  `native/src/native_guest_vd.cpp` avant toute nouvelle piste.

Preuve : `reports/ac6-retail-native-codegen-gate2-r85-wrong-object-corrected-20260831.md`.

# AC6 retail NTSC-U/J — r84 : bug d'endianness dans la méthode, chronologie réelle établie (2026-08-31)

- EXÉCUTÉ : la variante de sonde à thread unique proposée par r83 (no-op
  temporaire et non committé de `ExCreateThread`, changement révoqué après
  usage, CTest 9/9 revérifié après revert).
- CORRIGÉ (méthode) : `x/1xw` de GDB affiche la mémoire invitée (big-endian)
  comme si elle était little-endian. La lecture "0x05000000" de ce cycle
  était en réalité **5** une fois corrigée — cohérent avec l'init
  `sub_821E65B0` (= 3 si ancien était zéro, nouvellement lu ce cycle) plus
  un push via `sub_821E5D60` (+2, r79). Les lectures à zéro de r80-r83
  restent valables (zéro est invariant par endianness) — aucune conclusion
  antérieure à corriger.
- PROUVÉ : le premier appel à `sub_821E65B0` (objet frais, +0x2a9c=0) saute
  bien l'appel à `sub_821E64A8` (confirmé octet par octet sur le code x86
  compilé); le deuxième appel, avec +0x2a9c=5 (réel), entre dans l'appel —
  c'est CE MÊME appel qui finit bloqué.
- OUVERT (resserré, pas résolu) : les deux seuls écrivains statiques de
  +0x2a9c (`sub_821E5D60`: +=2; `sub_821E65B0`: =3 si zéro) n'écrivent
  jamais zéro. Comment le compteur redevient zéro plus tard dans le même
  appel bloqué reste sans écrivain identifié.
- DÉCISION : noter la correction d'endianness pour toute lecture GDB
  future de cette investigation. Prochaine étape : relire +0x2a9c à
  plusieurs points À L'INTÉRIEUR du même appel bloqué (pas seulement à la
  porte) avec la correction appliquée, sur la sonde à thread unique.

Preuve : `reports/ac6-retail-native-codegen-gate2-r84-endian-misread-and-timeline-20260831.md`.

# AC6 retail NTSC-U/J — r83 : l'escalade n'est que télémétrie; l'adresse de l'objet reste inexpliquée (2026-08-31)

- RÉFUTÉ : `sub_821E6A08` (l'escalade appelée au timeout de
  `sub_821E61A8`) ne touche aucun des champs `object` — c'est un compteur
  de performance pur (accumulation `mftb`, callback de profiling optionnel).
  N'explique pas la contradiction r82.
- NOUVEAU FAIT : `object=0x1a0010` (reproduit 5+ fois) est entièrement hors
  de l'image XEX statique (`0x82000400..0x82ac2dc7`) ET sous la plage de
  l'allocateur bump `NtAllocateVirtualMemory` (`0x10000000..0x7f000000`,
  r47). N'explique pas encore la contradiction, mais exclut une
  coïncidence avec les régions bootstrap PCR/thread (r49) et resserre
  l'hypothèse vers un pointeur du tas propre au jeu.
- DÉCISION : arrêter d'itérer des hypothèses une à une sur cette
  contradiction (rendements décroissants, même mise en garde que le scan
  d'appels à 76 sites de r79). Prochaine étape concrète : construire une
  variante de sonde à thread unique (no-op temporaire, non committé, du
  spawn `ExCreateThread`) pour éliminer les ~16-18 threads hôtes
  concurrents qui ont rendu les watchpoints peu fiables (r82), puis
  relire la porte de `sub_821E65B0` au point exact du check.

Preuve : `reports/ac6-retail-native-codegen-gate2-r83-escalation-is-telemetry-20260831.md`.

# AC6 retail NTSC-U/J — r82 : hypothèse r81 réfutée, nouvelle contradiction ouverte (2026-08-31)

- RÉFUTÉ : `sub_82331CA8` et le tronçon de `sub_821D5F48` autour de
  `0x821d6008` sont entièrement linéaires, sans branche conditionnelle.
  L'hypothèse r81 ("point d'entrée gardé prématurément satisfait") ne
  tient pas.
- OUVERT (contradiction non résolue) : la porte de `sub_821E65B0`
  (`0x821e65c4-0x821e65dc`) exige `object+0x2a9c!=0` ET `object+0x30!=0`
  pour atteindre `sub_821E64A8` — exactement la frame observée sur la pile
  de Thread 1 à chaque capture (r78/r80/r81, reproduit 5+ fois). Pourtant
  la lecture live de r80, plus profonde dans la même pile, montre ces deux
  champs à zéro, sans écriture intermédiaire trouvée ni autre thread
  touchant ces fonctions.
- INSTRUMENTS ESSAYÉS, NON CONCLUANTS : point d'arrêt conditionnel sur
  l'entrée de `sub_821E64A8` (jamais déclenché en 20s, probablement trop
  lent pour la fenêtre de blocage); watchpoints matériels/logiciels sur les
  trois champs (déclenchements bruyants attribués à des threads sans
  rapport — `NativeGuestVdService::poll_loop`, `NtClearEvent`,
  `NtWaitForSingleObjectEx` — cohérent avec une émulation de watchpoint
  logiciel peu fiable sur ~16-18 threads hôtes concurrents; écarté comme
  instrument inadapté ici, pas comme négatif fiable).
- DÉCISION : ne pas répéter ces deux expériences telles quelles. Prochaine
  étape : réduire la sonde à un seul thread vivant (stub temporaire de
  spawn `ExCreateThread`) avant de retenter, OU revérifier statiquement si
  la porte de `sub_821E65B0` opère sur de la mémoire fraîchement allouée
  potentiellement non significative la première fois, indépendamment de la
  question du handle de tas `0x8293B970`.

Preuve : `reports/ac6-retail-native-codegen-gate2-r82-gate-contradiction-open-20260831.md`.

# AC6 retail NTSC-U/J — r81 : le push d'anneau tourne avant la création de son propre tas (2026-08-31)

- PROUVÉ (runtime) : le global `0x8293B970` (handle de tas lu par
  `sub_821D74A8`) est entièrement à zéro au moment du blocage.
- PROUVÉ (statique) : le seul site d'écriture de `-0x4690(r31)` dans toute
  l'image est `sub_821D5F48:0x821d6200` — et `sub_821D5F48` est déjà une
  frame ancêtre dans notre propre pile capturée. L'appel qui descend vers le
  push d'anneau (`0x821d6008: bl sub_82331CA8`) précède l'écriture du handle
  (`0x821d6200`, via `bl sub_82222d80` à `0x821d61c8`) de 488 octets dans le
  MÊME flot d'exécution linéaire, sans boucle entre les deux.
- OUVERT : ce n'est probablement pas un bug d'ordre réel du jeu retail
  (peu plausible pour un titre shippé) — plus probablement, `sub_82331CA8`
  est un point d'entrée générique normalement gardé par un état/drapeau que
  notre couche HLE satisfait prématurément. Ce garde n'est pas encore
  localisé.
- DÉCISION : toujours aucune implémentation — écrire une valeur non-nulle
  synthétique masquerait un vrai bug d'ordonnancement plutôt que de le
  corriger. Prochaine étape statique : trouver ce qui garde `sub_82331CA8`
  (ou un ancêtre jusqu'à `_xstart`) de s'exécuter avant `0x821d6008` en
  exécution réelle.

Preuve : `reports/ac6-retail-native-codegen-gate2-r81-heap-init-ordering-20260831.md`.

# AC6 retail NTSC-U/J — r80 : l'allocation de l'objet bloqué a échoué (NULL), preuve runtime (2026-08-31)

- PROUVÉ (runtime, GDB sur la sonde bornée existante) : l'objet réellement
  bloqué est `0x1a0010` (reproduit identiquement sur deux runs
  indépendants), base mémoire invité `0x7ffef7000000`. `PPC_CONFIG_NON_VOLATILE_AS_LOCAL`
  est actif : r31/"object" vit dans `%rbp` hôte (établi depuis le prologue
  de `sub_821E61A8`), la base dans `%r14`.
- PROUVÉ : TOUS les champs pertinents de cet objet sont à zéro au moment du
  blocage — `+0x2abd=0x00`, `+0x540c=0`, `+0x34bc=0`, **`+0x2a90=0`**,
  `+0x2a9c=0`, `+0x30=0`, `+0x38=0`, `+0x2af8=0`, `+0x2a94=0`,
  `+0x3a44=0`, `+0x3a48=0`. L'objet n'a jamais été initialisé, ce n'est pas
  un anneau partiellement avancé.
- DIAGNOSTIC : `+0x2a90` (le pointeur déréférencé par l'attente) étant NULL,
  la comparaison `sub_821E61A8` est `0 >= 4`, faux par construction — la
  mémoire réservée non mappée se lit comme zéro sans fault. Le seul site
  d'écriture (`sub_821E65B0:0x821e6740`) stocke sans vérification le retour
  de l'allocateur `sub_821D74A8`; un retour NULL explique exactement l'état
  observé. Tracé un niveau plus loin : `sub_821D74A8` est un wrapper de pool
  avec verrou, qui appelle l'allocateur réel `sub_82222d80` via un handle de
  tas global (`-0x4690(r11)`); ce dernier n'est pas encore examiné.
- DÉCISION : ne rien implémenter. Prochaine étape : `sub_82222d80` et le
  handle de tas global — statique d'abord, puis vérifier si cela aboutit à
  un import kernel (`ExAllocatePool`-shaped) dont le stub générique
  `kOfflineStatus` de `materialize_native_import_stubs.py` serait la cause
  racine. N'écrire aucune valeur non-nulle synthétique dans `+0x2a90` avant
  cette identification.

Preuve : `reports/ac6-retail-native-codegen-gate2-r80-null-ring-allocation-20260831.md`.

# AC6 retail NTSC-U/J — r79 : l'objet est un allocateur générique, pas l'anneau Vd (2026-08-31)

- PROUVÉ : le seul site qui écrit `*(object+0x2a90)` avec la valeur qui
  débloquerait `sub_821E61A8` est `sub_821E5D60` (appelé depuis
  `sub_821E5E48`, lui-même appelé depuis `sub_821E60A8`), et cette écriture
  est conditionnelle à `object+0x540c == 0` ET un bit de `object+0x2abd`.
- PROUVÉ : la porte `sub_821E54B8` (appelée avant l'avance du curseur) n'est
  pas un simple booléen mais un **suballocateur** (bump-allocation depuis un
  pool `object+0x34bc`, appel indirect de secours, budget `+0x3a44/+0x3a48`);
  en échec il positionne le bit `0x20` de `object+0x2abd`.
- REFORMULATION : `sub_821E60A8` a 76 sites d'appel statiques répartis sur
  quasi tout le binaire (de `0x820fd2e8` à `0x821f1780`). L'objet tracé
  depuis r77/r78 est donc un allocateur de tampon de commandes générique
  réutilisé par le moteur, pas démontrablement l'anneau Vd/PM4.
- DÉCISION : arrêter le traçage statique du graphe d'appel (rendements
  décroissants sur un point d'entrée à 76 sites); la prochaine étape est une
  lecture runtime (GDB sur la sonde bornée déjà existante) des champs
  `object+0x2abd`, `+0x540c`, `+0x34bc`, `*(object+0x2a90)` et `+0x2a9c` au
  point d'arrêt déjà connu, pas une nouvelle recherche statique.

Preuve : `reports/ac6-retail-native-codegen-gate2-r79-generic-allocator-reframe-20260831.md`.

# AC6 retail NTSC-U/J — correction r78 : Thread 1 attend l'espace anneau, pas la fence (2026-08-31)

- CORRECTION : r77 a caractérisé le spin terminal `object+0x2AF8` de
  `sub_821E64A8` comme "le blocage réel" sans vérifier contre une trace
  capturée. Le GDB déjà existant pour r51
  (`artifacts/retail-us-native-build-gate2-r51-thread/entry-gdb-all/gdb.log:162-165`)
  montre Thread 1 (le thread invité, distinct des 16 workers hôtes) dans
  `sub_821E6AC8 → sub_821E61A8 → sub_821E64A8` — mais la pile confirme qu'il
  est dans l'appel conditionnel `bl 0x821e61a8` (espace anneau insuffisant),
  pas dans le fallthrough vers le spin `+0x2AF8`.
- PROUVÉ : `object+0x2a90` n'est pas un curseur mais un pointeur vers un
  bloc alloué de 0x60 octets (seul site d'écriture statique :
  `sub_821E65B0:0x821e6740`, juste après un appel allocateur). `sub_821E61A8`
  déréférence l'offset 0 de ce bloc et le compare à `object+0x2a9c`; la
  condition n'est jamais satisfaite, donc la boucle rappelle
  `sub_821E6AC8` sans jamais sortir.
- OUVERT : qui écrit l'offset 0 du bloc pointé par `object+0x2a90` n'est pas
  trouvé (recherche de déplacement plate insuffisante, la cible est
  `*(ptr)+0`); et si cet anneau est spécifique au graphisme ou un mécanisme
  générique réutilisé (8 appelants de `sub_821E64A8` très dispersés,
  `0x82172184`..`0x821f36xx`).
- DÉCISION : toujours aucun changement de code. Prochaine étape : suivre le
  pointeur retourné par l'allocateur (`bl 0x821d74a8` dans `sub_821E65B0`)
  pour trouver qui d'autre l'utilise, plutôt qu'un nouveau scan de
  déplacement plat.

Preuve : `reports/ac6-retail-native-codegen-gate2-r78-actual-wait-frame-20260831.md`.

# AC6 retail NTSC-U/J — Gate 2 la frontière post-IB est une fence, décrément non localisé (r77, 2026-08-31)

- PROUVÉ (statique, `ghidra-projects/ac6-us`) : le nom porté depuis r51
  ("sub_821E6AC8 → sub_821E61A8 → sub_821E64A8, attente queue/ring") est
  vérifié pour la première fois sur le binaire US retail lui-même (et non
  par analogie avec le cycle 295 PAL, qui concernait un binaire différent où
  la même valeur hex n'était pas une fonction). `sub_821E64A8` pousse un
  paquet de deux dwords dans l'anneau puis boucle sans borne ni retry sur
  `lwz r11,0x2af8(r31) / cmpwi r11,0 / bne` : c'est ce spin exact qui
  bloque la sonde.
- PROUVÉ : `object+0x2AF8` est un compteur/fence ajusté par `sub_821E5FD0`
  (verrou + `add r11,r11,r26`). Les quatre sites d'appel direct trouvés dans
  toute l'image (`0x821e5f80`, `0x821e6130`, `0x821ef2ac`, `0x821ef2f4`)
  chargent tous un delta de 0 ou +1 : aucun décrément direct n'existe
  statiquement. La seule écriture inconditionnelle à zéro est le chemin
  d'abandon `sub_821EFAF0`, atteint uniquement depuis le *second* wait
  (`sub_821E61A8`, borné à 5000 ticks via `sub_821E6AC8`), pas depuis le
  spin de `sub_821E64A8`.
- OUVERT : le décrément réel est donc indirect (probablement un callback
  d'interruption graphique/CP enregistré, cohérent avec r53 "le consommateur
  PM4/Vd natif n'est pas encore relié") ou passe par un chemin non encore
  localisé; `FindDirectCallsTo` ne voit pas les dispatchs `bctrl`.
- DÉCISION : ne pas implémenter de décrément tant que le déclencheur retail
  réel n'est pas localisé — écrire un décrément maintenant serait l'état
  synthétique que r53 a déjà refusé pour le readback ring. Prochaine étape :
  scanner les dispatchs indirects (`FindVirtualDispatchSlot.java`,
  `FindPpcBranchesTo.java`) autour de l'installation du callback
  d'interruption graphique plutôt qu'un nouveau scan d'appels directs.
- Aucun code natif, codegen ni test modifié ce cycle; CTest reste **9/9**
  (non affecté, aucune source C++/Python touchée).

Preuve : `reports/ac6-retail-native-codegen-gate2-r77-fence-frontier-20260831.md`.

# AC6 retail NTSC-U/J — Gate 2 runtime natif, codegen/liaison fermés (2026-08-31)

- PROUVÉ : receipt codegen r11 `pass` avec XenonAnalyse/XenonRecomp US,
  81 fichiers générés, 62 629 029 octets, zéro diagnostic et
  `unrecognized_instructions=[]`; les 70 intervalles switch et deux cibles
  externes sont configurés depuis la qualification US.
- PROUVÉ : guest généré compilé en 79 objets, lié avec
  `ppc_func_mapping.cpp` et 229 définitions d'imports offline; le probe forcé
  vérifie un symbole guest, `PPCFuncMappings` et la sentinelle, puis sort avec 0.
- PROUVÉ : build natif `gate2-codegen-linked`, CTest 9/9 et
  `tools/validate.py --target ntsc-uj --runtime native` passent.
- PROUVÉ : `ac6recomp` natif, `--self-test`, installation `bin/ac6recomp` et
  audit d'installation temporaire passent; le parseur XEX2 valide
  `assets/default.xex`, avec déchiffrement AES-CBC, compression basic et
  validation PE.
- PROUVÉ : `GuestAddressSpace` réserve 4 GiB virtuels, valide les bornes 32-bit
  et permet une vue à `0x82000000`; `NativeRuntime::boot()` exige cette
  réservation et écrit l'image XEX décodée.
- PROUVÉ : lecteur XDVDFS natif borné extrait `default.xex` de l'ISO US en
  mémoire, avec chemins insensibles à la casse, bornes et cycles rejetés.
- PROUVÉ : loader XEX2 natif déchiffre le payload AES-CBC retail, développe les
  blocs basic et mappe l'image `0xa98000` à `0x82000000`; probes assets et ISO
  sortent avec 0, validation PE Xenon incluse. Compression normal/LZX reste
  fail-closed.
- PROUVÉ : `ac6recomp` lie le guest r11 et exécute `sub_8209C0B4` avec base
  guest alignée après mapping XEX; smoke ABI neutre uniquement, aucun point
  d'entrée retail ni gameplay synthétique.
- PROUVÉ : table `PPC_LOOKUP_FUNC` host est peuplée pour 19 832 mappings
  générés, et l'entry `0x821f5ed0` se résout à `_xstart`; aucun appel entrypoint.
- TRACE BORNÉE : avec stubs TLS, fréquence Xenon, création handle et timeout
  événement offline, `_xstart` atteint `sub_821F9E10` en 12 s; la trace reste
  non qualifiée et le chemin normal n'appelle pas l'entrypoint.
- TRACE ÉTENDUE : même probe opt-in borné à 60 s finit `timeout`/124 sans
  retour; scheduler/état SDK restent la frontière causale ouverte.
- PROUVÉ r47 : le binding `NtAllocateVirtualMemory` réserve et zero-remplit
  une plage guest bornée `0x10000000..0x7f000000`; les bindings CRT pool et
  chaînes BE sont couverts par les tests ciblés, sans allocation hôte exposée.
- PROUVÉ r49 : le probe initialise un PCR/thread guest déterministe et place
  `r13` sur ce PCR; CTest 9/9 et pytest 122/122 restent verts.
- PROUVÉ r50 : `VdRetrainEDRAM`/HSIO et le cycle Vd d'initialisation renvoient
  le succès natif borné; la sonde dépasse la première attente EDRAM.
- PROUVÉ r51 : `ExCreateThread` valide les cibles générées, clone le contexte
  Xenon (r13 conservé), assigne une pile guest bornée et lance le shim dans un
  worker host; GDB observe 16 workers, tandis que le thread principal attend
  encore la queue/ring.
- PROUVÉ r52 : après synchronisation de la copie `native-source`, le contexte
  réellement exécuté entre avec `r1=0x8ff00000`, `r13=0x0f000000` et un PCR
  guest initialisé; le correctif n'est plus seulement présent dans le source.
- PROUVÉ r53 : `MmAllocatePhysicalMemoryEx` fournit les blocs guest du ring;
  `VdEnableRingBufferRPtrWriteBack` reçoit `0x164e003c` dans un bloc valide,
  avec readback 0 et write pointer 5 observés. Le consommateur PM4/Vd natif
  n'est pas encore relié, donc aucun pointeur synthétique n'est injecté.
- PROUVÉ r64–r75 : le service Vd observe le champ primaire qualifié
  `object+10952` (et non le curseur secondaire `+10908`), publie des indices
  dwords et écrit le readback exact `state+60`. La mémoire guest complète les
  IB; le paquet `PM4_ME_INIT` (19 dwords) et le lot bootstrap (12 dwords) sont
  décodés/acceptés sans ReXGlue. `PM4_REG_RMW`, `INVALIDATE_STATE`, binning,
  `EVENT_WRITE_SHD`, `IM_LOAD_IMMEDIATE` et `NOP` ont des enveloppes bornées.
- PROUVÉ r75 : build/CTest natif **9/9**, pytest retail **126/126**, audit
  d'installation et validator ordinaire passent. La sonde `_xstart` bornée
  (SDL dummy) expire après le lot IB; aucune entrée guest retournée, frame,
  mission ou save n'est revendiquée.
- PROVISOIRE r76 : les handles d'événements offline ont maintenant une table
  bornée (set/clear/pulse, auto/manual reset, wait et signal-and-wait sans
  blocage hôte). La sonde conserve l'acceptation des deux lots GPU mais expire
  encore au même point; cette tranche ne qualifie pas le scheduler Xenon.
- PROVISOIRE : les stubs build-only couvrent TLS, fréquence 50 MHz, mémoire
  virtuelle, pool/chaînes, PCR, handles/événements, Vd, mémoire physique et
  workers; ils ne remplacent pas le scheduler/kernel, le consommateur PM4 ou
  les bindings SDK qualifiés.
- OUVERT : les définitions d'imports sont des stubs HLE génériques, pas encore
  le runtime SDK Xenon/XAM/Vd/XMA; scheduler/événements restent bloqués après
  le lot IB, et la traduction `IM_LOAD_IMMEDIATE` vers SPIR-V n'est pas
  qualifiée. Aucun boot guest retail complet, gameplay M01, renderer
  présentable, campagne ou release n'est qualifié.
- DÉCISION : fermer le sous-gate codegen/liaison; migrer maintenant contexte,
  mémoire, scheduler/kernel, VFS/save, XAM/input, XMA/XAudio et Vd par
  tranches testées, puis atteindre M01 par code invité naturel. Le patch outil
  et le code généré restent ignorés, hors installation.

Preuve : `reports/ac6-retail-native-codegen-gate2-r11-20260831.md`.

# AC6 retail NTSC-U/J — Gate 2 corrections statiques, codegen ouvert (2026-08-30)

- PROUVÉ : PM4 hardware natif corrigé (TYPE0/TYPE1/TYPE2/TYPE3, prédicat,
  WAIT, DRAW_INDX, SET_CONSTANT, IB et XE_SWAP), avec capsule mise à jour.
- PROUVÉ : assertions des cinq tests C++ restent actives en Release; CTest 5/5,
  pytest retail 113/113 et `validate.py --runtime native` passent.
- PROUVÉ : ABI VMX128 à 128 registres, réservations PPC alignées/invalidation
  et CR0, saves case-insensitive; spans Vd possédés par le bridge.
- PROUVÉ : census statique de la table US: 229 imports uniques; la famille
  réseau est déclarée offline-error sans création de socket, sans binding natif
  encore installé.
- OUVERT : génération XenonRecomp directe; export Ghidra US 8 163 fonctions,
  sélection 89 propriétaires switch. L'essai complet expire à 20 min; l'essai
  propriétaire seul finit avec 2 881 diagnostics, puis le croisement mapping
  expire à 20 min (exit 124).
- DÉCISION : aucun code généré n'est compilé, lié ou installé; boot/gameplay,
  census des 229 imports et gates campagne/release restent bloqués par Gate 2.

Preuve : `reports/ac6-retail-native-gate2-static-correction-20260830.md`.

# AC6 retail NTSC-U/J — Gate 2 codegen r3 ouvert (2026-08-30)

- PROUVÉ : XenonAnalyse/XenonRecomp US direct produced 84 ignored files in a
  bounded r3 run; no oracle code linked.
- PROUVÉ : helpers ABI US `save/restgpr`, `save/restfpr`, `save/restvmx` et
  `save/restvmx_64` qualifiés par bytes; r3 conserve la réduction de huit
  diagnostics.
- PROUVÉ : `qualify_xenon_helpers.py` produit huit occurrences uniques et
  alignées dans `artifacts/retail-us-native-codegen-gate2/helpers.json`.
- OUVERT : receipt r3 `open-diagnostics`, 1 831 diagnostics (1 824 switch
  boundary errors, `dcbst`/`mulhdu`/`frsqrte` unrecognized). Output remains
  unqualified.
- DÉCISION : no generated C++ enters native runtime until helper/function
  boundaries are qualified and diagnostics reach zero.

Preuve : `reports/ac6-retail-native-codegen-gate2-r3-20260830.md`.

# AC6 retail NTSC-U/J — Gate 1 statique fermé, capture runtime ouverte (2026-08-30)

- PROUVÉ : profil `native` et capsule `ac6.xenos-capsule.v1` passent; PM4,
  MMIO, ring/IB, endian, EDRAM, shader boundary, services offline, kernel
  handles/timebase, ABI PPC et `NativeRuntime` lifecycle audit `nm` fail-closed.
- VALIDATION : 106 tests Python retail, CTest native 5/5, validator native
  pass avec `release_ready=false`.
- OUVERT : capture oracle read-only et replay Vulkan réel; aucun gameplay ou
  release n'est revendiqué.

Preuve : `reports/ac6-retail-native-gate1-20260830.md`.

# AC6 retail NTSC-U/J — Gate 0 fermé, Gate 1 actif (2026-08-30)

- DÉCISION : feuille active revenue à `recompilation/ace-combat-6-retail`;
  N2 de `reconstruction/ace-combat-6` abandonné pour cette feuille, sans
  restauration ni fusion de ses sources.
- PROUVÉ : identités US scellées (XEX, ISO, projet Ghidra `ac6-us`,
  AC6_recomp, route M01) et route `771a77…b6043`.
- PROUVÉ : XenonRecomp `ddd128…6ace` et XenosRecomp `990d03…69d1`
  restaurés en checkouts détachés propres, hors produit.
- CONSTAT : catalogue `.tools/knowledge-base/architecture-v1/catalog.json`
  absent; aucune assertion générique n'en est dérivée.
- VALIDATION : 94 tests Python retail passent.
- GATE ACTIF : capsule `ac6.xenos-capsule.v1` et renderer Xenos/Vulkan
  autonome, fail-closed, sans ReXGlue.

Preuve : `reports/ac6-retail-native-gate0-20260830.md`.

# Native US 2026-08-30 — N1b verte, N2 prêt

- PROUVÉ : deux processus natifs NTSC-U/J exécutent 3 600 ticks chacun avec
  replay, huit captures Vulkan et reçu identiques octet par octet.
- PROUVÉ : scène complète (cité, terrain, eau, F-16, HUD, pose/caméra live),
  `complete_render_scene=true`, `jv_eligible=true`, deux exits 0.
- PROUVÉ : CTest 88/88, scène US explicite, cache 926/926 et frontières
  source/ELF verts.
- DÉCISION : N1b fermée verte. N2 premier objectif est la prochaine frontière.

Preuve :
`reports/native-us-n1b-m01-b-20260830.md`.

# Native US 2026-08-30 — N1a verte, N1b prête

- PROUVÉ : deux processus natifs NTSC-U/J exécutent 1 800 ticks avec pose et
  caméra joueur live, terrain, cité, eau, F-16 et HUD Vulkan.
- PROUVÉ : pitch, roll, yaw, throttle et frein changent chacun le frame ; sept
  PPM, le manifeste et le replay sont identiques octet par octet entre runs.
- PROUVÉ : deux exits 0, reçu commun `74392499…869012c`, CTest 88/88 et scène
  US store-backed verte.
- DÉCISION : N1a fermée verte. N1b est la prochaine frontière ;
  `jv_eligible=false` demeure obligatoire jusque-là.

Preuve :
`reports/native-us-n1a-free-flight-20260830.md`.

# Native US 2026-08-30 — N0 vert, N1a actif

- PROUVÉ : cache NTSC-U/J complet, 926/926 entrées, index
  `d7071928…1a34df5b`, 5 409 550 519 octets décodés.
- PROUVÉ : contrat US séparé identité/route/payloads, build complet et 88/88
  CTest sans échec.
- PROUVÉ : frontière source/ELF et paquet 96 entrées verts, sans ReXGlue,
  Xenia, C++ généré ni octet retail.
- DÉCISION : N0 fermé vert. Gate actif N1a free-flight 1 800 ticks ;
  `jv_eligible=false` reste obligatoire.

Preuve :
`reports/native-us-n0-baseline-20260830.md`.

# Native US 2026-08-30 — R0 fermé négativement, N0 ouvert

- PROUVÉ : rebuild NTSC-U/J `46e30186…41e5a1c`, validation statique 16/16,
  cache Vulkan 193/193 et route unique qualifiée.
- PROUVÉ : `1AB6` atteint `PRESENT` et contient le HUD vert, mais la capture
  stable conserve un monde noir.
- ÉCHEC BORNÉ : après `sync-log`, Escape fait passer `world=1` à `world=0`;
  le wait post-edge expire, puis le runner force le teardown sans fatal guest.
- DÉCISION : aucun second run ReXGlue. Le produit actif devient
  `reconstruction/ace-combat-6`; gate N0 NTSC-U/J.

Preuve :
`reports/retail-us-r0-frontbuffer-validation-20260830.md`.

# Retail US 2026-08-30 — 0869 fermé, frontbuffer guest restauré en source

- PROUVÉ : le payload `08694231e8d17665:2+12` remplace les dérivées
  divergentes par LOD0 sur deux ressources `mip_range=0..0`; `tf10` reste
  inchangé.
- PROUVÉ : le run cache 193 `ca04c848…c4b5` conserve une scène non noire après
  `0869` et le dernier `50D9`, termine 86/86 et s'arrête proprement.
- PROUVÉ : l'ordre guest est `0311 → HUD/UI → resolve 1AB6 → PRESENT`.
- CORRIGÉ EN SOURCE : le fallback presenter, actif par défaut, substituait
  `1B9C` à `1AB6` et supprimait le HUD. Il est désormais opt-in.
- CORRIGÉ : `sync-log` interdit au wait post-Escape de consommer un ancien
  marqueur stable. 57 tests passent.
- NON VALIDÉ : le nouveau défaut presenter n'est pas encore rebuildé; aucun
  reçu gameplay/debrief/save n'est promu.

Preuve :
`reports/retail-us-0869-lod-present-frontbuffer-fix-20260830.md`.

# Retail US 2026-08-28 — service runtime fermé, contrat campagne v3 prêt

- PROUVÉ : le walker CRT atteint `0x823CB708`, qui construit `0x829BAF30`
  via `0x82120FD8` avant sa publication dans `0x823F9B28`.
- CORRIGÉ : le vptr de base est `0x8205CFC4`, non `0x8275CFC4`; le vptr
  effectif `CNuSound` est `0x8205D1A4` et `+0x168 -> 0x82124930` dépend de
  l'état `objet+0xCECC`. Aucun patch d'initialisation forcée.
- IMPLÉMENTÉ : gate/audit v3, débrief/visuel v2 automatisés, replay strict,
  save/cache chaînés et orchestrateur frais M01–M15. 84 tests passent.
- DÉCISION : le reçu M01 v2 devient historique (`gameplay_pass=0`). Le gate
  courant est un rebuild puis une seule observation read-only du service,
  renderer, heartbeat et teardown.

Preuve :
`reports/retail-us-mission01-campaign-service-owner-reachability-static-20260828.md`.

# Retail US 2026-08-28 — loader campagne statiquement qualifié

- PROUVÉ : dans le projet Ghidra US canonique, `0x8218F4F0` ne fait
  `state=0->1` qu'après résolution `0x821D3028` puis retour `>0` de
  `0x821D28C8`; l'agrégateur reste à zéro tant qu'une requête/enfant est
  incomplète et renvoie `-1` sur erreur.
- PROUVÉ : la tranche US `DATA.TBL[9]`/FHM enfant scénario 0 est complète et
  qualifiée ; l'hypothèse « scénario US absent » est fermée, sans preuve de
  complétude runtime du graphe.
- DÉCISION : l'absence de `state=1->2` dans le run analogique expiré ne permet
  pas de distinguer appel non effectué et ressources encore pendantes. Aucun
  nouveau runtime identique.
- PROCHAIN : qualifier statiquement ownership/ordre de vie du registre et du
  service `vtable+0x168`, puis `FlightActive`, postprocess/resolve et teardown.

La chaîne source `FlightActive`/heartbeat et le paquet MnK sont aussi qualifiés
(activation par timestamp `<300 ms`, wrappers physiques joueur, layout
`LT/RT/LX/LY/RX/RY`), mais aucun heartbeat runtime n'a été observé.
Le global US `0x823F9B28` pointe initialement `CAce6Sound`; son slot
`vtable+0x168` cible `0x823A2C30` (`li r3,0; blr`). La statique qualifie aussi
les sites d'écriture vers `0x829BAF30` (`0x821D6380`) puis un vptr candidat
`0x8275CFC4` (`0x823CE6AC`). Leur reachability normale, le slot terminal
effectif et l'ordre de vie restent ouverts ; ne pas généraliser le résultat du
vtable initial au runtime.

Preuves : `reports/retail-us-mission01-campaign-loader-static-20260828.md`,
`reports/retail-us-mission01-campaign-service-owner-static-20260828.md`,
`reports/retail-us-mission01-flightactive-static-20260828.md`.

# Retail US 2026-08-28 — sonde analogique bornée, frontière toujours ouverte

- PROUVÉ : le rebuild `job-mtct7k4h-0be226ed` est validé et installé sous le
  binaire `e93d9fbe…bd8115` (37 804 064 octets), ReXGlue/Vulkan, 16/16 tests,
  `bin/bin` absent.
- ÉCHEC BORNÉ : `job-mtctdvii-e9c22860` atteint seulement `2/96` sur la route
  longstart; `type28=30` manque après 249,461 s, aucune ligne analogique,
  `game_status=-9`, zéro fatal/trap.
- DÉCISION : ce run n’apporte aucune preuve d’entrée. La route originale
  qualifiée est maintenant explicitement admise au harness diagnostic, sans
  modifier le gate produit; une corrélation read-only unique est en cours sous
  `job-mtctm90d-3d748f51`.
- ÉTAT : M01 gameplay v2, débrief/save checkpoint, missions 02–15, parité
  shader/lumière et `release_ready` restent non promus; PAL reste bloqué.

Preuve : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — corrélation analogique originale expirée

- ÉCHEC BORNÉ : `job-mtctm90d-3d748f51` a expiré à 20 min (`exit 124`) avec
  le binaire `e93d9fbe…bd8115`, la route originale scellée et le seed complet;
  aucun `RESULT.json` final n’a été écrit.
- PROUVÉ : l’artefact brut contient 50 captures, `5 587 PRESENT`, `94 type28`
  et des boutons XAM non nuls (`0x0004/0x0010/0x1000/0x2000`), mais aucun
  `[ac6-campaign-transition] state=1->2`, aucune ligne analogique et aucune
  phase `world=1/hud=1`.
- DÉCISION : corrélation non concluante, sans attribution renderer/input;
  aucune répétition. Reprendre statiquement transition campagne, `FlightActive`,
  postprocess/resolve et teardown; M01 v2, débrief/save, missions 02–15 et PAL
  restent bloqués.

Preuve : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — gameplay v2 candidat, sortie cinématique ouverte

- PROUVÉ : le binaire `b004ee70…4d70` est buildé/validé et porte les
  observables read-only du scheduler (`0x822ED310→0x82267160`) et de
  l'objectif (`0x82256490→0x8226C068`).
- PROUVÉ : la sonde HUD diagnostique atteint `cinematic=0 world=1 hud=1
  stable=30` et produit cinq contrôles visuellement différents.
- ÉCHEC BORNÉ : deux essais vierges calent à `type28=30`; le run cache hôte
  atteint l'étape 75 mais manque la phase stable après l'edge `Escape` fixe.
- ÉTAT : le manifeste de campagne garde le reçu gameplay historique attaché
  à `d8b7b7b3…`; `debrief_pass=0`, `release_ready=false` et PAL reste bloqué.
- PROCHAIN : une unique route diagnostique adaptative, puis seulement un
  replay v2 et l'audit; pas de correction globale renderer, trace globale,
  A/B, guest write ou ouverture des missions 02–15 avant succès M01.

Preuve : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — frontière scheduler US qualifiée

- PROUVÉ : le projet Ghidra canonique `ghidra-projects/ac6-us` (`default.xex`,
  XEX `6eefba42…67cbbbc`) borne `0x822ED310..0x822ED467` comme handler du
  signal `-2`, avec gardes contexte/objet puis appel de `0x82267160`.
- PROUVÉ : `0x82267160..0x8226723B` avance le pas selon le délai du scénario,
  remet le compteur de temps à zéro et retourne `1` à l'épuisement ; les
  appels directs US observés sont `0x82258F8C` et `0x822ED408`.
- PROUVÉ : `0x822ED708` et `0x8226E158` sont des offsets internes, non des
  entrées de fonction ; aucune frontière scheduler ne reste ambiguë.
- OUVERT : l'activation native objet/arme/compteur qui doit atteindre `-2` en
  vol ; aucun runtime ni gameplay/debrief n'est promu par cette lecture.
- DÉCISION : préparer une seule trace runtime bornée et read-only des gardes,
  du signal, du retour d'avance et du premier compteur M01.

Preuve : `reports/retail-us-mission01-scheduler-static-20260828.md`.

# Retail US 2026-08-28 — timer M01 sans entrée, auto-complétion écartée

- PROUVÉ : avec le binaire candidat `54dd900a…5711f9`, la route
  `mission01-roundtrip-timer-candidate.steps` atteint le HUD stable puis reste
  sans entrée pendant `300 s` ; 96/96 étapes et 28 captures sont produites.
- PROUVÉ : arrêt propre après `755,014796 s`, `game_status=0`, aucune
  correspondance fatale et aucun trap/timeout ; `selector=1 value=1` et les
  transitions campagne `0→1→2` restent visibles.
- NON PROUVÉ : aucun `[ac6-post-mission]`, terminal `0x822E3248`, setter niveau
  2, débrief ou checkpoint de sauvegarde pendant la fenêtre temporisée.
- DÉCISION : l'hypothèse « le scénario s'auto-complète au repos » est fermée.
  Le gate reste le round-trip M01 ; qualifier statiquement les objectifs,
  unités et compteurs US avant toute nouvelle entrée de vol ciblée.

Preuve : `reports/retail-us-mission01-roundtrip-timer-diagnostic-20260828.md`.

# Retail US 2026-08-27 — handoff corrigé atteint le HUD, visuel encore ouvert

Après intégration du settle/hold Campaign dans le harness diagnostique, un run
unique `--mission-cinematic-handoff` atteint briefing, cinématique et la phase
`cinematic=0 world=1 hud=1 stable=30`. Le résultat est
`diagnostic-capture-ready`, `23` captures, arrêt propre (`game_status=0`) ; le
centre HUD est non noir (`mean=0.301670`, `stddev=0.302621`, nonblack `1.0`).
L’avion et les hautes lumières restent surexposés, et aucun contrôle ou débrief
n’est crédité. Le gate round-trip M01 reste ouvert.

Preuve : `reports/retail-us-mission01-cinematic-handoff-candidate-20260828-r2.md`.

# Retail US 2026-08-28 — bytes scénario US qualifiés par tranche

- PROUVÉ : `DATA.TBL[9]` de l’ISO US qualifiée a été lu sur sa seule plage
  `offset=16908288`, `length=13234635` ; le payload décompressé et l’enfant FHM
  0 ont été vérifiés par SHA-256.
- PROUVÉ : l’enfant scénario (`51c10abe…ac6d45`) passe le probe natif avec
  `root_slots=10`, 230 unités, 434 objets, quatre sous-missions et
  `reader_runs=666`, `reader_failure=null`.
- DÉCISION : l’absence de ressource US est fermée pour Mission 01 ; aucune
  preuve PAL n’est transférée et aucun payload n’est copié dans le produit.
- OUVERT : progression naturelle, débrief, setter niveau 2 et round-trip save.

Preuve : `reports/retail-us-mission01-scenario-static-qualification-20260828.md`.

# Retail US 2026-08-27 — confirmation campagne qualifiée par settle/hold

La route ciblée `mission01-campaign-confirm-candidate.steps` a utilisé une
seule différence d’entrée nommée : settle `8 s`, puis edges `space` tenus
`0,6 s` sur les écrans de configuration. Elle produit des captures distinctes
pour difficulté/contrôles/langue, le getter
`selector=1 value=1 lr=0x821BA918`, puis les transitions campagne
`state=0->1` et `state=1->2`. Le diagnostic Vulkan a été arrêté après sa
capture (`RESULT.status=fail` technique, `game_status=-9`) sans fatal/trap ; il
ne constitue pas un reçu gameplay.

La couture d’entrée est donc fermée et sa recette est réutilisable dans le
round-trip naturel M01. Aucun override renderer, A/B ou guest write n’est
autorisé ; `debrief_pass=0` reste inchangé. Preuve :
`reports/retail-us-mission01-campaign-confirm-candidate-20260828.md`.

# Retail US 2026-08-27 — handoff campagne figé après profil

La variante `--mission-cinematic-handoff` a franchi le profil/Game Data avec le
candidat Vulkan `54dd900a…5711f9`, puis est restée sur `CAMPAIGN / NEW GAME`
après la langue. Aucun `[ac6-campaign-transition] state=1->2` n'est apparu ;
`4 610` `PRESENT` ont été produits et deux captures à 12 s sont identiques
(`62a77bf1…`). L'arrêt cgroup contrôlé porte le statut `15`, sans fatal/trap.

Ce diagnostic borne une seconde couture d'entrée, sans attribution au renderer
et sans autoriser A/B, guest write ou répétition identique. Le gate reste le
round-trip naturel M01 ; preuve :
`reports/retail-us-mission01-cinematic-handoff-candidate-20260828.md`.

# Retail US 2026-08-27 — amorce longue figée, gate round-trip conservé

La route diagnostique `mission01-flight-long.steps` a été exécutée une seule
fois avec le candidat Vulkan `54dd900a…5711f9`. Après `8 min 25 s`, le movie
worker produisait encore des réveils et `349` `PRESENT`, mais aucun marqueur de
campagne, `type28` ou `selector44` n'était publié ; seules deux phases
`world=0`/`stable=0` sont apparues. Les captures Xvfb à `23:07:40` et `23:12:21`
ont le même SHA-256 `87a84b42…`. Le scope a été arrêté borné avec le statut
`15`.

Ce résultat est un diagnostic fail-closed de l'amorce, pas un reçu de gameplay
et pas une attribution à Vulkan. Il n'autorise ni override renderer, ni A/B,
ni nouvelle exécution identique. Le gate reste le round-trip naturel M01 ; le
rapport complet est `reports/retail-us-mission01-flight-long-candidate-20260828.md`.

# Retail US 2026-08-27 — tick monde vivant, phase visuelle encore instable

La capture diagnostique bornée du candidat `54dd900a…5711f9` confirme
`0x8226CEA0`/caméra actifs et une charge frontier Vulkan substantielle. Les 93
échantillons montrent `object:0`, `camera:1` sur 85 appels après
initialisation ; l'update objet `0x822704A0` n'est pas appelé par le tick et son
unique référence directe reste la route `0x82256490`. La phase visuelle ne
tient jamais 30 frames ; aucune correction renderer globale n'est retenue.

Le gate reste le round-trip M01 naturel (débrief, setter niveau 2, save
quiescent, relecture fraîche niveau 2). Le diagnostic est enregistré dans
`reports/retail-us-mission01-world-owner-diagnostic-20260827.md` et ne ferme
pas le gameplay.

# Retail US 2026-08-27 — candidat round-trip Mission 01 prêt

Quatre observables read-only qualifiés sont liés : entrée InterMissionSelect,
setter de niveau filtré aux callsites `0x821A64D0/0x821A64E8`, transitions
brutes du save manager et lectures de niveau sur changement. Build sans
codegen et validation statique réussis : Python 65/65, natif 16/16, Vulkan,
SDL dummy, zéro D3D12, `bin/bin` absent. Le candidat
`54dd900a…5711f9` n'est pas encore promu ; le round-trip runtime reste à faire.

Preuve : `reports/retail-us-mission01-persistence-observables-build-20260827.md`.

# Retail US 2026-08-27 — chaîne débrief/progression Mission 01 qualifiée

La fin monde `0x822E3248` est reliée canoniquement à Debriefing,
DemoIntermission, Unlock puis InterMissionSelect. `0x821A6400` porte
l'incrément de niveau `1 -> 2`, enregistre son `CSelectSaveLoadManager` via
`0x82158D00`, l'actualise via `0x82158D90`, puis publie MissionTitle. La
persistance effective et la relecture après redémarrage restent non prouvées ;
le gate courant devient un round-trip runtime borné, sans A/B ni trace globale.

Preuve : `reports/retail-us-mission01-debrief-progression-static-20260827.md`.

# Retail US 2026-08-27 — quinze routes DPL/DATA.TBL qualifiées

La frontière statique campagne US est fermée sans corriger artificiellement
Ghidra : `0x821CD168` est une feuille appelée directement entre les entrées
`.pdata` `0x821CD0D0` et `0x821CD2E8`. Le loader actif flag-2 publie la borne du
`DATA.TBL` à `0x8293B950`, puis la feuille range les DPL directs inchangés.
L'extraction XDVDFS du seul `DATA.TBL` depuis l'ISO exacte donne 14 824 octets,
926 entrées et le SHA-256 `bad3a157…863b2f`, distinct du PAL.

Le manifeste promeut les quinze routes statiques `1..15 → 9..23 → 9..23`.
Validation : Python 65/65, natif 16/16, `validate.py` pass ; campagne
`static_qualified=15`, `gameplay_pass=1`, `debrief_pass=0`, release non prête.
Aucun codegen ni runtime n'a été lancé. Le gate courant devient la qualification
statique de la fin Mission 01, du débrief et de l'écriture de progression.

Preuve : `reports/retail-us-campaign-manifest-static-20260827.md`.

# Retail US 2026-08-27 — manifeste 15 missions, frontière DPL ouverte

Le sélecteur US canonique `0x821B6EE8` lit `0x820657B0` et qualifie les
missions `1..15` vers les DPL `9..23`. Le manifeste campagne fail-closed est
intégré à `validate.py` : 15 sélecteurs qualifiés, 0 route DATA.TBL complète,
1 gameplay, 0 débrief, release non prête. La requête `0x821D1190` et le loader
`0x821CC288` sont qualifiés ; le call target `0x821CD168` doit encore être
réconcilié avec la `.pdata` canonique, puis le `DATA.TBL` US extrait de l'ISO
exacte doit être identifié. Aucun runtime ni codegen n'a été lancé.

Preuve : `reports/retail-us-campaign-manifest-static-20260827.md`.

# Retail US 2026-08-27 — gate v2 construit, capture D5B4 suivante

Le gate NTSC-U/J v2 est construit et validé statiquement avec le binaire
`b869e2b1…61fde55`. Il impose 96/96, 27 captures, une phase HUD stable hors
cinématique, un centre de monde non noir et cinq effets de contrôle. Aucun
runtime n'a suivi le build dans cette session lourde. Le prochain gate est une
capture RenderDoc unique `cinematic-d5b4` dans une nouvelle session. PAL reste
fermé.

# Retail US 2026-08-27 — F556 qualifié, resolve inconcluant

Le premier draw F556 reçoit un fetch `tf0` valide (format 6, endian 2, tuilé,
256×256), clamp edge et bordure noire : la piste de bordure blanche est
fermée. `0x1C95E000` est une destination de resolve GPU; les zéros d'une
lecture CPU sont stale. Le probe de contenu resolve n'a pas quitté le hangar,
donc aucune correction n'est retenue. Le gate US reste fermé et le PAL bloqué.

La route 96 a aussi été reclassée : ses captures `step-84`/`87`/`90`/`93`/`96`
sont la cinématique pré-mission, pas le vol. Seule la route longue
`flight-long2/step-87` montre un HUD/radar retail plausible au début du
gameplay, mais sur un monde entièrement noir.

Preuve : `reports/retail-us-f556-sampler-resolve-20260827.md`.

# Retail US 2026-08-27 — qualification des surfaces blanches

La cinématique montre désormais le relief et les avions, mais l'aplat D5B4
orientation-dépendant et les taches blanches de la passe point-list F556 restent
distincts. D5B4 a une cible et un fetch BC3 valides; F556 passe par une
expansion point-sprite Vulkan correcte, avec le fetch/sampler `tf0` reçu. La
prochaine preuve doit qualifier le contenu resolve/load sans confondre cette
cinématique avec le HUD de gameplay.

Preuve : `reports/retail-us-white-texture-static-boundary-20260827.md`.

# Retail US 2026-08-27 — water-gradient inconcluant

La sonde unique `ac6_fix_water_line=false` réutilise le binaire validé et
termine proprement 89/89 opérations, sans fatal ni trap. Elle ne produit
toutefois aucune vue de vol : après `step-69-post-weapon-confirm`, les captures
restent identiques au hangar « Deploy with this selection? ». Elle ne confirme
ni n'exclut donc la passe water-gradient; aucune nouvelle session n'est lancée
sur ce seul résultat.

Preuve : `reports/retail-us-stock-water-runtime-20260827.md`.

# Retail US 2026-08-27 — textures blanches : dé-swizzle et FBO écartés

Le bras comparatif `--ac6_fix_deswizzle=false` conserve les surfaces blanches
selon l'orientation; le correctif AC6 reste activé. Les observations BC3/D5B4
restent valides (format 20, tuilé, ressource présente), ce qui déplace la cause
vers la lumière/composition. Un bras unique `render_target_path_vulkan=fbo`
produit une image initiale mais n'atteint pas `type28=30` en 120 s; FSI reste
le chemin qualifié. Aucun reçu gameplay ni PAL n'est ouvert.

Preuve : `reports/retail-us-stock-deswizzle-runtime-20260827.md`.

# Retail US 2026-08-27 — FSI/post-process/frontbuffer borné

Le tooling renderer utilisateur comprend désormais `glslc` (shaderc 2026.1-1),
en plus de SPIR-V Tools/glslang et de la couche Khronos. Un probe distinct de
séquence complète (96 opérations) a atteint la campagne mais n'a pas produit
`[ac6-campaign-transition]` et a été interrompu à `type28=30`; il n'ajoute
aucun reçu gameplay. La route scellée 96/96 reste seulement mécanique : ses
captures de vol ne montrent pas le HUD de cockpit. PAL reste donc fermé.

Le binaire US `a150da4022425d0477e20ecfaa06cd73162fa44074f409dac8edaffa36974de9`
est statiquement validé avec FSI ReXGlue/Vulkan. Les outils SPIR-V installés
en espace utilisateur valident les 93 shaders (`spirv-val` 93/93 et réflexion
`spirv-cross` 93/93). Une route longue propre atteint la cinématique 3D
surexposée, puis le monde noir avec HUD invité ; aucun reçu Mission 01 de
gameplay complet n'est encore obtenu.

La frontière restante est bornée aux passes post-process après le compose et
au handoff final `0x1B9C0000` → `0x1AB60000`. Le fetch swap est chargé sans
erreur, mais la sortie présentée devient noire. Cette observation ne suffit
pas à choisir un shader ou une barrière ; pas de nouveau rebuild/codegen et
PAL reste fermé.

Preuves : reports/retail-us-fsi-handoff-logged-runtime-20260827.md et
reports/retail-us-flight-long2-runtime-20260827.md.

# Retail US 2026-08-26 — observable propriétaire validé

Le rebuild sans codegen passe et installe `64f34acf…46af5b8`. Les cinq wrappers
CModeTaskGame/tick/objet/caméra/radio sont liés ; la cvar et les marqueurs sont
exigés par la validation. 16/16 tests, Vulkan-only, SDL dummy, zéro D3D12 et
zéro `bin/bin`. Le prochain runtime causal attend une nouvelle session lourde.

Preuve : reports/retail-us-world-owner-observable-rebuild-20260826.md.

# Retail US 2026-08-26 — propriétaires US qualifiés

Ghidra canonique US et les corps générés convergent sur CModeTaskGame
`0x8219A510`, tick monde `0x8226CEA0`, objet `0x822704A0`, caméra `0x822638B0`
et radio `0x82271908`; `0x8219A170` est réfuté. Étendues `.pdata`, octets,
références et appels sont reçus. Un observable read-only borné est prêt ; son
rebuild sans codegen attend une nouvelle session lourde.

Preuve : reports/retail-us-world-owner-static-qualification-20260826.md.

# Retail US 2026-08-26 — soumission du monde effondrée au handoff

Le hangar est franchi et le probe atteint proprement le HUD noir sans panneau.
La cinématique soumet 969–975 draws, 91 resolves et 32 pointlists ; le handoff
chute de 1 197/84/31 à une signature stable 106/1/0 jusqu'au HUD. Le viewport et
la `guest_swap_texture` restent 1280x720. La rupture est en amont du swap, dans
l'activation/dispatch/soumission du monde. Runtime consommé : 19.

Preuve : reports/retail-us-flight-world-submission-collapse-20260826.md.

# Retail US 2026-08-26 — divergence bornée au hangar sans panneau

Le binaire validé termine proprement le probe et le panneau vert est absent.
L'audit visuel montre toutefois le même hangar `Deploy` des étapes 70 à 87 : le
premier A immédiat n'est pas accepté. Les étapes antérieures, le focus et les
présentations restent corrects. Une attente de deux secondes précède désormais
l'unique A du hangar. Runtime consommé : 18.

Preuve : reports/retail-us-no-panel-hangar-divergence-20260826.md.

# Retail US 2026-08-26 — rebuild résumé persistant sans panneau

Le rebuild unique réutilise le code généré et passe avec un binaire installé
`60c9fc6a…15da94`. Les 16 tests ciblés, Vulkan-only, SDL dummy, l'absence de
D3D12 et de `bin/bin` sont validés. Le dernier résumé non vide est conservé et
le panneau vert est opt-in, donc masqué par défaut. Le prochain runtime, dans
une nouvelle session lourde, journalisera les signatures bornées sans panneau.

Preuve : reports/retail-us-last-meaningful-rebuild-20260826.md.

# Retail US 2026-08-26 — résumé de frame non autoritatif

Le probe atteint 87 opérations et les captures cinématique 3D/HUD noir, mais
reste un échec fermé (`game_status=-9`, arrêt non propre). Les compteurs sont
nuls même sur la frame 3D visible : la frontière PRESENT vide écrase le dernier
résumé utile. Les appels générés sont bien liés aux wrappers. La source conserve
maintenant la dernière frame non vide ; aucun second job lourd dans cette
session. Ce correctif et le défaut qui masque le panneau vert attendent le
prochain rebuild unique.

Preuve : reports/retail-us-black-world-frame-summary-20260826.md.

# Retail US 2026-08-26 — HUD atteint, monde noir

Le seuil 180 PRESENT stabilise le démarrage. La route atteint le mode vol :
les primitives HUD sont visibles après A/Start, mais le monde 3D est noir.
Le run de 87 opérations termine proprement sans fatal ni trap. Le prochain
probe active uniquement le résumé de frame borné afin de compter draws,
clears, resolves, RT et viewport sur cette frame noire.

Preuve : reports/retail-us-hud-black-world-20260826.md.

# Retail US 2026-08-26 — seuil 1 PRESENT réfuté

Après attente du premier PRESENT, l'Escape initial arrête immédiatement toute
présentation : une seule frame totale, aucun type28, aucune capture, fatal,
trap ou erreur audio. Le run est interrompu par TERM au done_when et reste un
échec. Le prochain probe attend les 180 PRESENT historiquement qualifiés avant
toute entrée.

Preuve : reports/retail-us-cinematic-hud-one-present-20260826.md.

# Retail US 2026-08-26 — probe HUD non exercé

La tentative de handoff HUD s'est figée avant type28 après 1 630 PRESENT,
sans capture, fatal, trap ni erreur audio. Le focus et les impulsions restaient
actifs. Le timeout 124 sans RESULT.json classe la session en échec interrompu.
Le prochain probe attend obligatoirement le premier PRESENT avant toute entrée,
ce qui fixe la divergence temporelle nommée.

Preuve : reports/retail-us-cinematic-hud-startup-stall-20260826.md.

# Retail US 2026-08-26 — cinématique de lancement atteinte

La confirmation A depuis la carte tactique lance la cinématique 3D du F-16.
Les captures à 15 et 35 s montrent l'avion; à 65 s, la transition reste
floutée sans HUD. Le run termine proprement sans fatal ni trap. Le prochain
probe applique la recette historique A puis Start et s'arrête au premier HUD
candidat, sans contrôle de vol.

Preuve : reports/retail-us-tactical-map-confirm-20260826.md.

# Retail US 2026-08-26 — premier hangar franchi

Le probe exact confirme A depuis le premier hangar et atteint la carte tactique
Mission 01. Celle-ci reste stable jusqu'à 65 secondes avec A OK : elle attend
une seconde confirmation explicite. Le run est propre, sans fatal ni trap.
Le prochain probe ajoute uniquement cet A sur la carte.

Preuve : reports/retail-us-first-hangar-confirm-20260826.md.

# Retail US 2026-08-26 — divergence au premier hangar

Les captures ordonnées prouvent que Space progresse avion → armes → premier
hangar, puis que Shift renvoie aux armes. Le Space qualifié jusque-là comme
« lancement » ne faisait que revenir au hangar. Aucun probe précédent n'a
donc confirmé A depuis ce hangar. Le prochain probe s'arrête à l'opération 69,
confirme A une fois, puis reste passif.

Le panneau vert vient de l'appel Show() de l'application. Le nouveau cvar
ac6_graphics_diagnostics est désactivé par défaut dans la source, mais le
binaire installé n'est pas encore rebâti.

Preuve : reports/retail-us-hangar-route-divergence-20260826.md.

# Retail US 2026-08-26 — pression courte Mission 01 réfutée

Le diagnostic US atteint le hangar Mission 01 et observe la transition campagne
0→1→2. Après le seul Space de lancement tenu 0,1 s, les captures à 50, 70 et
100 s restent au hangar A OK / B CANCEL. La pression courte n'est donc pas une
confirmation suffisante. Le prochain probe borne uniquement ce même bouton à
0,6 s; aucune autre touche post-lancement n'est autorisée.

Preuve : reports/retail-us-mission-launch-short-press-20260826.md.

# Cycle 1849 — timeline marque normale, frontière enfant fermée

La négative à 3 200 ticks est requalifiée : le MovieController racine
`0x2E3CDD10` possède 2 220 frames et n'atteint sa dernière frame qu'autour du
tick 6 882. Son avancement est produit normalement par `0x82323BB8`, cadencé
par `0x82322438`; la promotion parent/enfant `+0xE4/+0xE8 -> +0xF4` existe
déjà. L'instantané `0x2E3CED10+0xDC=0` ne démontre donc aucun cue manquant et
ne contrôle pas EndMode.

La jointure exacte de la dernière entrée parent au cue Startup reste statique,
mais elle n'est pas requise pour le prochain test causal : un probe unique,
sans entrée, jusqu'à 7 200 ticks doit observer Title après le film naturel.
Aucun patch produit ni état de succès n'est promu.

Preuve : `reports/cycle-1849-demo-brand-movie-timeline-requalified.md` et
`artifacts/cycle-1849/`.

# Cycle 1848 — MovieController atteint, cue enfant ouvert

Le probe PAL démo HSIO=1 sans entrée atteint 3 200 ticks avec ring actif
(`1 182` soumissions, `1 089` présentations), sans frontend, mission ni
terminal. Le parent MovieController `0x2E3CDD10` avance, alors que son enfant
`0x2E3CED10` reste sur `DC=0` pendant la fenêtre de trace, malgré `D5=1` et
`D6=0`. Aucun appel EndMode n'apparaît.

Le garde connu de `0x82323BB8` est donc permissif; aucun patch de ces champs
n'est causal. La suite est une jointure Ghidra statique parent→enfant pour
nommer le cue/producteur réel, sans autre runtime ni modification produit.

Preuve : `reports/cycle-1848-demo-moviecontroller-runtime-negative.md` et
`artifacts/cycle-1848/`.

# Cycle 1847 — dispatch render post-fence non causal

Le plateau `0x822E559C -> 0x822F8848` est maintenant qualifié comme un update
render/context du main loop : `0x822E559C` est le LR du dispatch indirect dans
`0x822E5540`, non une entrée ni un wait. Son corps ne touche pas la chaîne
StartUp/EndMode/manager; aucun correctif n'est causal.

La frontière revient au flux guest SWG déjà nommé : le MovieController concret
de Startup doit avancer de frame0 vers frame1/list1 pour sélectionner EndMode.
La suite prépare une observation native unique de cette jointure, sans écrire
l'état invité.

Preuve : `reports/cycle-1847-demo-render-dispatch-noncausal.md` et
`artifacts/cycle-1847/`.

# Cycle 1846 — prédicat post-fence négatif borné

La chaîne invitée `EndMode -> listener StartUp -> manager+0x18 -> Title` est
qualifiée, mais n'est pas démontrée atteinte sous HSIO=1. Le garde
MovieController `0x82323BB8` est aval et non atteint; aucune correction de
son état n'est causale. Le dernier site guest démontré est l'indirect
`0x822E559C -> 0x822F8848` (objet `0x82934280`, vtable `0x8202A488`, slot
`+0x10`), dont le receiver n'est pas encore joint au listener StartUp.

La négative ferme le gate sans patch ni runtime. La suite est une jointure
Ghidra statique du receiver et des writers de `+0x0C`.

Preuve : `reports/cycle-1846-demo-post-fence-predicate-bounded-negative.md`
et `artifacts/cycle-1846/`.

# Cycle 1845 — fence scratch runtime réfuté

Le seul probe effectif PAL démo HSIO=1 sans entrée atteint 3 200 ticks sans
Title, mais le ring reste sain (1 182 soumissions, 1 089 présentations). Les
192 couples de trace source 1 montrent tous `0x16AE2000: 4 → 0` après retour
du dispatcher `0x821B9710`, y compris les callbacks `0x821C5190`,
`0x822E4240` et `0x822E4268`.

Le `pending_wait=4` terminal est donc un instantané de borne, pas un fence
bloqué. Aucun correctif Xenos, host, import ou PPC n'est justifié; la sonde
temporaire est retirée. La suite doit identifier statiquement le premier
prédicat invité encore actif avant `EndMode → manager+0x18 → Title`.

Preuve : `reports/cycle-1845-demo-scratch-fence-runtime-refuted.md` et
`artifacts/cycle-1845/`.

# Cycle 1844 — producteur scratch zéro qualifié statiquement

Le wait qui maintient StartUp est maintenant qualifié jusqu'à sa retraite
attendue : `SCRATCH_REG0` publie `4` à `0x16AE2000`, puis
`PM4_INTERRUPT(4)` livre la source 1 au dispatcher invité `0x821B9710`.
Après retour du callback actuel `0x822E4240`, ce dispatcher doit effacer le
bit CPU 2, donc `4 → 0`. Le host ne produit pas ce zéro.

Le contrat est déjà présent statiquement dans le processeur Xenos, le ring et
le cycle de vie. Aucun patch n'est causalement justifié. La seule suite est
une trace native bornée de cette retraite exacte, sans A/B ni entrée.

Preuve : `reports/cycle-1844-demo-scratch-fence-producer-qualified.md` et
`artifacts/cycle-1844/`.

# Cycle 1843 — Title naturel non atteint, writer scratch zéro ouvert

La correction texture existante passe les validations OFF 27/27 et ON 26/26.
Le run HSIO=1 sans entrée atteint 1 182 soumissions ring à 3 200 ticks, mais
StartUp reste actif et aucune frame Title n'existe. Le dernier wait CP lit le
dword `0x16AE2000` à 4 et attend 0; le producteur du retour à zéro n'est pas
nommé. La branche s'arrête sans patch produit ni cycle mission.

Preuve : `reports/cycle-1843-demo-title-natural-not-reached.md` et
`artifacts/cycle-1843/`.

# Cycle 1842 — divergence HSIO bornée, première frontière dure nommée

Le sélecteur invité est qualifié : `0x821C64E8` produit le cache
`0x827AD310`, `0x821BA780` garde `device+0x2ABD.bit1`, puis `0x821B9BC8`
sélectionne le ring sous HSIO=1. Ce choix est intentionnel et reste inchangé.
Le plateau worker est non causal et le `WAIT_REG_MEM` profond est conforme au
contrat CP testé. La première frontière dure de la longue exécution est le
profil texture BC3 320×160 rejeté au tick 4 911; sa prise en charge minimale
et son test existent déjà dans l'arbre courant. Aucun runtime ni patch produit
n'a été ajouté pendant ce cycle.

Preuve : `reports/cycle-1842-demo-first-post-startup-divergence.md` et
`artifacts/cycle-1842/`.

# Maintenance post-cycle 1841 — handoffs opérationnels nettoyés

`NEXT.md` et `RESUME.md` sont désormais current-only : 7 350 lignes cumulées
ont été ramenées à 49, avec un seul statut courant et aucun faux gate actif.
L'historique reste dans `STATE.md`, `EVIDENCE.md`, `reports/` et `artifacts/`.
`AGENTS.md` impose ce contrat, exige la lecture des ancres exactes avant patch
et nomme explicitement `recompilation/ace-combat-6-demo` comme arbre PAL démo
actif. Aucun code produit, artefact runtime ou résultat de gate n'a changé.

# Cycle 1841 — contrat threading déjà satisfait, branche bloquée sans patch

`RÉFUTÉ` : ni le vol auto-reset ni la famine d'un worker runnable n'explique
l'échec Title. Le backend courant réserve déjà le waiter via `granted_thread`
et sert tous les threads runnable par passe. Dans le run existant, t15 exécute
1 878 dispatches jusqu'au tick 2 997 pendant le spin massif de t1; à la borne,
seul t1 est runnable et les 22 autres threads sont bloqués.

Aucun changement produit, oracle retail ou nouveau runtime. Les validations
post-retrait existantes restent PASS (OFF 27/27, ON 26/26), mais les critères
finaux restent t1=5 et aucune transition `StartUp→Title` à 3 000 ticks malgré
1 114 soumissions ring. La branche demandée est arrêtée au blocker : une suite
doit ouvrir un gate distinct sur un prédicat invité ou service hôte post-StartUp.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

Preuves : `artifacts/goal-playable/threading-contract-review-20260825/`,
`reports/cycle-1841-demo-threading-contract-already-satisfied.md`.

# Cycle 1840 — les publications existent : deux sémaphores, origine invitée

`PROUVÉ` dans le projet Ghidra canonique PAL démo : `0xE000005C` et
`0xE0000130` sont créés par `NtCreateSemaphore` puis publiés par
`NtReleaseSemaphore`. Le census antérieur `set/pulse` ne pouvait pas les voir.

- `0xE0000130` : producteurs `0x820FF710/788/7F8/A88/B50`, cinq callsites
  vers `0x822E1E70 -> 0x821A6950`; garde `[obj+0x60d0] < 256`.
- `0xE000005C` : producteur partagé `0x822EF750`, release à `0x822EF7B4`;
  bootstrap `0x822EF7D0 -> 0x822EF750` à `0x822EF804`, sans garde de release.

Les atlas existants prouvent respectivement une et cinq publications avant le
ledger d'attente. Les deux origines sont `guest-import`; aucun post hôte ne
doit être ajouté et l'oracle retail n'est pas nécessaire. Frontière active :
contrat partagé scheduler/waiters après publication invitée.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

Preuves : `artifacts/goal-playable/thread-producer-static-20260825/`,
`reports/cycle-1840-demo-semaphore-producers-qualified.md`.

# Cycle 1839 — rotation scheduler seule réfutée et retirée

`RÉFUTÉ` : la variante round-robin déterministe échoue dans l'unique run PAL
démo HSIO=1 borné à 3 000 ticks : thread 1 reste à 5 handshakes (critère
`>5`), le ring atteint 1 114 soumissions, et `StartUp→Title` reste absente.
Le reçu d'exit manque après interruption du contrôleur ; le run n'est pas
qualifié de succès.

Le curseur, le helper et leur test ont été retirés. Validation post-retrait :
builds codegen-OFF/ON PASS, CTest OFF 27/27 et ON 26/26. Aucun correctif
scheduler n'est conservé. Frontière active : producteurs invités des posts
`0xE000005C` et `0xE0000130`. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# Pivot retail après échec clos du cycle 1850 (26 août 2026)

Le probe PAL démo codegen-ON s'est arrêté fail-closed après 5811 ticks sur
`unsupported Xenos draw shape`, avec 1959 présentations, sans frontend ni
mission. Ce gate et l'architecture runtime démo artisanale sont clos et
supersédés; aucun nouveau build ou long run n'est autorisé.

Le produit actif devient `recompilation/ace-combat-6-retail`, fondé sur le
sous-module BSD-3-Clause AC6_recomp
`09144bb092ad871584808aeead69c395edbd5200` et le renderer Xenos/Vulkan complet
de ReXGlue. Le bootstrap NTSC-U/J est le gate courant. PAL retail reste la cible
finale et est bloqué jusqu'au succès gameplay US. Preuve :
`reports/cycle-1850-demo-runtime-closed-superseded.md`.

Preuves : `artifacts/goal-playable/scheduler-rotation-20260825/`,
`reports/cycle-1839-demo-scheduler-rotation-refuted.md`.

# Cycle 1838 — les draineurs sont nommés : worker de file et pool de complétion

`PROUVÉ` (statique pure, basefile qualifié `b98a9ac1…14218` + atlas canonique,
zéro run) : les deux cibles de spin ont leurs décrémenteurs.

1. File de rendu (`0x82386CC0`, `[obj+24784]/[24788]`) : producteurs
   `sub_820FF710/788/7F8/A88/B50` ; initialiseur `sub_820FF9E0` qui crée le
   worker dédié via `0x822E1D30` (unique appelant) ; draineur **`sub_820FFCA0`**
   — pop, ++`[24788]` @0x820FFD78, dispatch `sub_820FEFA8` (mesuré thread 25).
   À vide le worker **spinne**, il ne dort jamais sur un événement.
2. Contexte de complétion (`0x82928B80`) : bootstrap `sub_822EF850` crée
   l'événement `[+152]` puis **trois threads** de corps thunk
   `0x822EF848 = b 0x822EF5B0` : attente infinie sur `[+152]` (wrapper
   `0x821A6AF0`), au réveil ++`[ctx+8]` @0x822EF5E0, pop de la file
   incorporée, dispatch vtable des items. Mesuré thr15 (handle 0x60 x24).

Attente côté thread 1 : corps de tick `sub_822DA9C0 -> sub_822E40E8`
(r3=`0x82928B80`) -> spin `sub_822EF7D0`. Ledger tick 260 : t15/t16/t17
bloqués sur l'événement `0xE000005C`, t25 (worker de rendu) sur
`0xE0000130`, et ces deux handles ne reçoivent **aucune publication dans les
deux régimes** — la contrainte liante est le post manquant, pas seulement le
partage de tranches. Sous HSIO=0, la causalité en miroir se confirme
(draineurs servis, Title au tick 2369).

Prochain gate : spécifier AVANT implémentation la correction minimale —
rendre les tranches aux non-primaires sans horloge murale (rotation de
l'ordonnanceur hôte), contre les observables fixés (handshakes t1 > 5,
soumissions ring > 0, StartUp→Title). Preuve :
`artifacts/goal-playable/drain-writer-census-20260825/RESULT.md`,
`reports/cycle-1838-demo-drain-writers-named.md`.

# Cycle 1837 — la garde des deux fenêtres : le thread 1 ne revient jamais au poll

`PROUVÉ` (statique pure + journaux existants) : la machine de soumission de
streaming est nommée — `sub_8219DF00(req)`, phases sur `[req+12]`, démarrage
tant que `[req+16] < 860`, poll vers `sub_8219AF20` tant que `[req+36]==0`,
enregistrement via `E1E28/E1E18` vers le pool `0x8261EBC8`, anneau
`sub_821A1B50` qui pose l'événement `28`. Le thread 1 y arrive par bctrl
depuis `sub_8219F5D0` : 99 848 polls sur ticks [66..222].

La garde liante n'est pas dans la machine : dès le tick ~206, le thread 1 est
capté par des spins de drain sans fin — `sub_820FF8D8` bouclant sur
`[obj+24784]/[obj+24788]` (n=1,86 M d'itérations mesurées jusqu'au tick 2997),
famille `0x822EC03C→F5CE8`, et `EF7D0 while([ctx+8]>0)`. Rien ne se draine
quand le thread 1 mange toutes les tranches ; pas de troisième poll, pas de
troisième fenêtre. Sous HSIO=0, ses handshakes bloquants rendent le CPU aux
draineurs — la charge avance jusqu'à 2369. Les deux régimes confirment la
même causalité en miroir.

Prochain gate : qui décrémente `[obj+24784/88]` et `[0x82928B80+8]`
(threads/sites de complétion), puis choix de la correction minimale contre les
observables fixés. Preuve :
`artifacts/goal-playable/streaming-guard-static-20260825/RESULT.md`,
`reports/cycle-1837-demo-streaming-guard-is-a-drain-spin.md`.

# Cycle 1836 — kickers morts dans les deux régimes ; pipeline de streaming nommé

`RÉFUTÉ` (négative bornée, trois preuves indépendantes) : le sous-système
kicker (`sub_821A1928`, pool `0x82774630`) n'exécute rien pendant le bootstrap,
**ni sous HSIO=1 ni sous HSIO=0** — zéro arête indirecte sur 3000 ticks pour
toute sa fermeture d'appelants, zéro publication des handles `04..3C` des deux
côtés du journal 1024. Il n'est pas le levier.

Instrument nouveau (observation pure) : backchain des LR sauvés aux shims
d'événements (`AC6_DEMO_WATCH_EVENT_CALLER`). Deux chaînes fermées :
- **Paceur** : `NtSetEvent(40)` x259 ← `sub_821C5090`+232 ← callback
  d'interruption `sub_821B9710`+176 — estampille et pacing dans la même
  fonction, une fois par tick.
- **Streaming** : thread 1 soumet via `sub_8219AF20`+332 → `sub_821A1B50`+128
  (anneau `0x82774898+152i`) → set `28` ; thread 9 attend/draine et clôt par
  clear/pulse `2C` (`sub_821A1D10`). **Deux fenêtres seulement en 260 ticks**
  (66→72, 205→219) ; sous HSIO=0 les lectures vont jusqu'au tick 2369.

La frontière suivante est la garde qui borne les soumissions de streaming du
thread 1 à deux occurrences. Preuves :
`artifacts/goal-playable/kicker-caller-attribution-20260825/RESULT.md`,
`reports/cycle-1836-demo-kickers-dead-streaming-pipeline-named.md`.

# Cycle 1835 — la découple de cadence est réfutée avant implémentation

`RÉFUTÉ` (statique pure, trois balayages parallèles, zéro run) : le correctif
envisagé au cycle 1834 — délivrer l'interruption graphique au rythme du temps
réel plutôt que par tick — tombe sur les trois points du gate. (1) Inventaire
clos : famille de pacing complète WORK `{40,44,48,4C}` / GATE `{54,58}` /
perf `0x82935270` (`50`) / complétion `0x82928B80` / pool workers
`0x82774630` ; l'interruption part une fois par tick *avant* les tranches
(`lifecycle.hpp:417-422`) et les journaux d'arêtes n'ont aucun plafond.
(2) Neuf fonctions lisent le timebase : estampilles, stats, un spin de
timeout — **aucun rendez-vous** ; découpler ne change aucune sémantique de
fence. (3) Les soumissions ring sont des stores MMIO invités, rien ne les
couple à la coïncidence interruption/tick ; un pacing temps réel casserait en
revanche le déterminisme des replays.

Le maillon nommé remplace la frontière : sous HSIO=1, les workers 4..11
dorment depuis le tick 0 faute de kick (`sub_821A18D0`, six appelants statiques
dont aucun ne s'exécute après la phase précoce), donc `EF7D0` spinne dans le
thread 1, donc tout le chemin titre affame. À 60 Hz matériels, le producteur
EST libéré chaque frame — ce qui manque au port, c'est la préemption par le
temps et les complétions asynchrones, pas la cadence.

Prochaine expérience unique : watcher opt-in sur `sub_821A18D0` et ses six
appelants, 260 ticks headless ; identifier l'appelant kicker et sa garde, ou
négative bornée. Aucune correction scheduler/renderer avant ce verdict.

Preuve : `artifacts/goal-playable/pacing-inventory-static-20260825/RESULT.md`,
`reports/cycle-1835-demo-interrupt-decouple-premise-refuted.md`.

# Cycle 1834 — l'interruption satisait d'avance chaque attente

`PROUVÉ` (statique + un run 260 ticks, instruments opt-in, zéro ligne changée) :
les cinq handshakes du thread 1 se répartissent sur **deux** objets — la file
de `WORK` (`{48,4C}`, via `4018`/`4080`) et le *gate* global (`{54,58}` à
`0x82933F98`, via `EEE68`). Dernier appel au tick 177, comme sur 3000 ticks.

Pourquoi ça cesse de bloquer : `sub_822E4268` (dispatch d'interruption
graphique) estampe un timebase **et** fait `NtSetEvent([obj+104])`. Sous
HSIO=1 l'interruption libère donc le producteur (`t12` attend `0xE0000040`)
une fois par tick : publication x259, compteur de gate basculant 0/1, et
chaque condition du consommateur déjà vraie à l'arrivée. Le handshake étant
le yield de l'invité, le thread 1 ne dort plus, épuise 2998/3000 tranches et
affame le streaming (fenêtres thread 9 aux ticks 66/205 seulement). Sous
HSIO=0, personne ne libère `40`, le thread 1 dort 5611 fois, l'horloge avance
par timeouts. Deux régimes faux en miroir.

Référence : Xenia Edge délivre l'interruption depuis un worker temps réel
(`graphics_system.cc:171-206,385-403`), indépendante de l'ordonnanceur ; le
port la tire une fois par tick — même variable que l'horloge.

Corrige : cycle 1833 (égalité sur le gate, pas sur la file ; la file n'a que
`4080` de bloquant), cycles 1829/1830 (estampilleur = paceur, `[obj+24]`
+ événement). Preuve :
`artifacts/goal-playable/gate-waiter-attribution-20260825/RESULT.md`,
`reports/cycle-1834-demo-interrupt-pacing-pre-satisfies-every-wait.md`.

Prochain gate : lister statiquement les paceurs dérivés de `[obj+104]`, puis
décider la découple cadence d'interruption / tick avant toute correction,
baseline ring (502 soumissions) comme garde-fou.

# Cycle 1826 — aucun événement ne manque : le thread 1 ne va plus se coucher

`RÉFUTÉ`, et c'est une affirmation de ce dépôt qui tombe : le journal
`event_publications` était un `std::array<…, 32U>` cessant d'enregistrer au 33ᵉ
événement, et les deux côtés de l'A/B l'avaient saturé. Porté à 1024, le relevé
complet montre les deux chemins publiant **la même chose** :

```
site invité      HSIO=1   HSIO=0        clés distinctes : 61 contre 60
0x821A61F0          52       52
0x821A688C           2        2
0x821A6AC4         970      970
```

Les seize handles `0xE0000088…0xE00000C8` sont publiés une fois chacun des deux
côtés. Seule `0xE0000050` est exclusive (27 contre 0) : clé d'attente du thread
13, réveillé par le régime d'interruption — signature attendue.

Tombent avec : « les dix-sept publications disparaissent » (cycle 1826), « le
streaming s'arrête » (55 lectures contre 61, compteurs non plafonnés), et la
lecture du cycle 1807 des seize handles comme publication en trop.

Ce qui survit, mesuré hors plafond :

```
thread 1, NtSignalAndWaitForSingleObjectEx @0x821A69CC
   HSIO=1      5 appels, dernier au tick 177
   HSIO=0   5611 appels, dernier au tick 2999
```

Le thread 1 ne dort pas faute d'être réveillé : il ne va plus se coucher. La
frontière est côté invité — quel prédicat fait sauter le handshake après le
tick 177.

Preuve : `artifacts/goal-playable/publication-ab-cap1024-20260824/RESULT.md`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1825 — le ring et la progression s'excluent sur un seul mot

`PROUVÉ` à une variable près : `VdIsHSIOTrainingSucceeded` décide seul si la
démo affiche quelque chose ou si elle progresse.

```
HSIO=1 (HEAD)  ring 502 soumissions   mode bloqué à StartUp, Title jamais publié
HSIO=0         ring 0 soumission      Title publié au tick 2369, état 1 au 2385
```

Le Title publié porte la vtable `0x820113E4`, le vrai
`CModeTaskTitleDemoOffline`; les ticks 2369/2385 reproduisent exactement
l'observation du cycle 1793, qui avait donc été faite sous HSIO=0.

Signature ordonnanceur : HSIO=0 → 23 threads bloqués, 0 runnable, **215**
épuisements de tranche sur 3000. HSIO=1 → 22 bloqués, 1 runnable, **2998**.
Sur le chemin ring, l'invité attend des événements que le port ne rend pas.

`RÉFUTÉ` par mesure : régression de binaire entre le 22 et le 24 août (le
témoin conservé et le binaire courant donnent des résultats rigoureusement
identiques) ; différence de store (byte-identique au store neutre) ; différence
de code invité (même manifeste codegen). Et rétrospectivement l'objet de
plusieurs cycles : « quel producteur naturel arme `manager+0x18` depuis
START » n'avait pas de réponse parce que la question n'avait pas d'objet —
sous HSIO=0, StartUp→Title se produit seule, sans START.

Décision : `kVdHsioTrainingSucceededResult` **reste à 1**. Passer à 0
échangerait un blocage contre un autre et masquerait le défaut. Le correctif
est de servir, sur le chemin ring, ce que l'invité attend.

Preuves : `reports/cycle-1825-demo-hsio-arbitrates-ring-against-progression.md`,
`artifacts/goal-playable/hsio-mode-transition-tradeoff-20260824/RESULT.md`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1824 — START est pressé à 42 % du film de marque

`CONFIRMÉ` (hypothèse utilisateur) : l'écran affiché quand START est envoyé est
le splash éditeur **Bandai Namco Games**, pas un titre interactif. Image
inspectée humainement :
`artifacts/goal-playable/title-start-timing-capture-20260824/png/frame-live-shape-inv.png`.

`RÉFUTÉ` — et c'est la première conclusion de ce cycle qui tombe : la timeline
SWG **n'est pas gelée**. Sur le binaire courant, l'owner racine `0x2E3CDD10`
passe de la frame 0 (tick 225) à la frame 85 (tick 480), soit exactement
`(480-225)/3`. Les owners `0x2E3CE490` et `0x2E3CED10` ont des tables de 1 et 2
frames : leur immobilité est **conforme**, pas une panne. Mesure :
`artifacts/goal-playable/swg-frame-advance-current-20260824/`.

Arithmétique qui recadre tout le problème :

```
racine 0x2E3CDD10 = 2220 frames        (end-begin)/8
cadence            = 1 frame / 3 ticks (frame 0 au tick 225)
fin du film        ≈ tick 6882
START au tick 3000 = frame 925/2220    soit 42 % du film
```

L'écran figé n'est donc pas un jeu bloqué : le film demande ensuite le handle
`0x0E000059` (attendu au tick ~1125) et le renderer échoue fail-closed sur son
fetch BC3 1280×720 ; la dernière image réussie persiste. Cela réconcilie la
contradiction avec `brandlogo-list2-window-runtime-20260823` sans régression.

Oracle Xenia, même XEX démo, session humaine
`ac6_demo_work/instrumentation-xenia/20260815-134231` : chargement à 15 s,
silence disque de 20 à 58 s pendant que le film joue, START humain à 58,55 s,
puis 514 lectures. La progression est pilotée par l'horloge du film, ni par le
disque ni par un événement externe. Écran cible capturé :
`ac6_demo_work/xenia-runtime-results-20260815-final/screenshots/xenia-title-check.png`.

Décision utilisateur : le renderer est « functional-enough », on n'y touche pas.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1824 (première rédaction, corrigée ci-dessus) — START sur un splash

`CONFIRMÉ` (hypothèse utilisateur) : l'écran affiché quand START est envoyé est
le splash éditeur **Bandai Namco Games** — sigle à trois lobes, « Games », `™` —
et non un écran titre interactif. Image inspectée humainement :
`artifacts/goal-playable/title-start-timing-capture-20260824/png/frame-live-shape-inv.png`
(canal R étiré puis inversé, transformation de lecture seule ; aucun changement
renderer).

`PROUVÉ` : le readback est **byte-identique** à celui du cycle 1811 à la borne
1160 ticks (`changed_pixels=0`). L'instrument est calibré : il détecte un pixel
modifié d'un niveau. 429 draws pour 8 frames uniques ; seules les textures
`0x57` (64×64) et `0x58` (512×512) sont tirées, jamais le wordmark `0x59`.

`PROUVÉ` indépendamment (guest, sans instrument partagé) : le Title reste à
l'état interne 1 du tick 2452 au tick 8000, soit 5548 ticks immobiles
(`analysis/demo/ac6-demo-title-natural-current-8000-v1.json`).

Conséquence : la question « quel producteur naturel arme `manager+0x18` depuis
START » était mal cadrée. Il n'existe pas d'écran titre prêt à recevoir START.
La chaîne cohérente est : timeline SWG figée sur frame 0 → le splash ne se
termine jamais → aucun `EndMode` → listener `0x8217C890` jamais appelé →
`manager+0x18` jamais armé. Le cycle 1789 avait nommé cette frontière avant que
la campagne parte sur la couleur pour treize cycles.

Décision utilisateur : le renderer est « functional-enough », on n'y touche
pas ; focus sur la progression titre → menus → gameplay.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1823 — consolidation, hygiène verte, pivot gameplay

**Redirect de priorité utilisateur (2026-08-24)** : focus sur l'atteinte du
début du gameplay ; toute run produit des captures pour validation automatique
et humaine.

Le fil couleur `BF` du logo titre (cycles 1810–1822, treize cycles dont onze
statiques) est **parké par redirect — ni réfuté, ni supersédé par preuve**. Sa
frontière exacte (callsites `0x82322438`/`0x82324118`) est conservée pour
reprise. Motif : les trois contrats mission01 portent
`"visual_parity_out_of_scope": true`; aucun contrat n'exige le fond blanc, et
le `done_when` « fond blanc » du cycle 1811 était auto-imposé.

Hygiène rétablie : `ctest` **26/26** (était 25/26 — `ac6-demo-complexity`
échouait, donc aucun commit n'était légal, donc l'arbre a grossi jusqu'à 51
fichiers suivis modifiés). La baseline de complexité est re-pinnée sur les
tailles atteintes ; les limites globales (220/1200/1000) sont **inchangées** et
le cliquet `aggravated over-budget baseline` continue d'interdire toute
aggravation. Gate mission01, artefacts contrats, adresses contrats, assert
liveness, numéros de CLAUDE.md et reset microexec : tous verts.

Aucune avancée causale guest dans ce cycle. Hypothèse à tester en premier au
cycle suivant, issue de l'utilisateur : *START était envoyé pendant les splash
screens*. Aucune capture n'a jamais été produite au tick du pulse (3000), et
`STATE.md` note par ailleurs que le logo Namco précède nettement le prompt
« PRESS START ». Un run naturel sans entrée, 8000 ticks, une capture par
présentation qualifiée, est lancé pour établir la chronologie réelle.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1822 — ligne alpha réduite au transform fourni

- `PROUVÉ` — `0x82323BB8` conserve `r3` dans `r31` et son argument `r4`
  dans `r30`; les stores `+0x14c/+0x15c` lisent donc le transform fourni.
- `PROUVÉ` — avec un record actif, les deux champs valent
  `transform[0x4c]*T[0x0c]` et
  `transform[0x5c]*T[0x0c]+T[0x1c]`; sans record, le bloc complet
  `transform+0x40..+0x5f` est copié.
- `PROUVÉ` — pour le type 4, `T[0x0c]=1` et `T[0x1c]=0`; les deux branches
  donnent donc la même entrée alpha
  `transform[0x4c]+transform[0x5c]`.
- `RÉFUTÉ` — la présence du record ne peut expliquer `BF` pour ce draw.
- Frontière : qualifier le builder du transform `r4` aux appelants externes
  `0x82322438` et `0x82324118`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Cycle 1821 — transform type 4 constant

`PROUVÉ` : l'entrée big-endian `0x8264CDF8[4]` vaut `0x82322EC8`, qui retourne
inconditionnellement `0x8264CDA0`. Ce global contient les floats
`[1,1,1,1,0,0,0,0]`; donc `T+0x0c=1` et `T+0x1c=0`. Les cinq champs couleur
passés à `0x820EB200` se réduisent exactement à
`MovieController+0x140/+0x144/+0x148/+0x14c/+0x15c`. L'octet `BF` vient de
`quantize255(clamp(C+0x14c + C+0x15c,0,1))`, pas du record type 4.
Prochaine frontière : stores/prédicats de `C+0x14c/+0x15c` dans
`0x82323BB8`. Aucun runtime ni changement renderer. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# Cycle 1820 — producteur local de la matrice couleur

`PROUVÉ` : `r5` à `0x820EB200` est la matrice locale `M=r1+0x50` construite
par `0x82326420`, transmise en `r6` au handler type 0 `0x82325E70`, puis
replacée en `r5` avant le slot 7 renderer. Les champs `M+0x40/+0x44/+0x48`
sont `T[0/+4/+8] * MovieController[0x140/+0x144/+0x148]`; `M+0x4c` vaut
`T[0x0c]*MovieController[0x14c]`; `M+0x5c` vaut
`MovieController[0x15c]*T[0x0c]+T[0x1c]`. `T` vient du trampoline
`0x823237B8 -> 0x8264CDF8[type]`. La limite de cinq batches laisse
`0x8264CDF8[4]` comme pivot statique unique. Aucun runtime ni changement
renderer. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# Cycle 1819 — pack couleur du record titre fermé

`PROUVÉ` : l'unique store vers `record+0x18` est `0x821DF018` dans
`0x821DEED8`. Il assemble quatre composantes quantifiées depuis
`parameters+0x3c/+0x30/+0x34/+0x38` lorsque `parameters+0x40 & 0x20`; sinon il
écrit le défaut `0xffffffff`. `0x820EB200` construit ces composantes depuis
`r5+0x40/+0x44/+0x48` et la somme bornée `r5+0x4c + r5+0x5c`. Son mot de flags
inclut toujours `0x860`, donc le bit `0x20` est actif. L'hypothèse d'une copie
depuis un champ unique est `RÉFUTÉE`. Le mot observé `BFFF0000` implique les
octets amont quantifiés `[BF,FF,00,00]`; la valeur flottante exacte produisant
`BF` reste inconnue. Aucun changement renderer ni runtime.

# Cycle 1818 — constructeur exact du nœud P1

`PROUVÉ` : `0x821185A8` est réfuté comme producteur du nœud source ; il copie
`source+0x24` vers `owner_de_travail+0x18` en aval. Le véritable producteur
est `0x82095DF0`, qui réserve `0x70` octets, appelle `0x821DEED8`, puis lie le
nœud par `owner+0x20`/`owner+0x24` et `tail+0x10`. Ce sont exactement les
champs consommés par `0x82119488`. La frontière est maintenant le store
interne de `record+0x18` dans `0x821DEED8`, puis son champ source dans le bloc
`r1+0x50` construit par `0x820EB200`. Aucun runtime ni changement renderer.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
Preuve :
`artifacts/goal-playable/title-p1-record-constructor-static-20260824/RESULT.md`.

# Cycle 1817 — le writer P1 reçoit un nœud de liste

`PROUVÉ` : `0x82119488` charge chaque owner depuis `r31+4+4*i`, prend la tête
à `owner+0x20`, parcourt les records par `record+0x10`, puis passe le record
courant en `r5` à `0x82118650`. Le dispatcher le transmet en `r4` au writer
sélectionné par `0x82009E78 + *(record+0x14)*0x14 + 0x0C`; le `r4+0x18` lu par
`0x82118D18` est donc exactement `record+0x18`. `0x821185A8` contient un
store candidat `0x8211860C: stfs fr12,0x18(r31)`, mais son `r31` n'est pas
encore joint à cette liste et `fr12` n'est pas relié à `BFFF0000`. Aucun
runtime ni changement renderer. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`. Preuve :
`artifacts/goal-playable/title-p1-record-color-source-static-20260824/BLOCKER.md`.

# Cycle 1816 — slots exacts du dispatch P1

`PROUVÉ` sur le `xex-basefile.bin` relié explicitement au XEX demo PAL : deux
descripteurs de `0x14` octets contiennent `0x82118D18` à `entry+0x0C`, aux
entrées `0x82009E8C` et `0x82009EB4`. Leurs tuples bruts sont respectivement
`(0000000D,09000001,826F61C0,82118D18,821187A8)` et
`(0000000D,09000003,826F61C0,82118D18,821187A8)`. Aucune branche PPC directe
ne cible le writer et aucune instruction ne construit les cellules exactes :
le consommateur charge une base et indexe la table. Callsite, règle d'index,
type de `r4`, store `r4+0x18` et provenance de `BFFF0000` restent ouverts.
Preuve :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`.
Aucun runtime ni changement renderer. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire courant — cycle 1814 (24 août 2026)

La contradiction P/Q est fermée. Le shader titre consomme le fetch constant
95/P1 : `103FB893 100A9002`, base `0x103FB890`. Ses quatre couleurs guest
valent `BFFF0000`. Le mode 3 `k16in32`, l'unpack `FMT_8_8_8_8` et le swizzle
`zyxw` produisent exactement `(191,0,0,255)`, rouge opaque à environ 75 %.

Le slot 94/Q1 analysé au cycle 1812 est un garde adjacent non consommé ; son
décodage rouge était correct mais ne qualifiait pas `vf0`. Aucune copie Q->P
ni erreur de mapping du bridge n'est présente sur ce draw. La frontière remonte
au record source du writer `0x82118D18`, qui copie `r4+0x18` sans
transformation aux quatre champs `vertex+0x30`. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1813 (24 août 2026)

La chaîne couleur du renderer titre est fermée du fragment shader au PPM : le
PS calcule `texture * couleur vertex`, le render target titre est un
`R8G8B8A8` direct, et son readback n'emprunte ni l'EDRAM ni
`copy_dest_swap`. Le rouge existe donc avant la copie et la présentation ; une
nouvelle permutation de canaux au readback serait une double correction.

La preuve vertex du cycle 1812 est toutefois qualifiée pour le fetch constant
94, alors que le VS titre consomme `vf0`, joint au slot 95 par le bridge. La
valeur exacte du slot 95 au même draw n'est pas encore conservée. La nouvelle
frontière est cette réconciliation P/Q : décoder les deux dwords du slot 95 au
draw qualifié avant de remonter le producteur de `vertex+0x30` ou de modifier
une sémantique couleur. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1812 (24 août 2026)

La sémantique du fetch couleur du titre est fermée. Le slot 94 vaut
`104A4893 100087F2`; ses bits endian valent 3, soit `k16in32` et non
`k8in32`. Les octets guest `FF FF 00 00` deviennent `0xFFFF0000` après
échange des demi-mots, sont décompactés en `(0,0,255,255)`, puis le swizzle
VS `zyxw` produit le rouge opaque `(255,0,0,255)` observé.

Le décodeur vertex, le swizzle de présentation et le readback sont donc
réfutés comme source du fond non blanc. La frontière remonte au producteur du
dword à `vertex+0x30` dans le buffer Q1, ou à un état de composition antérieur
qui doit établir le blanc. Aucun correctif produit n'est ajouté dans ce gate.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1811 (24 août 2026)

Le swizzle du fetch de présentation est maintenant appliqué au readback : le
descripteur demande `B,G,R,1`, complément exact du `copy_dest_swap=1`. Cette
correction transforme la capture bleue précédente en rouge et prouve que le
readback restitue désormais les canaux présents dans le frontbuffer invité.

La validation visuelle échoue toujours : la capture 1280x720 est rouge plein
écran, avec les lobes et « Games » plus sombres; ses coins valent `(255,0,0)`
et seuls les pixels du canal rouge sont non nuls. Les traces de draws montrent
un premier quad plein écran dont les quatre couleurs vertex valent
`0xFFFF0000`, puis le quad du logo en `0xBFFF0000`. La nouvelle frontière est
donc la sémantique de `FMT_8_8_8_8`/endianness et de la modulation couleur au
premier draw, avant la présentation. Après cinq batches, aucune seconde
correction n'est tentée. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1810 (24 août 2026)

Le contrat HSIO est corrigé : `VdIsHSIOTrainingSucceeded` retourne 1. Le run
codegen-on à 1160 ticks restaure exactement 502 soumissions, 4405 dwords et
407 présentations renderer. Retourner 0 activait le fallback logiciel invité
`0x821BA130` et contournait le ring; ce n'était pas une réparation valide.

La capture 1280×720 contient le logo complet mais échoue l'oracle : le fond
attendu blanc est bleu et tous les pixels ont R=G=0. La frontière est le
décodage/export de la couleur vertex `FMT_8_8_8_8` vers l'interpolateur du
pixel shader, avant toute nouvelle capture. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1809 (24 août 2026)

Le témoin positif du 22 août fournit désormais 53 tuples `NtReadFile` complets
à 1160 ticks, avec 502 soumissions et 407 présentations. Sa capture obligatoire
1280x720 est uniformément noire, vérifiée automatiquement et humainement.

Le côté courant de l'A/B est non qualifié : `build/` était codegen-off et a
refusé le probe à tick 0. Après cinq batches, le gate s'arrête avant toute
conclusion comparative. Le tracepoint minimal est prêt; la reprise exacte est
un rebuild `build-codegen-on` puis le seul run courant. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1808 (24 août 2026)

L'export Ghidra canonique ferme le site `0x821A61EC` : c'est le slot `+0x10`
de la table file-I/O globale `0x823C2D2C`, appelé par `Function_821A6168`,
avec traitement de `STATUS_PENDING` (`0x103`) et attente éventuelle via
`NtWaitForSingleObjectEx`. Le bridge courant joint ce chemin à `NtReadFile` et
publie immédiatement l'événement de complétion après sa lecture synchrone.

La piste d'un `NtSetEvent` direct ou d'une création du scheduler est réfutée.
Le binaire positif contient le même helper de publication auto-reset et
l'appelle également depuis sa branche `NtReadFile`; une différence locale de
contrat est donc réfutée. La frontière exacte devient le premier état d'entrée
`NtReadFile` divergent : fichier/offset/longueur, événement ou waiter.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1807 (24 août 2026)

La première divergence en amont du ring est maintenant événementielle. Le
binaire courant publie seize handles `0xE0000088…0xE00000C8` depuis le LR
invité `0x821A61F0`, absents du témoin ancien, puis bloque ses 23 threads. Le
témoin ancien garde le thread principal runnable et publie le ring.

L'atlas qualifié place le site dans `Function_821A6168`, mais son pseudocode
n'est pas conservé. Après cinq batches, le gate s'arrête sur l'export Ghidra
ciblé de cette seule fonction. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1806 (24 août 2026)

Le binaire codegen-on conservé du 22 août restaure à 1160 ticks 502
soumissions ring, 4405 dwords et 407 présentations renderer. Le binaire
courant reste à zéro aux mêmes entrée et borne. Le hook
`AC6_PPC_STORE_U32(0x7FC80714) -> apply_xenos_mmio_write` existe dans les deux
binaires : sa suppression est réfutée et la frontière remonte à la route
guest/scheduler qui atteint ce store.

Le readback obligatoire du témoin ancien est 1280x720 mais uniformément noir,
vérifié automatiquement et humainement. Les présentations ne valident donc
pas encore le frontend. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1805 (24 août 2026)

Le binaire courant rejoué exactement jusqu'au tick 1160 reste à
`RPTR=WPTR=0`, zéro soumission, zéro dword et zéro présentation renderer. Le
run positif historique produisait à cette même borne 502 soumissions, 4405
dwords et le logo Namco visible.

La régression native antérieure à `CP_RB_WPTR` est maintenant prouvée ; un
reset tardif des compteurs est réfuté. Aucun readback courant n'a été produit.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1804 (24 août 2026)

Un A/B natif strict avec le même binaire courant et la seule différence START
prouve que neutre et START restent tous deux à zéro soumission PM4, zéro dword
et zéro présentation renderer malgré 24 draws typés et 2894 `VdSwap`.

START et le changement de frame SWG sont réfutés comme cause de l'écran noir.
Le contrôle positif historique est visuellement confirmé : logo Namco non noir
en 1280×720. La frontière remonte à une régression ring/MMIO du binaire
reconstruit après ce run. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1803 (24 août 2026)

Le HIR Xenia Edge qualifié prouve que `0x822DA568` stocke son callback `r3` à
`node+0x08` et Q (`r4`) à `node+0x0C`; `0x822E35E8` les relit et appelle le
callback. Pour le record naturel type 1, la case `0x82009E9C` vaut exactement
`0x821187A8`. La chaîne Q→callback est fermée.

La frontière devient le premier événement postérieur à `0x821B4D80` qui
produit un kickoff GPU dans Edge mais manque au natif. Les screencaps natives
restent noires et `supported=false`.

# État autoritaire précédent — cycle 1802 (24 août 2026)

Le record naturel `0x827B3A80` du draw `0x12` est maintenant joint au premier
slot de queue. Avec `[0x826F61B8]=0x2E8B8C20` et `record+0x14=1`, il passe
`0x82119488 -> 0x82118FA0 -> 0x821185A8`, crée le slot `0x8270F598`, puis
atteint `0x821186B0`.

Les appels worker `0x821187A8 -> 0x821B4D80` observés au même tick précèdent
cette ingestion et ne sont pas attribués au lot courant. La frontière unique
est le transfert asynchrone `Q 0x8270F598 -> 0x822DA568/0x822E35E8 -> P ->
0x821187A8`. Le ring reste à zéro soumission et zéro dword. Quinze screencaps
1280×720 sont uniformément noires, dont une inspectée humainement.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1801 (24 août 2026)

Le chemin naturel `list13/draw_index=0x12` est fermé jusqu'à la création du
record renderer. Le handle exact est `0x0E000071`; les seize draws passent
`0x820EB200 -> 0x820EA9A0 -> 0x82095DF0 -> 0x821DEED8`, qui alloue les records
de `0x70` octets. La piste d'un index 0x12 invalide est réfutée.

La run Vulkan conserve 24 draws typés et `present_count=0`. Elle a cette fois
produit 21 screencaps Xvfb : toutes sont 1280×720, une couleur, moyenne 0, et
une inspection humaine confirme le noir uniforme. La frontière est désormais
le consumer du record puis son encodage PM4/Xenos ou sa condition de rejet.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1800 (24 août 2026)

Le chemin naturel `list_index=13` est fermé jusqu'au premier consumer
renderer. `table[13]={type=0, offset=0x1998}` désigne une liste de 16 records
type 0 à `0x2DCB2BB8`; ils portent `draw_index=0x12` et rejoignent tous
`0x82325E70 -> slot 7 (LR 0x82325ED4) -> 0x820EB200`.

Le run compte 24 draws typés mais `present_count=0`. Ni la screencap d'audit
ni le readback PPM n'ont été produits : aucune validation visuelle n'est donc
revendiquée. La frontière est l'entrée ABI 0x12 de `0x820EB200`, son handle de
draw/queue, puis le producteur de ressource présentable. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1799 (24 août 2026)

Le chemin type 4 est fermé statiquement jusqu'à la table relocalisée : pour
le sous-enfant `0x2E3F1350`, `list_index=13` désigne
`table[13]=[0x2DD796A4,0x2DD796AC)`. Les deux mots et le record cible sont
absents des artefacts statiques qualifiés ; l'égalité disque/heap complète de
`TitleUS` n'est pas établie. `TitleUS` est bien le bundle anglais présent dans
la démo PAL, distinct de `Title`, et non une preuve NTSC-U.

La frontière est un snapshot read-only unique au tick 3001 du descripteur,
de la liste et de ses records bornés. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# État autoritaire précédent — cycle 1798 (24 août 2026)

La branche enfant post-START est jointe exactement : frame 0 de
`0x2E3F8C50` = type 5 / index objet `0x42`, créant `0x2E3F1350` ; frame 0 du
sous-enfant = type 4 / `list_index=13`. Aucun bytecode VM n'est exécuté par
cette branche au tick 3001.

La frontière est `list_index=13 -> 0x82326420 -> record -> premier consumer
renderer/draw`. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État autoritaire précédent — cycle 1797 (24 août 2026)

Le run naturel post-START conservé prouve qu'au tick 3001 l'enfant
`0x2E3F8C50` n'exécute aucun offset VM : les deux `execute_raw` observés sont
liés au parent `0x2E3EDA90`. Son premier effet est la construction du
sous-enfant `0x2E3F1350`, avec `MovieMemory=0x2E3F1590`.

La frontière est désormais la seconde factory du même tick : qualifier ses
arguments `B/D`, la frame 0 du sous-enfant et le premier effet sortant de la
hiérarchie. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# État durable — AC6 démo PAL native

## Gate statique objet→shader — classe mapparts fermée (24 août 2026)

Dans la démo canonique, 169 des 170 objets `mapparts` portent la clé matériau
brute `0x30000010`. `Function_822E8668` résout `material+0x00` dans le même
registre `0x8296BF80` que `Function_822E0950` peuple depuis les descripteurs
NSXR. La clé rejoint exactement `vsCstCT.updb + psCT.updb`. `NU_FLAG1` varie
entre 0 et 1 sans changer cette paire ; les 4 312 descripteurs portent le
format vertex `06/13`, construit séparément par `Function_822EDF60`.

La variante `0x30040010` contient la même paire sous `Common_HDR`, mais le
writer de `material+0x14` reste à fermer. L'objet exceptionnel porte
`0x30000090`, absent des 51 NSXR. `NU_HASH` comme clé NSXR et les familles
`psMapCTF2_*` pour `0x30000010` sont réfutés. Aucun runtime n'a été utilisé.

Preuves : `research/object-shader-static-boundary.md` et
`artifacts/shader-selection-xrefs.txt`.

## État autoritaire courant — cycle 1795 — promotion et premier tick enfant fermés

`0x82323BB8` ferme la promotion recherchée : au début du tick parent, il copie
la tête pending `A+0xE4` dans `A+0xF4` puis vide `A+0xE4/A+0xE8/A+0xEC`.
Après les handlers de frame, il parcourt la nouvelle liste `A+0xE4` et
s'appelle récursivement sur chaque enfant. L'enfant construit par
`0x82323468` reçoit donc son premier tick dans la même invocation parentale,
puis devient la tête active du parent au tick suivant.

La publication différée n'est plus une frontière. Le prochain gate part du
premier tick enfant et cherche son premier effet persistant vers la VM, un
callback natif ou un propriétaire de transition qualifié. Aucun effet
`EndMode`, listener Title, `manager+0x18` ou frame frontend n'est encore
établi. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

## État autoritaire courant — cycle 1794 — enfant pending, promotion active ouverte

Le corps canonique `0x82323468..0x823235CF` ferme le consumer immédiat du
`MovieController` enfant observé au cycle 1793. Après construction ou
réutilisation, le résultat `C` est enregistré par le slot virtuel 4 puis
inséré dans la liste `A+0xE4/A+0xE8`. Le corps ne publie jamais `C` dans
`A+0xF4`; sa seule écriture à ce champ retire un ancien candidat actif.

`swg::MovieController` est traité comme contrôleur de timeline UI scriptée,
distinct d'un décodeur vidéo et de `CModeTaskTitleMovie`. La route active
qualifiée appelle `0x82323BB8(A+0xF4)`, où les offsets de `MovieMemory` sont
remis à la VM. La frontière est donc la promotion pending→active de
`A+0xE4/A+0xE8` vers `A+0xF4`, puis le premier tick de l'enfant.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

## État autoritaire courant — cycle 1793 — MovieController enfant observé, frontend ouvert

Le gate renderer reste fermé naturellement depuis le cycle 1786 : deux cold
runs sélectionnent `list 2 / draw_index 2 / handle 0x0E000059`, produisent Q1
côté guest, passent `921600/921600` samples et donnent le même logo Namco
visible. Aucun draw, handle, Q1, scheduler ou pixel n'est forcé.

Le contrat HLE fail-closed HSIO du cycle 1791 reste qualifié et la transition
naturelle Startup → Title reste fermée : au tick 2369, le manager publie le
vrai `CModeTaskTitleDemoOffline` de vtable `0x820113E4`, dont l'état atteint 1
aux ticks 2384/2385. Le pulse START tick 3000/release 3001 atteint ensuite les
globales logiques current/pressed à `0x10`, deux dispatchs virtuels et les cinq
callbacks GetCurrentLevel, GetCurrentMission, GetCurrentMode, SendMsgI(M102)
et OnVoice2D. L'input n'est plus la frontière.

Dans `[2990,3041)`, aucun appel `0x820EA4A8`, writer `manager+0x18`, changement
de tâche ou mutation du Title n'est observé. La statique ferme les writers
`MovieController+D5`, les quatre lanceurs SWG et la chaîne commune vers
`menu_endMode` sans trouver de producteur post-TitleUS joint au listener
`0x8217C890`. Elle ne permettait toutefois pas d'exclure un nouvel objet
`MovieController` construit par un événement guest distinct.

La qualification statique ferme désormais le caller naturel `0x82322300`,
l'agrégat `A`, le storage incorporé `B=A+0x08` et la sélection de `D` depuis
les tables de B jusqu'à la factory virtuelle `0x820D18C8`. Le décompresseur
global `0x82278F78`, `swg::MovieMemory` et ses slots Add/Count/GetAt/Clear
restent correctement séparés des helpers globaux ACC
`0x823233B0/0x82323468`. Le writer statique de record #2/draw 2/handle 59
reste une question d'auteur distincte ; il ne réouvre pas le gate renderer
déjà fermé par l'exécution naturelle.

Une trace observation-only qualifiée observe au tick 3001, sous le vrai Title,
le callsite enfant `0x8232356C` appeler la méthode virtuelle `0x820D18C8`,
puis l'initialiseur d'instance `0x82323808`. Le résultat est un
`MovieController` de vtable `0x820304D8`; `A`, `B=A+8`, `D`, `s=-1` et
`MovieMemory` concordent avec l'ABI statique. Cela réfute l'ancienne hypothèse
d'absence de reconstruction post-START.

La portée reste bornée : `0x8232356C` est une adresse de retour/callsite, pas
une fonction. L'observateur s'arrête au retour de cette factory enfant et ne
prouve ni publication racine dans `A+0xF4`, ni remplacement, ni `EndMode`, ni
listener Title, ni requête mode-manager. Les appels génériques ultérieurs à
`0x82323808` ne sont pas corrélés à cette invocation. Le report finit à
`max_ticks=3407` avec `frontend=false`, `mission=false`, `terminal=false`; le
backend headless ne qualifie aucune frame visible.

Le prochain gate reste statique : qualifier le corps contenant le callsite
`0x8232356C`, le prédicat de construction, la source de `D` et le
consumer/publicateur du résultat. Seulement depuis cette chaîne qualifiée,
chercher un réarmement D5, `EndMode`/`menu_endMode`, le listener
`0x8217C890` ou une requête `manager+0x18`. Une autre trace exige d'abord un
nouveau prédicat causal et un `done_when` plus précis. Le gate frontend reste
état guest persistant + frame visible post-transition. `supported=false`.

Preuves autoritaires :
`artifacts/goal-playable/title-start-edge-runtime-final-20260823/RESULT.md`,
`artifacts/goal-playable/swg-acc-caller-join-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-post-start-moviecontroller-join-runtime-20260823/RESULT.md`,
`analysis/demo/ac6-demo-structural-types-v1.json` et
`reports/cycle-1793-demo-post-start-child-moviecontroller.md`.

Les sections suivantes sont historiques ; leurs anciennes frontières restent
supersédées par le cycle 1793.

## Historique cycle 1788 — vrai Title et frame SWG supposée post-START

Le gate renderer est fermé. Deux cold runs naturels identiques sélectionnent
`list 2 -> draw_index 2 -> handle 0x0E000059`, produisent un Q1 invité non
nul et guest-owned, font passer `921600/921600` samples, puis affichent un
logo Namco centré reconnaissable dans le readback 1280x720. La trace est
identique octet par octet entre les deux runs (SHA-256
`389ba502569afa3c9b7956b84f98f5a777aa311a446dda7955cc5daed5bb889b`),
comme le PPM (`6c0ab7ac...`). Les couleurs restent imparfaites, mais aucun
draw, handle, Q1, scheduler ou pixel n'est forcé. Preuves :
`artifacts/goal-playable/title-vertex-window-runtime-20260823/RESULT.md` et
`artifacts/goal-playable/title-vertex-window-runtime-20260823-r2/RESULT.md`.

La reprise Ghidra canonique ferme ensuite `CTaskModeManager` :
`0x8218E970` sélectionne une factory globale, `0x8218EA88` est le setter
d'instance `manager+0x10`, `0x82190B18` est la méthode de transition qui
range finalement le nouvel objet dans `+0x08` et l'insère via
`[*0x82822F08]->slot+0x0C`, et `0x827435F8` est statiquement prouvé
`CTaskModeManager*`. Aucun shim n'est justifié. Preuve autoritaire :
`artifacts/goal-playable/frontend-mode-manager-canonical-ghidra-20260823/RESULT.md`.

Le vrai objet titre est maintenant séparé sans ambiguïté : la factory globale
`0x82192680` alloue `0x78` octets et appelle le constructeur d'instance
`0x8218DC10`; `0x820113E4` est la vtable primaire globale de
`CModeTaskTitleDemoOffline` et `0x82011384` sa vtable secondaire
`CSwgListener` à `this+0x68`. La méthode listener `0x8217C890` appelle le slot
parent `+0x48 = 0x8218AB98`, écrit `Title+0x44=3` et `Title+0x0C=2`; trois
updates `0x8218A7A8` plus tard, le slot `+0x4C = 0x8218AA30` précède
l'écriture de `manager+0x18=1`. Cette chaîne est qualifiée **si**
`menu_endMode` atteint le listener ; elle n'est pas une jonction naturelle
depuis START.

`CModeTaskMissionTitle` est un type distinct (`RTTI 0x823918C4`, vtable
`0x8200E5C4`, update `0x82185198`) et n'est pas le propriétaire du vrai titre
démo. Toute la famille `CModeTask{StartUp,Title,Loading,Game}DemoOffline` est
propre à la démo et absente des prototypes retail Preview; aucun layout,
constructeur, slot, attribut ou nom de méthode n'en est transféré. Le listener
global `0x826DF804` et `E000004C/0x821A8C88` sont également réfutés comme
arête manquante vers le manager.

Le SWG `brandLogo` contient bien `EndMode`, mais le cadrage
`M102=0/lookup 0x0B -> prédicat EndMode` est maintenant réfuté. Les deux
valeurs sont évaluées dans un item déjà ajouté à `MovieMemory`; l'audio est un
second item indépendant. Le vrai sélecteur est le handler type 6
`0x82322A80`, qui enfilerait `EndMode` uniquement pour une commande exacte
`{type=6,payload=0x00000E04}`.

Dans la fenêtre START qualifiée, le chemin d'initialisation republie la plage
du `swg::MovieController`, de deux entrées à une entrée type 4. Le même LR
`0x82323860` prouve ce chemin, mais ne distingue pas à lui seul une nouvelle
instance d'une adresse réutilisée, ni lequel de storage `B` ou descriptor `D`
change. Le handler type 6 disparaît, alors que la couche parallèle reste
vivante par
`0x82326608 -> 0x82326420 -> 0x82325E70 -> 0x820EB200`. La frontière jouable
ne se situe toutefois pas dans ce consommateur : RTTI et vtable prouvent que
`0x820EB200` est le slot 7 d'une instance `CSwgRenderer`, et le CFG ne fait
que produire un record CPU de layout `0x70`, ensuite sérialisé et mis en
file. `0x82325E70` est un callback de table globale, pas une méthode prouvée.

L'analyse exhaustive des douze descripteurs `brandLogo` couvre 2 351 frames
et onze éléments type 6, sans payload brut littéral `0xE04`. Cette absence ne
ferme pas la relocation vers le buffer heap décompacté. Les trois bords
statiques ouverts sont donc la carte brut→heap, le producteur de `B/D` à
START et l'état persistant de `CModeTaskTitleDemoOffline` pouvant déclencher
une transition indépendante. Aucune trace de `M102`, `0x0B` ou du renderer
n'est justifiée. Le logo Namco appartient à une époque nettement antérieure
au prompt visible « PRESS START ».

Preuves autoritaires :
`artifacts/goal-playable/title-start-route-static-20260823/RESULT.md`,
`artifacts/goal-playable/start-title-object-rtti-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-swg-post-start-static-correction-20260823/RESULT.md`,
`artifacts/goal-playable/title-frame-descriptor-static-20260823/RESULT.md`,
`artifacts/goal-playable/title-parallel-matrix-consumer-static-20260823/RESULT.md`
et `reports/cycle-1788-demo-title-swg-frame-switch-correction.md`.

Les sections ci-dessous sont l'historique antérieur ; lorsqu'elles décrivent
le sélecteur SWG ou la publication du mode comme « gate courant », elles sont
supersédées par l'état ci-dessus.

## Gate BrandLogo — rupture au sélecteur de liste SWG (23 août 2026)

La trace observation-only ciblée ferme la fenêtre titre 190..320 : l'owner
SWG imbriqué `0x2E3CE490` a une frame valide mais unique, sélectionne
`list_index=0`, puis une liste d'un seul record `draw_index=0` raccordé au
handle `0x57`. Aucun record `draw_index=2/handle59` n'est publié. L'owner
parent avance simultanément de la frame 0 à 31, ce qui réfute un gel global du
scheduler ou de la task. La perte du logo est donc antérieure au renderer,
au worker et à PM4 : elle se situe au producteur/script qui doit changer la
frame ou la liste imbriquée vers l'entrée `draw2`. Preuve :
`artifacts/goal-playable/brandlogo-draw2-selector-runtime-20260823/RESULT.md`.

## Gate renderer titre — blend fermé, logo sélectionné en amont (23 août 2026)

Le pipeline Vulkan titre applique maintenant le blend Xenos naturellement
observé (`RB_BLENDCONTROL0=0x00010706`) sous une qualification fail-closed;
les autres pipelines restent en overwrite. Le build et six tests ciblés
passent. Un replay borné à 400 ticks produit 66 captures 1280x720 et des
readbacks évolutifs, mais chaque capture reste un aplat bleu. La perte n'est
donc plus le blend ni le resolve.

Le même payload invité enregistre naturellement trois NTXR : `0x57` (64x64),
`0x58` (512x512) et `0x59` (`003_NTXR`, wordmark Namco 1280x720). Les draws
atteints sélectionnent `0x57`, puis brièvement `0x58`, mais pas encore `0x59`.
La frontière active est exclusivement le sélecteur/producteur du record
`draw2 -> 0x59`; aucune substitution de ressource n'est autorisée. Preuves :
`artifacts/goal-playable/title-blend-runtime-20260822/RESULT.md` et
`artifacts/goal-playable/brandlogo-resource-entry-runtime-20260822/RESULT.md`.

## Gate BrandLogo — constructeur de commandes atteint (22 août 2026)

Le probe borné `brandlogo-consumer-runtime-20260822` ferme le consommateur de
liste et son worker : la garde `0x826F61B8` est valide, le record type 1 avec
ressource `0x0E000057` passe par `0x82119488 -> 0x82118FA0 -> 0x821185A8 ->
0x821186B0`, puis le thread 14 appelle naturellement `0x821187A8 ->
0x821B4D80` avec le contexte renderer `0x10041A00`. Pourtant le ring reste à
`RPTR=WPTR=0`, sans soumission ni paquet PM4. La frontière ouverte est donc le
cursor/flush après `0x821B4D80`, pas la ressource ou la liste BrandLogo. Voir
`artifacts/goal-playable/brandlogo-consumer-runtime-20260822/RESULT.md`.

## Gate BrandLogo — géométrie publiée, consommation aval ouverte (22 août 2026)

Le probe natif borné `acc-resource-runtime-20260822` ferme les deux gardes
amont : les slots `0x0E000057/58` ne sont pas le sentinel `-1`, la géométrie
est non nulle, puis le guest atteint naturellement `0x82095DF0 -> 0x821DEED8`
et publie des records de 112 octets sous l'owner `0x8281EAF0`. Le noir ne
vient donc ni du mapping ACC immédiat ni de `0x820EA9A0`. Le premier contrat
ouvert est le consommateur/flush aval de cette liste et son raccord à la
soumission Xenos; aucun record, draw ou pixel synthétique n'est justifié.
Voir `artifacts/goal-playable/acc-resource-runtime-20260822/RESULT.md`.

## Gate SWG callback — appelé mais non armé (22 août 2026)

La trace native bornée `artifacts/goal-playable/swg-callback-arm-20260822/`
ferme l'ambiguïté du dispatch SWG : `CSwgCallback` (`vtable 0x820061FC`)
atteint naturellement son slot `+0x14 → 0x820CDF30`, avec `this+8=1`. Le
champ `this+9` reste nul, car les mots lus via
`*(u32*)0x823C27E0 + 0x23980/+0x23988/+0x2398C/+0x23990` sont des bitsets
d'état logique/action (et non une table de ressources) et restent nuls; la
garde `0x826DFC48` vaut également zéro. Aucun draw normal, PM4 visuel ou pixel
RT0 n'est produit; le frontbuffer reste `0/921600` non nuls à 3000 ticks.

Le VFS lit bien `DATA.TBL` et `DATA00.PAC` au démarrage (`vfs2/`), mais cette
passe est bornée avant de pouvoir attribuer le chargement SWG. Les écrivains
qualifiés de ces bitsets sont `0x821DE990` et `0x821DE6E0`; le bord causal
restant est leur état SWG/script ou le consommateur de la garde globale, pas
le renderer. Aucun bit, callback, record ou pixel n'a été synthétisé. Voir
`artifacts/goal-playable/swg-callback-arm-20260822/RESULT.md` et
`BLOCKER.md`.

## Gate courant — entrée de jeu non publiée (22 août 2026)

La passe `task-loading-publication` ferme aussi le faux bord de terminaison :
`0x8218CE20`/`0x8218CCD0` ne font que sonder et terminer `CTaskLoading`,
`0x82259D10` ne met à jour que les tâches déjà insérées, et
`0x82259E18`/`0x82259FF8` gèrent insertion/initialisation de listes. `0x821929A8`
ne fait qu'avancer les champs de requête du `CTaskModeManager` et sonder son
loader embarqué. Aucun de ces points ne publie `CModeTaskGameDemoOffline` ou
n'appelle `0x8217C678`; le prochain bord causal est donc la factory amont qui
construit le mode puis l'insère. Preuves :
`artifacts/goal-playable/task-loading-publication/RESULT.md` et `BLOCKER.md`.

Le follow-up owner/r8 ferme la provenance AVI : `0x8200B6FC/+0x04` cible
`0x82165CC0`, mais aucun constructeur ne publie `base+0xC` comme receiver et
aucun producteur de `r8` n'est identifié. Le writer type 1 reste inaccessible
sans état invité synthétique. Preuve :
`artifacts/goal-playable/owner-r8-followup/RESULT.md`.

Le slice statique `game-entry-static` ferme l'absence de pont partagé
qualifié : `0x8217C678` installe `0x8217C4D8`, mais
`CModeTaskGameDemoOffline` n'est jamais publié. Le thunk START `0x820D32D0`
reste un dispatch virtuel `vtable+112` dépendant de l'objet; le remplacer ou
forcer `0x8217C4D8(-3)` serait un état guest synthétique. Provider, listener et
réveil `E000004C` sont déjà qualifiés. Le prochain discriminateur est la
terminaison/publication de `CTaskLoading` vers le mode manager autour de
`0x8218CE20`, `0x82259E18/0x82259FF8`, `0x821929A8`. Preuves :
`artifacts/goal-playable/game-entry-static/RESULT.md` et `BLOCKER.md`.

## Gate courant — route du logo Namco (22 août 2026)

La sonde native bornée du 22 août (`runtime-current-20260822`) a atteint le
timeout à 1622 ticks sans produire de record type 1–4, AVI receiver ou paquet
PM4; les seules écritures observées concernent les pointeurs de contrôle de
queue, dont les slots restent nuls. Les tranches ACC et AVI parallèles ferment
également les deux jonctions candidates : `0x8219EE40`/`0x8219F5D0` ne
publient pas le descripteur consommé par `0x8219E580`, et
`0x82166550`/`0x821674A8` n'exposent pas leur receiver ajusté à
`0x82165CC0`. Aucun changement runtime n'est justifié par ces résultats.
Preuves : `artifacts/goal-playable/runtime-current-20260822/`,
`artifacts/goal-playable/acc-provider-deep/RESULT.md` et
`artifacts/goal-playable/avi-record-deep/RESULT.md`.

La dernière tranche statique autour de `0x8218BFB0`/`0x8219BFB0` est
négative : `0x8218BFB0` sélectionne des clés/tables de providers et
`0x8219BFB0` n'est qu'un `lwz r4,0x20(r31)` dans `0x8219BF40`. Aucun des deux
ne relie `DATA.TBL[170/171]`, `brandLogo` ou `texture_id 2` au `source+0x20`
consommé par `0x8219E580`. Le gate reste donc bloqué au raccord contenu→ACC;
aucun mapping synthétique n'est autorisé. Preuve :
`artifacts/goal-playable/acc-guest-join-next/RESULT-final.md`.

La route du premier logo est qualifiée comme un film/cue SWG
(`0x820E8F90 → 0x820EA4A8 → 0x821728C0`) et son payload disque est maintenant
identifié : `DATA.TBL[170/171]` contient `FHM → FHM → SWG "brandLogo"`
(118244 octets) avec huit feuilles NTXR soeurs, chacune portant `GIDX`
`0x08000000`. Le tableau de textures du SWG sélectionne précisément
`texture_id 2 → 003_NTXR.ntxr`, dont le décodage est le mot-symbole rouge
`namco®` sur fond blanc ([artefact visuel](artifacts/goal-playable/brandlogo-swg-refs/png/0002_1280x720_fmt14_mip1.png)).
Cette preuve nomme enfin les octets Namco de la démo, mais ne relie pas encore
le SWG/NTXR exact à la table ACC guest consommée par `0x8219E580`.
Le renderer existant conserve toutefois la chaîne réutilisable
`0x8219E580 → 0x821A00E8 → 0x8219F080 → 0x820EB200 → … → 0x822F5BC8`.

Le runtime reste donc sans image visible : aucun draw guest non-bootstrap ne
change RT0 et aucune capture 1280×720 non noire n'est produite. Aucun logo
host-peint, appel forcé, record synthétique ou nouveau chemin Xenos n'est
autorisé. La provenance immédiate de l'ACC est maintenant qualifiée
(`0x8219E580 ← 0x821A02C0 → 0x8219E768`, ressource parallèle
`0x821A0180 → 0x8219F080/0x8219F1C0 → 0x8219E428`), mais la jointure vers
PAC/FHM/DATA.TBL et les handles `0x0E000057..0x0E00005E` reste ouverte.
Preuve : `artifacts/goal-playable/namco-logo-route/RESULT.md`,
`artifacts/goal-playable/brandlogo-swg-refs/RESULT.md` et
`artifacts/goal-playable/brandlogo-loader-join/RESULT.md`.

La dernière tranche statique ferme la fausse jonction par le callback de
démarrage : `0x820E8F90 → 0x820EA4A8 → 0x821728C0` ne fait que déléguer à la
tâche propriétaire et passer son état à 2. Elle ne construit ni ACC ni
record. Les consommateurs restent séparés (`CResourceLoaderSwg` vtable
`0x82012DC4/+0x14 → 0x8219E580` et `CSelectAircraftSetupManager`
`0x8200A254/+0x04 → 0x82127D40`). Aucun write qualifié vers `+0x18`, `+0x1c`
ou `+0x20` ne relie encore ces routes au payload `brandLogo`; aucun patch
runtime ou renderer n'est donc justifié. Preuve :
`artifacts/goal-playable/acc-guest-join-next/RESULT-followup.md`.

La passe du registre DPL ferme négativement la piste `0xCB..0xCF` : ces
valeurs deviennent les clés `DPL::[cb..cf,0]`, rejoignent seulement
`0x8219E768`, et ne sont reliées ni à `0x8219E428` ni à PAC/FHM/DATA.TBL/SWG.
Le producteur ACC et la jointure `brandLogo → ACC` restent donc le seul
blocage statique nommé. Preuves :
`artifacts/goal-playable/logo-key-registry/RESULT.md` et
`artifacts/goal-playable/swg-payload-static/RESULT.md`.

## Gate courant — owner/r8 et premier record guest (22 août 2026)

Le gate statique/runtime courant ne qualifie encore aucun payload visuel. Les
constructeurs initialisent `0x8200B6FC` à `base+0xC`, mais aucune publication
qualifiée de cet adjusted receiver vers `0x82165CC0` n'est connue. L'îlot
`0x82374AD0` est classifié comme initialiseur de la base `0x82731A30`, sans
publication AVI. Le writer type 1 est relié à la queue
(`0x821075A0 → 0x82117410 → 0x820FF788`) et exige
cinq ressources non nulles; le worker utilise le service global `0x82386C58`,
slot vtable `+0x08`, dont la valeur-flow reste inconnue.

La passe titre complémentaire a identifié les writers `0x820B3E80` et
`0x82321E18` pour des champs `+0xE8`, mais aucun join statique vers l'owner de
`0x820D29E0`, l'AVI ou les records n'est fermé.

La passe AVI finale confirme que `0x82166550` et `0x821674A8` sont les seuls
constructeurs qualifiés qui écrivent `0x8200B6FC` à `objet+0xC`; aucune
publication vers `0x82165CC0` n'est qualifiée. Une trace bornée (ticks 220:223,
plage stores
`0x2e000000..0x2f000000`, 300 ticks) termine avec 192 PRESENT mais zéro
soumission ring/PM4 et aucun owner ciblé. Elle confirme le blocage sans
autoriser de shim ou de changement renderer.

La passe provider sépare les contrats : `0x82165AF8` sélectionne dans
`DAT_826F6188`; `0x821080D0` convertit les cinq clés issues du payload en IDs
pour `DAT_826F6124`; `FUN_821ee130` alimente seulement `record+0x108`. Le
dernier join statique est la population des entrées de `DAT_826F6188` par
`0x82165040`/sa voie jumelle et la cible du service `0x82386C58`.

La trace bornée START (ticks 3000/3001, fenêtre 2990:3040) atteint
`0x820FEFA8` avec `record_type=0`, sans `0x8210A1C0`, `0x82117410` ou
`0x82165CC0`, et reste pré-frontend avec zéro paquet PM4. Aucun état Xenos,
shim ou record synthétique n'est autorisé. Le détail est dans
`artifacts/owner-r8-static/RESULT.md` et `BLOCKER.md`.

Validation native du checkpoint : codegen-ON CTest `26/26`; codegen-OFF
build + CTest `27/27` (refus CLI guest inclus); installation au préfixe
portfolio réussie avec `test ! -e bin/bin`. Le prochain gate doit qualifier
statiquement le service/vtable et les ressources avant toute extension du
renderer.

## Gate statique limité — producteur IB non observé par conception

La watch historique ne couvre que `0x1274A000..0x1274CF54`; elle exclut donc
les adresses heap du bootstrap. Aucun PC de producteur ne pouvait apparaître
dans les anciens runs. La prochaine observation est une watch native d'un
tick sur le seul IB `0x16AE0980..0x16AE0A40`.

## Gate limité — lot bootstrap de 24 draws

Le lot précoce de 24 `PointList` est précédé de `RB_COLOR_MASK = 0` et ne
livre ni payload vertex ni état RT0 qualifiant. Il ne peut pas être promu
comme writer visible, mais l'absence d'effet interne Xenos ne se déduit pas
de cette seule observation. Reprendre par le producteur de cet IB.

## Gate fermé — contrat RT0 vers lecture

Le draw normal qualifié produit le buffer RGBA consommé par la résolution
neutre, avec le draw de copie et le PRESENT déjà qualifiés. Le harness EDRAM
couvre des sources uniforme et structurée, et rejette une sortie altérée. Ce
contrat n'établit pas encore de pixel non noir ni de frontend.

## Gate fermé — sélection du draw RT0 qualifié

La construction Vulkan conserve désormais le rectangle normal et le rectangle
de copie après qualification exacte de leurs registres, avant le court-circuit
du cache de pipelines. La compilation et les tests configurés passent. La
frontière suivante est le contrat de lecture RT0, non le shader ou le rendu.

## Gate limité — owner transmis au slot virtuel

`0x820D29E0` est le slot `+0x0C` de la vtable `0x820064D8`. Ses constructeurs
écrivent cette vtable mais pas l'owner `+0xE8`; l'unique appelant direct du
constructeur est à nouveau une entrée de table. La reprise doit trouver le
site qui invoque ce slot et y remonter `r3` et le second argument.

## Gate limité — installation du contexte SWG

`0x82324188` installe conditionnellement `owner + 0xE8` dans
l'interpréteur puis appelle son slot `+4`; il n'initialise pas la vtable. Son
unique appelant construit l'interpréteur mais reçoit l'owner en argument. La
reprise statique remonte donc les appelants de `0x820D29E0` pour qualifier cet
owner avant de chercher le producteur de `+0xE8`.

## Gate fermé — handlers SWG post-START

Les indices 12, 19, 26 et 68 sont des thunks vers les slots `+0x80`, `+0xC8`,
`+0x64` et `+0xA8` de la vtable de `context + 0x0C`. L'indice 50 lit un mot
de flux puis appelle le slot `+0xCC`. Aucun handler ne touche directement le
sélecteur global. La prochaine frontière statique est l'installation de cette
vtable et ses cinq cibles concrètes.

## Gate fermé — consommateur de table SWG

`0x82325160` consomme les mots du flux à `context + 0x14` et appelle
`DAT_8264CEA8 + mot * 4`. Les cibles post-START sont les indices 12, 19, 26,
50 et 68. Son CFG n'accède ni au sélecteur global `0x823C27E0` ni au setter
`0x82171988`; le prochain slice statique est donc les handlers indexés.

## Gate limité — tables SWG post-START

Les neuf cibles relevées après START sont référencées par des tables de
données; six n'ont pas de fonction Ghidra. Les xrefs ne relient pas ces
entrées au producteur du sélecteur global et ne prouvent pas l'absence d'un
chemin indirect. La reprise statique doit reconstruire la table
`0x8264CED8..0x8264CFB8` et son consommateur.

## Gate fermé — divergence virtuelle SWG au START

La sonde bornée aux ticks 3000–3035 relève 22 appels sélectionnés au tick
3000, puis 69 au tick 3001. Au site `0x823251B8`, START introduit notamment
`0x82324548`, `0x82324440`, `0x823244B8`, `0x823248C8`, `0x82324C20`,
`0x820D7768`, `0x820DAFA8`, `0x820DD0B8` et `0x820DD158`. Le callback
`0x8218AB98` n'apparaît pas dans le périmètre des trois sites instrumentés;
cette absence n'est pas une conclusion globale. Reprendre statiquement par
ces cibles exclusives au tick 3001.

## Gate fermé — dispatcher native du titre borné

`0x820EA238` appelle le slot du contexte SWG; le chemin neutre atteint
`0x8217C890`, puis `0x8218AB98`. Aucun CFG ni xref direct de cette chaîne ne
mène au setter mission global `0x82171988`. Le choix START/neutre reste dans
le film SWG; la capture suivante est définie et ne doit relever que la cible
virtuelle, son contexte et son retour.

## Gate limité — objet titre distinct du sélecteur global

Le callback titre `0x8218AB98` stocke dans `this + 0x70` de son objet
`CModeTaskTitleDemoOffline`. Les producteurs de mission utilisent séparément
`PTR_DAT_823c27e0 + 0x70`. L'égalité de déplacement ne prouve aucune identité
d'objet; reprendre par le dispatcher titre et ses xrefs, sans runtime.

## Gate fermé — vocabulaire compact du loader

`0x82278F78` est qualifié statiquement comme décompresseur bitstream :
réservoir de bits, tables de nœuds, émission de littéraux et copies arrière.
Les mots hors `0..7` du flux sont des valeurs de sortie; leur source compacte
reste à relier au clip titre.

## Gate fermé — décodage borné du flux titre

La décompilation de `0x823246C0` et la reconstruction des octets déjà écrits
par `0x82278F78` expliquent les 23/23 appels box observés dans
`0x2DCB2438..0x2DCB2680` (opcode `0`, avec 13 opcode `7` intercalés). Le
premier mot hors du vocabulaire `0..7` est `0x2DCB2448 = 0x1A`; la suite doit
qualifier le format compact du loader, sans nouveau runtime par défaut.

## Gate fermé — chargeur de flux relié au contexte titre

Le chargeur `0x82278F78` écrit les 8 192 octets de `0x2DCB2000..0x2DCB3FFF`
au tick 2435. Au tick 3001, les PC du contexte titre sont dans cette même
plage. Le flux de titre est donc qualifié; reste à décoder localement la
fenêtre d'opcodes qui produit les requêtes mission/niveau.

## Gate fermé — frontière statique du branchement de film

Le flux de statements est un buffer heap interprété par `0x823246C0`, chargé
par décompactage depuis une entrée non reliée statiquement au contexte titre.
Les tables de natives ne révèlent pas ce flux. La suite requiert donc une
capture bornée chargeur → buffer → interpréteur titre, avec ses critères déjà
définis.

## Gate fermé — consommateurs statiques du triplet sélectionné

La table de commandes du titre lie `GetCurrentMission` et `GetCurrentLevel`
aux natives démo `0x820EA550` et `0x820EA598`. Leurs CFG appellent les getters
de mission/niveau qualifiés; le second remappe 6 et 7. Les lecteurs sont donc
attribués au script titre, mais pas encore au branchement de film post-START.

## Gate fermé — écrivain direct de mission au START écarté dans la fenêtre

Le slot mission réellement sélectionné est `0x823C2F14`, non l'ancienne cible
déréférencée. Avec START injecté au tick 3000, il reste inchangé aux ticks
3000–3004 et la capture de stores bornée sur ce slot ne relève aucune écriture.
Les neuf appelants directs du setter ne l'atteignent donc pas dans cette
fenêtre précise; cela n'établit ni les appels après le tick 3004 ni le chemin
de progression du film.

## Gate fermé — producteurs statiques de mission bornés

Neuf appelants directs du setter mission démo sont qualifiés. Ils couvrent des
valeurs littérales, calculées et issues d'une table UI, mais leur exécution au
START et l'écrivain de niveau ne sont pas prouvés. Une capture ciblée devra
relever uniquement le PC appelant et la valeur de mission écrite.

## Gate fermé — bornes START dans le XEX démo

Dans `ace-combat-6-demo`, les quatre observables START appartiennent aux
fonctions attendues : `0x82095B80` lit la mission et `0x82171988` écrit le
même champ sélectionné; `0x820E9290` lit le niveau voisin. Reprendre par le
producteur statique de l'index et des champs, sans nouveau runtime.

## Gate fermé — entrée du codegen démo qualifiée

`build-codegen-on` consomme `demo-game-file/extracted/stfs-root/Default.xex`
et son manifeste Ghidra démo dédié. Les observables du runtime codegen relèvent
donc de `ace-combat-6-demo`, pas du corpus retail `ace-combat-6`. La prochaine
preuve qualifie les adresses START dans ce projet démo.

## Gate fermé — réconciliation du corpus Ghidra canonique

Le corpus `ace-combat-6` / `default.xex` contient les fonctions qualifiées,
mais ses bornes démontrent que les adresses du code généré du demo ne portent
pas leur sémantique dans ce PAL. Ne pas transférer les noms ou CFG générés vers
le corpus canonique. La provenance de `gs+112` reste non établie pour le demo
tant que son entrée binaire n'est pas qualifiée statiquement.

## Gate fermé — cible du setter de mission au START réfutée

Dans la fenêtre START 3000–3004, l'état de jeu actif reste `0x82774B00`, mais
son champ `+112` vaut `0x8201DFDC`, table de méthodes et non sous-objet de
slots. Aucun store n'atteint l'ancienne adresse `0x82775234`; cette absence ne
qualifie donc pas `0x82171988`. Reprendre par la provenance statique du champ
`gs+112` et des getters réellement exécutés, sans nouveau runtime.

## Gate fermé — valeurs SWG après START

Les getters START renvoient `0 / 0 / 2`; monde et clip SWG ne changent pas et
`menu_endMode` ne réapparaît pas. Les substitutions ciblées déjà faites ne
modifient pas ce résultat. Le prochain causal est le producteur de cet état ou
le bytecode du clip, jamais un draw RT0.

## Gate fermé — sélecteur SWG post-START du titre

Le monde SWG est publié par l'unique écrivain `0x820CE368`; ses wrappers ne
portent aucun choix entre attract et START. Le choix est dans le bytecode du
film ou ses valeurs lues. La prochaine preuve est une capture bornée des trois
getters et du contexte, pas un trace renderer.

## Gate fermé — avancement neutre de l'attract title

Le callback SWG neutre atteint `0x8218AB98`, qui écrit l'état titre à `+0x70`;
l'état 2 mène ensuite à la transition de tâche. START ne produit pas de draw :
il choisit des handlers distincts et supprime ce callback. La prochaine
frontière est donc le sélecteur SWG, avant tout examen RT0.

## Gate fermé — consommateur statique du compteur de titre

`0x820D3AC8` n'a qu'une référence de données, sa case de vtable. Aucun appelant
direct ne permet de rattacher statiquement le compteur heap à une transition.
Le burst SWG observé ne devient pas persistant : il ne prouve aucun draw RT0
non nul. La recherche revient aux producteurs de contenu après progression
guest qualifiée.

## Gate fermé — récepteur virtuel du START pendant le titre

Le slot `+0x70` des deux objets de titre cible `0x820D3AC8`. Cette routine
appelle le slot `+0x5C` d'un contexte puis soustrait son résultat à un compteur
guest. Le chemin est une évaluation de VM de script; aucune transition menu ou
mission n'en découle encore de manière prouvée.

## Gate fermé — dispatch START pendant le titre

La boucle atteint le titre avant que START puisse être consommé. À cette phase,
`0x820D32D0` est un thunk PPC : vtable de `r3`, slot `+0x70`, puis `bctr`.
Ce n'est ni une fonction métier ni un producteur d'état. La prochaine frontière
est le récepteur dynamique de ce slot, avec une capture bornée de son objet et
de sa cible seulement.

## Gate fermé — producteur PM4 du draw RT0

`0x821B55C0` construit le `DRAW_INDX_2` RT0 atteint et `0x821B9BC8` publie le
ring via le WPTR Xenos. Ce CFG n'écrit pas EDRAM depuis le CPU : un contenu
RT0 non nul ne peut être produit qu'à l'exécution Xenos du draw. La prochaine
frontière est une progression guest qui atteint un contenu non nul.

## Gate fermé — reflection varying du draw normal

Le vertex shader atteint exporte position et `interpolator0`, mais ses trois
couleurs et l'entrée `color0` du pixel shader sont nulles. Le geometry shader
de test perd les interpolateurs hors position, sans pouvoir expliquer la
noirceur de ce draw précis. Le prochain causal est le premier écrivain non nul
de RT0/EDRAM, sans runtime.

## Gate fermé — interface vertex du draw normal

Le record `XenosDrawCommand` ne retient aucun descripteur fetch ni octet guest,
mais le backend lie séparément le descripteur shared-memory qui couvre la plage
du rectangle normal. Le `vkCmdDraw` de trois sommets ne réfute donc pas une
entrée vertex au shader traduit. La frontière est désormais limitée à
l'interprétation shader/format/lane, sans pixel ni frontend promu.

## Gate fermé — disposition d'installation native

L'installation temporaire du build courant fournit le binaire sous `bin/` et
sa configuration sous `share/`; l'invariant d'absence de `bin/bin` passe.
Cette preuve est limitée à la disposition de l'artefact installé.

## Gate fermé — intégration locale du bridge natif

Le build courant de `recompilation/ace-combat-6-demo` compile et ses 27 CTest
passent avec l'audio dummy. Cette validation couvre l'intégration locale du
bridge, sans promouvoir de frontend, mission ou readback.

## Gate en pause — producteur de `VdGlobalDevice+0x4084`

`0x821C64E8` initialise le device, enregistre `0x821B9710` via
`VdSetGraphicsInterruptCallback`, puis publie ce device dans
`VdGlobalDevice`. L'écrivain du champ interne `+0x4084`, lu ensuite par
`0x821C5190`, appartient à l'interface Vd externe et reste non qualifié.
Après cinq slices statiques, aucun runtime.

## Gate fermé — interface du callback indirect graphique

`0x821C5190` appelle conditionnellement `VdGlobalDevice+0x4084` sous le
spinlock `+0x4130`, avec un record local de six mots provenant des compteurs
du même objet. La cible concrète reste inconnue, mais l'interface est bornée
statiquement ; aucun effet PM4, pixel ou frontend n'est attribué.

## Gate fermé — contrat CPU du callback XAudio

Le contrat local de sélection CPU du callback XAudio compile dans le bridge
actuel et son test ciblé passe. Cette vérification ne promeut aucune exécution
audio, frontend ou mission ; le choix CPU explicite reste borné par les
preuves déjà consignées.

## Gate en pause — écrivain non nul du record render

`0x820FF710` initialise explicitement le champ de contrôle du slot puis avance
le producteur ; le worker recopie le record de 96 octets. La recherche des
appels directs vers ce producteur ne donne aucun appel `bl` : son entrée reste
indirecte ou atteinte par saut. Après cinq observations statiques, aucun
écrivain non nul ni consommateur de payload n'est qualifié. Aucun runtime.

## Gate fermé — faux lecteur de payload render `0x820FEFA8`

Le worker `0x820FFCA0` copie le slot de 96 octets dans sa pile puis le passe à
`0x820FEFA8`. Ce callee remplace immédiatement `r3` par le retour de
`0x82327104`, sans préserver l'argument du slot ; il ne peut donc pas lire ce
payload. La suite est le premier écrivain statique non nul du record, sans
runtime.

## Branche en pause — lecteur fournisseur `+0x20`

Les écritures `0x822FFCC8`/`0x822FFDB8` vers `fournisseur+0x20` sont établies,
mais leur lecteur n'est pas identifiable par le scan D-form seul : 1 066
candidats `r3`/`r4` restent sans provenance d'objet. Le runtime n'est pas
autorisé pour combler cette frontière statique.

## Gate fermé — corps des slots `+0` de fournisseurs non-bootstrap

- Les trois entrées sont des feuilles PPC sans frontière Ghidra reconnue et sans appel direct : `0x822F2F20` retourne immédiatement `0x80004001` (`E_NOTIMPL` dans le header SDK local), `0x822FFCC8` copie 16 octets de `r4` vers `r3+0x20` puis retourne zéro, et `0x822FFDB8` copie 16 mots (64 octets) au même offset puis retourne zéro.
- Avant leurs `blr`, aucun de ces corps ne fait de call/dispatch, n'accède à un ring, ni n'écrit de MMIO ou d'état GPU global. Ils réfutent donc la liaison ou soumission shader immédiate des fournisseurs du profil.
- Le seul effet non nul est l'état local `fournisseur+0x20`; le prochain gate doit trouver son premier consommateur statique. Aucun runtime n'est justifié.

## Gate fermé — vtables des fournisseurs non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, le balayage RTTI qualifie sept vtables `Shader::Parameter*` : `0x8202ADF8`, `0x8202AE14`, `0x8202AE30`, `0x8202AE4C`, `0x8202AE68`, `0x8202AE84` et `0x8202AEA0`.
- Leurs slots `+0` ne sont pas homogènes : ils ciblent exactement `0x822F2F20`, `0x822FFCC8` ou `0x822FFDB8`. Le dispatch `0x822E0B90` doit donc conserver cette indirection à trois implémentations.
- La zone d'initialisation `0x822FB2B8–0x822FB610` installe ces vtables, mais aucune frontière de fonction n'y est reconnue : aucun pseudocode n'est retenu comme preuve. Le prochain gate qualifie seulement les trois corps de slot `+0`; aucun runtime n'est justifié.

## Gate fermé — effet de fournisseur `0x822E0B90`

- Le CFG PPC de `0x822E0B90` charge `*(u32 *)r3`, puis `*(u32 *)vtable`, place ce mot dans `CTR` et exécute `bctr`.
- Cette routine est une dispatch virtuelle de slot `+0` en queue. Elle n’écrit ni buffer local, ni état GPU, ni PM4.
- Les trois fournisseurs du profil atteignent donc une interface exacte dont l’implémentation de slot `+0` porte tout effet ultérieur. Le prochain gate doit qualifier cette vtable; aucun runtime n’est justifié.

## Gate fermé — lecteurs du profil global non-bootstrap `0x829D0800`

- Le seul lecteur distinct de l’initialiseur est la trampoline `0x822DEBD8`, qui passe le global `0x829D0800` à `0x822E9BD8`.
- Le prologue de `0x822E9BD8` conserve ce premier argument dans `r29`. Sa branche principale charge `r29+0xc`, `r29+8` puis `r29+0x10` aux sites `0x822E9D80/90/9C`, et appelle `0x822E0B90` pour chacun.
- Le CFG prépare ensuite un contexte et appelle `0x822F5608`, sans écrire de PM4 lui-même. La prochaine frontière est le callee commun `0x822E0B90`; aucun runtime n’est justifié.

## Gate fermé — provenance statique de l’enregistrement de profil non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, `0x822E9968–0x822E996C` matérialise `r3 = 0x829D0800`, puis `0x822E9974` pose le drapeau à `+0` et `0x822E9978` appelle `0x822E9A18`.
- `0x822E9A18` copie ce `r3` dans `r31` et y conserve les fournisseurs. `0x8232710C` est seulement l’aide de prologue qui préserve les registres non volatils.
- Le profil non-bootstrap a donc un stockage global exact `0x829D0800`. Aucun consommateur shader/PM4 n’est encore qualifié; le prochain gate doit partir exclusivement de ses lecteurs.

## Gate fermé — consommateur du profil de fournisseurs non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, `0x822E9A18` écrit les trois fournisseurs résolus dans un enregistrement retourné via `0x8232710C`.
- L’accesseur est un stub vide et ses appelants sont transversaux, y compris de nombreux sites du cluster `0x822E…`. Il ne fournit donc ni identité statique de cet enregistrement, ni xref sélective vers un consommateur.
- Aucun CFG qualifié ne relie les trois champs à un shader, microcode ou paquet PM4. La frontière reste opaque à cette indirection; aucun changement natif ni runtime n’est justifié.

## Gate fermé — implémentation de la slot de résolution `+0x2c`

- Le constructeur `0x822F8E68` installe la vtable `0x8202AAB4` de `Shader::ShaderContextXenon`. Son mot `+0x2c` cible `0x822F2B88`.
- `0x822F2B88` parcourt deux listes de fournisseurs puis appelle `0x822F89D0`; ce dernier applique un prédicat sur l’identifiant demandé et, en cas de succès, renvoie le pointeur du fournisseur. La slot résout donc une ressource, sans charger de microcode ni écrire de PM4.
- Dans l’initialiseur `0x822E9A18`, les trois valeurs retournées sont conservées dans l’enregistrement de profil. Le prochain lien utile est leur premier consommateur; aucun changement natif ni runtime n’est justifié.

## Gate fermé — sélection du pixel shader non-bootstrap

- Dans le projet Ghidra canonique `ace-combat-6-demo`, module `Default.xex`, le CFG de `0x822E3858` appelle le bootstrap puis `0x822E9948`; ce dernier pose cinq drapeaux globaux et appelle cinq initialiseurs. Aucun de ces deux CFG ne construit une clé de technique, ne charge un shader pixel ou n’émet un `IM_LOAD`.
- Le résolveur partagé `0x822E0B10` obtient un objet depuis le gestionnaire global puis délègue la résolution à la slot virtuelle `+0x2c`. Cette dispatch est la frontière exacte entre les profils initialisés et un éventuel shader consommé.
- La question est donc bornée statiquement, sans lien établi vers un pixel shader non-bootstrap. Aucun changement natif ni runtime n’est justifié.

## Gate fermé — origine EDRAM / readback du draw normal démo

- La chaîne locale est fermée : `execute_vulkan_neutral_resolve` passe les octets `normal.resolved_rgba8` au resolve, puis son certificat différentiel refuse tout writeback dont les données tuilées ne restituent pas exactement cette entrée.
- La matérialisation EDRAM n’est pas la divergence : son oracle isolé couvre un motif uniforme non nul, son placement EDRAM et la préservation du canari. La copie et la conversion qui suivent ne peuvent donc pas transformer silencieusement une source nulle en image exploitable.
- Pour le draw normal démo atteint, le contrat qualifié indique une entrée de readback entièrement nulle. La noirceur est antérieure à EDRAM, à la copie et au writeback; aucun changement natif ni runtime complet n’est justifié.

## Gate fermé — enveloppe de soumission pilote `0x821BFBA8`

- Le buffer passé à `NtWriteFile` est celui du canal sélectionné; sa longueur est calculée en unités de `0x200` octets. Le CFG ajoute un en-tête de `0x10`, copie conditionnellement `0x38` octets depuis `device+0x3584`, puis ajoute conditionnellement `0x600` octets avant de publier la longueur.
- `0x821A66E0` transmet le pointeur et cette longueur à `NtWriteFile` sans décoder le buffer. `0x821BA880` ne fournit pas de layout supplémentaire : il sauvegarde un curseur et déclenche le drain.
- Aucun champ observé ne relie cette enveloppe à un WPTR ou à un descripteur PM4. Le format interne est donc opaque derrière le pilote; aucun changement natif, test synthétique ou runtime n'est justifié.

## Gate fermé — contrat natif de soumission de `0x821BFBA8`

- Le rapprochement réfute l'isomorphisme supposé : le CFG démo de `0x821BFBA8` assemble un buffer de canal puis appelle `0x821A66E0` / `NtWriteFile`; il ne publie pas directement un WPTR MMIO.
- Le point partagé natif est séparé : le store guest à `0x7FC80714` est routé dans `guest_bridge.cpp` vers `apply_xenos_mmio_write`, qui consomme le ring et met à jour le read pointer seulement après consommation.
- Les tests ring couvrent la progression, le wrap et le rejet avant writeback. Aucun écart dans le publisher Xenos n'est donc établi et aucun changement de code n'est justifié. Le format de l'enveloppe pilote demeure une frontière statique distincte.

## Gate fermé — premier producteur PM4 non-bootstrap

- La prémisse est réfutée : les cinq initialiseurs appelés par `0x822E9948`, dont `0x822E9A18` et `0x822EA010`, créent un objet via `0x822E6A38`, le configurent par dispatch virtuel, puis résolvent trois handles par `0x822E0B10`.
- Ils n’écrivent aucun buffer PM4, ring ou MMIO. `0x822E0B10` lui-même délègue la résolution à la slot virtuelle `+0x2c`; ce chemin est une frontière de ressources, non une soumission Xenos.
- `FUN_8232710c` est un stub sans corps dans cette analyse et ne permet pas de typer le receveur des stores relatifs. La première jointure ressource → PM4 doit donc être cherchée chez les consommateurs des handles, sans assimiler ces initialiseurs au producteur PM4.

## Gate fermé — effets de rendu bootstrap et transition non-bootstrap

- Les `IM_LOAD` du template entrent dans `XenosCommandProcessor`, créent deux `XenosShaderLoadCommand`, puis alimentent l’état vertex/pixel recopié dans chaque `XenosDrawCommand`; `GuestBridge::apply_xenos_typed_batch` place ces commandes dans la boîte renderer.
- Le CFG démo de `0x822E3858` appelle sans condition `0x822F0340` (bootstrap) puis `0x822E9948` (cinq initialiseurs de profils NSXR non-bootstrap). Cette séquence initialise des globaux distincts, sans sélectionner un shader ni émettre un paquet PM4.
- La première émission PM4 qui consomme un profil non-bootstrap n’est pas jointe par ce CFG ; elle reste une frontière statique séparée. Aucun runtime ne serait discriminant avant l’identification de ce producteur.

## Gate fermé — format PM4 du buffer indirect de `0x821B1D58`

- Le template compte 74 DWORDs (indices 0–73) : `INVALIDATE_STATE` (`0x3B`, un mot), deux `IM_LOAD` (`0x2B`, 26 puis 11 mots), puis des Type 0 vers des registres jusqu’à `0x2312`.
- Les deux `IM_LOAD` ont les formes natives exactes : stage sommet, taille 24 ; stage pixel, taille 9. L’invalidation exige aussi exactement un mot.
- Les Type 0 restent sous la capacité native de `0x8000` registres, et leurs indices ne franchissent aucune frontière opaque avec une valeur non qualifiée. Le scanner du ring conserve toutes ces formes sémantiques.
- Aucun écart statique n’est établi : ni test isolé ni runtime ne sont requis.

## Gate fermé — opérandes indirectes de `0x821B2BC8`

- `0x821B2BC8` écrit `0xC0013F00`, puis `device+0x35c4` comme adresse et `device+0x35c0 & 0x00ffffff` comme nombre de DWORDs.
- `0x821B20A0` alloue le buffer de 0x2000 octets via `MmAllocatePhysicalMemoryEx`; son implémentation locale aligne l’adresse au moins à la page. L’encodage de `+0x35c4` préserve donc les deux bits bas nuls et le `& ~3` de `capture_indirect` est une identité.
- `0x821B1D58` produit exactement 73 DWORDs (dernier store à l’index 72) et `0x821B20A0` les place dans `+0x35c0`. Le masque natif `& 0xFFFFF` préserve donc aussi le compte et l’unité est le DWORD des deux côtés.
- Aucun écart statique n’est établi : ni test isolé ni runtime ne sont requis.

## Gate fermé — paquets écrits après le drain `0x821BA780`

- Après un drain, `0x821B0D20` écrit les quatre Type 3 à un mot `0x60`, `0x62`, `0x61` et `0x63`, que le processeur natif qualifie comme sélecteurs de bin.
- `0x821B2BC8` écrit un Type 3 `0xC0013F00` à deux opérandes ; le bridge le reconnaît comme descripteur indirect et développe son contenu avant de l’envoyer au processeur de commandes.
- Les en-têtes et longueurs coïncident avec les deux décodeurs natifs. Aucun écart statique, test synthétique ou runtime n’est requis.

## Gate fermé — consommateur de la publication de `0x821BA780`

- Les appelants démo `0x821B0D20`, `0x821B2BC8`, `0x821C57D0` et `0x821C64E8` appellent `0x821BA780` quand leur curseur de commandes dépasse la limite courante, puis continuent à écrire le lot suivant.
- Le flux déjà qualifié de `0x821BA780` vers `0x821BA058`, puis `0x821B9BC8`, relie donc ce seuil au publisher WPTR du ring système.
- `0x821C64E8` appelle aussi `0x821BAAD0` sur son chemin d’initialisation, mais cette co-occurrence ne remplace pas la jointure de drain ci-dessus et ne nécessite aucun runtime.

## Gate fermé — sémantique native du segment de plage optionnel

- Les deux segments optionnels émis par `0x821B9DB0` ont la même forme : Type 0 vers `0xA31`, Type 0 de deux mots vers `0xA2F–0xA30`, puis Type 3 `WAIT_REG_MEM` (`0x3C`) à cinq mots.
- Le décodeur natif accepte les Type 0 dans les bornes du fichier de registres; `0xA2F`, `0xA30` et `0xA31` ne font pas partie de sa frontière opaque. Il traite aussi exactement le `WAIT_REG_MEM` cohérent de cinq mots.
- Il n’existe donc aucun écart statique entre ce contenu et le décodeur : aucun changement ni test synthétique n’est justifié, et aucun runtime n’a été utilisé.

## Gate fermé — entrées de contenu de `0x821B96B8`

- `0x821B96B8` échange atomiquement la paire 64-bit `device+0x2e28` contre le sentinelle et retourne ses moitiés basse et haute.
- `0x821C6928` initialise cette paire; `0x821B9648` la met à jour atomiquement depuis les chemins `0x821B8090` et `0x821B86F8`.
- `0x821B9DB0` consomme les deux moitiés pour émettre son segment optionnel de plage; elles ne changent ni le contrat de descripteur ni le publisher WPTR.

## Gate fermé — producteur de contenu de `0x821B9DB0`

- `0x821B9DB0` alloue lui-même le contenu par `0x821B9AE0`, l’écrit localement, puis renvoie son adresse matérialisée dans la première sortie.
- Sa seconde sortie est une longueur de `11` ou `22` dwords, déterminée par deux états privés du device.
- `0x821BA780` transmet ensuite exactement cette adresse et cette longueur à `0x821BA058`; la valeur optionnelle insérée dans le contenu provient de `0x821B96B8`.

## Gate fermé — source des descripteurs indirects du publisher

- Dans le CFG démo, `0x821BA780` obtient la longueur et l’adresse du contenu auprès de `0x821B9DB0`, réserve l’en-tête par `0x821B9AE0`, puis appelle `0x821BA058`.
- `0x821BA058` forme un descripteur local d’un élément et le transmet à `0x821B9BC8`; le publisher reste ainsi séparé de la production du contenu.
- `0x821B2BC8` peut appeler `0x821BA780` lorsque la capacité courante l’exige, mais cette relation ne prouve pas que le template qu’il écrit ensuite est le descripteur alors drainé.

## Gate fermé — contrat natif du publisher WPTR

- Le bridge valide l’écriture `CP_RB_WPTR`, parcourt le ring de façon circulaire, puis décode le triplet Type-3 `0xC0013F00` démontré pour `0x821B9BC8` avec adresse alignée et compte borné.
- Après le décodage du batch, il publie le WPTR et le pointeur de lecture local au même point partagé ; ce contrat correspond à la publication guest après écriture du triplet.
- La construction locale et les suites `ac6-demo-core-tests` et `ac6-demo-xenos-command-tests` passent. L’inclusion du traceur graphique est maintenant commune aux variantes générée et non générée, sans changement de la sémantique du ring.

## Gate fermé — publication du ring système

- Dans le projet Ghidra démo `ace-combat-6-demo` / module `Default.xex`, `0x821BAAD0` appelle `MmGetPhysicalAddress` puis `VdInitializeRingBuffer`, et initialise base, masque et producteur du ring aux offsets `+0x3a18`, `+0x3a1c` et `+0x2ac8`.
- `0x821B9BC8` relit ces trois champs, insère chaque triplet `0xC0013F00`, adresse, longueur dans le ring avec retour circulaire, puis écrit l’index final à `0x7FC80714` avant les barrières `sync/eieio/sync`.
- La jointure ring initialisé → writer WPTR est donc statique et directe. Aucun runtime n’a été utilisé.

## Gate fermé — contrat d’initialisation du ring natif

- Le wrapper `VdInitializeRingBuffer` reçoit la base physique et le logarithme calculé par `0x821BAAD0`; `configure_xenos_ring` conserve ces valeurs.
- `xenos_ring_snapshot` calcule `1 << (size_log2 + 1)` DWORDs. Cette capacité donne exactement le masque guest `(taille_en_octets >> 2) - 1` initialisé à `+0x3a1c`.
- `VdEnableRingBufferRPtrWriteBack` transmet l’adresse de writeback au bridge; la complétion publie le RPTR final à cette adresse. Les tests core couvrent initialisation, passage circulaire et writeback.

## Gate fermé — writer WPTR du ring système

- `0x821BAAD0` initialise le ring système : base `device+0x3a18`, masque
  `device+0x3a1c` et producteur `device+0x2ac8`, puis appelle
  `VdInitializeRingBuffer`.
- `0x821B9BC8` encode des triplets dans ce ring, met à jour
  `device+0x2ac8`, puis publie cet index à `0x7FC80714` entre
  `sync`, `eieio`, `sync`.
- La jointure est par les mêmes champs du device ; aucun appel direct entre
  l'initialiseur et le publisher n'est requis ni observé.

## Gate fermé — enveloppe pilote de `0x821BFBA8`

- Dans le binaire démo, `0x821BFBA8` passe par `0x821BA368` puis
  `0x821BA130` avec `VdGlobalDevice` et le mode `6` avant `NtWriteFile`.
  Cette chaîne réserve/synchronise le buffer global ; elle n'encode pas un
  payload PM4 documenté.
- Au point d'appel, handle et buffer sont lus par index dynamique dans le
  sous-objet de canal, et la longueur est dérivée/arrondie. Les offsets de ce
  canal ne sont pas couverts par le layout public `D3DDevice` localement
  disponible.
- L'enveloppe `NtWriteFile` est donc une interface pilote opaque, distincte
  du ring PM4. Aucun changement du bridge MMIO natif n'est justifié.

## Objectif

Faire fonctionner `recompilation/ace-combat-6-demo` sur l’unique
`Default.xex` démo PAL qualifié, du cold boot au menu visible, puis à une
mission jouable avec succès et échec endogènes, replays déterministes,
capsules et readbacks guest non noirs.

Identité canonique : Xbox 360 Xenon/Xenos, SHA-256 XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
projet Ghidra `ace-combat-6-demo`.

## Faits établis

- Le cold path natif atteint 3 036 ticks avec START au tick 3 000 et relâche
  au tick 3 001, sans trap : 23 threads guest et 2 899 appels `VdSwap`.
- `VdSwap` publie un frontbuffer guest `0x1374A000`, format 6, `1280×720`.
- Le second ring primaire est `0x126CA000`, capacité 131 072 dwords. Dans le
  run final il reste à 0 soumission, RPTR 0, WPTR 0.
- Le batch primaire de boot historique de 25 dwords est intégralement
  consommé par l'état courant : deux soumissions de 22 puis 3 dwords, sans
  attente pendante. L'ancien constat `RPTR=7/WPTR=25` ne décrit donc pas le
  processeur de commandes actuel.
- Le debugger du runtime généré observe un label nommé `sub_821B9BC8` avec
  `device+10941=0x16` et `device+21508=0` après l'init finale. Ce reçu est
  limité au binaire généré : il n'est pas encore une preuve PAL.
- Dans Ghidra PAL canonique, `0x821B9BC8`, `0x821BA058` et `0x821B9D24`
  appartiennent à l'unique fonction/PData `0x821B99B8–0x821BA273`, et
  `0x821B9D24` est un `bctrl`, pas le store MMIO prétendu par le généré.
  L'identité du XEX générateur doit être réconciliée avant toute autre
  interprétation de ses labels ou de son contrôle.
- La provenance est maintenant résolue comme divergente : le codegen lié est
  qualifié pour `ace-combat-6-demo` / `Default.xex`, alors que le projet
  Ghidra canonique actif est `ace-combat-6` / `default.xex`. Les deux chaînes
  ne peuvent pas être croisées à adresse égale.
- Pour la démo, le draw normal est bien rasterisé avec couverture complète,
  puis résolu et écrit en mémoire guest, mais ses pixels sont tous noirs. Le
  pixel shader exact du draw est un bootstrap : il n'a ni entrée couleur ni
  fetch de texture et écrit RGBA zéro. Le problème n'est donc ni le copy, ni
  le tiling, ni l'absence de rasterisation.
- Le source Xenia Edge `e4b13738c3c461b2c06241fa3f54b5a669b6a304`
  transmet à `EnableReadPointerWriteBack` uniquement l’adresse fournie. Le
  runtime natif fait désormais de même.
- Les packets PM4 atteints `0x61`, `0x62`, `0x63` et les valeurs dynamiques
  qualifiées de `EVENT_WRITE_SHD` sont acceptés avec tests bornés.
- Build codegen-on, CTest `26/26`, installation et absence de `bin/bin` sont
  validés. Détails et hashes :
  `analysis/demo/ac6-demo-ring-readback-frontier-v1.json`.

## Résultats négatifs importants

- Vulkan valide 2 modules, mais crée 0 pipeline raster, 0 normal draw,
  0 writeback guest et aucun screenshot d’audit.
- Aucun packet Xenos `PRESENT`, readback guest non noir, frontend qualifié,
  mission ou terminal n’est atteint.
- Les 2 899 notifications `VdSwap` ne constituent pas une preuve visuelle.
- Écrire le timebase hôte dans l’inconnu `KTHREAD+0x58` est inutile : retiré.
  Les digests headless/Vulkan finaux restent identiques sans cette écriture.
- Le faux writeback hôte à `adresse_RPTR-0x3c` touchait un champ guest distinct
  et n’est pas conforme au source Edge : retiré.

## Hypothèses ouvertes

- La chaîne démo qualifiée gouverne le runtime ; le projet Ghidra canonique
  reste une chaîne statique distincte et ne peut pas lui être joint par
  adresse égale.
- La condition qui fait quitter le batch bootstrap noir pour atteindre un
  pixel shader non bootstrap reste inconnue. Dans le projet Ghidra démo,
  `0x822E9948` initialise sans condition cinq profils NSXR non-bootstrap ; le
  bootstrap est initialisé par `0x822F0340 -> 0x822F8078`. Aucune jointure
  statique qualifiée ne relie encore ces profils à une émission PM4 ou au draw
  observé.
- Le prévol GDB NSXR n'a pas lancé le programme : son script appelait
  `continue` sans `run`. Le symbole `__imp__sub_822E0B10` est néanmoins résolu
  par GDB ; cette erreur hôte ne porte aucune information guest.
- La fenêtre GDB corrigée reste bloquée avant le guest si elle emploie un store
  temporaire vierge : le montage VFS exige le marqueur `.ac6-demo-store` et
  l'ensemble des neuf fichiers qualifiés. Le chemin d'import natif est le seul
  moyen retenu pour dériver un store isolé sans modifier la référence.
- L'import natif a publié un store démo isolé sous
  `/fastdata/lavaulta/tmp/ac6-nsxr-import.inm9D9/store`. Son inventaire contient
  uniquement le marqueur et les neuf fichiers qualifiés ; il peut servir à la
  fenêtre GDB bornée.
- La fenêtre GDB NSXR a observé au tick 0 les 12 lookups attendus : bootstrap
  `-14`, puis les profils `-13` à `-9`, avec les LR des wrappers démo. Elle
  prouve l'initialisation complète, non une consommation de profil par PM4.
- `0x8232710c` est un thunk sans effet et ne désigne pas un registre de profils.
  L'initialiseur global `0x822E3858` a un seul appelant statique démo connu,
  `0x822DA7F0`, qui devient le pivot de la prochaine analyse de consommateur.
- Le pivot `0x822DA7F0` ne consomme pas un profil pour PM4 : il orchestre des
  constructeurs, puis appelle `0x822E3858`. Son seul appelant direct connu est
  l'orchestrateur global `0x821A3C30`.
- L'atlas des appels directs de `0x821A3C30` contient `0x822DA7F0`, mais aucun
  des producteurs Xenos déjà qualifiés. Cet orchestrateur ne joint donc pas
  directement l'initialisation NSXR au PM4.
- `0x821B1D58` construit un template PM4 dans son second argument et y copie
  le microcode pixel bootstrap à l'offset `0x80`. Cette fonction ne soumet pas
  elle-même ce template ; son appelant direct connu est `0x821B20A0`.
- L'atlas inverse relie `0x821B20A0` à `0x821C64E8`, qui initialise le device
  global et appelle ce constructeur après ses prérequis. Cette jointure reste
  une initialisation : aucune soumission du template n'y est démontrée.
- `0x821B20A0` réserve un bloc `0x2000`, construit le template bootstrap à son
  début, puis publie adresse et longueur dans le device aux offsets `+0x35bc`,
  `+0x35c0` et `+0x35c4`. Les autres templates partagent ce bloc ; aucune
  soumission n'est présente dans ce CFG.
- `0x821B2BC8` consomme `+0x35c4` et `+0x35c0`, les émet dans le buffer de
  commandes courant, puis appelle `0x821B1F28`. Le passage template→buffer est
  établi ; le commit final de ce buffer reste à qualifier.
- `0x821B1F28` et son enfant `0x821AE1E8` ajoutent uniquement des paquets
  d'état au même curseur `device+0x30` (avec extension par `0x821BA780`). Ils
  ne publient pas ce curseur vers le ring.
- Le chemin bootstrap est appelé par `0x821C5DB8` pendant l'initialisation du
  device, avec les paramètres `(0,0,0)`. L'autre appelant direct,
  `0x821BFBA8`, est distinct et constitue le prochain pivot statique runtime.
- `0x821BFBA8` construit un batch puis appelle `0x821BF720`; celui-ci avance
  un index et un offset de canal (`+0x254`, `+0x17c`) et pose le bit
  `+0x258:0x08` au retour au canal zéro. Il ne publie pas directement le ring.
- L’ordre asynchrone du CP dans Edge peut expliquer une partie de la chaîne
  d’attente D3D, mais son impact dans ce runtime reste spéculatif.
- Les packets construits par `VdSwap` existent dans des buffers guest ; la
  condition qui doit les joindre au ring final reste inconnue.

## Décisions

- Utiliser le code source Xenia Edge comme référence statique ; ne pas lancer
  son runtime et ne pas utiliser Wine.
- Ne faire aucune optimisation avant affichage natif du début de mission.
- Ne pas lancer d’A/B par défaut ; seulement pour une ambiguïté causale nommée.
- Ne jamais promouvoir un visuel sans pipeline/readback guest et RGB non nul.
- Conserver les frontières inconnues fail-closed et ne jamais modifier le C++
  généré.
- Aucun CPJ, worker automatique ou sous-agent.

## État Git exact du checkpoint

- Branche : `main`, upstream `origin/main`.
- Base fonctionnelle avant les documents de checkpoint :
  `aa9b05347ad0c26dba16e1ca77bef902a664d47d`, alors synchronisée `+0/-0`.
- Les documents sont publiés par le commit contenant ce fichier ; le retrouver
  par `git log -1 --format=%H -- STATE.md`.
- Modifications préexistantes non incluses dans le checkpoint :
  `recompilation/ace-combat-6-demo/{CMakeLists.txt,src/guest_bridge.cpp,src/guest_bridge/dynamic_object_vtable_trace.hpp,src/guest_bridge/graphics_dispatch.hpp,src/guest_bridge/graphics_interrupt_trace.hpp,src/guest_bridge/import_journal.hpp,src/guest_bridge/lifecycle.hpp,src/guest_bridge/transition_memory_trace.hpp,src/guest_bridge_resources.cpp,tests/test_xam_return_chain.py,tools/map_xam_return_chain.py}` et
  `reports/AC6_DEMO_XAUDIO_CALLBACK_CPU_FRONTIER.md`.
- Artefacts préexistants non suivis :
  `analysis/demo/ac6-demo-{graphics-interrupt-gate-v1,ring-writeback-boundary-v1,title-doorbell-start-v1,title-imports-queue-ab-v1,title-matrix-consumer-ab-v1,title-queue-ab-v1,title-selector-ab-v1,title-task-list-ab-v1,title-xma-kick-v1,vdswap-corridor-v2,vulkan-xenos-frontier-v1,xaudio-cpu-ab-v1}.json`,
  `analysis/demo/ac6-demo-post-resume-ab/sha256/940637146a447e48fc1619471b9910278c962ca0b261017a269c3cc4affca0c8/`,
  `analysis/demo/ac6-demo-xam-return-chain-ab/sha256/{bcf3382c64ec0d415110ffdbc309161df26fe226ce84fc88e03ccc90ab19bee4,c939f016eec34118ffd46df7a4df81e1b916bd0353a26f788705e5ab1dce2e14}/`
  et `recompilation/ace-combat-6-demo/install/`.
## Checkpoint statique — drain de batch

`0x821BF720` travaille sur le sous-objet `device+0x5494` : son bit `+0x258:0x08` est donc `device+0x56ec:0x08`. Son premier consommateur qualifié, `0x821BF7D8`, lit ce bit pour sélectionner la borne de drain mémorisée, sans l'effacer ni publier directement le ring. Les appels suivants `0x821A6530` et `0x82337E38` restent la frontière de publication.
## Checkpoint statique — frontière NtWriteFile

Le drain `0x821BF7D8` atteint `0x821A66E0`, wrapper synchrone de `NtWriteFile`. Il lui passe le handle du sous-objet, un descripteur local et une longueur `0x800`; aucune écriture MMIO n'est visible dans ce wrapper. L'identité et le producteur du handle restent à qualifier avant de conclure à une soumission graphique.

`0x821A6530` ne fait que positionner ce handle par dispatch virtuel. `0x82337E38`
appelle `NtQueryInformationFile` puis deux fois `NtSetInformationFile`; aucun
des deux appels n’est une publication de batch. La frontière est donc bien le
wrapper `NtWriteFile` et la provenance de son handle.

Le wrapper obtient ce handle par `FUN_8232710C`, qui est un stub sans corps
dans le projet démo. Cette provenance est donc une frontière indirecte ; la
suite statique utile est le producteur du buffer de `0x800` octets.

Le buffer est alloué dans la pile de `0x821BF7D8`. Seuls ses 56 premiers
octets sont remplis explicitement par la copie `FUN_82327D90` depuis
`device+0x3584`; aucun accès PPC D-form direct à cet offset n’existe. Son
producteur devra être recherché par les adresses composées, sans l’identifier
comme PM4.

La recherche par constante relie `device+0x3584` à `0x821BFBA8` et
`0x821C57D0`. Le premier le copie dans un batch ring ; le second le transmet
à deux chemins qui construisent des commandes/scaler sans le remplir. Le
producteur reste indécidable dans cette fenêtre statique.
## Checkpoint statique — producteur du handle

`0x821BEFF0` construit les deux canaux à `device+0x5494`; `0x821BEE60` leur alloue de la mémoire physique mais ne renseigne pas leur champ `+0x14`. Les accès D-form voisins ne montrent aucune écriture directe à ce champ. Son producteur reste une route d'alias ou d'initialisation indirecte, sans preuve de device Xenos.
## Checkpoint statique — alias du champ de handle

Les scripts existants ne relient aucun store au champ `device+0x54a8` : ni le scan D-form du sous-objet, ni la recherche des accès indexés avec l'offset complet ne fournissent de producteur. La provenance du handle est indécidable avec ce projet Ghidra et cet outillage; aucune conséquence de publication GPU n'en est tirée.
## Checkpoint statique — chaîne PM4 vers I/O

`0x821BFBA8` est la chaîne qualifiée de publication : il appelle `0x821B2BC8` pour émettre le template PM4, construit le buffer de canal, puis le transmet via `0x821A66E0` à `NtWriteFile`. Les appelants `0x821A1170` et `0x822FA5A0` sont des voies I/O génériques sans ce contexte PM4.
## Checkpoint natif — frontière pilote

Le point natif partagé `GuestBridge::apply_xenos_mmio_write` publie directement le WPTR MMIO `0x7FC80714`, capture le ring circulaire puis en applique le stream. La chaîne démo qualifiée remet à la place un buffer de canal à `NtWriteFile`. Sans décodage de l'enveloppe pilote, ces contrats ne sont pas comparables instruction pour instruction; aucun changement natif n'est autorisé.
# Gate fermé — entrées de contenu de `0x821B96B8`

- Dans le projet Ghidra démo `ace-combat-6-demo` / `Default.xex`, `0x821B96B8` échange atomiquement le qword de `state+0x2e28` contre le sentinelle `-1`, puis retourne ses moitiés basse et haute à ses deux sorties.
- `0x821B9DB0` consomme ces sorties lorsque le qword n’était pas sentinelle : la moitié basse devient la base alignée à la page et la moitié haute borne la longueur alignée.
- Le même CFG réserve 11 mots par plage et écrit sa séquence de commande, dont `0xC0043C00`; il retourne l’adresse du buffer et le nombre de mots au consommateur amont.
# Revalidation — producteur de contenu de `0x821B9DB0`

- La décompilation démo actuelle de `0x821B9AE0` confirme une réservation de `param_2 × 4` octets et le comptage associé dans `state+0x3a3c`.
- Celle de `0x821BA780` confirme que les deux sorties de `0x821B9DB0` sont transmises sans transformation à `0x821BA058`, après réservation de l’en-tête de quatre dwords.
# Revalidation — source des descripteurs indirects du publisher

- `0x821BA058` prend un curseur d’en-tête, une adresse et une longueur ; sur son chemin direct, il forme localement `{ longueur & 0x00ffffff | 0x81000000, adresse }` et appelle `0x821B9BC8` avec un seul descripteur.
- `0x821B9BC8` consomme ces deux dwords comme longueur puis adresse et les écrit dans le ring derrière l’en-tête indirect Type-3 `0xC0013F00`.
- `0x821BA780` fournit à `0x821BA058` l’adresse et la longueur de `0x821B9DB0`; `0x821B2BC8` ne rejoint `0x821BA780` que sur sa branche de capacité, sans identité statique avec son template ultérieur.
## Gate fermé — ABI publique XMA Create

Le SDK qualifie `XMACreateContext` comme un `HRESULT` avec unique sortie `PXMACONTEXT*`; le PAL fournit le slot `entry+64` et le relit. L'adaptateur peut donc écrire seulement ce pointeur opaque et le statut. Taille/cardinalité du contexte, MMIO et décodage restent hors preuve et ne constituent pas une preuve de contenu RT0.
## Gate fermé — cycle XMA sans lien RT0

La trace bornée des trois `XMAReleaseContext` consomme les pointeurs du corridor local, puis atteint une frontière séparée; elle ne montre ni paquet ni écriture RT0/EDRAM. Ce cycle n'est donc pas un producteur de contenu graphique. La reprise commence au quatrième store XMA et à son consommateur PAL.
## Gate fermé — quatrième store XMA

Le CFG PAL de `0x82357240` produit le store du quatrième kick par la même boucle de création : index physique divisé par 64, bit one-hot, aperture XMA et barrière I/O. Le consommateur établi est matériel, pas guest; aucun lien RT0/EDRAM n'est établi.
## Gate fermé — handoff scheduler de `E000004C`

Le PAL crée `E000004C` par `NtCreateEvent` et le signale par `NtSetEvent`. Les traces jointes montrent 351 set→wake→reprise au même tick sur neutral et START. Le scheduler n'est pas la source du gel; aucun réveil synthétique n'est autorisé.
## Gate fermé — livraison START, frontière post-dispatch

START est qualifié dans le snapshot guest puis dans le mapper logique (`0x10 → 0x400 → 0x10`), mais les consommateurs menu ne sont pas atteints dans la fenêtre. Les contrôles tardifs excluent un second bouton manquant. La divergence est post-START dans l'état/dispatch guest.

## Gate fermé — consommateurs mission/niveau natifs

Dans le projet Ghidra démo `ace-combat-6-demo` pour `Default.xex`,
`0x820EA550` et `0x820EA598` sont désormais couverts nativement dans
`guest_mission_consumers.inl`. Build, audit, CTest (26/26), symboles et
absence de divergence des fichiers générés passent. Une sonde bornée à 120
ticks reste en état guest sans milestone et n'atteint aucun des deux wrappers;
la frontière de progression mission reste ouverte.

## Gate fermé — getters mission/niveau et validity gate natifs

Dans le projet démo qualifié, `sub_82095B80`, `sub_820E9290` et
`sub_820E9300` concordent entre Ghidra et le corps généré, puis sont couverts
par le bridge natif avec les wrappers mission/niveau déjà présents. Build,
audit, CTest (26/26) et symboles forts passent. La sonde bornée à 120 ticks
reste `guest/max_ticks` sans milestone ni entrée getter : reachability encore
indécidable, sémantique statique fermée.

## Gate fermé — écrivain de validity mission natif

Le balayage Ghidra démo trouve sept appels directs à \`0x820E9300\`. Le corps
\`0x821714C0\` est maintenant couvert nativement : branche de validité, écriture
du champ sélectionné \`+0x6c8\`, publication globale \`+0x20\`, puis appels aux
helpers de table encore générés. Build, audit, CTest (26/26) et symbole fort
passent; la reachability runtime reste une question distincte.

## Gate fermé — helpers de publication de validity natifs

Les helpers \`sub_8218E088\` et \`sub_8218EA88\` sont qualifiés dans le projet
Ghidra démo et concordent avec les corps générés. Ils sont maintenant natifs
dans \`guest_mission_consumers.inl\`; build, audit, CTest (26/26) et symboles
forts passent. La prochaine frontière statique est l'appelant \`sub_8216DB10\`.

## Gate fermé — appelant validity \`sub_8216DB10\` natif

\`sub_8216DB10\` et son helper de table \`sub_8218DF70\` concordent entre
Ghidra et le PPC généré : prédicat validity, publication globale et écritures
\`-0x24=3\` et \`-0x5c=2\`. Les deux fonctions sont natives; build, audit,
CTest (26/26) et symboles forts passent. La prochaine frontière est
\`sub_8216D760\`.

## Gate fermé — parent validity \`sub_8216D760\` natif

Dans le projet Ghidra démo \`ace-combat-6-demo\`, le corps PPC qualifie \`r3\`
comme objet d'entrée (l'appel \`0x8232710C\` est \`__savegprlr_29\`), publie la
table \`sub_8218DF70\` via \`sub_8218EA88\`, puis appelle les slots virtuels 13
et 14 aux LR \`0x8216D79C\` et \`0x8216D7E8\`. Il sélectionne \`mission+207\` ou
\`223\`, exécute \`sub_8219EE40\`/\`sub_8219EC88\`, et écrit \`+0xc=0\`,
\`+0x44=4\` et \`+0x60930=0\`. Le corps est maintenant natif; le dispatcher
indirect ne qualifie que ces deux LR et slots. Build, audit, CTest (26/26),
invariants statiques et symbole lié passent. La prochaine frontière est la
qualification et le remplacement des deux enfants récursifs.

## Gate fermé — enfants récursifs de \`sub_8216D760\` natifs

Dans le projet Ghidra démo \`ace-combat-6-demo\`, \`sub_8219EAA0\` parcourt une
liste (élément, suivant) depuis \`head\`, appelle le slot virtuel 1 au LR
\`0x8219EAE8\` et renvoie le nœud dont l'identifiant égalise la clé.
\`sub_8219EC88\` marque \`object+0x24=1\`, parcourt \`object+0x1c\`, appelle
le slot 3 au LR \`0x8219ECBC\` et se récurse lorsque le résultat signé vaut 1.
\`sub_8219EE40\` essaie d'abord \`EAA0(key,head)\`, puis le même slot 3 au
LR \`0x8219EE94\` et la récursion avec la clé au LR \`0x8219EEB0\`; il renvoie
le premier élément trouvé ou zéro. Les trois corps sont maintenant natifs et
les trois sites virtuels sont qualifiés par LR/slot exacts. Build, audit,
CTest (26/26), whitespace et symboles forts passent. La reachability runtime
et les cibles concrètes des slots restent des questions distinctes.

## Gate fermé — mutateur partagé des listes mission \`sub_8219ECF8\` natif

Le slicing statique des 249 appels à \`sub_8219EE40\` réduit le prochain point
commun à \`sub_8219ECF8\` : il appelle \`sub_8219EAA0\`, manipule la tête
\`object+0x1c\` et recycle la free-list globale \`0x827745ec\`. Son corps
généré qualifie neuf dispatchs aux slots 1, 5, 14, 12 et 11, avec les LR exacts
\`0x8219ed34\` à \`0x8219ee34\`; ces neuf sites sont maintenant enregistrés
dans le dispatcher et le mutateur est natif. Build, audit, CTest (26/26), diff
et symboles forts passent. Les wrappers producteurs et les cibles concrètes
des slots 1/3 restent la prochaine frontière statique.

## Gate fermé — wrappers producteurs et cibles virtuelles mission natifs

Le projet Ghidra démo et les corps générés qualifient `sub_8219EEE8`,
`sub_8219F080`, `sub_8219F1C0`, `sub_8219F300` et `sub_8219F468` : allocations
des éléments et nœuds, constructeurs, free-list `0x827745e0`, insertion à
`object+0x20`, et retours anticipés sont concordants. Les cinq wrappers sont
maintenant natifs avec conservation `r27..r31`/LR. Les 24 dispatchs de ces
wrappers sont enregistrés par LR et slot exacts; les cibles communes des slots
1 et 3 (`0x82311960` et `0x820d2c60`) sont aussi natives. Build, audit, CTest
(26/26), comparaison de contrats et symboles forts passent. La prochaine
frontière est le slicing statique des appelants de ces producteurs; aucune
sonde runtime n'est nécessaire à ce stade.

## Gate fermé — appelants directs des wrappers producteurs mission natifs

Dans le projet Ghidra démo canonique `ace-combat-6-demo` (`Default.xex`), les
xrefs directs donnent 36 appels répartis sur 21 appelants des cinq wrappers
producteurs. Les contrats des helpers `sub_8219E428` et `sub_8219E768` sont
concordants; six appelants (`sub_821A00E8`, `sub_821A0180`, `sub_821A0240`,
`sub_821A02C0`, `sub_821A0338`, `sub_821A03A8`) sont maintenant natifs avec
modes, registres, offsets de nœud, scratch guest, restauration callee-saved/LR
et retours slot 1 exacts. Les six LR virtuels `0x821A014C`, `0x821A01F0`,
`0x821A02A0`, `0x821A0324`, `0x821A0398` et `0x821A0410` sont qualifiés dans
le dispatcher. Les assertions de contrats, le build, l'audit de complexité,
CTest (26/26) et l'installation passent; aucune reachability runtime n'est
encore déduite. La prochaine frontière est une trace bornée vers Mission 01.
## Gate fermé — runtime borné: alias free-list avant Mission 01

L'import qualifié puis la sonde `SDL_AUDIODRIVER=dummy` sous Xvfb s'arrêtent au
tick 61 sur `LR=0x8219ECBC`, cible nulle. Les écritures hôte bornées montrent
que `0x827745E0` reçoit `0x18BB0300`, puis que `ac6_mission_take_node` écrit
`0x18960180` dans le mot 0 de cet objet; le snapshot de dispatch confirme le
vtable/slot 3 mappé mais nul. Le corps généré de `0x8219F080` concorde avec
l'aide native, donc aucune garde ou modification du dispatcher n'est justifiée.
Mission 01 reste non atteinte. La prochaine frontière est le producteur et le
chaînage qui introduisent l'adresse de tas dans la free-list `0x827745E0`.

## Gate fermé — resolver mission: sélecteur hors bornes

La correction de `ac6_mission_take_node` (retour de l'élément alloué, non de la
tête de free-list) supprime le trap du tick 61 et porte la sonde au tick 67.
Dans la chaîne qualifiée `0x821EDF40` → `0x821EE0F8` → `0x82278160`, le second
descripteur observé possède `count=2`, table `0x18BD1014`, mais reçoit l'index
`0x30`. La recherche retourne donc zéro par la branche statique hors bornes;
`0x82278160` transmet ce zéro à `0x821EDF40`, qui lit `r4+4` et déclenche le
trap à l'adresse `0x4`. Ce n'est ni un slot virtuel nul ni une raison d'ajouter
une garde au dispatcher. Build, CTest et installation codegen-on repassent.
La prochaine frontière est le producteur statique du sélecteur `0x30` et des
données du descripteur `0x18BD1000`.

## Gate fermé — producteur statique de l'index resolver `0x30`

Le slicing qualifié du projet démo canonique relie `sub_8219E7B0` aux champs
`object+0x14` (racine), `object+0x18` (descripteur) et `object+0x20` (index).
`sub_821A03A8` écrit son `r8` dans `object+0x20`; `sub_82191088` fournit ce
`r8` par le retour d'un appel virtuel à l'offset de vtable 160 (`0x82191148`),
avant l'appel à `0x821A03A8` (`0x82191164`). Le tick 67 est expliqué jusqu'à
cette frontière: `0x30` n'est pas un immédiat produit par le wrapper natif.
Le contrat local est fermé, l'origine amont reste indécidable sans qualifier
le service virtuel; aucune garde resolver n'est justifiée. La prochaine
frontière est le producteur concret de cet appel virtuel ou la documentation
explicite de cette limite de service.
## Gate fermé — cible virtuelle de l’index (offset 160)

La chaîne constructeur qualifiée `sub_821908B8` → `sub_8218EA98` appelle
`sub_8223D4D0` sur `objet+0x222D60`; ce constructeur écrit le vptr
`0x820210D0`. La vtable Ghidra qualifie l’offset 156 vers `0x8223D4C0` et
l’offset 160 vers `0x8223D4C8`. Le premier stocke `r4` à `receiver+3164`, le
second le relit; `sub_82191088` écrit donc `48` puis récupère exactement
`0x30`, avant que `sub_821A03A8` ne le place dans `resolver+0x20`. La frontière
virtuelle est locale et fermée; aucune garde resolver n’est ajoutée.

La prochaine frontière est le contrat statique du resolver après l’index
`0x30`: déterminer si le descripteur `count=2` attend réellement cet index ou
si le producteur/layout est incohérent.
## Gate fermé — contrat du resolver après l’index `0x30`

`sub_821EE0F8` est un lookup borné : il lit `count` à `+0`, compare
l’index signé avant tout accès, lit la table à `+12`, puis renvoie
`base(+4)+entry` seulement pour une entrée non nulle. `sub_821EDF40`
construit exactement ce descripteur; `sub_82278160` abandonne si sa
construction retourne zéro. Avec `count=2` et `index=0x30`, le retour nul
est donc le comportement PPC attendu, pas une garde manquante.

Le gate ferme la question du resolver et réfute l’hypothèse d’un bug de borne.
L’incohérence restante est le contrat amont du champ `resolver+0x20`
(valeur `0x30` consommée comme index de deux éléments). La prochaine
frontière est de réconcilier ce champ avec ses producteurs et le layout de
l’objet réellement exécuté.

## Gate fermé — setter object+8 et cible virtuelle slot 16

Le contrat statique de `sub_820D2C68` est un store direct de `r4` vers
`object+8`; les constructeurs concernés l'initialisent à zéro et
`sub_8219E580`/`sub_8219E7B0` le consomment sans garde. Le probe borné confirme
que le slot 16 (`lr=0x8219F594`) reçoit `0x18BC0100` pour l'objet
`0x189600C0`, puis que le consommateur `0x8219E7B0` relit cette même valeur au
tick 67. Le trap restant vient d'un autre objet (`0x18960100`) consommé par
`0x8219E580`, dont `object+8` vaut zéro. Le setter et l'aliasing sont donc
réfutés comme cause; aucune garde null n'est ajoutée. La validation finale
build/CTest/installation/layout passe.

La prochaine frontière est le producteur statique et le constructeur de l'objet
`0x18960100`/vtable `0x82012DC4`, afin de qualifier pourquoi son champ `+8`
reste nul avant `sub_8219E580`.

## Gate fermé — producteur E550 et argument du setter F42C

Le RTTI qualifie `0x82012DC4` comme le vtable E550 : le slot 16 (`+0x10`)
vise `sub_820D2C68` et le slot 20 (`+0x14`) vise `sub_8219E580`. Le setter
écrit directement `r4` vers `object+8`; F300 lui transmet `memory.load_u32(out)`
au retour `0x8219F42C`. La trace bornée
`artifacts/object8-null-producer-gate/F42C-zero-context.txt` montre trois
objets E550 construits au tick 61 puis des stores F42C de zéro à `+8`.
L'hypothèse « aucun setter n'est attendu » est réfutée; aucun garde ni patch
du constructeur n'est justifié. La prochaine frontière est le producteur de
`out` (`sub_8219EE40` ou le fallback allocator) avant F42C.

## Gate fermé — cadre ABI de F300 et producteur de `out`

Le slicing PPC qualifie `sub_8219F300` comme une fonction à cadre de 128
octets; `sub_821E1CD8` réserve elle-même `-128(r1)`. Le remplacement natif ne
réservait pas ce cadre, et la capture ciblée montre `out` non nul après les
slots 8/28/36 et ECF8 puis nul exactement après E1CD8 taille `0x20`. F300
réserve/restaure maintenant le cadre PPC attendu. La relecture bornée donne
trois valeurs `out` non nulles à E1CD8/E550/F42C et aucun trap nul.

Le probe atteint 100 ticks mais reste `max_ticks` avec le scheduler épuisé
avant le jalon mission; aucune preuve de gameplay visible n'est encore acquise.
La validation canonique build/CTest (27 tests)/installation/layout est verte.
La prochaine frontière est la progression du scheduler vers le jalon mission,
sans modifier davantage F300.

## Gate fermé — attente scheduler après F300

La fenêtre bornée à 300 ticks ne produit aucun trap et conserve le contrat
`out`/F42C corrigé, mais le rapport reste `max_ticks` avant mission. Les
compteurs compacts donnent `runnable=0`, `blocked=23`, `finished=0` et
`slice_exhaustions=201`; la frontière est explicitement « all started guest
threads blocked before mission milestone ». Le prochain test doit qualifier
le wait/événement qui maintient ces threads bloqués, sans élargir la trace.

## Gate fermé — frontière statique des réveils d'événements mission

Le slicing des attentes et publications est fermé dans
`artifacts/mission-wakeup-gate/event-gate-synthesis.txt`. Le bridge associe les
attentes aux clés `(wait_kind, wait_key)` et ne possède que les voies de réveil
bornées pour les événements, sémaphores, fins de thread, objets noyau et timers.
La source `src/guest_bridge/lifecycle.hpp` documente explicitement que le
client render-driver est enregistré mais jamais appelé dans le chemin par
défaut; le thread mixer reste donc sur huit événements auto-reset sans
producteur local, le signal étant fourni par le pilote audio console.

Le gate est fermé statiquement: aucun runtime supplémentaire n'est requis pour
établir cette frontière. Le prochain gate doit qualifier la frontière audio/XMA
et le callback render-driver, sans injecter de signal synthétique dans le
chemin de parité.

## Gate fermé — frontière audio/XMA du réveil mixer

Le slicing statique de `artifacts/audio-xma-boundary-gate/static-synthesis.txt`
qualifie l'enregistrement du client render-driver, le callback
`sub_8236DD98` → `sub_82355E58` et le global `0x829DA528`. Les imports XAudio
du bridge ne font qu'enregistrer, compter une soumission-sink et désenregistrer;
les imports XMA bornent la création/libération de contextes. Aucun de ces
chemins ne publie les huit événements auto-reset.

Le callback reste désactivé par défaut tant que
`AC6_DEMO_EXPERIMENTAL_XAUDIO_DRIVE` n'est pas demandé et que le global audio
n'est pas non nul. La frontière est donc fermée statiquement sur le producteur
console audio/XMA absent du chemin natif par défaut; aucun runtime ni signal
synthétique n'est requis. Toute suite doit rester derrière une frontière
audio/XMA explicitement nommée.

## Gate fermé — contrat externe XAudio/XMA

La recherche officielle Microsoft Learn est regroupée dans
`artifacts/audio-xma-external-contract-gate/official-source-check.txt`.
Les pages publiques établissent seulement le lien statique XAudio2 Xbox 360 et
son thread audio périodique; elles ne décrivent pas les exports privés
`XAudioRegisterRenderDriverClient`/`XAudioSubmitRenderDriverFrame` ni le contrat
XMA kernel. Le SDK local Rexglue corrobore un descripteur `(callback,
callback_arg)` et un worker qui appelle le callback quand la file est basse,
mais reste une source secondaire d'émulation.

La décision compacte est dans
`artifacts/audio-xma-external-contract-gate/static-boundary-decision.txt` et
`gate.status`: aucun producteur local des huit événements du mixer n'est
qualifié; la frontière restante est le pilote audio/XMA console. Aucun
runtime, signal synthétique ou activation de
`AC6_DEMO_EXPERIMENTAL_XAUDIO_DRIVE` n'est justifié. La suite doit fournir un
oracle externe borné ou un contrat XDK/xboxkrnl qualifié.

## Gate fermé — producteur guest de l'état XAudio

Le slicing qualifie la chaîne `0x8234F600 → 0x8234F058 → 0x82356410 →
0x82355F70 → 0x82356070`. Le constructeur `0x82355F70` installe les tables
audio, initialise l'objet et publie son adresse dans `0x829DA528`. Les atlas
neutral/start bornés exécutent chacun ces fonctions une fois au tick 106; une
arête indirecte vers `0x8234F058` est observée sur le thread 1. Le producteur
guest n'est donc pas manquant ni inatteignable.

Le callback `0x8236DD98` et le dispatcher `0x82355E58` ne sont pas observés
jusqu'au tick 253. Le gate ferme seulement la construction/publication du
global; la frontière suivante reste l'activation du producteur audio/XMA et
la publication des événements du mixer. Preuves compactes :
`artifacts/audio-xma-client-state-producer-gate/static-chain-decision.txt`,
`runtime-reachability-summary.txt` et `gate.status`.

## Gate fermé — activation bornée du callback XAudio

Une sonde native isolée, explicitement expérimentale et limitée à 140 ticks,
lit `0x829DA528 = 0x1005CEBC` aux ticks 62–67 et entre effectivement dans
`0x8236DD98` (arête `lr=0`, 212 appels). Elle s'arrête au tick 67 sur une
lecture guest non mappée à l'adresse `0x4`, avant qu'un producteur des huit
événements du mixer soit qualifié. La première tentative avec `--atlas` a été
refusée par le CLI avant lancement; elle n'est pas une observation guest.

Le gate d'activation est donc fermé : le callback n'est pas absent, il atteint
un enfant qui reste à qualifier. La frontière suivante est l'accès
`0x82355E58`/`0x4` et la publication éventuelle vers les handles mixer; le
chemin par défaut reste inchangé. Preuves compactes :
`artifacts/audio-xma-callback-activation-gate/runtime-summary.txt` et
`gate.status`.

## Gate fermé — cause du trap dans l'enfant du callback XAudio

Le slicing qualifie `0x8236DD98` comme un simple chargement de
`0x829DA528` dans `r3` suivi d'un tail-call vers `0x82355E58`; `r4` reste donc
hérité du bridge. Le dispatcher lit l'objet `r3+0x40`, son vtable, puis
appelle le slot `vtable+0x44`, observé vers `0x82278160`. Cette fonction passe
`r3+0x1c` à `0x821EDF40` sans remplacer `r4`. Or la première instruction de
`0x821EDF40` lit `r4+4`. Le trap runtime `address=0x4`, `r4=0`,
`lr=0x82278184`, tick 67, correspond exactement à cette lecture.

`0x82355E58` contient aussi un `KeSetEvent` conditionnel (callsite
`0x82355EA8`, garde `object+0x130`), mais aucune publication runtime de la
capture bornée ne lui est attribuée; aucun lien avec les huit handles mixer
n'est qualifié. Le gate ferme donc la cause immédiate sans modifier le
bridge. La frontière suivante est le producteur/layout du descripteur `r4`
et la correspondance des événements. Preuve :
`artifacts/audio-xma-callback-child-gate/static-child-decision.txt` et
`gate.status`.

## Gate fermé — séparation du descripteur d'enregistrement et du `r4` enfant

Le wrapper `0x8234D0E8` est qualifié : son `r4` entrant est `0x8236DD98`,
qu'il place en `descriptor[0]`; `descriptor[1]` vient de `object+0x0c`, et
le second argument ABI est le pointeur de handle. Ce descripteur
d'enregistrement n'est donc pas le `r4` du callback enfant. `0x8236DD98` ne
fait qu'alimenter `r3` avec `0x829DA528` puis tail-caller `0x82355E58`; aucun
producteur guest qualifié ne remplit le `r4` ultérieur. La capture bornée
confirme `r4=0`, trap `0x4`, `lr=0x82278184`.

Le gate ferme la recherche statique du producteur guest et conserve la
frontière XDK/XMA du contexte enfant ainsi que l'attribution de `0x82355EA8`
aux handles mixer. Aucune modification du bridge ni signal synthétique
n'est justifié. Preuve :
`artifacts/audio-xma-r4-descriptor-gate/static-r4-producer-decision.txt` et
`gate.status`.

## Gate fermé — objet d'événement interne XAudio

Le généré PPC qualifié établit que `0x82356070` construit les objets statiques
`0x829DA518` et `0x829DA508`, et initialise le sémaphore `0x829DA4F4` avec
une limite de 6. `0x82355818` publie `KeSetEvent(0x829DA518)` après le test du
compteur de clients; `0x82355CE0` attend cet objet, libère le sémaphore puis
signale `0x829DA508`. `0x82355E58` réutilise `0x829DA518` avant son attente
multiple. Les références Ghidra qualifiées de `0x829DA518` sont limitées à
ces fonctions et au constructeur, sans alias vers l'enregistrement ou les
huit handles mixer. `0x829DA4E4` est séparé et lié à la notification de
terminaison. Le pont natif sait déjà attendre et signaler un objet événement
guest via `object+4`; aucun patch n'est justifié.

Le gate ferme donc l'attribution de cet événement à un worker XAudio interne.
La frontière active reste le contexte enfant privé XDK/XMA passé en `r4`; le
chemin expérimental demeure désactivé par défaut. Preuves :
`artifacts/audio-xma-event-object-gate/static-event-object-decision.txt`,
`gate.status` et `static-ref-owner-map.txt`.

## Gate fermé — contexte enfant `r4` XDK/XMA non qualifié

Le code Xenia local qualifie seulement son oracle : `RegisterClient` stocke
`descriptor[0]`/`descriptor[1]`, puis le worker invoque le callback avec un
unique argument guest. Il ne fournit donc pas de second `r4`. Dans AC6,
`0x8236DD98` ne remplit pas `r4`, `0x82355E58` le transmet implicitement,
et `0x821EDF40` lit `r4+4`; la capture observe `r4=0` et le trap attendu à
`0x4`. Le contexte de registration `object+0x0c` est nul, et le couple CPU4
`+0x2c/+0x30` n'est pas relié statiquement à cet enfant.

Le gate ferme donc la recherche guest et laisse explicitement ouverte la
frontière privée XDK/XMA. Aucun patch du bridge, binding heuristique ou signal
synthétique n'est justifié. Preuves :
`artifacts/next-xdk-xma-r4-context-gate/static-external-context-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — table et writers du slot XMA

Les fenêtres PPC qualifient la table construite par `0x82356528` : compteur
à `table+0x00`, flags à `table+0x04`, base des entrées à `table+0x08`, stride
`0x60`. Chaque entrée réserve son slot de contexte à `entry+0x40` et son index
de lane à `entry+0x50`. `0x82357240` appelle `XMACreateContext` uniquement si
ce slot est nul, puis calcule l'index physique et atteint le premier store
MMIO inconnu à `0x823572D8`.

L'allocation appelle `0x821A4B70` au callsite `0x82356610`; son `dcbzl` efface
l'allocation avant les écritures de table. La libération `0x823567E0` appelle
`XMAReleaseContext` puis efface `entry+0x40`. Le gate ferme donc la propriété
guest du slot et ses writers connus, mais pas l'ABI privé XMA ni l'effet MMIO.
Preuves : `artifacts/xma-output-slot-frontier-gate/static-slot-table-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — réutilisation de la free-list XMA

Le header SDK local `sdk/xdk-xenon-6132.6/XDK/include/xbox/xmadecoder.h`
qualifie `XMACreateContext(PXMACONTEXT*)` comme une création par out-pointer
HRESULT et `XMAReleaseContext(PXMACONTEXT)` comme une fonction `VOID` qui remet
le contexte sur une free-list. Le type `IXMAContext` reste opaque : aucune
taille ou layout privé n'est déduit de cette source.

Le bridge conservait pourtant seulement un index haut-water monotone; un slot
libéré ne pouvait donc jamais être attribué de nouveau. `allocate_xma_context`
réutilise maintenant le premier slot inactif du préfixe déjà alloué, puis
n'étend ce préfixe seulement lorsqu'il est plein. Les validations, le zeroing
et la borne 320 restent inchangés. Le test isolé confirme la réutilisation après
trois releases, et la suite complète passe 27/27. Ce gate ne modifie ni l'ABI
XMA, ni le layout opaque, ni le producteur audio.

Preuves : `artifacts/xma-context-free-list-gate/static-sdk-decision.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — couche privée XMA identifiée, effet MMIO toujours ouvert

Les deux PDB locaux du noyau de debug XDK nomment `_XMA_CONTEXT_DATA`,
`_XMA_REGISTERS`, `CXMADecoder::CreateContext/ReleaseContext/Enable`,
`m_pHWContexts`, `m_pRegisters`, `m_contextsInUse` et la chaîne
`No free client contexts`. Cela confirme une couche noyau privée pour les
contextes matériels, les registres et la free-list, mais ne fournit aucun
offset de champ, aucune adresse de registre ni sémantique du store PPC
`0x823572D8`. Le bridge conserve donc ses mappings MMIO expérimentaux sans
leur attribuer d'effet device-facing; aucun patch ni signal audio synthétique
n'est justifié.

Preuves : `artifacts/xma-private-mmio-gate/rank1-pdb-summary.txt`,
`private-layer-decision.txt`, `gate.status` et `validation.txt`.

## Gate fermé — registres XMA indexés et rôles Enable/Disable qualifiés

Le désassemblage PPC borné du noyau de debug officiellement apparié, puis le
scan statique de toute sa section `.text`, qualifie la formule privée : le
contexte est indexé par blocs de 64 octets, `group = index >> 5`, et la valeur
écrite est le bit `1 << (index & 31)`. Les adresses indexées sont
`0x7FEA1A40 + 4*group`, `0x7FEA1A80 + 4*group`, `0x7FEA1940 + 4*group` et la
lecture `0x7FEA1840 + 4*group`.

`CXMADecoder::Enable` écrit `1A80` puis `1940`; `CXMADecoder::ReleaseContext`
écrit `1A40` puis `1A80`; `XMADisableContext` lit `1840`, teste le bit puis
écrit `1A40`. Le scan complet ne trouve que ces cinq stores indexés. Le
cross-match Xenia confirme la même arithmétique et les rôles Enable/Disable,
mais reste secondaire; la contribution officielle de `XMAInitializeContext`
ne contient pas le store indexé `1A80`.

Le gate ferme donc l'arithmétique et les rôles indexés, mais laisse ouverts le
layout privé `_XMA_REGISTERS`, le sens des registres directs, l'attribution de
`1A80` à l'initialisation publique et l'effet device-facing du store guest
`0x823572D8`. Aucun mapping expérimental ni signal audio n'est activé.

Preuves : `artifacts/xma-private-mmio-gate/xma-kernel-register-addresses.txt`,
`xma-kernel-methods-full.txt`, `variant-pairing.txt` et `gate.status`.

## Gate fermé — layout XMA privé statique épuisé

La recherche de rang 1 est maintenant close : le SDK officiel garde
`IXMAContext` opaque, `xmahardwareabstraction.h` n'expose aucun registre, et
le TPI du PDB officiel ne fournit aucun enregistrement de type exploitable.
Le noyau officiel qualifie les familles indexées et leurs rôles opérationnels,
mais pas les offsets privés ni les registres directs. Xenia/ReXGlue ajoutent une
table générique (`1A40`/`1A80`/`1940`) et un contexte de 64 octets, conservée
comme corroboration de rang inférieur uniquement; aucun nom générique n'est
promu au PAL AC6.

Le store `0x823572D8` reste donc une frontière calculée/observée dont l'effet
device-facing est ouvert. Aucun mapping fonctionnel, producteur PCM ou patch
du bridge n'est justifié. La prochaine gate est une capture bornée des octets
du contexte et des consommateurs immédiats, avec critères H1/H2 pré-enregistrés.

Preuves : `artifacts/xma-register-layout-gate/static-decision.txt`,
`third-party-decoder-extract.txt`, `third-party-register-table.txt`,
`hypothesis-criteria.txt`, `gate.status` et `validation.txt`.

## Gate fermé — état guest après les trois premiers stores `0x1A80`

Un observateur lecture seule, activé uniquement par
`AC6_DEMO_WATCH_XMA_CONTEXT`, a capturé la fenêtre bornée `buttons16` autour
de `0x823572D8`. Trois stores one-hot acceptés à tick 916 (indices 0, 1, 2)
pointent respectivement vers `0x2E7FF000`, `0x2E7FF040` et `0x2E7FF080`; les
16 mots big-endian de chaque contexte sont tous nuls. Aucun appel enfant ou
consommateur immédiat n'apparaît dans la sortie bornée, ce qui concorde avec
la décompilation de `0x82357240` qui retourne après la barrière I/O.

H1 (transition de contenu du contexte) est donc réfutée pour ce chemin observé.
H2 est étayée comme contrôle one-hot sans producteur PCM observé, mais reste
indécidable quant à l'effet device-facing: la fenêtre n'a pas atteint un
consommateur matériel qualifié. Aucun mapping MMIO fonctionnel ni signal audio
n'est ajouté.

Preuves : `artifacts/xma-runtime-state-gate/capture-criteria.md`,
`capture-analysis.txt`, `context-watch-capture/buttons16.stderr.log`,
`ghidra-xma-functions-noanalysis.log` et `validation.txt`.

## Gate fermé — aucun consommateur direct `1A80` dans le PAL qualifié

Le scan Ghidra read-only du projet `ace-combat-6-demo` ne trouve aucune
matérialisation fixe de `0x7FEA1A80`, `0x7FEA1940` ou `0x7FEA1A40`; seul
`0x7FEA1840` est matérialisé directement dans `0x82357050`. Les 33 références
de l'aperture `0x1800..0x1AA4` se concentrent dans `0x82356510`, `0x82357050`,
`0x82357390` et `0x82357458`. `0x82356528` construit les entrées de 0x60
octets puis appelle `0x82357240`; cette dernière retourne après le store
one-hot et `eieio`, sans enfant post-store.

Le premier consommateur device-facing de `1A80` n'est donc pas fermé par le
PAL statique. La prochaine frontière est la chaîne de cycle de vie ultérieure
(`0x82357310`/`0x823575A8`) ou un consommateur noyau officiellement qualifié;
aucun mapping MMIO n'est activé.

Preuves : `artifacts/xma-runtime-state-gate/post-store-static-decision.txt`,
`post-store-consumer-ghidra.log`, `post-store-consumer-raw-materialization.log`,
`post-store-callers-ghidra.log`, `xma-create-caller-decomp.log`, `gate.status`
et `validation.txt`.

## Gate fermé — chaîne XMA ultérieure et état guest distinct

La décompilation PAL ferme la chaîne de cycle de vie sans lui attribuer une
sémantique MMIO. `0x82357458` compare l’index de chaque entrée au registre
direct assemblé `0x7FEA1818..0x7FEA181B`, puis recopie et neutralise des lignes
de cache avant de poser le flag `0x20000`. `0x823575A8` flushe les blocs,
recopie le payload de l’entrée vers `entry+0x40`, émet ensuite le one-hot à
`0x7FEA1940` et efface les flags `0x10000/0x20000`. `0x82357390` ajoute le
store direct `0x7FEA1804=0x03000000` dans le chemin de récupération.

Cet état guest est distinct du store immédiat `0x7FEA1A80`, mais le PAL ne
nomme ni le registre direct, ni un format PCM, ni un effet audible. Aucun
mapping matériel n’est activé. La frontière suivante est le consommateur
noyau officiellement qualifié ou une observation locale du nouvel état
matériel; le trap connu du cinquième bit ne doit pas être répété.

Preuves : `artifacts/xma-runtime-state-gate/later-chain-static-decision.txt`,
`later-chain-extract.txt`, `post-store-callers-ghidra.log` et `gate.status`.

## Gate fermé — consommateur noyau ISR XMA

Le PDB XDK officiel nomme `CXMADecoder::InterruptServiceRoutine` à `0007:0xACE68`.
Dans le binaire apparié, l'ISR lit `0x7FEA1808` avec `lwbrx`, teste trois bits,
puis écrit les commandes `0x100`, `0x200` ou `0x400` vers `0x7FEA1A08` avec
`eieio` avant l'épilogue commun. Cela qualifie le consommateur noyau de l'état
direct, sans donner de nom aux champs ni d'effet PCM/audible. Les headers
publics et le TPI PDB restent opaques; aucun mapping matériel n'est activé.

Preuves : `artifacts/xma-kernel-consumer-gate/decision.txt`,
`isr-static-scan-corrected.txt`, `isr-prologue-body.txt` et `gate.status`.

La recherche suivante n'a trouvé aucun layout privé qualifié dans les headers
XDK, le TPI PDB (zéro record de type) ou les sources Xenia. Le prochain test,
s'il devient nécessaire, est borné aux accès `0x1808`/`0x1A08` autour des trois
premiers stores `1A80`; ses critères H1/H2 sont dans
`artifacts/xma-kernel-consumer-gate/next-observation.md`.

## Gate fermé — ISR XMA non instrumentable dans la route native

Les deux sorties générées ne contiennent que les imports XMA de création et de
libération de contexte; aucune routine générée ni aucun littéral direct
`0x7FEA1808`/`0x7FEA1A08` n'est présent. Le bridge ne mappe pas ces apertures,
et une lecture non mappée est piégée avant l'observateur tardif. La relation ISR
reste donc une preuve noyau statique, non une observation native disponible.
H1/H2 sont indécidables dynamiquement à cette frontière; aucun mapping ou
émulation MMIO n'est ajouté.

Preuves : `artifacts/xma-kernel-consumer-gate/native-isr-decision.md`,
`native-isr-instrumentability.txt`, `native-isr-validation.txt` et
`gate.status`.

## Gate fermé — ABI guest des imports XMA create/release

Le header XDK officiel rend `PXMACONTEXT` opaque et déclare
`XMACreateContext(PXMACONTEXT*)` avec un retour `HRESULT`; le PPC à
`0x82357298` passe le slot en `r3`, teste le statut puis relit le contexte
avant `MmGetPhysicalAddress`. Les seuls imports XMA effectivement appelés
sont `XMACreateContext` et `XMAReleaseContext`; le bridge existant respecte ces
formes sans attribuer de sémantique PCM ou privée.

Preuves : `artifacts/xma-import-abi-gate/decision.md`, `static-extract.txt`,
`validation.txt` et `gate.status`.

## Gate historique supersédé — ancienne lecture du slot XMA

Le bloc suivant conserve l'observation historique, mais son interprétation est
désormais invalidée par le producteur statique qualifié ci-dessous.

La réconciliation statique ne trouve aucune canonicalisation entre le calcul
PAL de `sub_82357240` et `PPC_MM_STORE_U32` : l’adresse brute atteint
`GuestMemory`, dont les MMIO sont exacts. `MmGetPhysicalAddress` renvoie
l’adresse virtuelle inchangée, tandis que l’allocateur natif commence à
`0x10000000` et ses adresses sont alignées sur 0x1000. Avec l’identité
physique actuelle, les 12 bits bas du contexte sont donc nuls; le décalage
depuis `0x829DA52C` ne peut pas produire l’index requis pour le store exact
`0x7FEA1A80`. Si le tableau est la première allocation, l’exemple est
`0x896B` → `0x7FEA2BAC`; le bridge ne mappe que `0x7FEA1A80`.

Le mismatch est confirmé; aucune sémantique de registre ni correction de
traduction virtuelle→physique n’est ajoutée. La prochaine frontière doit
qualifier le producteur de l’adresse physique attendue avant toute modification
de `MmGetPhysicalAddress` ou du mapping.

Cette conclusion traitait à tort `0x829DA52C` comme une adresse fixe. Elle est
supersédée par la qualification de son producteur ci-dessous et ne doit plus
guider une modification de `MmGetPhysicalAddress` ou du mapping.

## Gate fermé — producteur statique de la base XMA

`sub_82356510` lit `0x7FEA1800` avec `lwbrx` et écrit la valeur dans le slot
global `0x829DA52C`. `sub_82357240` recharge ce slot après
`MmGetPhysicalAddress`, le soustrait au pointeur retourné, puis dérive l'index
du `stwbrx` XMA. Le bridge publie précisément ce même tableau à la lecture
`0x7FEA1800` et conserve l'identité de l'adresse physique allouée.

La capture bornée existante confirme le flux `0x2E800000` → global →
`MmGetPhysicalAddress` → tentative `0x7FEA1A80`; l'ancien « mismatch » est donc
réfuté. Aucun code natif n'est modifié.

Preuves : `artifacts/xma-physical-producer-gate/producer-summary.md`,
`validation.txt`, `checkpoint.txt` et `gate.status`.

## Gate fermé — consommateur privé tardif XMA (structure guest)

La chaîne PAL après le store initial est maintenant qualifiée statiquement au
niveau du contrôle guest : entrées de 96 octets, index u16 à `entry+80`,
groupes `index>>5`, one-hot vers `0x7FEA1A40` dans `0x82357310`, copie/cache
vers `entry+0x40` puis one-hot vers `0x7FEA1940` dans `0x823575A8`, et accès
directs `0x7FEA1804`/`0x7FEA1818` dans les routines voisines. Les barrières
`eieio`, les bits de gestion `0x10000`/`0x20000` et les constantes d'adresse
sont vérifiés localement.

Cette gate ferme le consommateur privé et les mutations d'état guest, pas la
sémantique hardware/PCM. Les registres directs et le layout privé restent
opaques; aucun mapping MMIO ni patch natif n'est ajouté. La prochaine preuve
doit obtenir un consommateur device-facing qualifié ou ouvrir une frontière PAL
indépendante avec un discriminant nouveau; le trap connu ne doit pas être
répété.

Preuves : `artifacts/xma-private-consumer-gate/decision.md`,
`formula-validation.txt`, `source-anchors.txt`, `validation.txt` et
`gate.status`.

## Gate fermé — formats de fetch Xenos atteints

Le snapshot statique du draw atteint qualifie `format=57` pour la position
(trois composantes, full fetch vers `r1`) et `format=38` pour la couleur
(quatre composantes, mini fetch vers `r0`). Les tables Xenos locales concordent
avec ces deux correspondances. Les exports `position=max(r1,r1)` et
`interpolator0=max(r0,r0)` sont joints, mais les mots de microcode et les octets
du vertex buffer ne sont pas publiés; la sortie PS reste non jointe.

Cette gate ferme uniquement la cartographie statique des formats. Le traducteur
natif reste fail-closed et aucun pixel n'est synthétisé. La prochaine preuve
doit joindre le producteur du vertex buffer et qualifier séparément la sortie
du pixel shader.

Preuves : `artifacts/shader-static-frontier-gate/decision.md`,
`source-anchors.txt`, `validation.txt` et `gate.status`.

## Gate fermé — jointure producteur du vertex buffer / fetch atteint

La plage guest chargée par le chemin natif est `[0x127CA03C, 0x127CA0A8)`.
Son préfixe de 84 octets contient les 21 dwords du draw normal et se joint aux
fetches atteints `format=57` (position, stride 7) et `format=38` (couleur,
offset 3). Le calcul ReXGlue/Xenon envoie `0x127CA03C` au binding 2, offset
local `0x027CA03C`, exactement l'offset d'écriture de `VulkanSharedMemory`.

Cette gate ferme le producteur et l'adressage du vertex buffer, sans runtime,
sans patch et sans conclure sur la sortie PS ou l'effet EDRAM.

Preuves : `artifacts/vertex-buffer-producer-gate/decision.md`,
`arithmetic-validation.txt`, `source-anchors.txt`, `validation.txt`,
`gate.status` et `SESSION_ROTATE`.

## Gate fermé — sortie pixel exacte et chaîne writer PM4

Le pixel shader atteint `[0x82013E80,0x82013EA4)` contient 9 dwords :
`alloc colors`, `exece`, puis `max oC0,r0,r0`. La modification pixel par défaut
ReXGlue ne demande ni interpolateur ni paramètre généré; `r0` et la cible
couleur 0 sont initialisés à `[0,0,0,0]`, et `StoreResult` mappe `oC0` sur cette
cible. La traduction SPIR-V et la validation locale passent. La couverture
native jointe remplit tous les échantillons du draw et résout 230400 pixels
RGBA zéro, ce qui exclut un draw absent.

Le paquet RT0 est écrit par `0x821B55C0` au PC `0x821B5840`, puis le paquet
`RB_COPY` par `0x821B6FD0` au PC `0x821B7C04`. Ces points qualifient la chaîne
guest writer → PM4 et son ordre, pas les échantillons matériels EDRAM. Le
contenu guest-owned avant le copy et le premier frame non noir restent
indéterminés; aucun patch natif n'est ajouté.

Preuves : `artifacts/pixel-output-edram-gate/decision.md`,
`arithmetic-validation.txt`, `observations.md`, `validation.txt` et
`gate.status`.

## Gate fermé — frontière pixel non-bootstrap atteinte

Les inventaires PM4 neutre/START sont exhaustifs pour les routes déjà
qualifiées : `neutral-first` charge seulement le pixel de 9 dwords avant ses
24 points; le main IB et START chargent `27V, 9P, 15V` avant leurs deux
rectangles. Le résumé renderer conserve 5 loads, 26 draws et 1 present, et
les deux rectangles partagent ce même pixel bootstrap.

Le corpus NSXR contient cinq pixels de 60 octets candidats et deux pixels de
36 octets; seuls `0x8264B610`/`0x8264BA10` sont joints au pixel atteint. Aucun
writer statique supplémentaire ne distingue les samples EDRAM physiques.
La gate ferme donc la frontière statique, sans patch ni pixel synthétique.

Preuves : `artifacts/next-nonbootstrap-gate/decision.md`,
`route-shader-census.txt`, `pm4-nsxr-focused.txt`, `source-anchors.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — fenêtre post-START indécidable

Le runner interactif historique a été refusé avant exécution par incompatibilité
de CLI. Un unique `probe` codegen-on a ensuite consommé 3 036 ticks avec START
au tick 3 000 puis relâchement au tick 3 001. Le rapport atteint 2 928
présentations mais laisse `frontend`, `mission` et `terminal` faux : tous les
threads guest démarrés restent bloqués avant le jalon frontend. La trace confirme
l'entrée bornée, mais ne publie aucun événement shader/draw/pixel/EDRAM; le
résumé renderer n'expose que 2 loads et 24 draws sans payload décodable.

Cette gate ferme le choix d'outil et classe la question pixel non-bootstrap
indécidable, sans patch natif et sans recours à Xenia Wine ou Edge. La prochaine
gate doit qualifier statiquement l'attente guest/kernel au point de blocage et
ne préparer une nouvelle fenêtre qu'après preuve qu'elle franchira frontend.

Preuves : `artifacts/post-start-runtime-window/decision.md`,
`validation.txt`, `direct-probe-20260821-a/report-detail.txt`,
`direct-probe-20260821-a/trace-events-summary.txt` et `gate.status`.

## Gate fermé — identité du blocage guest qualifiée

Le rapport borné décrit 23 threads bloqués; le thread primaire ré-entre
`0x822F8848` depuis `0x822E559C`, puis attend `0xE000004C` avec le LR
`0x821A69CC`. La jointure statique PAL identifie `0x821A69C8` comme l'appel
direct à `NtSignalAndWaitForSingleObjectEx` ordinal 251. La cible dynamique est
le slot 4 de `0x8202A488` pour `0x82934280`, taille `0xA8`, rôle encore inconnu.

Le scheduler natif et les wrappers d'événements sont cohérents avec cette paire
signal/wait; la cause du non-réveil frontend et son producteur d'événement restent
ouverts. Aucun patch ni élargissement runtime n'est autorisé.

Preuves : `artifacts/static-guest-block-gate/decision.md`,
`current-waits-compact.txt`, `event-writer-static-summary.txt`,
`event-caller-list.txt`, `validation.txt` et `gate.status`.

## Gate fermé — producteur de la paire d'événements qualifié

La paire est publiée par `sub_822EED70` dans `0x82934748/0x8293474C` via les
stores `0x822EEDA8/0x822EEDB4`, avec les valeurs `0xE0000048/0xE000004C`.
`sub_822EEE10` relit le wait et appelle `NtSetEvent` (`0x821A6AC0`), tandis
que `sub_822E4080` relit signal/wait et appelle
`NtSignalAndWaitForSingleObjectEx` (`0x821A69C8`). La route bornée observe
351 couples SetEvent/réveil, mais le frontier post-START reste bloqué avant
frontend; l'ownership/scheduling du writer à ce frontier est donc la seule
incertitude immédiate. Aucun patch ni runtime élargi.

Preuves : `artifacts/static-event-producer-gate/decision.md`,
`validation.txt`, `producer-reports-compact.txt` et `gate.status`.

## Gate fermé — ownership du writer et contrainte scheduler

Le thread 12 possède le chemin `0x822E3EC0 -> 0x822EEE10` et produit
normalement `NtSetEvent(0xE000004C)`. Au frontier, ce même thread est bloqué
sur `0xE0000040`; les 23 threads sont bloqués et le scheduler fiber mono-host
ne réveille que les couples `(kind,key)` exacts. Le writer ne peut donc pas
progresser par lui-même. La prochaine question est le producteur de
`0xE0000040`; aucun patch ni runtime élargi.

Preuves : `artifacts/static-event-writer-ownership-gate/decision.md`,
`frontier-thread12.txt`, `generated-callchain-extract.txt`, `validation.txt`
et `gate.status`.

## Gate fermé — producteur statique de `0xE0000040`

Le producteur exact est maintenant joint : le thread 2 atteint
`sub_822E5660`, qui fournit l'objet `0x82934708` à `sub_822E3EB8`; ce dernier
charge le champ `+0x58` à `0x82934760` puis appelle le wrapper
`sub_821A6AB0`. Les atlas bornés observent cette chaîne aux ticks 1–252
(`count=252`). La frontière restante est l'enregistrement et l'activation du
callback au LR indirect `0x821C5178`; aucun patch ni runtime élargi.

Preuves : `artifacts/static-event-producer-0xE0000040-gate/decision.md`,
`producer-detail.txt`, `generated-event0040-candidates.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — enregistrement du callback `0x821C5178`

Le thread 1 installe le callback `0x822E5660` via
`0x822F85B8 -> 0x822F85A8 -> 0x822E5670 -> 0x821C5D68`; le dernier store
écrit le slot `+16520`. Le dispatcher `0x821C5090` relit ce slot et exécute
le callback à LR indirect `0x821C5178`; les atlas neutral/start l'observent
sur le thread 2 aux ticks 1–252. L'objet exact et sa correspondance
affinité restent ouverts. Aucun patch ni runtime élargi.

Preuves : `artifacts/static-callback-registration-gate/decision.md`,
`registration-detail.txt`, `generated-callback-registration-extract.txt`,
`validation.txt` et `gate.status`.

## Gate fermé — objet de registration et frontière d’affinité

`sub_822F85B8` construit l’objet publié dans `stack+80` par
`sub_821BB4C8`, initialise son mot `+0x56F8` à `0x0C000001`, puis passe ce
 même pointeur à `sub_822F85A8`. La chaîne
`0x822F85B8 -> 0x822F85A8 -> 0x822E5670 -> 0x821C5D68` installe donc le
callback `0x822E5660` dans le même objet, au slot `+16520`.

La construction ne contient aucune arête vers `sub_821A5390` ou
`KeSetAffinityThread`; ses sept callsites sont hors de cette chaîne. Les
atlas d’affinité ne montrent aucun appel sur le thread 2. L’objet → callback
est fermé, l’objet → affinité/CPU reste indécidable statiquement. Aucun patch,
runtime large ni oracle Wine/Edge n’a été utilisé.

Preuves : `artifacts/static-owner-object-affinity-gate/decision.md`,
`owner-affinity-detail.txt`, `affinity-wrapper-callers.txt`,
`constructor-callers.txt`, `validation.txt` et `gate.status`.

## Gate fermé — dispatch runtime objet/affinité

Le probe natif borné à 3036 ticks observe une arête indirecte unique
`0x821C5178 -> 0x822E5660` sur le thread invité 2, 3035 fois. Le snapshot
final donne `r31=0x10041A00` et `r11=0x822E5660`, ce qui ferme la jonction
objet enregistré → slot callback → cible exécutée. Dix-neuf transitions
d'affinité valides passent par `0x821A53DC` sur les threads 1, 12–17, 20 et
25; aucune ne concerne le thread 2. L'affectation CPU de l'objet reste donc
indéterminée. Aucun patch de scheduler ou de sémantique native.

Preuves : `artifacts/bounded-dispatch-object-affinity-gate/decision.md`,
`runtime-summary.txt`, `validation.txt` et `gate.status`.

## Gate fermé — validation native et O3

La suite CTest qualifiée passe 27/27 avec `SDL_AUDIODRIVER=dummy`. Les six
contrats ciblés passent dans la configuration courante et lors d'une seconde
compilation directe en `-O3`; aucune différence de résultat n'est observée.
Le guest généré n'a pas été modifié et aucune sémantique native n'a été
patchée par cette gate. Les changements du worktree étaient déjà présents au
préflight.

Preuves : `artifacts/native-validation-o3-gate/decision.md`, `results.txt`,
`o3-results.txt`, `validation.txt` et `gate.status`.

## Gate fermé — observation visuelle native

La fenêtre Vulkan native bornée à 3036 ticks se termine avec le code 4. Elle
produit 2928 notifications de présentation, mais aucune présentation
qualifiée, aucun fichier de capture et aucun jalon frontend ou mission. Le
rapport réduit indique que les threads invités restent bloqués avant le
frontend. La gate est donc fermée sans preuve visuelle de gameplay; aucun
oracle Wine/Edge ni patch de rendu n'a été utilisé.

Preuves : `artifacts/visual-native-gameplay-gate/decision.md`,
`runtime-summary.txt`, `validation.txt` et `gate.status`.

## Gate fermé — réveil natif du producteur 0xE0000040

La documentation XDK confirme la sémantique auto-reset utilisée par cet
événement. L'analyse statique suit le producteur
`0x822E5660 -> 0x822E3EB8 -> 0x821A6AB0`, via l'objet `0x82934760+0x58`, vers
`0xE0000040`. Une sonde native bornée à trois ticks retourne 4 : aux ticks 1 et
2, le thread 2 publie 0040, réveille le thread 12, puis celui-ci reprend à
`0x821A8C88`, publie 004C et se remet en attente sur 0040. Frontend et mission
restent faux. Le producteur et le réveil natif sont donc confirmés ; aucun
correctif scheduler/événement n'est retenu.

Preuves : `artifacts/event0040-native-producer-gate/decision.md`,
`runtime-summary.txt`, `static-summary.txt`, `validation.txt` et
`gate.status`.

Prochaine frontière : qualifier statiquement le consommateur après reprise à
`0x821A8C88` et le chemin primaire `0x821A69CC`.

## Gate fermé — boucle guest post-réveil

Le projet Ghidra qualifié du demo (`ace-combat-6-demo` / `Default.xex`) montre
que `0x821A8C50` est le wrapper d'attente et que `0x821A6AF0` lui passe un
timeout nul. Le callback runtime `0x822E3EC0` attend 0040, incrémente son
compteur sous verrou, appelle `0x822EEE10`, signale 004C, puis réattend 0040
tant que son drapeau `state+0x18` reste nul. Les fonctions
`0x822E4018`, `0x822E4080` et `0x822EEE68` réutilisent la même boucle côté
`SignalAndWait`. La séquence observée `resume 0040 -> set 004C -> reblock
0040` est donc une arête guest attendue, pas une perte du réveil natif.

Preuves : `artifacts/postwake-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `caller-decompile.raw.txt` et
`signal-caller-decompile.raw.txt`.

Prochaine frontière : identifier statiquement le compteur ou le drapeau
primaire qui doit progresser pour atteindre le frontend.

## Gate fermé — compteur primaire et cible de comparaison

Le projet démo `ace-combat-6-demo` attribue l'objet propriétaire à
`0x822DA9C0 -> 0x822E40E8`, avec base `0x82934708`; l'objet de synchronisation
primaire est `owner+0x40 = 0x82934748`. Ses handles sont `0xE0000048` et
`0xE000004C`, et son payload `+0x10` est écrit par `0x822EEE10`.

Le callback `0x822E3EC0` incrémente `owner+0x08`, publie ce compteur dans le
payload primaire, signale `owner+0x60` (0040), puis réattend tant que
`owner+0x18` vaut zéro. Les boucles `0x822E4018` et `0x822E4080` comparent le
payload primaire à `owner+0x10` avant d'attendre 0048/004C.

La cible `owner+0x10` est maintenant attribuée : `0x821A3CEC` appelle
`0x822DA7F0(...,1)`, qui route l'objet `0x82934680` vers `0x822E52D0`;
celui-ci écrit le dernier argument moins un à `owner+0x98`, soit exactement
`0x82934718 = 0x82934708+0x10`. L'initialisation statique met donc la cible à
zéro. Aucun patch scheduler/événement/rendu ni runtime supplémentaire n'est
requis pour cette gate.

Preuves : `artifacts/primary-counter-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status` et `marker-scan.txt`.

Prochaine frontière : qualifier le chemin guest qui doit quitter ce protocole
primaire pour atteindre le frontend; ne pas retester la provenance déjà fermée.

## Gate fermé — successeur guest et frontière service

La sortie de `0x822DA9C0` est suivie statiquement vers `0x822E5540`, le
`RenderContextDefault` `0x82934700`, puis la vtable `0x8202A488` et
`0x822F85B8`. La première branche locale est le résultat de `0x821BB4C8`
avant la suite d'initialisation du contexte de rendu.

La boucle principale `0x821A4808` réappelle les wrappers d'état, le timer
`0x821DEAC0` et son delta `0x821DECE8`. Le service `0x8238CDA0` est qualifié :
il pointe vers `0x82386CC0`, vtable RTTI `0x82008EF0`, slot `+0x2C` vers
`0x820FF988`, puis slot `+0x24` vers `0x820FF8D8`. Ce dernier dépend du
drapeau BSS `0x826E2374`; les pointeurs `0x82822F08` et `0x828819A4` restent
également non initialisés dans l'image statique.

Conclusion : la première frontière non qualifiable est bornée au résultat du
contexte de rendu et aux services BSS; aucun jalon frontend n'est encore
prouvé. Aucun runtime ou patch n'est requis pour cette gate.

Preuves : `artifacts/frontend-successor-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status` et `marker-scan.txt`.

Prochaine frontière : retrouver statiquement les écrivains des trois champs
BSS; ne lancer une capture ciblée qu'après épuisement de cette slice.

## Gate fermé — écrivains BSS et services statiques

Les écrivains et types des trois dépendances BSS sont fermés dans le projet
`ace-combat-6-demo` / `Default.xex`. `0x826E2374` est écrit par `0x820FFCA0`
avant la vidange de la file du worker et lu par `0x820FF8D8`. `0x82822F08` est
construit par `0x82259FF8` depuis `0x821A3C30`; sa vtable address-point
`0x82013084` est RTTI `CAce6TaskManager@ACE6`, avec les slots appelés
`+0x04 -> 0x82259D10`, `+0x08 -> 0x82259DA8`, `+0x0C -> 0x82259E18`,
`+0x10 -> 0x82259E90` et `+0x18 -> 0x82259F58`. `0x82259D10` itère les
tâches et `0x82259F58` retire un nœud de file.

`0x828819A4` reçoit statiquement `0x823C0D90` à `0x823732DC`; l'objet porte
la vtable `0x82012C04`, RTTI `CLayeredDrawCallBack`, et ses consommateurs
qualifiés sont `0x821A30F0`, `0x821A4690` et `0x82266400`. Aucun de ces
chemins ne prouve encore un jalon frontend ou mission.

Preuves : `artifacts/bss-service-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`,
`correct-service-slots.raw.txt` et `marker-scan.txt`.

Prochaine frontière : qualifier le premier consommateur frontend après les
services task/layered; aucun runtime n'est justifié avant ce slice.

## Gate fermé — premier consommateur frontend

La transition statique `0x821929A8 -> 0x82190B18` choisit une fabrique de
tâche et transmet son résultat au slot d'insertion `0x82259E18` du gestionnaire
`0x82822F08`. La fabrique `0x82191468` alloue `0x78` octets et appelle
`0x8218A5F0`, qui pose la vtable `0x8200F01C`. Le RTTI de cette vtable est
`CModeTaskTitle`, avec les bases SWG et Ace6 qualifiées.

Le gestionnaire lie l'objet à une liste de vingt entrées, puis
`0x82259D10` le visite; pour le titre, `+0x28` est le poll PPC
`0x8216C940` et `+0x10` atteint `0x8218A7A8`. Le premier consommateur frontend
est donc fermé statiquement. La branche choisie par l'état courant du demo
reste une question distincte; aucun runtime ni patch natif n'est requis ici.

Preuves : `artifacts/frontend-consumer-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`, `marker-scan.txt` et
les sorties Ghidra ciblées.

Prochaine frontière : qualifier la provenance de la source compacte du clip
titre, selon `NEXT.md`.

## Gate fermé — provenance de la source compacte du clip

Dans `ace-combat-6-demo` / `Default.xex`, la chaîne
`0x82278F78 <- 0x8227A898 <- 0x8227AAC0` est bornée. Ses quatre appelants
amont (`0x82127D40`, `0x8219A060` et `0x821A1170`) fournissent des
descripteurs, tables et buffers génériques par `0x821EE0F8`,
`0x821A6808` et `0x821A6168`; aucun corps qualifié ne relie l’entrée au
constructeur ou à la ressource du clip titre. La provenance du pointeur source
est donc indécidable statiquement, sans nouveau runtime.

Preuves : `artifacts/compact-title-provenance-gate/decision.md`,
`static-summary.txt`, `upstream-slice.txt`, `validation.txt`, `gate.status`
et `marker-scan.txt`.

Prochaine frontière : décoder isolément la fenêtre de flux titre post-START;
ce gate est maintenant fermé par `artifacts/title-stream-parser-gate/`.

## Gate fermé — parser local du flux titre

Le CFG de `0x823246C0` et le parser local expliquent 23 appels `BOX_CASE0` et
13 événements `opcode=7` dans `0x2DCB2438..0x2DCB2680`. Le premier mot hors du
vocabulaire est `0x2DCB2448 = 0x1A`, consommé par le chemin par défaut. Une
ligne agrégée historique annonçait `0x2DCB243C`; la séquence détaillée la
réfute et l’anomalie est bornée dans le rapport de fermeture.

Preuves : `artifacts/title-stream-parser-gate/decision.md`,
`static-summary.txt`, `closure-check.txt`, `validation.txt`, `gate.status` et
`marker-scan.txt`.

Prochaine frontière : qualifier les producteurs de l’état `mode / mission /
level` lu par le film de titre.

## Gate fermé — absence de producteur natif après START

Le slice Ghidra des lecteurs et écrivains du triplet est épuisé. Le lecteur
mission `0x82095B80` et le lecteur niveau `0x820E9290` utilisent le sélecteur
de slot, les offsets `+0x6C4`/`+0x6D0` et le stride `0xAAB8`; aucun écrivain
entier de niveau n’est trouvé et le seul store entier direct de mission reste
`0x82171988`. Les écrivains du champ mode `base+0x78` sont connus mais ne
sont pas couplés à ce chemin dans la fenêtre.

Le probe codegen-on-b, avec START au tick 3000 et stores bornés à
`0x823C0000..0x823C5000`, atteint 3005 ticks. Aux ticks 3000–3004, l’index
reste 0, mission `0x823C2F14=2048` et niveau `0x823C2F20=4096`; aucun store
ne cible ces champs ni `base+0x78`. Le résultat ferme seulement l’absence de
producteur natif dans cette fenêtre ; il ne qualifie pas une source bytecode
ou VM.

Preuves : `artifacts/title-film-state-producers-gate/decision.md`,
`EVIDENCE.md`, `runtime-compact.txt`, `validation.txt`, `gate.status` et
`marker-scan.txt`.

Prochaine frontière : attribuer statiquement le producteur bytecode/VM du
film autour du mot hors vocabulaire `0x2DCB2448=0x1A`.

## Gate fermé — attribution bytecode/VM du film

Le slice statique ferme `0x823246C0` comme interpréteur consommateur : il
fetch/avance un PC dans le buffer, décode les opcodes `0..7` et dispatch vers
les slots de l’`ASContext`. `0x82278F78` remplit ce buffer par dépaquetage sur
un autre thread. Le corps de l’interpréteur ne référence pas `+0x6C4`,
`+0x6D0`, `+0x78`, ni les wrappers/getter/setter de l’état du titre.

Les producteurs qualifiés restent natifs : `0x820EA550`/`0x820EA598` lisent
le singleton via `0x82095B80`/`0x820E9290`, et `0x82171988` écrit la mission.
La VM n’est donc pas attribuée comme producteur du triplet. Détails :
`artifacts/title-bytecode-vm-static-gate/`.

Prochaine frontière : écrivains natifs hors fenêtre START, en particulier le
champ niveau `base + index * 0xAAB8 + 0x6D0`.

## Gate fermé — écrivains explicites du niveau

Le scan PPC de `+0x6D0` borne cinq stores `stfs`. Les cinq corps construisent
leur base d’écriture dans des zones globales distinctes et ne référencent ni
`0x823C27E0`, ni le sélecteur du slot, ni le stride `0xAAB8`. Les trois chemins
qui passent un pointeur `+0x6D0` à `0x821F1500` ne produisent pas un store : le
callee lit seulement des métadonnées du pointeur.

Aucun écrivain explicite du niveau du slot titre n’est donc attribué. Détails :
`artifacts/title-level-writer-static-gate/`.

Prochaine frontière : construction, publication et initialisation groupée du
singleton titre `0x823C27E0` et de son sous-objet `+0x70`.
## 2026-08-21 — dispatch vtable `+0x0C`

Le rattachement statique d'un appel de slot `+0x0C` à l'instance de titre est limité : 289 dispatches partagent cet offset. Aucun runtime n'a été lancé.
## 2026-08-21 — producteur du contexte titre

`Function_820D29E0` transmet son second paramètre inchangé à `FUN_82324188`; ce paramètre provient d'un dispatch virtuel non résolu statiquement. Aucun runtime n'a été lancé.
## 2026-08-21 — frontière XMA écartée

La garde XMA contextualisée ne modifie ni le scheduler bloqué ni le frontend. Le prochain producteur qualifié à examiner est `RB_COPY` vers EDRAM/surface ; aucun runtime supplémentaire n'est requis à ce stade.
## 2026-08-21 — contrat `RB_COPY` fermé

Le draw RT0 guest, `RB_COPY`, le readback Vulkan et la matérialisation EDRAM forment une chaîne qualifiée. Le premier contenu non noir du draw normal manque encore ; l'analyse bascule sur ce writer réel.
## 2026-08-21 — sélection RT0 manquante

Le frontend natif prépare shaders et mémoire partagée, mais ne renseigne jamais `normal_draw_command_`. Le premier writer RT0 est donc inatteignable avant EDRAM ; le prochain changement minimal est sa sélection depuis le lot de commandes qualifié.

## 2026-08-21 — provenance du bootstrap IB indécidable

Le watcher historique d'IB ne couvre pas le heap `0x16AE...`. Le watcher
générique a été appliqué à `[0x16AE0980, 0x16AE0A40)` pendant un tick sur un
build contenant le guest. Aucun store n'est observé, mais aucune soumission
d'IB ne l'est non plus ; le résultat ne confirme ni ne réfute un producteur
guest. Le gate est limité après cinq lots.

## 2026-08-21 — première soumission IB qualifiée

Le hook partagé existant capture `0x16AE0980`, 48 dwords, au tick 0 sur deux
builds guest. Les générations du premier et du dernier mot sont nulles et le
watcher générique ne voit aucun store. La session établit que la zone heap
n'existe pas avant l'entrée guest. L'hypothèse initiale d'une écriture générée
hors couverture est ensuite réfutée par le routage `AC6_PPC_STORE_*` ; le
contenu courant de la capture doit être requalifié. Le gate est limité après
cinq lots.

## 2026-08-21 — routage des stores générés fermé

Les stores PPC générés scalaires et VMX passent déjà par les cinq adaptateurs
`AC6_PPC_STORE_*`, puis par `GuestMemory::store_*`. La fenêtre configurable
est donc active sur leur point partagé. L'hypothèse d'un hook généré séparé à
étendre est réfutée et aucun code n'est ajouté. L'identité des 48 dwords de la
capture courante reste à qualifier.

## 2026-08-21 — mapping du bootstrap IB qualifié

Un runtime GDB borné ordonne `map_zero(0x16ADF000, 8192)` avant la capture de
`0x16AE0980`, 48 dwords, au tick 0. La région est donc engagée dans ce mapping.
Le payload courant reste indécidable : l'appel de capture est inline et GDB ne
fournit pas la valeur de retour de `load_u32` dans ce build optimisé. Le gate
est limité après cinq lots.


## 2026-08-21 — payload du bootstrap IB qualifié

Le premier tick soumet l’IB `0x16AE0980` avec 48 mots exactement composés de
24 paires `C0003600,00010081`. L’opcode `0x36` est
`PM4_DRAW_INDX_2`; l’IB est donc une séquence réelle de 24 draws PointList,
ni un buffer nul ni un payload opaque. L’ancienne attente `0x2D` est réfutée.
# Checkpoint — producteur `0xE0000040` qualifié

Le producteur exact de l'événement attendu par le thread 12 est le callback du
thread 2 `0x822E5660`, via `0x822E3EB8` puis le wrapper `0x821A6AB0`. Le handle
est chargé depuis `0x82934760`. La frontière active est désormais
l'enregistrement et l'ordonnancement de ce callback à `LR=0x821C5178`.

## 2026-08-21 — framebuffer du tick 5800 qualifié

Les 5 692 présentations comptées sont des notifications `VdSwap`, pas des
présentations du renderer. Le rapport contient 24 draws mais aucune soumission
de ring et aucune commande `present` typée. Sans cette commande, le resolve,
le writeback guest et le screencap sont inaccessibles : aucun framebuffer
rendu ne peut encore être qualifié comme noir ou visible.

## 2026-08-21 — contrat producteur de `present` identifié

Le parseur `XE_SWAP` et le consommateur renderer existent déjà. `VdSwap`
construit également le paquet qualifié dans le buffer système guest. La rupture
est entre ce buffer et le point de soumission Xenos : aucune publication ne le
fait traverser au processeur dans le corridor courant.

## 2026-08-21 — raccord `VdSwap` implémenté, runtime indécidable

Le buffer système traverse désormais `apply_xenos_typed_batch` par une API
bornée qui rejette toute consommation partielle. Le test ciblé observe une
commande `present` exacte et passe. La compilation complète passe ; la suite
globale reste à 25/26. Le probe isolé n'a pas produit de résumé exploitable
avant la limite du gate, donc la validation runtime reste ouverte.

## 2026-08-21 — soumission directe de `VdSwap` réfutée et retirée

Le rapport existait : 171 ticks, 88 appels `VdSwap`, 88 commandes `present`,
puis rejet du lot pour présentations multiples. Cela contredit la route
qualifiée antérieure, où de nombreux swaps correspondaient à un seul paquet
Xenos. Le raccord direct et son test ont été retirés ; build et test core
repassent. La frontière correcte est le writer WPTR/IB réel.

## 2026-08-21 — fausse porte WPTR `0x827AD2F0` fermée

`0x821B9BC8` publie le WPTR du bootstrap, mais son chemin post-`KickOff`
n'est pas la publication normale. `0x827AD2F0` et `device+21508` appartiennent
aux compteurs de performance Microsoft ; les forcer serait incorrect. La
publication post-bootstrap doit être cherchée dans les callbacks D3D.

## 2026-08-21 — vérificateur callback D3D limité par provenance

L'image du build courant contient les 33 ancres, 8 fonctions et 5 producteurs
attendus. Le vérificateur ne signale aucune divergence structurelle ; son seul
échec est une garde d'identité interne non utilisable comme garde de révision
ordinaire. La provenance directe de l'image doit être réconciliée.

## 2026-08-21 — provenance de l'image callback D3D fermée

Le `xex-basefile.bin` courant est produit directement depuis le `Default.xex`
de la démo PAL par `build_demo.py`, avec le manifest Ghidra canonique
`ace-combat-6-demo/Default.xex`. Les images plates PAL historiques conservées
sont directement identiques octet par octet à l'image courante. Les 33 ancres,
8 fonctions et 5 producteurs sont donc qualifiés ; la garde d'identité interne
du vérificateur est obsolète et ne constitue plus un blocker.

Le prochain gate suit statiquement `KickOff` jusqu'au callback qui publie le
travail ou réveille le consommateur. Aucun shim ni runtime avant d'avoir
identifié ce contrat.

## 2026-08-21 — callback post-KickOff réfuté comme publisher

Le ring produit des commandes mais son consommateur reste immobile. Les
interruptions `PM4_INTERRUPT` et leurs callbacks se trouvent en aval de
l'avance du ring : elles ne peuvent pas produire le kick qui permet leur propre
livraison. `0x821B9710 → 0x821C5190` consomme l'interruption et ne publie pas le
WPTR. `0x821C4A60 StartWorkerQueue` est également transporté par le ring bloqué.

La frontière active redevient donc le passage du curseur produit par
`0x821C57D0` au store WPTR MMIO de `0x821B9BC8`. Ne pas réintroduire la fausse
porte des compteurs ni une soumission dans `VdSwap`.

## 2026-08-21 — jointure `0x821C57D0 → WPTR` réfutée

Les preuves historiques corrigées montrent que `0x821C57D0` modifie un IB déjà
référencé sans faire croître le ring primaire. La file guest surveillée n'a pas
non plus de consommateur bloqué : `0x820FF710` et `0x820FFCA0` exécutent leur
danse producteur/consommateur chaque tick. L'ancienne métrique n'observait que
l'état remis à zéro en fin de tick.

La panne discriminante est plus en amont : les slots de payload de 96 octets
restent nuls. La prochaine frontière est donc le premier writer non nul de ces
slots, pas un nouveau kick WPTR.

## 2026-08-21 — producteur de payload limité par xrefs dynamiques

Le corps Ghidra qualifié de `0x820FF710` prend seulement le pointeur de file,
écrit le littéral zéro dans `slot[index]+64`, incrémente l'index et signale la
synchronisation. Aucun argument ni champ source ne fournit un payload.

Les exports structurés actuels ne donnent aucune arête entrante ni référence
directe vers cette fonction et ne recensent pas exhaustivement les stores
adressés par `base + index*0x60`. Le prochain test est un slice P-code Ghidra
borné de `base+0x110`, stride `0x60`, champ `+0x40`.

## 2026-08-21 — balayage des déplacements immédiats limité

Le programme Ghidra canonique ne contient aucun accès mémoire PPC D-form avec
le déplacement immédiat `0x110`, alors que `0x820FF710` reconstruit cette
adresse dans le C décompilé. L'adresse est donc formée par registres et le
balayage des seuls déplacements ne peut pas recenser les écrivains du slot.

La prochaine reprise doit normaliser les expressions d'adresse du P-code haut.
Aucun runtime ni payload synthétique n'est justifié.

## 2026-08-21 — première passe P-code indécidable

`FindScaledStoreWriters.java` normalise récursivement les entrées des `STORE`
et impose `0x820FF710` comme témoin. Ghidra démarre le script, mais le premier
lancement se termine autour de 30 secondes avant fermeture du fichier de
résultat. Aucun checkpoint n'a donc été persisté.

Le prochain lancement doit vider le fichier après calibration et pendant le
balayage, puis être attendu comme processus long.

## 2026-08-21 — famille des writers de payload fermée

Le processus Ghidra original a finalement terminé. La passe P-code retrouve
le témoin type `0` puis quatre writers directs qui posent les sélecteurs
`1`, `2`, `3`, `4`. La vtable canonique confirme leurs slots contigus
`+0x10..+0x1C`.

`0x82117410` appelle sans garde locale les writers types `1` et `4`. Le zéro
observé dans la file ne vient donc pas d'un writer incomplet : cette fonction
n'est pas atteinte dans le corridor courant. La prochaine frontière est son
appelant et sa condition d'activation.

## 2026-08-21 — appelant types 1/4 réduit à un site

Ghidra trouve une seule référence vers `0x82117410` : appel direct
`0x8210A1C0:0x8210ADB4`. Le callsite est au fond d'une chaîne de gardes dans le
C décompilé. La fenêtre textuelle ne suffit pas à identifier honnêtement la
condition dominante.

Prochaine passe : CFG et dépendances de contrôle du bloc `0x8210ADB4`, puis
slice du premier champ mémoire testé.

## 2026-08-21 — contrôle `0x8210ADB4` exporté

Le CFG haut de `0x8210A1C0` contient 502 blocs. Dix-huit choix de branche sont
nécessaires pour atteindre le producteur : dispatch initial sur 16 bits, puis
cinq paires de validation lookup/champ `+0x10C`.

La fonction a un seul appelant, `0x82165CC0:0x82165D8C`. Les paramètres fournis
à ce site sont maintenant la frontière statique utile ; le getter global à
33 appelants n'est pas discriminant.

## Gate 2026-08-21 — arguments producteur render queue

- Gate `render-queue-producer-arguments-gate` fermé statiquement.
- `0x82165CC0:0x82165D8C` appelle toujours `0x8210A1C0` avec `uVar2`, résultat du slot vtable `+4`, puis paramètres appelant 5 à 8.
- Dans `0x8210A1C0`, paramètre 4 bas 16 bits sélectionne type. Route vers `0x82117410` exige type `1`.
- Cinq ressources sont ensuite résolues par `0x821080D0`; chaque index `-1` ou champ d’enregistrement `+0x10C == 0` évite appel.
- Aucun contrat natif ajouté: offsets payload et origine des cinq clés restent à qualifier.

## Gate 2026-08-21 — layout payload type 1

- Gate `render-queue-type1-payload-layout-gate` fermé statiquement.
- Payload commence par cinq chaînes consécutives `int16 length; byte[length]`.
- Après cinquième chaîne: groupe optionnel de 3 drapeaux, puis groupe optionnel de 2 drapeaux.
- `0x8231BCB0` est producteur observé du mot ressource `+0x10C`; `0x820CBAB0` possède chemin de remise à zéro byte pour kinds 6/7.
- Aucun changement natif: RHS exact du producteur non nul et ses appelants restent à slicer.

## Correction 2026-08-21 — faux producteur +0x10C

- Gate `resource-10c-producer-slice-gate` limité après cinq batches.
- Hypothèse `0x8231BCB0` producteur du record render `+0x10C` réfutée: fonction parse RIFF/WAVE sans argument et écrit structure globale retournée par `FUN_823270DC`.
- Même déplacement `+0x10C` est collision de layout; aucun alias vers table `DAT_826F6124` prouvé.
- Scan exhaustif brut non exécuté: launcher `analyzeHeadless` absent environnement courant.
- Aucun changement natif.

## Gate 2026-08-21 — inventaire stores +0x10C

- Gate `record-10c-store-enumeration-gate` limité par quota après inventaire canonique.
- Launcher qualifié retrouvé sous `.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless`; projet `ace-combat-6-demo`, programme `Default.xex`.
- Scan exact sur `0x82000000..0x83000000`: 101 instructions store D-form au déplacement `0x010C`.
- Famille forte: `0x8210D950` écrit `param2` à objet `+0x10C`; `0x8210D9C0` et `0x8210DA80` consomment/libèrent/remettent zéro; `0x8210DB10` initialise champ à zéro.
- Alias de cet objet vers records de `DAT_826F6124` reste non prouvé. Aucun changement natif.

## Gate 2026-08-21 — alias record ressource

- Gate `resource-record-alias-gate` limité par quota avec jointure presque complète.
- `0x8210D8A0` ne retourne pas record: il écrit type `.nud/.nut` à `param1+0x110`.
- `0x8210DD70` reçoit manager en `param1`, alloue record de taille `0x428`, l’initialise, le stocke à `manager+(index+2)*4`, puis appelle `0x8210D950`.
- Consumer `0x8210A1C0` utilise même formule `DAT_826F6124+(index+2)*4`.
- Le callsite `0x82108A98` charge `DAT_826F6124`, le passe en `param1`, puis appelle `0x8210DD70`. Aucun changement natif.

## Gate 2026-08-21 — base manager ressource

- Gate `resource-manager-base-identity-gate` fermé par réfutation de la décompilation.
- `0x82327100` est helper de sauvegarde `r26..r31/LR`; il préserve `r3` et ne retourne aucun manager.
- `0x8210DD70` copie le `r3` entrant vers `r31` à `0x8210DD88`.
- `0x82108A6C` charge `DAT_826F6124`; `0x82108A8C` le place en `r3`; `0x82108A98` appelle `0x8210DD70`.
- Ce chemin producteur écrit donc dans la même table que le consumer `0x8210A1C0`. Aucun changement natif.

## Gate 2026-08-21 — contrat natif d’enregistrement ressource

- Gate `native-resource-registration-contract-gate` limité après cinq batches.
- Sources maintenues et build hôte courant ne contiennent pas les adresses PPC ciblées; build courant a codegen désactivé.
- Sorties guest qualifiées existent sous `build-codegen-on/codegen/`, dont objet guest et manifeste.
- Dernière inspection a été interrompue par `nm | head` sous `pipefail` avant rapport compact; couverture des fonctions reste indécidable.
- Aucun code modifié dans reconstruction native; seul état durable mis à jour.

## Reprise limitée 2026-08-21 — couverture guest ressource

- Objet guest qualifié contient symboles pour `0x82108918`, `0x8210DD70`, `0x8210D950` et `0x8210A1C0`.
- Manifeste annonce 13 071 fonctions confirmées, 53 unités compilées, zéro diagnostic et zéro instruction unsupported.
- Parcours relocations depuis quatre racines a désassemblé 223 fonctions: 67, 61, 3 et 175 fonctions visitées respectivement.
- Jointure imports échoue car 238 entrées du manifeste n’utilisent aucune clé d’adresse supposée par analyse (`address`, `thunk_address`, `guest_address`).
- Gate encore limité; aucun runtime et aucun changement natif.

## Gate 2026-08-21 — jointure imports natifs

- Gate `native-import-schema-join-resume-gate` fermé par réfutation utile.
- Le callgraph sauvegardé est une table directe `caller -> [callee]`; la carte qualifiée contient 228 adresses de thunks.
- Les quatre closures visitent respectivement 67, 61, 3 et 175 fonctions, sans atteindre un seul thunk d'import.
- Aucun handler xboxkrnl/XAM/XMA direct n'est donc le contrat manquant de cette chaîne; prochaine frontière: dispatch indirect guest.
- Aucun runtime et aucun changement natif.

## Gate fermé 2026-08-21 — chaîne statique global/vtable

- Chaîne qualifiée: `0x82386C10 -> objet 0x82386C0C -> vtable 0x82008E50`.
- Slot 1 cible `0x820FEED8`; slot 13 cible `0x820FEF70`.
- Les deux cibles figurent exactement une fois dans le manifeste codegen.
- La famille de trois dispatchs de `0x821075A0` est réfutée comme contrat natif manquant. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — prochaine famille indirecte

- `0x82107870` et `0x82107A00` réutilisent la vtable déjà résolue; éliminées.
- Premier receiver nouveau: retour de `0x82220670`, passé à `0x821154C0` slot 20 (`+0x50`).
- Branche sœur `0x82115530`: slot 21 (`+0x54`).
- Décompilation producteur sauvegardée mais résumé final absent; type/vtable encore indécidable. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — dispatch indirect ressource

- Dix-neuf fonctions de la closure utilisent le helper indirect; neuf sont à un niveau de `0x8210A1C0`.
- Premier candidat `0x821075A0`: trois sites, LR `0x821075E4`, `0x821075FC`, `0x82107614`.
- Le helper reçoit la cible depuis le contexte `+0x108`; ces LR ne sont pas dans la table de slots qualifiée mais utilisent encore la résolution générique.
- Objet, slot et ensemble de cibles restent indécidables après cinq batches. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — slice dispatch `0x821075A0`

- Décompilation qualifiée dans `ace-combat-6-demo` / `Default.xex`.
- Objet commun des trois appels: `PTR_PTR_82386C10`.
- LR `0x821075E4` et `0x821075FC`: slot 1 (`+0x04`).
- LR `0x82107614`: slot 13 (`+0x34`).
- Cibles concrètes encore indécidables; prochain slice sur producteurs du global. Aucun runtime, aucun changement natif.

## Gate limité 2026-08-21 — global/vtable `0x82386C10`

- 24 xrefs directs qualifiés, tous en lecture; aucun store direct référencé.
- Le script de slot existant attend un receiver contenu dans l'objet global, mais ici le global est déjà le receiver; son résultat vide n'est pas probant.
- Prochaine preuve: lecture big-endian directe `global -> objet -> vtable -> slots +0x04/+0x34`.
- Aucun runtime et aucun changement natif.
## Gate resource-slot20-producer-resume limité — 2026-08-21

`0x82220670` est un lookup de tableau : il retourne une entrée, remplacée par
son alias `+0x18C` lorsqu'il existe. Ce retour alimente les dispatchs slots 20
et 21 de `0x821154C0`/`0x82115530`. L'export PPC canonique n'a pas été produit :
`Ac6XenonWords.java` refuse les arguments préfixés par `0x`. Reprendre sans
préfixe dans un nouveau gate. Détails :
`artifacts/resource-slot20-producer-resume-gate/BLOCKER.md`.
## Gate resource-slot20-ppc-abi limité — 2026-08-21

L'ABI Xenon est maintenant prouvée : `r3` porte le conteneur global `+0x308`,
`r4` l'identifiant payload éventuellement remappé, et le retour devient le
receiver des dispatchs slots `+0x50/+0x54`. L'ensemble des types d'entrée ou
d'alias `+0x18C` reste à borner par leurs producteurs statiques. Détails :
`artifacts/resource-slot20-ppc-abi-gate/BLOCKER.md`.
## Gate resource-slot20-type-producers limité — 2026-08-21

Deux routes d'alias sont prouvées : `0x8210A1C0` copie des objets
`DAT_826F6124 record+0x10C` dans `entrée+0x18C`; `0x82220550` y écrit le retour
du sélecteur à deux niveaux `0x821E1D80`. Les callgraphs sauvegardés ne couvrent
aucun appelant du setter. Reprendre avec des xrefs Ghidra directs. Détails :
`artifacts/resource-slot20-type-producers-gate/BLOCKER.md`.
## Gate resource-slot20-setter-xrefs limité — 2026-08-21

Le setter `0x82220550` n'a qu'un appel direct : `0x82216614` dans
`0x82216498`. Il reçoit exactement le conteneur global `+0x308` et un indice
actif normalisé, puis `0x82220750` reçoit le même conteneur avec la valeur `1`.
Il reste à résoudre les trois arguments PPC du sélecteur `0x821E1D80` pour
borner les vtables d'alias. Détails :
`artifacts/resource-slot20-setter-xrefs-gate/BLOCKER.md`.

### Gate limité : table de sélection des slots 20/21

Les arguments de `0x821E1D80` sont fermés : table globale à `+0x29698`, indices
issus des octets `+0x57/+0x58`, résultat écrit à `entry+0x18C`. La recherche de
producteurs par constantes est trop large et ne borne pas les vtables. Voir
`artifacts/resource-slot20-alias-selector-args-gate/BLOCKER.md` pour le prochain
test exact `global load + 0x29698 + lwzx/stwx`.

### Gate limité : accès indexés exacts à `+0x29698`

Le classifieur PPC recense 120 accès indexés exacts dans 114 fonctions, dont le
consommateur `0x82220640` et au moins deux écritures candidates. Le décalage seul
ne qualifie pas le bon objet. La prochaine branche doit propager localement le
registre chargé depuis `0x823C27E0` et ne retenir que les `lwzx/stwx` utilisant
cet alias comme base. Voir le blocker du gate.

### Gate limité : provenance CFG de la base globale

Deux stores `+0x29698` sont compris : le constructeur `0x821E0B7C` initialise
le champ de son objet avec l'adresse du sous-objet adjacent; `0x82212E54` écrit
dans le champ du singleton global depuis `param1+0x2E4` ou `param1+0x2E8`.
La tentative de classement linéaire des 120 sites est invalide et ne doit pas
être citée. Une propagation minimale par basic blocks est le prochain test.

### Gate limité : classifieur SSA du champ global

Un classifieur high-p-code minimal a été ajouté pour rechercher conjointement
`0x823C27E0` et `0x29698` dans les adresses LOAD/STORE. Son premier lancement
n'a traité aucune fonction : les adresses sans préfixe ont été données à
`Long.decode`. Le résultat vide est invalide. La prochaine branche commence par
la correction hexadécimale d'une ligne et relance le même test calibré.

### Gate limité : sur-approximation du classifieur SSA

Le classifieur SSA fonctionne et calibre correctement les deux accès globaux
connus tout en excluant le constructeur. Son inventaire de 1119 sites est
cependant trop large à cause des expressions `MULTIEQUAL` transitives. La
prochaine branche doit appliquer l'ensemble exact des 120 PC PPC comme
allowlist avant toute inspection SSA.
## 2026-08-21 — allowlist SSA `+0x29698` limitée

- Observé: 120 PC exacts classifiés; 117 qualifiés (116 loads, un store), deux
  rejetés, un orphelin `0x82210188` sans fonction Ghidra; zéro échec de
  décompilation.
- Observé: `0x82220640` et `0x82212E54` sont qualifiés; `0x821E0B7C` est rejeté.
- Inférence: l'inventaire SSA ne surcompte plus les opérations voisines, mais
  l'orphelin exige une preuve PPC brute bornée.
- Gate non fermé après cinq batches; voir
  `artifacts/resource-slot20-global-base-ssa-allowlist-gate/BLOCKER.md`.
## 2026-08-21 — orphelin `0x82210188` qualifié

- Observé: `0x82210178..0x82210188` construit `0x29698`, charge
  `*(0x823C27E0)`, puis exécute `lwzx r3,r11,r10`.
- Conclusion: `0x82210188` lit exactement
  `*( *(0x823C27E0) + 0x29698 )`.
- Inventaire final: 118 accès qualifiés, deux rejets, zéro inconnu.
- Gate fermé sans runtime ni shim; voir
  `artifacts/resource-slot20-orphan-82210188-gate/RESULT.md`.
## 2026-08-21 — appels du writer `global+0x29698`

- Observé: `0x82212DF0` a 15 appels directs répartis dans 9 fonctions.
- Observé: son premier argument est l'objet d'état courant; le second choisit
  `object+0x2E4` ou `object+0x2E8`.
- Observé: à `0x82216498`, le PPC conserve l'entrée `r3` dans `r30` avant
  l'appel; la signature Ghidra sans paramètre est erronée.
- Inférence: les producteurs utiles sont les stores des deux champs, pas les
  15 consommateurs du setter global.
- Gate limité après cinq batches; voir
  `artifacts/resource-slot20-table-object-producer-gate/BLOCKER.md`.
## 2026-08-21 — producteurs `+0x2E4/+0x2E8` qualifiés

- Observé: 34 stores bruts; trois paires pertinentes dans `0x82095E98`,
  `0x82174A80`, `0x82176B60`, plus l'initialisation zéro `0x82213628`.
- Observé: les paires pointent vers des conteneurs embarqués et tous les
  producteurs les peuplent via `0x820A4F58`.
- Observé: `0x820A4F58` écrit le tableau de pointeurs à `entry+0xD8` et sa
  borne à `entry+0xDC`, contrat consommé par `0x821E1D80`.
- Inférence: le receiver des slots 20/21 est l'objet `piVar16` du populator.
- Gate limité; voir
  `artifacts/resource-slot20-object-field-stores-gate/BLOCKER.md`.
## 2026-08-21 — factory des receivers localisée

- Observé: les objets `piVar16` publiés dans `entry+0xD8` viennent uniquement
  de `container->slot5`, à `vtable+0x14`.
- Observé: `r3` de `0x820A4F58` est le conteneur; ses six variantes embarquées
  convergent vers la vtable finale `0x82000B94`.
- Observé: les constructeurs propriétaires sont `0x82094CD0`, `0x82174888`
  et `0x82176930`; chacun passe par `0x820A3AF0` puis pose la même vtable.
- Gate limité avant lecture de l'entrée `0x82000BA8`; voir
  `artifacts/resource-slot20-pivar16-producer-gate/BLOCKER.md`.
## 2026-08-21 — slots receiver 20/21 résolus

- Observé: `0x82000BA8` cible `0x82093658`, qui retourne le conteneur entrant
  et restaure sa vtable finale `0x82000B94`; les kinds ne sélectionnent pas
  une famille de receivers distincte ici.
- Observé: les slots `+0x50/+0x54` ciblent respectivement `0x8220E428`
  (retour constant `0`) et `0x82211040` (lecture/différence des floats aux
  offsets `+0x70/+0x74`).
- Observé: aucune des deux adresses n'est présente textuellement dans l'arbre
  natif actuel.
- Gate fermé; voir
  `artifacts/resource-slot20-container-factory-vtable-gate/RESULT.md`.
## 2026-08-21 — couverture codegen non encore qualifiée

- Observé: le C++ XenonRecomp spécifique au jeu est un produit build-only;
  son absence de l'arbre maintenu ne prouve pas une lacune.
- Observé: aucune forme textuelle des starts `0x8220E428/0x82211040`
  n'apparaît dans le checkout actuel, produits de build compris.
- Incertitude: le manifeste structuré peut les représenter sans adresse dans
  le nom généré.
- Gate limité après cinq batches; aucun code modifié. Voir
  `artifacts/resource-slot20-codegen-coverage-gate/BLOCKER.md`.
## 2026-08-21 — hypothèse de trou codegen réfutée

- Observé: l'ensemble effectif contient 13 071 starts, dont 4 745 chunks
  qualifiés hors `.pdata`.
- Observé: `0x8220E428` (8 octets) et `0x82211040` (36 octets) sont tous deux
  dans cet ensemble.
- Observé: les deux figurent dans `ppc_func_mapping.cpp`; le C++ généré
  contient aussi des callsites directs vers `sub_8220E428`.
- Conclusion: aucun stub ni changement de configuration n'est requis pour
  ces slots. Gate fermé et hypothèse réfutée; voir
  `artifacts/resource-slot20-codegen-manifest-gate/RESULT.md`.
## 2026-08-21 — frontière courante réalignée sur cycle 1761

- Observé: les routes fraîches atteignent 5 463 PRESENT puis 23 threads
  bloqués/0 runnable, sans frontend/mission/terminal.
- Observé: chacune possède un unique load64 post-reprise, PC `0x82327154`,
  adresse `0x7F0409D8`, valeur zéro.
- Observé: les traces divergent mais les états outcome/milestones/graphics/
  scheduler rapportés sont identiques.
- Réfuté comme premier levier: `E000004C` est déjà joint set→wake→activation
  scheduler sur 351/351 paires par route.
- Gate limité avant extraction de la première divergence persistante; voir
  `artifacts/first-real-native-boundary-gate/BLOCKER.md`.
## 2026-08-21 — aucune divergence guest dans les traces cycle 1761

- Observé: chaque trace contient exactement 22 153 événements alignables.
- Observé: seules deux lignes divergent après tick 1, toutes deux dans le
  domaine `input`; la première est `buttons=0/16` au tick 252.
- Observé: zéro divergence non-input et zéro différence non-input persistante.
- Conclusion: la capsule ne contient aucun store guest, PM4/draw, scheduler,
  renderer ou readback divergent à slicer. Gate fermé; voir
  `artifacts/post-resume-first-divergence-gate/RESULT.md`.
## 2026-08-21 — fenêtre START minimale retrouvée dans les captures existantes

- Observé: la chaîne START publie `0x10` à `0x829D1550`, normalise `0x400` à
  `0x827B37E0`, puis écrit le bit logique `0x10` à `0x82798488` avant remise
  à zéro au tick 253.
- Observé: au tick 268, le store START-only est
  `0x820CDC20 → [0x2E3D3C0C] = 0`.
- Observé: la route neutre possède à la place la séquence
  `0x823255F0/0x82325644 → [0x2E3D44F0] = 0/0xFFFFFFFF`.
- Conclusion: aucun nouveau runtime requis; slicer statiquement ces trois PC.
  Gate fermé; voir
  `artifacts/start-minimal-observation-window-gate/RESULT.md`.
## 2026-08-21 — divergences tick268 reclassées en bruit de pile

- Observé: les valeurs rapportées comme `pc` sont les LR transmis au hook de
  stores, et non les instructions de store.
- Observé: `0x820CDC20` suit l'appel de `0x82321E20` puis force `r3=1`;
  `0x823255F0` suit le helper de sauvegarde de registres `0x8232710C`;
  `0x82325644` suit un appel virtuel et exécute une comparaison.
- Conclusion: les deltas à `0x2E3D3C0C/0x2E3D44F0` sont des différences de
  pile/cadre d'appel, sans consommateur persistant, tâche ou rendu qualifié.
- Gate fermé et hypothèse sémantique réfutée; voir
  `artifacts/tick268-divergent-pc-static-slice-gate/RESULT.md`.
## 2026-08-21 — aucun store START persistant promu

- Observé: `0x829D1550`, `0x827B37E0` et `0x82798488` forment une chaîne
  transitoire; le dernier bit logique est remis à zéro au tick 253.
- Observé: l'A/B tick268 ne couvre que `0x2E3C0000..0x2E3F0000` et ne peut
  pas qualifier les écritures dérivées de ces globals.
- Conclusion: aucun store persistant hors pile n'est démontré; il faut résoudre
  statiquement tous les lecteurs de `0x82798488`.
- Gate limité; voir
  `artifacts/start-first-persistent-store-static-gate/BLOCKER.md`.
## 2026-08-21 — effet durable START résolu mais propriétaire non atteint

- Observé: les consommateurs typés de `0x82798488` sont `0x82170FCC`
  (demo) et `0x82185210` (mission/title).
- Observé: le premier effet demo est `0x82171128`, qui écrit `1` à
  `[0x827435F8]+0x18`.
- Observé: les fonctions propriétaires `0x82170F58` et `0x82185198` ne
  sont atteintes dans aucune des deux routes bornées.
- Conclusion: le verrou est la construction/publication/dispatch de la tâche,
  pas le shim d'entrée. Gate fermé; voir
  `artifacts/start-logical-bit-reader-xrefs-gate/RESULT.md`.
## 2026-08-21 — consommateurs START qualifiés hors phase bootstrap

- Observé: les vtables consommateurs sont `0x8200C904`
  (`CModeTaskDemoBase`) et `0x8200E5C4` (`CModeTaskMissionTitle`).
- Observé: `0x82259D10` ne dispatche que startup-demo, loading et
  mode-manager; aucune mutation de liste n'apparaît après le tick 221.
- Observé: `CTaskLoading` atteint plus tard l'état 1, sans transition
  sortante dans sa propre update; l'état 2 de requête de mode n'est pas vu.
- Conclusion: le verrou est le contrat de terminaison/retrait de la tâche
  loading, pas une factory menu manquante. Gate fermé; voir
  `artifacts/start-consumer-owner-factory-gate/RESULT.md`.
## 2026-08-21 — état 1 de CTaskLoading sans transition interne

- Observé: la branche état 1 `0x8217E42C..0x8217E440` appelle seulement le
  slot `+0x20` du sous-objet `this+0x1C`, puis quitte sans écrire l'état.
- Observé: seule la branche état 2 décrémente `this+0x44` et peut écrire la
  requête `1` à `[0x827435F8]+0x18`.
- Incertitude: le contrat exact entre le thunk `0x8218CE20`, le dispatcher
  et les mutations de liste reste à extraire du listing déjà capturé.
- Gate limité; voir
  `artifacts/loading-task-retirement-contract-gate/BLOCKER.md`.
## 2026-08-21 — retrait par retour de CTaskLoading réfuté

- Observé: `0x82259D58` teste seulement le retour du slot `+0x28` pour
  décider d'appeler l'update.
- Observé: après le slot 4 à `0x82259D70`, `0x82259D74` avance au nœud
  suivant sans consommer `r3`.
- Observé: `0x82259E18` est une insertion/activation et `0x82259FF8` un
  initialiseur; aucune fonction ne retire une tâche.
- Conclusion: l'état 1 attend un writer externe/asynchrone ou un autre
  producteur de transition. Gate fermé; voir
  `artifacts/loading-task-dispatcher-cfg-gate/RESULT.md`.

## Loading vtable identity gate — CLOSED

- Observé : `CTaskLoading` utilise la vtable `0x8200F388`, installée par le
  constructeur `0x8218BF18` au store `0x8218BF30`.
- Observé : le constructeur initialise `this+0x0A=1` et `this+0x0C=0`.
- Observé : le vrai slot 4 est `0x8218CE20`; il teste `this+0x0A`, puis appelle
  `0x8218CCD0` seulement lorsque ce byte vaut zéro.
- Correction : `0x8217E3E0` n'est pas l'update propre de `CTaskLoading`; cette
  fonction partagée appelle `0x8218CCD0` depuis `0x8217E458`.
- Artefact : `artifacts/loading-vtable-identity-gate/RESULT.md`.

## CTaskLoading field-transition contracts — CLOSED

- Observé : `0x8218CBD8` arme `CTaskLoading` avec le triplet
  `(+0x09,+0x0A,+0x0C)=(1,0,1)` après création réussie d'une ressource.
- Observé : `0x8218CCD0` ne modifie pas l'état tant que `0x8219F5D0` renvoie
  zéro; sur statut non nul, il écrit `(0,1,0)` et propage le signe du statut.
- Observé : `0x8218CD78` remet également le triplet à `(0,1,0)` sur son chemin
  terminal.
- Réfuté : aucun callback externe n'est requis pour écrire directement
  `CTaskLoading+0x0C`; les writers sont internes à la classe.
- Nouvelle frontière : provider asynchrone `0x8219EE40 → 0x8219F5D0`.
- Artefact : `artifacts/loading-field-transition-contracts-gate/RESULT.md`.

## CTaskLoading async-status provider — CLOSED

- Observé : `0x8219F5D0` draine une file de travaux à `manager+0x20` et appelle
  le slot virtuel `+0x14` de chaque travail.
- Observé : slot `0` = pending, `-1` = erreur, autre = terminé; les travaux
  terminés sont publiés/libérés puis retirés. Le drain est borné à 64 travaux.
- Observé : `0x8219EE40` recherche récursivement la ressource dans l'arbre de
  gestionnaires à `manager+0x1C`.
- Prouvé absent : la route `ac6-native` ne contient ni ce provider, ni un
  contrat de file ternaire équivalent, ni du PPC généré câblé au build.
- Artefact : `artifacts/loading-async-status-provider-gate/RESULT.md`.

## Native loading contract integration gate — closed

Observed: native asset and retail-session loading is synchronous. The retail constructor auto-advances its frontend controller through the product states to Mission, and the render loop consumes only mission state. The guest asynchronous loading provider and any environment shim are off this executed path.

## Visible native frontend contract gate — closed

The PAL frontend controller and seven-pack resource closure exist, but native code has no visual producer for pre-mission states. `RetailFrontendResources` validates FHM/NFH payloads and discards their bytes; no decoder, glyph metrics/atlas representation, state draw list, or frontend compositor reaches the presentation boundary.

## PAL NFH glyph producer contract gate — closed at first unresolved field

Official XUI outputs and PAL FHM/NFH closure are qualified, but NFH record semantics are not. The first unresolved field is the big-endian word at `NFH+4`; native code only proves the range 1..4096. `NFH+8` is only proved nonzero. No parser or runtime implementation is justified until their PPC consumers establish cardinality, stride, metrics and texture association.

## Canonical PPC NFH consumer slice — branch stopped at batch limit

Canonical Ghidra evidence reduces NFH recognition to one image constant at `0x82028F5C` and one direct read at `0x822E2870` inside `FUN_822E2858`. The consumer semantics remain open because the final decompiler invocation used the wrong script argument order. See the gate blocker; no dynamic evidence or parser implementation was introduced.

## Unique NFH reader classification — branch stopped

`FUN_822E2858` is a four-byte `NFH\0` recognizer. Its sole caller `Function_822CC9F0` initializes a view and sets its data pointer to `leaf+0x450`. The next consumer is indirect through the data-table entry at `0x82027B0C`; exact vtable start/class/slot remain unqualified after the five-batch limit.

## NFH view table identity — CLOSED

The exact target scan finds `0x822CC9F0` in `.rdata` at `0x82027B0C` and a non-RTTI copy in `.pdata`. The real function table starts at `0x82027A64`; the target is slot `+0xA8`. Constructors `0x822CC118`, `0x822CC168` and `0x822CC200` install that table. Slot `+0xA8` recognizes `NFH\0` at `this+0x08` and writes `leaf+0x450` to `this+0x10`. Slot `+0xB0`, `0x822CC378`, is the first reader: it checks the view data and bound, then returns `this+0x10 + index*0x20`. The next boundary is the record layout.

## NFH view record consumer gate — limited

The 24 numeric `+0xB0` dispatch candidates do not statically resolve to the
NFH vtable `0x82027A64`. Their owner decompilations contain no materialized
table identity or explicit post-dispatch read of `this+0x10`; the slot offset
is shared by unrelated classes. Constructor xrefs are qualified, but receiver
provenance to a candidate is not. The gate is limited after five batches; see
`artifacts/nfh-view-record-layout-gate/BLOCKER.md`.

## NFH receiver provenance gate — limited

Les six appelants de constructeurs ont été qualifiés. Quatre chemins
installent une table dérivée puis terminent par `0x822CC118`, donc la table
NFH `0x82027A64` est vivante pour ces sous-objets; les deux autres restent sur
des tables dérivées. Les fenêtres dérivées ne contiennent pas le lecteur
`0x822CC378`, et aucun receiver n'est encore relié à un dispatch `+0xB0`.
Le dernier traceur a été invoqué avec un seul argument alors que son contrat
est `START END`; le résultat de déplacement large est non discriminant. Gate
limité après cinq lots; voir
`artifacts/nfh-receiver-provenance-gate/BLOCKER.md`.

## NFH direct `+0xB0` consumer gate — CLOSED BY REFUTATION

Le traceur corrigé (`START=0x82000000`, `END=0x8233ffff`) retrouve 24
dispatchs. Aucun retour `r3` n'est utilisé comme pointeur dans la fenêtre
bornée après `bctrl`; les quelques lectures de `r3` sont scalaires ou des
copies sans lecture de record. Les propriétaires ne matérialisent pas la
vtable `0x82027A64` et n'appellent pas directement ses constructeurs. La
branche « consommateur direct parmi ces 24 slots » est réfutée; le layout du
record reste ouvert et le prochain bord est un appel indirect plus long ou un
autre slot. Voir
`artifacts/nfh-receiver-provenance-gate/RESULT.md`.

## IB bootstrap producer static gate — CLOSED BY STATIC BOUNDARY

The generated object retains the `0x821B55C0` and `0x821B9BC8` entry points,
but their guest memory addresses are computed from `PPCContext`; no known
bootstrap address occurs in the object. The native ring captures an address
already published by the guest and contains no bootstrap allocator/producer.
The direct address-based static producer hypothesis is therefore refuted,
without proving absence of a dynamic guest writer. See
`artifacts/ib-producer-static-resume-gate/RESULT.md` and `BLOCKER.md`.

## Generated-store hook gate — CLOSED BY RUNTIME ATTRIBUTION

The bounded address watcher already exposed the needed guest PC/LR/function
metadata, but `AC6_PPC_STORE_U128` bypassed it. The U128 path now reports the
16-byte store while preserving its existing guest-byte ordering and memory
write. The codegen-enabled runtime and core test build pass, and the core CTest
passes 1/1. The existing one-tick capture attributes all 48 writes of the
bootstrap reservation to `0x821B20A0` (U32 stores, LR `0x821B212C`) and shows
the 24 PointList pairs. The remaining boundary is writer → publication →
renderer/scheduler, not writer discovery. See
`artifacts/generated-store-hook-gate/RESULT.md` and `BLOCKER.md`.

## Record writers / consumer gate — CLOSED BY STATIC P-CODE SLICE

La passe `FindScaledStoreWriters.java` du projet démo a parcouru 11 253
fonctions et retrouvé le témoin `0x820FF710` ainsi que quatre writers de
records non nuls : `0x820FF788` (type 1), `0x820FF7F8` (type 2), `0x820FFA88`
(type 3) et `0x820FFB50` (type 4). Le worker `0x820FFCA0` copie les slots et
`0x820FEFA8` dispatch explicitement ces quatre types. Le zéro du chemin type 0
n’est donc pas l’unique contrat de la file. La question restante est le type
atteint au boot et son raccord à la publication renderer/scheduler. Voir
`artifacts/ib-writer-publication-static-gate/RESULT.md` et `BLOCKER.md`.

## Record type reachability checkpoint — STATIC CORRIDOR QUALIFIED

Le slice CFG borné de `0x8210A1C0` vers `0x8210ADB4` qualifie 18 gardes et
confirme que `0x82117410` est atteignable statiquement. Le corridor exige un
état d'objet non nul, un `param_4` dans la famille `0x1102..0x1210` avec les
exclusions du P-code, puis des lookups non nuls via `0x821080D0` et
`0x826F6124`. Cela ferme la crainte d'un callsite mort, mais ne prouve pas
qu'un record type 1–4 est produit dans le plateau natif. La prochaine frontière
est une capture bornée callsite → type de slot → publication, avec observables
pré-déclarés dans `artifacts/record-type-reachability-gate/BLOCKER.md`.

## Record type route capture — OBSERVED ROUTE REFUTED; STATIC CORRIDOR UNACTIVATED

Le watcher opt-in du hook d'entrée générique a été compilé sans modifier le
code généré. Le slice statique montre que `0x820FEFA8` commute sur
`*(r3+0x40)`, pas sur `r4`. La capture START bornée (ticks 2990–3021) n'entre
ni dans `0x8210A1C0` ni dans `0x82117410`; elle observe `0x820FEFA8` 31 fois
avec `record_type=0`. La route worker observée ne publie donc aucun type 1–4
dans cette fenêtre. 3021 ticks/2913 PRESENT ne franchissent pas le frontend.
Le corridor statique reste non réfuté mais son activation est hors fenêtre.
Voir `artifacts/record-type-reachability-gate/RESULT.md` et `BLOCKER.md`.

## Paramètre du corridor record — PRODUCTEUR STATIQUE QUALIFIÉ

Le seul appel direct `0x82165CC0:0x82165D8C` transmet à `0x8210A1C0` son
huitième argument : `r8` est sauvegardé dans `r26`, puis restauré en `r6` au
site d'appel. `0x8210A1C0` calcule ensuite `param_4 = r6 & 0xFFFF` à
`0x8210A20C`. Le producteur statique est donc l'argument d'une arête
indirecte/vtable, pas une constante ou un service natif. La fenêtre
d'activation reste à trouver; aucun shim n'est justifié.
Voir `artifacts/record-type-reachability-gate/param4-producer-static-run5-summary.txt`.

## AVIObjectDemo vtable — TABLE QUALIFIÉE, RÉCEPTEUR AJUSTÉ OUVERT

`0x8200B6FC` est la vtable RTTI `AVIObjectDemo`; son slot `+4` est
`0x82165CC0`. `0x82166550` et `0x821674A8` écrivent cette vtable à `objet+0xC`.
La dispatch `0x821710FC` observée sur l'objet retourné lit au contraire la
vtable primaire `0x8200B844`; les consommateurs du pointeur global ne
montrent pas de `global+0xC` explicite. La table/slot est donc qualifiée, mais
le récepteur ajusté exact et l'activation du corridor restent ouverts. Aucun
shim env ou contrat natif n'est justifié. Référence compacte :
`artifacts/record-type-reachability-gate/avi-vtable-static-run3-summary.txt`.

## AVIObjectDemo — témoin runtime borné

Le hook d'entrée dédié est compilé et les tests core/trace passent. La fenêtre
START `2990..3021` (inputs aux ticks `3000/3001`, store frais, audio dummy)
termine à 3021 ticks / 2913 PRESENT, sans frontend, mission ou terminal. Aucun
appel n'entre dans `0x82165CC0`, `0x8210A1C0` ou `0x82117410`; l'objet généré
contient pourtant bien les hooks vérifiés par désassemblage. L'activation est
donc indécidable dans cette fenêtre, non réfutée globalement. Ne pas répéter ce
runtime; reprendre par la source statique de l'arête virtuelle et du récepteur
ajusté. Voir `artifacts/record-type-reachability-gate/avi-receiver-run1-summary.txt`.

## AVI virtual edge — candidats slot +4

Le scan Ghidra en lecture seule trouve 510 dispatchs virtuels `+4` dans le
binaire, dont 11 dans `0x82160000..0x82172000`. `0x82165D6C` est l'appel
virtuel interne de `0x82165CC0`; le `bl` direct vers `0x8210A1C0` suit à
`0x82165D8C`. Les dix autres sites locaux restent des candidats sans vtable
résolue. Les flux globaux `0x82731A30/+0xC` et `0x823CD6E0/(+0xC,+0x84)` ne
produisent aucun dispatch qualifié. Reprendre par la provenance statique de
la vtable, sans runtime répété. Voir
`artifacts/avi-virtual-edge-static-gate/RESULT.md` et `BLOCKER.md`.

## AVI virtual edge — passe constructeurs/récepteurs statiques

La passe finale confirme que `0x8200B6FC` n'est pas un pointeur de données
statique : seuls `0x82166550` et `0x821674A8` le matérialisent, à `objet+0xC`.
`0x82373090` construit `0x82731A30`; le consommateur connu
`0x821710FC` lit sa vtable primaire `0x8200B844`, pas la sous-vtable cible.
Les dix dispatchs locaux restent donc indéterminés individuellement, mais les
flux globaux/constructeurs connus ne cachent plus d'arête qualifiée vers
`0x82165CC0`. Le gate reste ouvert sur cette frontière statique; aucun shim
env, contrat natif ou runtime répété n'est justifié. Voir
`artifacts/avi-virtual-edge-static-gate/constructor-flow-final.txt`.

## AVI virtual edge — census des callers, gate limité

La seconde passe statique confirme `0x821600C8` comme receiver générique
`param_1` et ne trouve aucune fonction contenante pour `0x8216B3B4`. Les xrefs
directs vers les owners `0x82166AE0`, `0x8216E218`, `0x8216F640`,
`0x82169B38` et les deux annotations `bctrl` vers `0x821600C8` sont consignés,
mais leur vtable n'est pas propagée. Le gate est arrêté à la limite de lots,
sans runtime répété; reprendre par ces callers. Voir
`artifacts/avi-virtual-edge-static-gate/candidate-callers-run1.log` et
`BLOCKER.md`.

## AVI virtual edge — un candidat réfuté statiquement

La disassembly bornée montre que `0x821A4454` passe `0x82390574` à
`0x82169B38`; le premier mot de cette structure est `0x8200BAEC`, donc le
dispatch `0x82169B7C` ne peut pas utiliser la vtable cible `0x8200B6FC`.
Les deux annotations `bctrl` précédemment attribuées à `0x821600C8` sont des
appels via `PTR_PTR_82390034[+8]`. Les receivers state/manager restants sont
encore ouverts; aucun runtime ou shim n'est justifié. Voir
`artifacts/avi-virtual-edge-static-gate/caller-disassembly-run3.log`.

## AVI virtual edge — limite de provenance des managers

La passe ciblée révèle les receivers restants : `0x82165230` via
`FUN_82327108()+0xB5BC`, `0x82166AE0` via `FUN_82327104()+0xD274`,
`0x8216E218`/`0x8216F640` via `FUN_82327100()+0x7C`, `0x8216F7F8` via
`FUN_8232710C()` et sous-objet `+0x68`, `0x82170CD0` via
`PTR_DAT_8238FEF4`, et `0x82170F58` via `FUN_82327108()`/la route globale
primaire. Ghidra modélise `FUN_82327100`/`FUN_8232710C` comme stubs `void`
vides : la valeur de retour et sa matérialisation restent indéterminées, sans
preuve de pointeur nul. Aucun receiver n'est qualifié avec `0x8200B6FC`;
aucun shim env ni runtime répété n'est justifié. Voir
`artifacts/avi-virtual-edge-static-gate/manager-factory-summary-run5.txt`.

## AVI virtual edge — correction ABI save-helper

La passe précédente attribuait à tort `FUN_82327100/04/08/0C` le rôle
d'accessors de managers. La disassembly canonique montre des entrées contiguës
du helper ABI : `0x82327100` sauvegarde `r26`, `0x82327108` `r28`, et
`0x8232710C` `r29`; l'évidence ABI qualifiée établit que le helper préserve le
`r3` entrant. Les valeurs `iVar = FUN_8232710X()` produites par Ghidra sont
donc des artefacts de retour pour ce `r3`, pas des managers. Les receivers de
`0x82165230`, `0x82166AE0`, `0x8216E218`, `0x8216F640`, `0x8216F7F8` et
`0x82170F58` doivent être propagés depuis leurs callers. `PTR_DAT_8238FEF4`
vaut `0x82731150`; son stockage initial est zéro, donc `0x82170CD0` reste
indéterminé. Voir
`artifacts/avi-virtual-edge-static-gate/helper-boundary-correction-run8.txt`.

## AVI virtual edge — indirect table refuted as qualified vtable

La table `0x8200C5C0..0x8200C664` contient des pointeurs exécutables contigus,
dont `0x8200C624 = 0x8216F7F8`, puis des constantes non-code. Aucun alignement
testé ne possède un locator RTTI MSVC valide; les matérialisations PPC brutes
de `0x8200C5C0`, `0x8200C600` et `0x8200C624` donnent zéro hit, comme la
recherche U32 de ces adresses. `0x823270F8` est bien `std r24,-0x48(r1)`, une
entrée du helper ABI qui préserve `r3`. La table reste donc une table de
dispatch sans propriétaire objet statiquement prouvé, et ne relie aucun
receiver à `0x8200B6FC`. Le gate AVI est fermé par réfutation/indécidabilité;
il faut revenir au premier producteur de réveil observé, sans runtime répété.
Références : `artifacts/avi-virtual-edge-static-gate/indirect-vtable-provenance-run10.log`,
`indirect-vtable-owner-run10.log`, `indirect-vtable-rtti-run10.log` et
`indirect-vtable-raw-materialization-run10.log`.

## AVI virtual edge — caller propagation checkpoint

Le census direct qualifié trouve `0x82165744 → 0x82165230`,
`0x82167378 → 0x82166AE0`, `0x8216EAC4/0x8216EEC4 → 0x8216E218` et
`0x8216F920 → 0x8216F640`; aucun appel direct vers `0x8216F7F8` ou
`0x82170F58`. `0x82167320` passe son paramètre `r3` directement à
`0x82166AE0`. `0x8216EA20`, `0x8216ECE0` et `0x8216F7F8` utilisent
`0x8232710C` comme entrée ABI et conservent donc leur `r3` entrant; le dernier
passe ce receiver à `0x8216F640`. Le seul caller direct de `0x82165490` est
`0x821662CC`; cette fonction obtient le receiver de `FUN_823270F8()`, dont
l'identité ABI reste à qualifier. `0x8216F7F8` a une référence de données à
`0x8200C624`, mais sa vtable n'est pas encore reconstruite. Aucun receiver ne
porte encore une preuve `0x8200B6FC`; le prochain gate est la vtable indirecte,
sans runtime.
Références : `artifacts/avi-virtual-edge-static-gate/caller-direct-census-run9.log`,
`caller-propagation-run9.log`, `parent-caller-census-run9.log`.

## Primary counter contract checkpoint

Le gate statique du compteur primaire est fermé. L'état callback `0x82934708`
est initialisé avec `state+0x18=0` et `state+0x10=0`. Le callback live
`0x822E3EC0` incrémente `state+0x08`, puis `0x822EEE10(state+0x40,count)` écrit
le compteur publié `state+0x50` et signale `0xE000004C`. Le worker
`0x822E40E8` consomme cette valeur via les attentes `>= state+0x10` et
`> state+0x10` (`0x822E4018`/`0x822E4080`). La route d'initialisation
`0x822E52D0` réécrit aussi `state+0x10` avec `last_argument-1`; l'argument
qualifié vaut `1`, donc le seuil reste zéro. `state+0x18` ne passe à un que par
la fermeture `0x822E3E48`.

Conclusion : la chaîne wake/counter est complète statiquement; elle ne fournit
pas le producteur natif manquant. Le prochain verrou est la production du draw
normal/frontend, sans relancer le runtime à ce stade. Preuve :
`artifacts/primary-counter-static-gate/gate.status` et `RESULT.md`.

## Frontend visual producer — static gate fermé

La reconstruction native possède un `FrontendController` qui qualifie la
transition `Title -> NewGame -> Briefing -> Hangar -> Loading -> Mission`, mais
ce contrôleur ne produit ni draw-list, ni texture, ni framebuffer. De même,
`RetailFrontendResources::open` vérifie la fermeture FHM/NFH PAL et conserve
seulement des résumés de fonts; le payload NFH, les métriques et les pixels
d'atlas restent opaques. `run_play_impl` ne transmet aucun état frontend aux
renderers : il rend uniquement la scène mission (Vulkan ou CPU capture) puis
présente la cible. Aucun producteur visuel frontend n'est donc présent dans la
source non-test.

Le gate est fermé par preuve statique, sans runtime. Le verrou causal restant
est : `FHM/NFH -> décodage glyph/atlas -> draw-list Title -> present`.
Prochaine preuve discriminante : qualifier le layout d'une feuille NFH PAL et
son contrat de draw Title sur un fixture réel, sans texte synthétique ni
placeholder. Voir `artifacts/frontend-visual-producer-static-gate/RESULT.md`.

## CSwgListener `+0x20` — gate statique fermé

Le sweep RTTI du programme `Default.xex` du projet Ghidra `ace-combat-6-demo`
résout 89 dérivés de `CSwgListener`; 40 ont un handler non-stub à `+0x20`.
Cependant, le snapshot post-START de `SendMsgI("M102")` ne contient que
`CSelectMessageDlgManager` et `CModeTaskTitleDemoOffline`, dont le slot reste
`0x820AC748`. Le canal n'est donc pas mort globalement, mais aucun
implémenteur caché n'explique le plateau actif. Suite : layout NFH PAL réel et
producteur de draw Title. Preuve :
`artifacts/listener-slot20-sweep-gate/RESULT.md`.

## NFH PAL — borne structurelle fermée

Les 28 leaves NFH extraits vérifient `size = 0x450 + count*0x20`, et le
contrat PPC relie `leaf+0x450` au lecteur `base + index*0x20`. Le mot `NFH+4`
reste sémantiquement indéterminé et aucun champ n'est relié à un atlas/draw;
aucun parser natif n'est donc justifié. Reprise : l'arête indirecte du reader
de record. Preuve : `artifacts/nfh-layout-static-gate/RESULT.md`.
## NFH consumer provenance — gate fermé

- Le slice statique de reprise a réfuté que l'un des 24 dispatchs indirects au
  déplacement `+0xB0` consomme directement le record NFH `0x20` renvoyé par
  `0x822CC378`.
- La table de base qualifie `+0xB0 -> 0x822CC378`; `0x82027BE8` qualifie le
  slot homologue `+0xB0 -> 0x822CE210`, sans lien receiver→record démontré.
- Le prochain bord utile est une arête indirecte plus longue vers le lecteur
  de record/atlas ; pas de parser NFH, shim env ni runtime à ce stade.
## NFH canonical PPC accessor — gate fermé

Le slice Ghidra canonique qualifie `0x822E2858` comme reconnaisseur `NFH\0`,
son unique xref `0x822CC9F0`, l'initialisation `this+0x10 = leaf+0x450` et le
reader `0x822CC378` au stride `0x20`. Les getters `0x822CC3B0` et
`0x822CC3B8` exposent `leaf+4` comme `u16` et comme `float`. Cette preuve est
structurelle seulement : elle ne relie pas encore un record à un atlas ou à un draw
Title. Aucun shim env ni parser n'est ajouté. Voir
`artifacts/canonical-ppc-nfh-consumer-slice-gate/RESULT.md`.
## Title resource manager — gate statique fermé par réfutation

Le chemin `CModeTaskTitle` est réel, mais `0x8219EB20` ne fait que construire
une clé et `0x8219F7F8` gère un `CResourceManager` (`0x82012E1C`). Son slot
`+0x40` (`0x8219DC50`) initialise seulement deux flags; `0x822CC2A8` écrit
seulement `this+0x18`. Aucun de ces helpers ne décode NFH/FHM ni ne produit une
commande de draw. Le prochain bord est donc le child resource/view qui relie
le payload aux lecteurs `0x822CC9F0/0x822CC378`. Voir
`artifacts/frontend-compact-title-next/RESULT.md`.
## Child resource/view → NFH metrics — gate fermé

Le chemin child/view est maintenant qualifié dans le projet Ghidra canonique.
Les wrappers `0x822CC7A8`, `0x822CC8E8` et `0x822CC988` valident le child puis
appellent les slots de métriques de la classe NFH; `0x822CCD98` reste dans le
même contrat scalaire. `0x822CCCF8` ne fait qu'une copie de 0x20 octets via
`0x82327D90`, qui est un memcpy PPC optimisé. `0x821A00E8` ne fait qu'enfiler
des enregistrements de ressource utilisés par le titre.

Conclusion : cette branche atteint des métriques et des copies de records,
mais aucun atlas, draw-list, PM4, framebuffer ou present. Elle est donc
réfutée comme producteur visuel manquant. Preuve :
`artifacts/child-resource-view-next/RESULT.md`.

## First non-bootstrap RT0 writer — gate statique fermé

Le premier writer de couleur réellement qualifié est la chaîne indirecte
`0x822F84E0 -> 0x821B6708 -> 0x821B6078 -> 0x821B5B10 -> 0x821B58B0 ->
0x821B55C0`. Le champ `this+0x24` (mot couleur) traverse l'ABI via `r8`, la
pile `+0x5c`, puis `r10` du builder; le record flottant en `r6` alimente les
vertices. `0x821B55C0` émet un vrai `DRAW_INDX_2`/`0xc0003601`, raccordé au
resolve/copy RT0 déjà qualifié.

La provenance ABI est donc fermée statiquement. Il reste à démontrer que le
runtime natif appelle ce slot vtable et que la cible devient non noire; aucun
shim env ni optimisation n'est justifié avant cette vérification ciblée.
Preuve : `artifacts/rt0-writer-callers-next/RESULT.md`.

## RT0 writer — invocation runtime non atteinte, verrou amont identifié

La sonde native bornée n'observe aucun appel du slot `0x822F84E0` ni draw
rectangle/resolve : elle ne produit que les 24 PointList bootstrap à
`tick=0`, avec `surface=0`, `color_mask=0`, et expire avant d'écrire son
rapport. Le rapport natif qualifié à 3021 ticks donne le verrou causal :
`render_queue.producer=5647`, `consumer=0`, `packet_count=0`, `draw_count=24`,
`present_count=0`, alors que `vd_swap.calls=2928`.

Conclusion : l'invocation du writer n'est pas encore testable dans le chemin
actuel; le premier contrat manquant est le consommateur de la render queue,
pas un shim d'environnement ni le décodage NFH. Preuves compactes :
`artifacts/global-progress/report-index-compact.txt`,
`artifacts/native-writer-runtime/compact.txt`.

## Erratum — render queue consumer présent, payload manquant

La conclusion précédente « consommateur absent » est supersédée. Les preuves
exactes montrent que `0x820FF710` publie l'index producteur et que
`0x820FFCA0` publie puis réinitialise l'index consommateur à chaque tick. Le
snapshot `consumer=0` était pris après ce reset.

Le worker calcule `queue + index*0x60 + 0xD0`, copie le record de 96 octets et
appelle `0x820FEFA8`. Le probe RR corrigé reçoit un objet pile non nul, mais
`+0x40..+0x58` sont nuls et aucune branche de type 1–4 ne s'exécute. Les
writers statiques `0x820FF788` et `0x820FF7F8` savent publier des records de
type 1/2, mais ne sont pas atteints dans la route active. Le verrou est donc
la production du payload/menu, pas le consumer ni l'environnement.

Preuve compacte : `artifacts/render-queue-consumer-gate/RESULT.md`.

## Checkpoint — corridor payload qualifié, route active sans payload

La vtable `0x82008EF0` et le P-code qualifient les writers type 1–4; le
writer type 1 (`0x820FF788`) est atteint par l'appel indirect de
`0x82117410`, dont l'unique appelant direct est `0x8210A1C0`. Cette route est
protégée par le sélecteur `param_4` et des gardes de tables.

La capture bornée existante n'entre pas dans ce corridor : `0x820FEFA8` reçoit
31 records de type 0 et aucun type 1–4. Le contrôle de queue reste donc fermé,
mais son producteur utile n'est pas encore identifié. Reprendre par le
producteur/récepteur statique de `param_4`; ne pas ajouter de shim env, d'appel
direct writer, de parser NFH ou d'optimisation.

Preuve : `artifacts/render-queue-consumer-gate/RESULT.md` et
`artifacts/record-type-reachability-gate/RESULT.md`.

## Checkpoint 2026-08-22 — producteur du réveil `state+0x56F8`

L'entrée indirecte unique vers `0x822F85B8` est jointe à la construction
`0x822E52D0 -> 0x822F23F0 -> vtable 0x8202A488`. Son seul appel direct à
`0x821BB4C8` passe `r6=1`, ce qui initialise `state+0x56F8` à `0x0C000001`.
Le seul writer ultérieur de ce champ ne peut modifier que le bit 0; aucun
writer généré ne positionne le bit `0x4` testé par `0x821C57D0`.
`0x821C64E8`, appelé après l'initialisation, ne touche pas ce champ. Le gate
statique est donc fermé : `static_bit4_producer_absent_in_recompilation`.

Pas de patch du sélecteur, du délai, du state object, de shim env ou de `-O3`.
Le prochain bord est une qualification Ghidra binaire de l'éventuelle voie
indirecte/importée qui pourrait fournir une autre valeur de `r6`. Preuve :
`artifacts/wake-producer-gate/RESULT.md`.

## Checkpoint — table constructeur reclassée comme `.pdata`

L'occurrence `0x821BB4C8` à `0x8207DF10` est dans le bloc Ghidra `.pdata`
read-only (`0x82077200..0x82087637`). Les paires voisines sont des
`BeginAddress`/métadonnées de longueur de fonction, non une table de dispatch;
les scans de références et de branches ne trouvent aucun consommateur de ces
mots. Le faux chemin constructeur est donc fermé
(`pdata_false_alternate_constructor`). Le prochain bord reste un producteur
importé/omis hors de cette construction. Preuve :
`artifacts/wake-producer-gate/constructor-table-classification.md`.

## Checkpoint — chaîne CX360UnitManager vers le ring qualifiée

La chaîne renderer est maintenant fermée statiquement : le slot `+0x14` de la
vtable RTTI `CX360UnitManager` (`0x820A45E0`) est le seul producteur de
`(17,6)` (`0x820A4778`), `0x821ADAB8` est le seul écrivain de
`device+0x5460`, et `0x821C57D0` lit ce champ avant toute soumission normale.
Le scan frais ne trouve aucun appel direct à `0x820A45E0` ou `0x821ADAB8`;
les sept sites de construction connus de l'instance restent hors du parcours
natif. Le verrou causal est donc l'activation/construction du gestionnaire
d'unités, pas un shim env, XMA ou une optimisation.

Reprendre par la garde mission qui doit atteindre l'un des sept constructeurs,
puis par un harness borné `(17,6) → +0x5460 → soumission`. Preuve compacte :
`artifacts/cx360-unit-manager-gate/RESULT.md`.

## Checkpoint — activation reclassée derrière le provider de readiness

`0x8217C4D8` est le callback enregistré par le slot `+0x0C` des
`CModeTaskGame*`; son bras `-3` sélectionne ensuite les deux gestionnaires de
mission qui construisent `CX360UnitManager`. La construction n'est donc pas le
premier contrat manquant. Le chemin de chargement atteint par la route forcée
reste dans `0x8219DF00`/`0x8219F5D0`, en attente du provider
`0x8219AF20`/`0x82195B50`; le flag lu par `0x8217E258` n'est pas publié sur la
route observée. Le prochain travail est ce producteur de readiness, sans shim
env, appel direct renderer ni optimisation. Preuve :
`artifacts/cx360-construction-activation-gate/RESULT.md`.

## Checkpoint — le provider de readiness n'est pas le premier verrou

La capture runtime bornée du chemin recompilé a observé l'objet de chargement
`0x2E3B0040` aux ticks `4116..4123`. Après un premier état `0`, il reste en
état `1`; `0x8219AF20` est atteint à chaque poll, `0x82195B50` ne l'est
jamais, et le retour du provider passe de `0` à `1` au tick `4123`.
Le plateau demeure pourtant à `5600` ticks (`5492 PRESENT`, `23 blocked`,
`frontend=false`, `mission=false`).

Le gate « `0x8219AF20` ne termine jamais » est donc réfuté pour cette route.
Le provider est atteint et signale une fin, tandis que le chemin
`0x82195B50` est inactif. Le prochain bord est la transition post-provider et
le premier payload de rendu non-bootstrap, pas un shim d'environnement ni la
construction CX360. Preuve :
`artifacts/readiness-provider-gate/RESULT.md`.

## Correctif — retour observé limité au callback virtuel

La capture suivante qualifie précisément l'objet de travail `0x2E3B0040` :
`0x8219AF20` est atteint, `0x82195B50` reste inactif, puis le callback virtuel
`0x8219DF00` fait `+0x0C: 1→2→0` et retourne `1` au tick 4123. Le champ
`result` du watcher est donc le retour de `0x8219DF00` (LR `0x8219F64C`), pas
une lecture directe du retour de `0x8219AF20` ni de l'agrégateur `0x8219F5D0`.
La transition de l'agrégateur et de `CTaskLoading` reste à qualifier; le gate
post-provider demeure ouvert. Preuve :
`artifacts/post-provider-transition-gate/RESULT.md`.

## Checkpoint — transition aval du task qualifiée (2026-08-22)

À `tick=4123`, `sub_8218A4A0` appelle le slot virtuel 15 `0x8216CB40` de
`0x2E3D0080` (LR `0x8218A55C`), puis écrit `task+0x0C=1` au LR `0x8218A564`;
`sub_820CDF88` publie aussi `0x2E3D00E8` dans `0x826DF804`. L'hypothèse d'une
transition task absente est réfutée. Le runtime reste à `4140/4032 PRESENT`,
`23 blocked/0 runnable`, sans frontend ni mission. Le prochain bord est le
listener/callback et la reprise autour de `0xE000004C` / `0x821A8C88`, pas le
provider, le manager, un shim env ou `-O3`. Preuve :
`artifacts/readiness-consumer-gate/runtime-consumer/`.

## Checkpoint — manager de readiness identifié et drainé (2026-08-22)

`sub_8219EE40` est confirmé comme résolveur de liste. Aux ticks `4115..4123`,
la clé `0x5908E2C8` résout depuis `0x827745F0` vers le manager `0x18BB0100`.
Son layout runtime confirme `+0x20=0x18BB0120` (tête de liste) et
`+0x24=0x18BB0124` (statut); la liste est drainée puis `+0x20` devient nul et
`+0x24` passe `1→0` au LR `0x8219F6B4`. Le gate agrégateur est fermé, mais la
fin n'entraîne aucun réveil : `4140` ticks, `4032 PRESENT`, `23 blocked`,
`frontend=false`, `mission=false`. Le prochain verrou est l'aval
publication/FSM, sans shim env ni `-O3`. Preuve :
`artifacts/post-provider-transition-gate/RESULT.md`.

## Current checkpoint — transition aval du task qualifiée (2026-08-22)

À `tick=4123`, `sub_8218A4A0` appelle le slot virtuel 15 `0x8216CB40` de
`0x2E3D0080` (LR `0x8218A55C`), puis écrit `task+0x0C=1` au LR `0x8218A564`;
`sub_820CDF88` publie `0x2E3D00E8` dans `0x826DF804`. Le gate de transition
aval est fermé. Le runtime reste à `4140/4032 PRESENT`, `23 blocked/0 runnable`,
sans frontend ni mission. Prochain bord : listener/callback puis reprise de
`0xE000004C` / `0x821A8C88`, sans shim env ni `-O3`.

## Checkpoint 2026-08-22 — listener et attente primaire séparés

Le slice statique qualifié réfute la chaîne causale unique proposée. `0x826DF804`
est le second slot du tableau indexé `0x826DF800`; `sub_820E9838` consomme les
listeners via le callback `+0x20`, déjà qualifié no-op, et `sub_820EA4A8` via
le callback `+0x54`, piloté par le script. La boucle `E000004C` appartient au
worker signal/wait (`0x821A69CC` / `0x821A8C88`) et sa reprise est déjà
fonctionnelle jusqu'au premier accès guest `0x82327154`.

Aucun contrat kernel/import manquant n'est établi sur cette arête. Reprendre
statiquement par le sélecteur `param_4` de `0x8210A1C0 → 0x82117410 →
0x820FF788` et le premier record type 1–4 non-bootstrap.

## Checkpoint 2026-08-22 — gate r8/record utile bloqué

Les slices parallèles ont réfuté l’identification de `0x821710FC` comme
dispatch AVI : ce site utilise le sous-objet `CDemoDataManager`/manager
primaire, tandis que `0x8200B6FC/+4 → 0x82165CC0` reste sans owner ni
définition qualifiée de `r8`. Le worker `0x820FEFA8` lit les records 1–4 mais
aucune jonction vers la chaîne RT0/Xenos n’est établie.

Le probe codegen-ON borné à 8 000 ticks a produit 7 892 PRESENT, sans
frontend/mission, sans soumission ring (`0` paquets décodés); il ne discrimine
pas l’arête `r8` et ne justifie aucun patch. Reprise : owner/dispatcher AVI
statique, puis première émission pixel `IM_LOAD_IMMEDIATE`/`DRAW_INDX_2`.
Preuves : `artifacts/first-useful-record-r8-gate/`.

## Checkpoint — seam causal Xenia Edge exécuté (2026-08-22)

Le checkout Edge qualifié est compilé avec les hooks DTRACE/FTRACE. Le seam
opt-in observe les entrées PPC `0x82386C58`, `0x82165CC0`, `0x8210A1C0`,
`0x82117410`, puis les stores I32 bornés; le flux PM4 existant reste le seul
observateur GPU. Le test ciblé Edge passe `79/79` cas (`3723` assertions).
Trois fenêtres Edge de 90 s sur `Default.xex` PAL, avec cache neuf pour les
deux runs instrumentés, n'atteignent aucun PC nommé et n'émettent aucun PM4.
Le natif de comparaison reste à 192 PRESENT, zéro ring/PM4 et zéro owner
ciblé. Le contrat causal reste ouvert : revenir à la qualification statique
du corridor owner/r8/record avant tout correctif renderer.
Preuve : `artifacts/xenia-edge-causal-gate/RESULT.md`.

## Checkpoint — writer de provider qualifié statiquement (2026-08-22)

Le slice Ghidra démo canonique ferme le sous-gate d'activation du provider :
`0x82165490` construit la configuration, puis `0x82114F58` appelle
`0x82114798`. Ce dernier obtient le count depuis la ressource d'index 1,
alloue chaque entrée, écrit le slot `+0x04` depuis le tableau d'index 0,
installe les ressources via `0x82108918`, puis exécute la séquence
`0x8210E548`/`0x8210E5E0` et `0x82108D58`. Ce n'est donc ni une table vide ni
un état synthétique. La jonction vers l'owner AVI/r8 et vers le premier
record type 1 reste ouverte; aucun correctif runtime ou renderer n'est
justifié. Preuve :
`artifacts/static-owner-shared-gate/provider-entry-contract-decomp.log`.

## Checkpoint — validation codegen et package (2026-08-22)

Le build codegen-OFF est propre et CTest passe `27/27`; le build codegen-ON,
la cible `ac6-demo-recomp` et les 7 tests Xenos/Vulkan ciblés passent aussi.
Les quatre runners autonomes de différentielle EDRAM/pixels/padding,
certificat writeback, oracle EDRAM et screencap passent avec leurs variantes
sanitizer quand applicable. L'installation CMake vers la racine termine avec
`bin/bin` absent et le package installé ne contient aucun XEX, PAC/TBL, base
Ghidra ou fichier C++ généré. Cela ferme uniquement la lane de validation
locale; le runtime guest reste bloqué avant tout payload non-bootstrap.
Preuves : `artifacts/build-validation/`.

## Checkpoint — slices statiques owner/records fermés par blockers précis (2026-08-22)

Les passes parallèles ont qualifié trois contrats sans modifier le runtime :
`0x82165490 → 0x82114F58 → 0x82114798` peuple bien les sous-entrées et leurs
clés; `0x820FEFA8` ne rejoint pas directement PM4/Xenos et laisse ouvertes les
callbacks `[entry+0x20]`/`[entry+0x1c]`; l'owner portant `0x8200B6FC` et le
producteur de `r8` avant `0x82165CC0` restent indirects. Aucun correctif
renderer, shim ou état synthétique n'est autorisé. Prochaine passe : producteur
de la liste amont `state+0xB5B4/B5B8`, puis ces callbacks si nécessaire.
Preuves : `artifacts/goal-playable/{avi-owner,record-population,record-pm4}/RESULT.md`.

## Validation longue — audit de complexité hors lane guest (2026-08-22)

Le CTest complet borné a exécuté 88 tests : 87 succès, 4 skips attendus, et
`ac6-cpp-complexity` échoue uniquement parce que des sources d'analyse déjà
présentes sous `artifacts/` dépassent son budget. Aucun test Xenos/Vulkan,
runtime ou renderer n'a régressé; ce résultat ne justifie aucune modification
du C++ généré ni de l'audit. Preuve : `artifacts/goal-playable/ctest-refresh/`.

## Checkpoint — producteur B5B4/B5B8 qualifié (2026-08-22)

Le slice Ghidra a identifié `0x82165E68` comme writer partagé : il initialise
`DAT_826F6188`, remet `state+0xB5B4/B5B8` à zéro, puis remplit ces deux champs
depuis les clés `param_2+0x12/+0x16` via `0x8219EE40` et
`0x821EE130/0x821EE0F8`, avant de publier `DAT_826F6188+0x3B88=1`. Le
producteur d'adresse est donc fermé; les valeurs de configuration et la
publication de l'instance secondaire `0x8200B6FC`/`r8` restent ouvertes.
Preuve : `artifacts/goal-playable/state-list-producer/RESULT.md`.

## Checkpoint — owner AVI et callbacks records épuisés statiquement (2026-08-22)

Le suivi AVI confirme que seuls `0x82166550` et `0x821674A8` publient
`0x8200B6FC` à `object+0xC`; les deux dispatchs trompeurs
`0x82169B7C`/`0x821710FC` sont réfutés et les huit autres candidats restent
sans owner/vtable qualifié. Aucun `r8` ne peut donc être attribué à
`0x82165CC0`. Le suivi records borne les six callbacks data-driven
`entry+0x20`/`entry+0x1c`; leurs wrappers sont CPU-only et aucun target PM4/Xenos
ne se matérialise statiquement. Le gate reste bloqué avant tout payload guest;
aucun renderer, shim ou état synthétique n'est ajouté.
Preuves : `artifacts/goal-playable/avi-owner/RESULT-followup.md` et
`artifacts/goal-playable/record-pm4/RESULT-followup.md`.

Le groupe prioritaire `0x82160104/0x82165340/0x82167020/0x8216E310` est
également épuisé : receivers arbitraires ou offsets `+0xB5BC/+0xD274/+0x7C`,
sans arête `owner+0xC` ni définition de `r8`. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-candidate-followup.md`.

Le dernier census de configuration n'a trouvé aucun appel direct qualifié vers
`0x82165E68` ou `0x82165CC0`, aucune écriture des champs entrants
`param_2+0x12/+0x16`, et aucune vtable primaire résolue parmi les 11 dispatchs
locaux. Le contrat amont restant est donc l'objet `param_2` et son producteur
de clés; le writer et le renderer restent inchangés. Preuve :
`artifacts/goal-playable/record-population/RESULT-config-followup.md`.

## Checkpoint — import window refuted (2026-08-22)

Une fenêtre native bornée a ciblé la boucle apparente de section critique
`0x8219AF48/0x8219B0E4` sur `0x82392EB0`. Les 11,025 appels importés n'ont
produit aucun import non géré; le code invité `0x8219AF20` est un lookup borné
qui se termine, déjà observé complet par le gate de polling de ressources.
Il n'y a donc pas de correctif scheduler/lock/XAM justifié. Le blocage reste
la provenance guest de `param_2+0x12/+0x16`, de `base+0xC → 0x8200B6FC` et de
`r8` vers `0x82165CC0`. Preuve :
`artifacts/goal-playable/runtime-frontier/import-window.md`.

## Checkpoint — queue caller/selector census (2026-08-22)

Le census Ghidra canonique ne trouve aucun `bl` direct vers `0x820FF710` ou
`0x820FFCA0`; les deux sont néanmoins qualifiés comme slots indirects de la
file, respectivement producteur type 0 et worker consumer. Le corridor
`0x82165CC0 → 0x8210A1C0 → 0x82117410` et ses writers type 1/4 sont fermés
statiquement. La seule arête restante est l'owner AVI secondaire
`0x8200B6FC/+4 → 0x82165CC0` et son `r8` (dont le low16 doit sélectionner 1),
avec les cinq ressources non nulles. Preuve :
`artifacts/goal-playable/record-population/RESULT-followup-next.md`.

## Checkpoint — census AVI final (2026-08-22)

Le runner Ghidra final confirme que les deux constructeurs sont les seuls
stores qualifiés de `0x8200B6FC` à `object+0xC`, qu'aucun appel direct vers
`0x82165CC0` n'existe dans le corpus, et que les dispatchs génériques ne
résolvent pas l'owner ajusté. Le `r8` sélectionnant `param_4=1` reste donc
guest/opaque; aucun patch partagé n'est autorisé. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-final.md`.

## Checkpoint — MAIN_THREAD_PROMPT exécuté et payload guest borné (2026-08-22)

La voie Xenia Edge demandée a été instrumentée et construite dans le checkout
Edge local, avec le test ciblé passé (`79` cas, `3723` assertions). Trois
exécutions PAL qualifiées, sous `SDL_AUDIODRIVER=dummy`, ont atteint le
chargement normal mais ont expiré avant les quatre PCs nommés; aucun fichier
causal ni PM4 exploitable n'a donc été produit. Le seam reste une observation
seulement et n'est pas une dépendance native. Preuve :
`artifacts/xenia-edge-causal-gate/RESULT.md`.

Le probe natif codegen-ON borné (`--until frontend --max-ticks 3036`) a expiré
à 120 s au tick guest 1220. La file publie et consomme, mais ses 2048 slots de
96 octets restent nuls et aucun record type 1–4, owner AVI ou PM4 n'est atteint.
Cela ferme le contrat queue/import/renderer comme cause immédiate et laisse la
provenance guest de la valeur vers `0x820FF710`, l'owner `0x8200B6FC/+4` et
`r8→param_4` comme frontière active. Aucun code runtime n'a été modifié.
Preuves : `artifacts/goal-playable/runtime-frontier/RESULT-current-run.md` et
`artifacts/goal-playable/native-contract/RESULT.md`.

## Checkpoint — table indirecte séparée de l'AVI (2026-08-22)

Le slice statique de `0x8200C5C0..0x8200C664` qualifie une vtable distincte
(`0x8200C614`, constructeurs `0x8216F4A8/0x8216F500`), sans arête vers
`0x8200B6FC → 0x82165CC0`. La correction de dispatch place le `bctrl` à
`0x82165D70`; l'AVI reçoit son receiver depuis le `r4` entrant et son
sélecteur via `r8 → r26 → r6` à `0x82165D80`, puis `low16(r6)` dans
`0x8210A1C0`. Le producteur indirect de ces deux valeurs reste non qualifié;
aucune donnée ne justifie un patch. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-dispatch-table.md`.

## Checkpoint — cible titre persistante P1/P2/P0 (2026-08-22)

Le renderer Vulkan conserve désormais la cible titre 1280×720/1× entre les
draws qualifiés et utilise un render pass `LOAD` dédié pour les couches
suivantes. Le replay naturel rend les trois slots exacts du pool vertex
`P1 → P2 → P0`, puis continue cette rotation jusqu'au tick 330 sans trap.
Chaque draw touche 921600 pixels et chaque copy/writeback reste exactement
conforme au différentiel CPU/Vulkan. Les captures non noires évoluent avec la
couleur guest du fade, mais restent des aplats : les IB observés conservent le
même fetch BC3 64×64 à `0x0DF22000` et ne soumettent pas encore le logo.
Les sept tests renderer ciblés passent. `supported=false` reste inchangé.
Preuves :
`artifacts/goal-playable/title-array-writeback-runtime-20260822/` et
`artifacts/goal-playable/title-texture-mutation-frontier-static-20260822/RESULT.md`.

## Checkpoint — payload BANDAI NAMCO qualifié, draw 512 encore sans fragments (2026-08-23)

La soumission naturelle 512×512 atteint désormais le chemin renderer avec un
payload guest non nul ; son dump BC3 décodé montre le mot-symbole BANDAI NAMCO.
La capture native reste néanmoins un aplat bleu : le draw 64×64 compte
921600 fragments, le draw 512×512 en compte zéro. Une A/B `LOAD`/clear et une
A/B du mapping sampler clamp ne changent pas ce résultat. Les builds
codegen-OFF/ON et les probes bornés passent sans trap ni injection. Le
prochain seam est donc le contrat statique fetch → shader BC3 ; aucun pixel
guest n'est encore revendiqué comme visible.

Preuves : `artifacts/goal-playable/brandlogo-texture-dump-runtime-20260823/`,
`artifacts/goal-playable/brandlogo-clamp-runtime-20260823/`.

## Checkpoint — Q writer et index fetch fermés, capture native noire (2026-08-23)

La qualification SDK confirme que `xe_gpu_vertex_fetch_t` fait deux dwords :
`0x4800 + 2*94 = 0x48B8` est bien le slot 94, et son adresse `0x104A4893`
alimente Q1 `0x104A4890`. Le slot n'est donc pas mal indexé. Une trace bornée
des writers titre montre que le record naturel `type=1` de `0x82118D18`
écrit les 208 octets dans P1/P2/P0 seulement; les compteurs Q restent à zéro
et aucun store n'intersecte Q. Le draw qualifié lit alors Q1 nul, produit zéro
fragment et la capture native est noire.

Le dump guest BC3 conserve le mot-symbole BANDAI NAMCO, mais il n'est pas une
capture native. Aucun fallback P→Q, appel forcé ou pixel fabriqué n'est
autorisé. La frontière suivante est le producteur naturel des records type
4/5/6 et leur owner/publication, avec la preuve handle `0x59`/record #2 encore
à fermer.

Preuves : `artifacts/goal-playable/title-vfetch-register-index-static-20260823/RESULT.md`,
`artifacts/goal-playable/brandlogo-q-writer-trace-runtime-20260823/RESULT.md`,
`artifacts/goal-playable/brandlogo-slotfix-runtime-20260823/RESULT.md`.

## Checkpoint — owner logo réinitialisé par le guest (2026-08-23)

La trace bornée `brandlogo-owner-store2-runtime-20260823` suit directement
`0x2E3CED10` entre les ticks 225 et 240. Le chemin naturel
`0x820E5124 → sub_82322D28` remet périodiquement le curseur `owner+0xD8` à
zéro, puis `sub_82322D04` réarme `owner+0xD5`; `sub_82323BB8` republie ensuite
les états transitoires. Cette réinitialisation guest explique pourquoi
l'owner à deux frames reste bloqué à l'index zéro. Aucun changement renderer
ou pixel synthétique n'est justifié. La prochaine fermeture statique porte sur
le résultat de `sub_820D5268` et le producteur du second record logo.

Preuve : `artifacts/goal-playable/brandlogo-owner-store2-runtime-20260823/RESULT.md`.

## Checkpoint — retour resource-index qualifié (2026-08-23)

Le probe `brandlogo-resource-index-runtime2-20260823` ferme le sous-contrat
`0x820D5268`: le slot 6 renvoie `8`, le slot 21 renvoie un descripteur de type
`15`, et la branche statique choisit son champ `+4`. L'index naturel transmis à
l'owner est `0`, puis le chemin guest réinitialise de nouveau `+0xD8`; aucun
record #2/Q n'apparaît. Ne pas modifier Xenos/Vulkan. La prochaine gate est le
producteur statique du record type 4/5/6 et sa publication vers Q.

Preuve : `artifacts/goal-playable/brandlogo-resource-index-runtime2-20260823/RESULT.md`.

## Checkpoint — producteur liste/handle 0x59 non joint (2026-08-23)

La gate statique ferme la ressource `003_NTXR` et le contrat consumer, mais
pas leur publication dans le titre. `0x82323808 → 0x820D18C8 → 0x820D16A8`
est la factory/insertion SWG qualifiée ; aucun xref ne rattache encore
`0x1AA48` à l'owner `0x2E3CED10`. La bascule naturelle exige deux frames,
`+213!=0`, `+214==0`, puis une liste de trois records avec le second en type
2 et handle `0x0E000059`.

Le callback Q `0x82119048` est validé pour types 4/5/6, mais le chemin titre
connu n'en produit pas. Le renderer reste donc correctement fail-closed ; la
prochaine recherche porte sur l'énumérateur/producteur de liste, pas sur
Xenos/Vulkan.

Preuve : `artifacts/goal-playable/brandlogo-frame-owner-static-20260823/RESULT.md`.

## Checkpoint — retour factory SWG borné, payload toujours absent (2026-08-23)

La sonde bornée `swg-factory-return-runtime-20260823` confirme les retours
naturels de `0x820D18C8` à T222/T225 puis T402 : les owners enfants existent et
partagent `0x2E3C3ADC`, mais aucune liste de trois records ni handle `0x59` ne
se matérialise. La borne atteint 500 ticks et 189 presents sans frontend.
Tous les screencaps d'audit restent noirs (`rgb_nonzero=0`), donc le renderer
reste inchangé et fail-closed. Les sondes temporaires ont été retirées après
la capture ; la compilation codegen-ON et les six tests Xenos/Vulkan ciblés
passent.

Preuve : `artifacts/goal-playable/swg-factory-return-runtime-20260823/RESULT.md`.

## Checkpoint — interpréteur SWG et writer Q toujours non joints (2026-08-23)

Les deux recherches statiques parallèles ferment les faux propriétaires :
`0x820E50D8/0x820E5140` ne font que résoudre un index et modifier
`owner+213/+214/+216`, tandis que `0x82119048` reste seulement un candidat
de writer Q1. Aucun chemin qualifié ne relie `0x1AA48` à la factory, au
record #2/handle `0x59`, ni au remplissage naturel de
`[0x104A4890,0x104A4960)`. Le screencap natif reste noir ; renderer et bridge
restent fail-closed.

Preuves : `artifacts/goal-playable/swg-interpreter-static-followup-20260823/RESULT.md`;
`artifacts/goal-playable/title-vertex-writer-followup-20260823/RESULT.md`.

## Checkpoint — route record précoce négative (2026-08-23)

La trace native bornée jusqu'au tick 300 observe 32 appels du worker
`0x820FEFA8`, toujours avec `record_type=0` et `record+0x10c=0`. Aucun appel
à `0x8210A1C0`, `0x82117410` ou `0x820FFCA0` n'apparaît dans la fenêtre
190–430. Le worker titre sonde donc un record vide ; il n'existe toujours pas
de source guest qualifiée qui puisse remplir Q1. Le screencap reste noir et le
renderer ne doit pas être modifié.

Preuve : `artifacts/goal-playable/record-route-early-runtime-20260823/RESULT.md`.

## Checkpoint — matérialisation SWG toujours ouverte (2026-08-23)

La passe statique complémentaire qualifie `0x820E50D8/0x820E5140 →
0x82322D20` comme producteur du curseur/frame actif seulement. `0x82323BB8`
publie et consomme les frames existantes ; il ne crée ni liste, ni record, ni
handle. `0x823246C0` ne fait que dispatcher les opcodes vers les slots virtuels
ASContext. Le record #2/handle `0x0E000059` reste donc sans producteur qualifié
et le renderer reste inchangé.

Preuve : `artifacts/goal-playable/swg-materialization-followup-20260823/RESULT.md`.

## Checkpoint — récepteur SWG qualifié, producteur toujours ouvert (2026-08-23)

La passe statique `swg-slot4-receiver-20260823` qualifie la cellule
`0x820064E8 -> 0x820D18C8` et ses deux callsites naturels
`0x8232342C`/`0x82323594`. Ils transmettent un nouveau nœud et
`param_3`, mais n'écrivent ni `owner+0x20`, ni compteur/liste, ni record #2 ou
handle `0x0E000059`. Le producteur de `global_swg_context+0x10` et la forme de
`param_3` restent donc le seam causal. Aucun changement runtime/renderer n'est
justifié.

Preuve : `artifacts/goal-playable/swg-slot4-receiver-20260823/RESULT.md`.

## Checkpoint — slot 4 SWG exécuté, join titre toujours absent (2026-08-23)

La trace bornée `swg-slot4-trace-runtime-20260823` confirme trois passages
naturels par `0x820D18C8`, puis `0x820D0DB8 → 0x82323808`, avec descripteurs
`0x0B` et `0x01`. Le récepteur est donc vivant, mais aucun type 4/5/6, record
#2, handle `0x0E000059` ou écriture Q1 n'atteint le propriétaire titre. Le
probe finit à 300 ticks (`frontend=false`) ; renderer et screencap restent
inchangés et fail-closed.

Preuve : `artifacts/goal-playable/swg-slot4-trace-runtime-20260823/RESULT.md`.

## Checkpoint — writer du contexte SWG qualifié, join record toujours absent (2026-08-23)

La passe statique `swg-context-writer-static-20260823` ferme les seuls writers
qualifiés de `context+0x10` : initialisation via `0x820CF4E8` et teardown via
`0x820D0A00`. Le lien owner→contexte via `0x820E8B58 → 0x82321E18` est prouvé,
mais aucun raccord vers le descripteur titre, record #2, handle `0x59` ou Q1
n'est établi. Le renderer reste inchangé et le screencap reste noir.

Preuve : `artifacts/goal-playable/swg-context-writer-static-20260823/RESULT.md`.

## Checkpoint — producteur descripteur/blob SWG toujours manquant (2026-08-23)

La passe `swg-descriptor-producer-static-20260823` réfute le pseudo-hit
`0x821846A0/0x821846FC` pour `0x1AA48` : le registre est écrasé avant le
chargement, donc aucun producteur qualifié du descripteur n'est établi.
`0x82326B80` est confirmé comme constructeur de `owner+0x20`, mais le writer
du blob/list (`count`, liens, `record+0x0C`) reste inconnu. Aucun changement
renderer ou guest n'est justifié.

Preuve : `artifacts/goal-playable/swg-descriptor-producer-static-20260823/RESULT.md`.

## Checkpoint — fetch titre qualifié, couverture toujours nulle (2026-08-23)

Le probe Vulkan borné à 430 ticks accepte les profils naturels 64x64 et
512x512 et reçoit bien leurs payloads texture. Le draw titre atteint toutefois
la chaîne Vulkan/RT0 avec quatre vertices Q nuls : `passed_samples=0`,
`rgb_nonzero=0`, 921600 pixels noirs. Aucun nouveau profil renderer n'est
justifié ; la causalité reste le matérialiseur SWG/Q1 guest.

Preuve : `artifacts/goal-playable/title-fetch-trace-runtime-20260823/RESULT.md`.

## Checkpoint — writers provider absents jusqu'au tick 430 (2026-08-23)

La sonde bornée `provider-population-runtime-20260823` appelle explicitement
les writers qualifiés `0x82114350/0x82114798/0x82114A58` et leurs sélecteurs :
aucun événement n'apparaît entre les ticks 190 et 430. Le worker
`0x820FEFA8` reste à `record_type=0` et `record+0x10c=0`; aucun corridor
`0x8210A1C0/0x82117410/0x820FFCA0` n'est atteint. Le readback reste noir
(`rgb_nonzero=0`, `passed_samples=0`) malgré `texture_nonzero=86074`. Le
blocage précède donc la population provider/Q1, sans nouvelle sémantique
Vulkan/Xenos autorisée.

Preuve : `artifacts/goal-playable/provider-population-runtime-20260823/RESULT.md`.

## Checkpoint — writer B5B4/B5B8 isolé, caller non qualifié (2026-08-23)

La passe statique ciblée confirme que `0x82165E68` est le seul writer direct
de `state+0xB5B4/+0xB5B8`, après les prédicats `0x821EE130` et les lectures
`0x821EE0F8` des clés `param_2+0x12/+0x16`. `0x82165490` ne fait que les
consommer sous `state+0x119E4 == 0`; aucun caller virtuel qualifié de
`0x82165E68` ni instance concrète de `param_2` n'est établi. La frontière
guest est donc nommée précisément; aucune modification renderer/kernel n'est
justifiée.

Preuve : `artifacts/goal-playable/provider-state-fields-static-20260823/RESULT.md`.

## Checkpoint — caller qualifié, contrat `param_2` encore manquant (2026-08-23)

`0x82216498` charge le slot `+4` de la vtable primaire `0x8200B63C` et peut
donc atteindre `0x82165E68`. Il transmet pourtant `bVar6` comme second
argument, tandis que `0x82165E68` lit `param_2+0x12/+0x16`; les producteurs et
la représentation binaire de ces champs ne sont pas qualifiés. Une sonde
bornée de cette entrée est en cours; le renderer reste inchangé.

Preuve statique : `artifacts/goal-playable/caller-param2-static-20260823/RESULT.md`.

## Checkpoint — caller `param_2` non atteint dans la fenêtre frontend (2026-08-23)

La trace bornée de `0x82165E68` jusqu'au tick 440 n'observe aucune entrée
naturelle ni valeur pour `param_2+0x12/+0x16`. Elle produit toutefois le même
draw titre avec texture non nulle et Q1 nul : `rgb_nonzero=0`,
`black_pixels=921600`, `passed_samples=0`. Le wrapper a été interrompu après
la fin du processus enfant sans marqueur de statut ; cette passe est
observationnelle et ne ferme aucun gate. Le renderer et les valeurs guest
restent inchangés.

Preuve : `artifacts/goal-playable/caller-param2-runtime-20260823/RESULT.md`.
# État autoritaire courant — cycle 1796 (24 août 2026)

Le gate statique qualifie `0x82323468 -> 0x820D18C8 -> 0x82323808 ->
0x82326B80 -> 0x82323BB8` jusqu'à la sélection déterministe de la frame 0 du
nouvel enfant et au drain éventuel de `MovieMemory` vers `0x82325288`.

L'attribution de `D=0x2DD7936C` à un descripteur constant BrandLogo est
réfutée : la sélection passe par la table relocalisée du storage Title `B`.
Les mots runtime requis n'ont pas été capturés. La frontière exacte est donc
la lecture bornée de `B`, de la commande source, de `D`, de frame 0 et des
offsets VM au tick 3001.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
# Cycle 1815 — producteur couleur P1 : frontière de dispatch indirecte

Le gate statique parti de `0x82118D18` n'a pas atteint son `done_when` après
cinq lots. `PROUVÉ` : ce chunk type 1 lit le record `r4`, copie `r4+0x18` vers
les quatre couleurs P et consomme aussi `r4+0x48/+0x4C/+0x54`. Aucun autre HIR
ne référence son adresse et l'atlas canonique donne `direct_calls=[]` : la
remontée par appel direct est réfutée, une cellule/table de dispatch indirecte
reste la frontière exacte. Adresse de cellule, index/type, store vers `+0x18`
et provenance amont de `BFFF0000` restent `INCONNU`. Aucun runtime ni changement
renderer. Preuve :
`artifacts/goal-playable/title-p1-color-producer-static-20260824/BLOCKER.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
# Cycle 1816 — slots exacts du dispatch P1

`PROUVÉ` sur le `xex-basefile.bin` relié explicitement au XEX demo PAL : deux
descripteurs de `0x14` octets contiennent `0x82118D18` à `entry+0x0C`, aux
entrées `0x82009E8C` et `0x82009EB4`. Leurs tuples bruts sont respectivement
`(0000000D,09000001,826F61C0,82118D18,821187A8)` et
`(0000000D,09000003,826F61C0,82118D18,821187A8)`. Aucune branche PPC directe
ne cible le writer et aucune instruction ne construit les cellules exactes :
le consommateur charge une base et indexe la table. Callsite, règle d'index,
type de `r4`, store `r4+0x18` et provenance de `BFFF0000` restent ouverts.
Preuve :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`.
Aucun runtime ni changement renderer. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.
# Gate retail US — échec pré-codegen SSSE3 (26 août 2026)

Le manifeste US exact est préparé. L'unique tentative lourde s'arrête avec
Clang 21 dans `rexcore/memory.cpp` : `_mm_shuffle_epi8` requiert SSSE3, absent
des options du target. Aucun codegen, binaire ou run gameplay n'a suivi. La
surcouche contient le correctif CMake ciblé `-mssse3`, non rebâti dans ce gate.
PAL reste bloqué.

Preuve : `reports/retail-us-build-ssse3-boundary.md`.

# Gate retail US — Vulkan valide, synchronisation de route absente (26 août 2026)

Le correctif SSSE3 a permis l'unique génération et le build Clang 21/C++23.
Les 16 tests ciblés passent, ReXGlue Vulkan et SDL dummy sont liés, D3D12 et
`bin/bin` sont absents. La session corrigée présente le dialogue de données de
jeu sans trap fatale mais expire à 4/96 étapes : les marqueurs `type28`,
`selector44`, `state40` et campagne de la route scellée ne sont pas publiés par
le host US stock. PAL reste bloqué ; le gate courant est le cross-match Ghidra
US des quatre fonctions de synchronisation candidates. Aucun nouveau runtime
ou rebuild avant cette qualification.

Preuve : `reports/retail-us-vulkan-route-sync-boundary.md`.

# Gate retail US — observables qualifiés, route échouée à 10/96 (26 août 2026)

Le projet canonique `ghidra-projects/ac6-us` qualifie les quatre fonctions de
synchronisation et leurs étendues `.pdata` depuis le XEX US exact. Les wrappers
read-only, la carte de hooks et le relink sans codegen passent les 16 tests
statiques ; le binaire installé est `44e75813…95d30c`.

La session unique observe `type28=30`, puis `37`, mais l'état suivant est `36`
et non le `35` exigé. Résultat : 10/96 étapes, zéro capture, aucune trap/audio
fatale, arrêt non propre après l'échec de prédicat. Le gate US est fermé en
échec et PAL reste bloqué. Aucun second run, A/B ou rebuild n'est autorisé.

Preuve : `reports/retail-us-route-type35-failed-closed.md`.

# Gate retail US — frontière RT/resolve du monde noir (26 août 2026)

La route Mission 01 termine proprement avec 24 captures et sans panneau. La
cinématique rend le monde, puis le HUD apparaît sur fond noir. Au premier HUD
noir, CModeTaskGame et les phases objet/caméra/radio restent actives et le
backend reçoit ~1 200 draws et 85–87 resolves. La chute ultérieure des draws
suit l'entrée Escape/Start et n'est pas la cause initiale. Le défaut est borné
à RT/resolve/frontbuffer. Une journalisation détaillée, read-only côté invité,
est préparée mais non rebuildée. PAL reste bloqué.

Preuve : `reports/retail-us-black-world-resolve-boundary-20260826.md`.

# Gate retail US — observable resolve rebuildé (26 août 2026)

Le relink unique réutilise le codegen existant et termine avec statut 0. Le
binaire `98bf39a9…37e4e3` est validé Vulkan/SDL dummy, sans D3D12 ni `bin/bin`,
et contient les détails RT/resolve/frontbuffer requis. Aucun runtime n'est
lancé dans cette session. PAL reste bloqué jusqu'à la prochaine corrélation.

Preuve : `reports/retail-us-resolve-observable-rebuild-20260826.md`.

## Retail US — panneau diagnostics et validation bornée de `37 -> 35` (26 août 2026)

Le diagnostic 15/15 atteint `type28=37` avec `NO`, applique `Left`, capture
`YES`, valide puis atteint `type28=35`; arrêt propre et zéro fatal. Le panneau
hôte reste toutefois visible, car sa fermeture avait précédé sa création
ImGui. La fermeture a été resynchronisée au premier point de capture, mais la
tentative corrective s'est arrêtée avant tout `type28` après 1 725 `PRESENT`
et 4/15 étapes. Aucun PNG sans panneau n'a été obtenu. PAL reste bloqué.

Preuve : `artifacts/retail-us-type37-clean-20260826/RESULT.json` et
`artifacts/retail-us-type37-clean-v2-20260826/RESULT.json`.

# Gate retail US — captures `30/37/36`, route révisée non atteinte (26 août 2026)

L'extension autorisée capture les trois écrans et prouve que `37` propose la
création avec `NO` sélectionné ; `space` menait donc correctement à
l'avertissement `36`. Le `Left` inutile sur l'écran `30/OK` est déplacé après
`37`, et la route reste à 96 étapes.

La validation complète révisée ne dépasse pas le démarrage : 841 `PRESENT`,
puis arrêt des présentations, zéro marqueur `type28`, 4/96 étapes en 913 s,
aucun fatal/audio. La correction de sélection n'est donc pas encore validée en
route complète et PAL reste bloqué.

Preuve : `reports/retail-us-route-type35-failed-closed.md`.
# Gate retail US — frontière intermédiaire Vulkan préparée (26 août 2026)

Le runtime unique produit 24 captures sans panneau : cinématique 3D visible,
puis HUD sur monde noir. Les deux états conservent les mêmes resolves principal
et final et le même frontbuffer ; la cause est désormais bornée à une passe ou
ressource intermédiaire Vulkan. Le processus a dépassé la fenêtre de nettoyage
après `Execution complete` et le reçu reste donc en échec. Une sonde hôte
opt-in, agrégée et échantillonnée est prête avec 22 tests légers, sans rebuild
dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-intermediate-resolve-boundary-20260826.md.
# Gate retail US — observable frontière Vulkan validé (26 août 2026)

Le rebuild unique sans codegen termine sous cgroup avec statut 0 et installe
`b854c984…b4de0`. La validation statique passe : Vulkan complet, SDL dummy,
zéro D3D12, pas de `bin/bin`, et les trois marqueurs hôte sont liés. La sonde
et le panneau diagnostics restent désactivés par défaut. Aucun runtime n'a été
lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-vulkan-frontier-observable-rebuild-20260826.md.
# Gate retail US — sortie fragment D5B4 bornée (26 août 2026)

Le runtime unique termine proprement avec 24 captures sans panneau, zéro fatal
et le HUD noir reproduit. La transition conserve 378 draws monde D5B4, leur RT
et 65 resolves ; disparition du pass et perte globale de composition sont
réfutées. Un override blanc limité à la sortie finale D5B4 est préparé, false
par défaut, avec 23 tests légers ; il n'est pas rebuildé dans cette session.
PAL reste bloqué.

Preuve : reports/retail-us-vulkan-frontier-runtime-20260826.md.
# Gate retail US — diagnostic final-white D5B4 validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `0f208aa3…5c4c` lie la cvar et le marqueur final-white ; la
validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun `bin/bin`.
L'override et le panneau restent désactivés par défaut. Aucun runtime n'a été
lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-final-output-rebuild-20260826.md.
# Gate retail US — final-white négatif, depth/stencil borné (26 août 2026)

Le runtime final-white unique termine proprement avec 24 captures sans panneau,
zéro fatal et marqueur confirmé. Les trois HUD restent sur monde noir et le
centre vaut 0.0 : la valeur finale D5B4 n'est pas la cause suffisante. Un bypass
depth/stencil limité à D5B4 est préparé, false par défaut, avec 24 tests légers,
sans rebuild dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-final-white-runtime-20260826.md.
# Gate retail US — bypass depth/stencil D5B4 validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `d00b9022…eb1c` lie la cvar et le marqueur depth/stencil ;
la validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun
`bin/bin`. Tous les diagnostics et le panneau restent false par défaut. Aucun
runtime n'a été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-depth-stencil-rebuild-20260826.md.
# Gate retail US — depth/stencil négatif, culling borné (26 août 2026)

Le runtime depth/stencil unique termine proprement avec 24 captures sans
panneau, zéro fatal et les deux marqueurs confirmés. Le monde reste noir avec
un centre à 0.0 : le rejet Z/stencil n'est pas la cause suffisante. Un bypass
culling limité à D5B4 est préparé, false par défaut, avec 25 tests légers, sans
rebuild dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-depth-stencil-runtime-20260826.md.
# Gate retail US — bypass culling D5B4 validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `8600824e…97d1` lie la cvar et le marqueur culling ; la
validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun `bin/bin`.
Tous les diagnostics et le panneau restent false par défaut. Aucun runtime n'a
été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-cull-rebuild-20260826.md.
# Gate retail US — culling négatif, fenêtre raster bornée (26 août 2026)

Le runtime culling unique termine proprement avec 24 captures sans panneau,
zéro fatal et trois marqueurs confirmés. Le monde reste noir avec un centre à
0.0 : le culling n'est pas la cause suffisante. Le catalogue read-only est
enrichi avec viewport/scissor par passe, avec 25 tests légers, sans rebuild
dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-cull-runtime-20260826.md.
# Gate retail US — observable fenêtre raster validé (26 août 2026)

Le rebuild unique réutilise le codegen et termine sous cgroup avec statut 0.
Le binaire installé `4b4a7140…41af` lie viewport/scissor dans le catalogue de
passes ; la validation confirme Vulkan complet, SDL dummy, zéro D3D12 et aucun
`bin/bin`. L'observable est read-only, panneau et overrides false par défaut.
Aucun runtime n'a été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-raster-window-rebuild-20260826.md.
# Gate retail US — runtime fenêtre raster fermé au hangar (26 août 2026)

Le runtime unique termine techniquement proprement avec 24 captures et sans
panneau vert, mais les captures 70 à 88 sont strictement identiques au premier
hangar. La route n'a atteint ni carte, ni cinématique, ni HUD. Le filtre lourd
n'a émis aucune passe D5B4. Le harness rejette désormais ce faux positif et le
catalogue read-only inclut explicitement les frames D5B4. 26 tests légers
passent ; aucun rebuild n'a suivi le runtime. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-raster-window-runtime-20260826.md.
# Gate retail US — catalogue D5B4 inclusif validé (26 août 2026)

Le rebuild/relink unique réutilise le codegen et termine avec statut 0. Le
binaire installé `bec7412e…c75ac` inclut toute frame D5B4 dans le catalogue
viewport/scissor ; la validation confirme Vulkan complet, SDL dummy, zéro
D3D12 et aucun `bin/bin`. Le panneau et les overrides restent false par défaut.
Aucun runtime n'a été lancé dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-inclusive-catalog-rebuild-20260826.md.
# Gate retail US — startup arrêté pendant le brand movie (26 août 2026)

Le runtime unique échoue fermé à l'opération 5, sans fatal/trap ni capture de
route. Une observation bornée montre le générique 3D « PRODUCED BY NAMCO
BANDAI », sans panneau vert. Les 180 PRESENT surviennent vers 8 secondes et ne
prouvent pas le menu prêt ; l'entrée arrête la présentation avant `type28=30`.
Le harness attend désormais 45 secondes avant toute entrée. 26 tests légers
passent ; aucun rebuild n'a suivi. PAL reste bloqué.

Preuve : reports/retail-us-brand-movie-startup-stall-20260826.md.
# Gate retail US — settle 45 s manque le titre (26 août 2026)

Le runtime unique ne relève aucun fatal/trap mais émet 1 830 PRESENT puis se
fige sur une transition noire, sans `type28=30`, capture de route ou D5B4. La
fenêtre est fermée après l'observation causale et le jeu retourne 0. Le settle
après 180 PRESENT place l'entrée vers 53 s, hors de la fenêtre historique. Le
harness emploie désormais Escape à 35 s, A à 42 s, puis une attente passive.
26 tests légers passent ; aucun rebuild. PAL reste bloqué.

Preuve : reports/retail-us-brand-movie-settle-runtime-20260826.md.
# Gate retail US — fenêtre titre seule expire (26 août 2026)

Le runtime unique continue à présenter 3 499 frames mais expire après 70 s et
cinq opérations, sans `type28=30`, capture de route, fatal/trap ou D5B4. La
paire 35/42 s seule est insuffisante sur ce lancement. Le harness conserve cette
fenêtre puis applique au plus 60 s de retries conditionnels, arrêtés par l'état
invité exact ; un échec produit désormais une capture automatique. 27 tests
légers passent ; aucun rebuild. PAL reste bloqué.

Preuve : reports/retail-us-title-window-runtime-20260826.md.
# Gate retail US — D5B4 valide, composition bornée (26 août 2026)

Le runtime unique franchit startup et hangar, exécute 88/88 opérations, produit
24 captures et s'arrête proprement sans fatal/trap ni panneau. La cinématique
3D est visible, puis le HUD repose sur un monde noir. 44 échantillons D5B4 ont
des fenêtres non vides couvrant la cible pitch 640. Le resolve final vers
`0x1AB60000` persiste ; `0x1C191000` et `0x1B9C0000` passent de 7→1 et 7→4.
La prochaine frontière est statique, dans la chaîne de composition. PAL reste
bloqué.

Preuve : reports/retail-us-d5b4-window-valid-compose-boundary-20260826.md.
# Gate retail US — propriétaires Resolve qualifiés (26 août 2026)

La statique US qualifie `0x82337C68`/LR `0x82337CC4` comme premier propriétaire,
`0x8234D550`/LR `0x8234D5F4` comme dernier, et `0x821E2BB8` comme ABI Resolve.
Les records intermédiaires détaillés étaient volatils. Un catalogue hôte
read-only persiste désormais ordinal, LR, arguments et shadow state sur les
mêmes samples bornés que Vulkan. 28 tests passent ; aucun job lourd n'a été
lancé. PAL reste bloqué.

Preuve : reports/retail-us-compose-resolve-static-qualification-20260826.md.
# Gate retail US — catalogue Resolve aligné validé (26 août 2026)

Le rebuild/relink unique réutilise le codegen et termine avec statut 0. Le
binaire installé `f77554a7…572ea` lie le catalogue sample/ordinal/LR/arguments/
shadow state et passe la validation complète. 28 tests légers, Vulkan, SDL
dummy, zéro D3D12 et aucun `bin/bin` sont confirmés. Aucun runtime n'a été lancé
dans cette session. PAL reste bloqué.

Preuve : reports/retail-us-compose-resolve-catalog-rebuild-20260826.md.

# Gate retail US — divergence Resolve nommée (26 août 2026)

Le runtime unique exécute 88/88 opérations et produit 24 captures, mais échoue
fermé au teardown (`game_status=-2`). La cinématique 3D devient un HUD sur monde
noir. `0x1C191000` passe 7→1 et `0x1B9C0000` 7→4 tandis que le frontbuffer final
persiste. La couture invitée est LR `0x8234D5F4` dans `0x8234D550` : ses objets
640×360/1280×720 disparaissent au profit de 208×144/320×360. Le panneau hôte de
l'ancien artefact type37 est absent du run courant. PAL reste bloqué.

La statique précise que `0x8234D550` résout `*(r3+0)+28` sans sélectionner
l'objet. Les sélecteurs directs sont `0x8234EF60` (liste `manager+36`) et
`0x8234F558` (entrée de 304 octets indexée par `state+20`).

Preuve : reports/retail-us-compose-resolve-runtime-20260826.md.

# Gate retail US — relink owner parent échoué fermé (26 août 2026)

La statique réduit la provenance à LR parent `0x8234F0E8` (liste
`0x8234EF60`) ou `0x8234F598` (entrée indexée `0x8234F558`). Le catalogue
read-only `owner_lr` est prêt. L'unique relink échoue avec statut 1 car la
synchronisation avait retiré de la copie build le hunk legacy
`GetFrameCaptureSummary()` ; ce hunk est restauré après l'échec. Aucun second
job lourd n'est lancé, le binaire installé validé reste intact et PAL bloqué.

Preuve : reports/retail-us-compose-owner-rebuild-failed-20260826.md.

# Gate retail US — catalogue owner parent validé (26 août 2026)

Le relink unique corrigé termine avec statut 0, sans codegen. Le binaire
build/install `345dca09…e4f61`, 37 686 960 octets, passe la validation complète
et contient `owner_lr`. Le catalogue distingue désormais `0x8234F0E8` de
`0x8234F598` sans écriture invitée. Aucun runtime n'est lancé dans cette
session ; PAL reste bloqué.

Preuve : reports/retail-us-compose-owner-relink-20260826.md.

# Gate retail US — runtime owner arrêté au film de marque (26 août 2026)

Le runtime unique échoue fermé après 117,9 s et 5/88 opérations, sans
`type28=30`, fatal ou trap. La capture montre le film Namco Bandai 3D, panneau
absent ; aucun sample Mission 01 `owner_lr` n'est collecté. Les reçus réussis
antérieurs montrent que `type28=30` précède l'appui A fixe. Le harness pulse
maintenant uniquement Escape après 20 s, conditionnellement pendant 90 s, puis
réserve A à l'état invité exact. PAL reste bloqué.

Preuve : reports/retail-us-compose-owner-startup-failure-20260826.md.

# Gate retail US — runtime owner corrélé proprement (26 août 2026)

Le runtime borné sur `345dca09…e4f61` termine avec `85/85` opérations,
24 captures, `game_status=0`, `xvfb_status=0` et `clean_shutdown=true`, sans
fatal/trap. `step-77` conserve une cinématique 3D; `step-79/82/85` montrent le
HUD invité sur monde noir. Le panneau hôte est absent.

Le catalogue read-only attribue les surfaces intermédiaires à
`owner_lr=0x8234F0E8` (`0x8234EF60`) et le resolve final à
`owner_lr=0x8234F598` (`0x8234F558`), vers `0x1AB60000`. D5B4 reste soumis après
la transition noire; la frontière active est `ResolveInfo`/ownership/dump.
PAL reste bloqué.

Preuve : reports/retail-us-compose-owner-runtime-20260826.md.

# Gate retail US — `ResolveInfo` statique préparé (26 août 2026)

La chaîne active est maintenant bornée à `GetResolveInfo` →
`DumpRenderTargets` → copie EDRAM/shared memory → `MarkRangeAsResolved` →
`RequestSwapTexture`. Une instrumentation read-only
`ac6_log_resolve_info` consigne les champs source/destination et la plage
EDRAM (8 premiers resolves puis 1/256). Elle est présente dans les copies
upstream et build mais n'a pas encore été exécutée. Un seul rebuild/runtime
reste autorisé dans une session fraîche; PAL reste bloqué.

Preuve : reports/retail-us-resolve-info-static-20260826.md.

# Gate retail US — runtime `ResolveInfo`/ownership (26 août 2026)

Le runtime instrumenté termine proprement avec 85/85 opérations et 24
captures. Chaque span d'ownership échantillonné sélectionne un rectangle de
dump (`rectangles=1`), tandis que D5B4 persiste après la première capture noire.
`world_center_mean=0`; le reçu gameplay US n'est pas atteint. La couture active
est désormais `RequestSwapTexture` → `LoadTextureData`, instrumentée dans les
copies source/build par `[ac6-swap-texture]`. PAL reste bloqué.

Preuve : reports/retail-us-resolve-info-runtime-20260826.md.

# Gate retail US — texture de swap fermée, transition manager restante (26 août 2026)

Le rebuild ciblé et la validation passent avec le binaire
`bf80b852…cca56a0a`. La route render-summary atteint 85/85 opérations, 24
captures, le vol HUD, et s'arrête proprement sans fatal/trap ni panneau hôte.
Les marqueurs `[ac6-swap-texture]` restent `ok` et
`[ac6-texture-load]` montre `prepared → commit-ok` pour
`0x1AB60000+0x398000`, avec `outdated=0x1` avant chargement et `0x0` après.

La couture texture/présentation est donc réfutée comme cause suffisante. Au
sample 2460/frame 12683, le manager passe à `flags8=0x4` et HSM
`0x822EB1B0`, mais les frames conservent encore 1 600–1 850 draws et 82–87
resolves ; le passage ultérieur à `0x822E71A0` et les deltas nuls sont
postérieurs au premier noir. La frontière revient au contenu des surfaces
intermédiaires `0x1C191000`/`0x1B9C0000` avant `0x1AB60000`. PAL reste bloqué.

Preuve : reports/retail-us-texture-load-runtime-20260826.md.

# Gate retail US — sonde D5B4 reportée par faux rejet statique (26 août 2026)

La route `retail-us-d5b4-texture-runtime-20260826` s'est terminée proprement,
mais le filtre D3D12 de `validate.py` a rejeté à tort l'adresse hexadécimale
`0x00d3d120`; l'installation est donc restée sur l'ancien binaire et le
marqueur D5B4 n'a pas été consommé. Le filtre est corrigé et testé (31 tests),
le nouveau binaire `dce792bd…7691` est installé avec le marqueur. PAL reste
bloqué ; un seul runtime frais est requis, sans rebuild.

Preuve : reports/retail-us-d5b4-texture-runtime-20260826.md.

# Gate retail US — retry sonde D5B4 déplacé au point consommateur (27 août 2026)

- PROUVÉ : le runtime frais avec `dce792bd…7691` a terminé `85/85`, 24
  captures, statut 0, arrêt propre, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : les passes `[ac6-frontier-pass]` sont présentes, mais ni le hash
  pixel shader D5B4 (`D5B4F4A878949938`) ni la sonde placée après
  `RequestTextures` n'apparaissent dans le log.
- CORRIGÉ STATIQUEMENT : le marqueur read-only est maintenant placé juste
  après la construction `Ac6FrontierPass` pour le pixel shader D5B4, dans les
  copies upstream et build.
- PROCHAIN : un seul rebuild/relink sous cgroup, puis une seule route fraîche
  pour consommer la sonde. PAL reste bloqué par le reçu US de gameplay.

Preuve : reports/retail-us-d5b4-texture-runtime-retry-20260827.md.

# Gate retail US — rebuild sonde D5B4 relocalisée (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans nouvelle
  génération C++.
- PROUVÉ : validation statique pass, Vulkan/ReXGlue, SDL dummy, zéro D3D12,
  `bin/bin` absent ; binaire installé `6db8d006…0768`.
- PROUVÉ : le binaire contient `[ac6-d5b4-texture]` et le shader
  `D5B4F4A878949938`; 32 tests ciblés passent.
- PROCHAIN : une seule route `render-summary` fraîche dans une session lourde
  séparée pour établir l'atteinte du draw D5B4. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-texture-rebuild-20260827.md.

# Gate retail US — sonde texture D5B4 consommée (27 août 2026)

- PROUVÉ : route fraîche avec `6db8d006…0768`, `85/85`, 24 captures, statut
  0, arrêt propre, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : `[ac6-d5b4-texture]` atteint 3 472 fois ; les fetchs sont des
  textures tuilées format 20 (`k_DXT4_5`), `used_mask=0x1`, bases guest
  variables autour de `0x06A40000`–`0x07601000`.
- NON RÉSOLU : `world_center_mean=0` persiste après la cinématique ; ce n'est
  pas encore le reçu gameplay US.
- PRÉPARÉ : le marqueur read-only consigne maintenant masque couleur brut et
  normalisé, rasterisation et mode EDRAM pour qualifier la cible D5B4.
- PROCHAIN : un seul rebuild/relink puis une seule route fraîche. PAL reste
  bloqué.

Preuve : reports/retail-us-d5b4-texture-runtime-20260827.md.

# Gate retail US — rebuild observables masque/cible D5B4 (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans génération C++.
- PROUVÉ : validation statique pass ; Vulkan/ReXGlue, SDL dummy, zéro D3D12,
  `bin/bin` absent et 32 tests ciblés.
- PROUVÉ : binaire installé `24b8d603…9856`, avec le marqueur D5B4 étendu aux
  masques couleur brut/normalisé, rasterisation et mode EDRAM.
- PROCHAIN : une seule route fraîche en session lourde séparée ; PAL reste
  bloqué jusqu'au reçu gameplay US.

Preuve : reports/retail-us-d5b4-target-mask-rebuild-20260827.md.

# Gate retail US — masque logique D5B4 fermé (27 août 2026)

- PROUVÉ : route `24b8d603…9856`, `85/85`, 24 captures, arrêt propre,
  `game_status=0`, zéro fatal/trap et panneau hôte absent.
- PROUVÉ : 3 406 échantillons D5B4 ont tous `RB_COLOR_MASK=0xF`,
  `normalized_color_mask=0xF`, `raster=1`, `edram=4`.
- RÉFUTÉ : masque couleur logique et rasterisation comme causes suffisantes du
  monde noir.
- PRÉPARÉ : observables read-only du chemin Vulkan, de la clé render pass et
  des bits d’attachement.
- PROCHAIN : un seul rebuild/relink puis une seule route fraîche ; PAL reste
  bloqué.

Preuve : reports/retail-us-d5b4-target-mask-runtime-20260827.md.

# Gate retail US — rebuild attachement hôte D5B4 (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans génération C++.
- PROUVÉ : validation statique pass ; Vulkan/ReXGlue, SDL dummy, zéro D3D12,
  `bin/bin` absent ; ctest 16/16 et suite Python 32/32.
- PROUVÉ : binaire installé
  `b6497489…45eaa` (37 713 112 octets), avec chemin Vulkan, clé render pass
  et bits d’attachement ajoutés au marqueur D5B4.
- PROCHAIN : une seule route `render-summary` fraîche dans une session lourde
  séparée ; le reçu gameplay US manque encore et PAL reste bloqué.

Preuve : reports/retail-us-d5b4-host-target-rebuild-20260827.md.

# Gate retail US — échec startup avant D5B4 (27 août 2026)

- ÉCHEC FERMÉ : la route `render-summary` avec
  `b6497489…45eaa` s’est arrêtée à l’opération 2 (`type28=30` non atteint),
  après 119,7 s, avec `game_status=-9`, `clean_shutdown=false` et zéro
  fatal/trap.
- PROUVÉ : seuls 165 `PRESENT` du logo Bandai Namco ont été émis ; aucun
  `type28`, `selector44`, `state40`, frontier ou D5B4 n’a été atteint.
- PROUVÉ : la capture d’échec ne montre aucun panneau de diagnostic vert.
- DÉCISION : aucune conclusion renderer ni rebuild n’est autorisé sur cette
  session ; un seul retry startup avec le même binaire reste à consommer dans
  une session lourde distincte.

Preuve : reports/retail-us-d5b4-host-target-runtime-startup-failure-20260827.md.

# Gate retail US — retry host-target D5B4 (27 août 2026)

- PROUVÉ : le retry frais avec `b6497489…45eaa` a exécuté `85/85` opérations,
  produit 24 captures, terminé avec statut 0, arrêt propre et zéro
  fatal/trap.
- PROUVÉ : D5B4 est consommé 3424 fois ; toutes les lignes ont
  `render_path=0`, `render_pass=0x8D`, `attachments=0x3`,
  `RB_COLOR_MASK=0xF`, `normalized_color_mask=0xF`, `raster=1`, `edram=4`.
- RÉFUTÉ : cible/attachement hôte manquant et masque couleur comme causes
  suffisantes du monde noir.
- NON RÉSOLU : la cinématique (`step-77`) montre la 3D, mais les captures
  post-cinématique 79/82/85 restent HUD/radar sur monde noir
  (`world_center_mean=0`).
- PROCHAIN : qualification statique de la couture composition/world-content,
  puis au plus un rebuild/relink et un runtime frais si une divergence causale
  est prouvée. PAL reste bloqué.

Preuve : reports/retail-us-d5b4-host-target-runtime-retry-20260827.md.

# Gate retail US — qualification composition/world-content (27 août 2026)

- QUALIFIÉ STATIQUEMENT : `0x82337C68`/`0x82337CC4` et
  `0x8234D550`/`0x8234D5F4` conservent les destinations de composition
  canoniques ; aucun override de cible n'est présent.
- QUALIFIÉ STATIQUEMENT : la chaîne Vulkan est
  `GetResolveInfo` → `DumpRenderTargets` → compute resolve →
  `MarkRangeAsResolved`, sous `Path::kHostRenderTargets`.
- PRÉPARÉ : le marqueur D5B4 read-only couvre maintenant bases/formats
  couleur-profondeur, pitch, MSAA, viewport et scissor, dans upstream et la
  copie build ignorée.
- PROCHAIN : un unique relink/rebuild ciblé dans une session lourde neuve,
  validation statique, puis un runtime frais dans une session distincte.
  PAL reste bloqué.

Preuve : reports/retail-us-compose-world-content-static-qualification-20260827.md.

# Gate retail US — relink sonde composition/world-content (27 août 2026)

- PROUVÉ : relink/rebuild ciblé sous cgroup, statut 0, avec réutilisation de
  `generated/sources.cmake` et sans nouvelle génération C++.
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `43a35a00…e313a`, contenant la sonde D5B4 étendue aux bases/formats,
  pitch, MSAA, viewport et scissor.
- PROCHAIN : une seule route runtime fraîche dans une session lourde distincte,
  puis comparaison de la première divergence composition/world-content.
  PAL reste bloqué.

Preuve : reports/retail-us-compose-world-content-probe-relink-20260827.md.

# Gate retail US — runtime sonde composition/world-content (27 août 2026)

- ÉCHEC FERMÉ DU REÇU : la route a exécuté `85/85` et produit 24 captures,
  mais le teardown finit `clean_shutdown=false`, `game_status=-2`, sans erreur
  ni fatal/trap ; ce n'est pas un reçu gameplay.
- PROUVÉ : avant/après le noir, D5B4 conserve
  `color_base=0`, `depth_base=720`, pitch 640, MSAA 2×, render pass `0x8D` et
  attachements `0x3` ; les frames gardent une charge élevée de draws/resolves.
- NON RÉSOLU : les captures 79/82/85 restent HUD/radar sur monde noir après la
  cinématique 3D.
- PRÉPARÉ : cvar read-only `ac6_log_resolve_content` et option
  `--mission-resolve-content` pour échantillonner la plage finale
  `0x1AB60000` après readback borné.
- PROCHAIN : build/relink ciblé de cette sonde dans une session lourde neuve,
  validation statique, puis runtime séparé. PAL reste bloqué.

Preuve : reports/retail-us-compose-world-content-probe-runtime-20260827.md.

# Retail US — build sonde resolve-content (27 août 2026)

- PROUVÉ : build/relink ciblé sous cgroup, statut 0, avec réutilisation de
  `generated/sources.cmake` et sans nouvelle génération C++.
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `4f8142aa…0784a`, intégrant la sonde read-only du readback final
  `0x1AB60000` activée par `--mission-resolve-content`.
- PROCHAIN : une seule route runtime fraîche dans une session lourde séparée ;
  gameplay US non validé et PAL fermé.

Preuve : reports/retail-us-resolve-content-probe-build-20260827.md.

# Retail US — runtime sonde resolve-content (27 août 2026)

- PROUVÉ : runtime séparé terminé proprement (`exit=0`, `85/85`, 24
  captures, `game_status=0`, zéro fatal/trap).
- PROUVÉ : `step-77` conserve la 3D ; les étapes 79/82/85 restent HUD vert
  sur monde noir, sans panneau hôte.
- PROUVÉ : le contenu readback final riche tombe à `3448` octets non nuls et
  `byte_sum=6108` au moment du noir.
- QUALIFIÉ : les destinations 160×90 alternent entre `0x1AB60000` et
  `0x1B9C0000`; le fetch swap courant n'est pas encore corrélé.
- PROCHAIN : trace ciblée swap-source/texture-load/décision swap dans une
  session lourde unique ; gameplay US non validé et PAL fermé.

Preuve : reports/retail-us-resolve-content-probe-runtime-20260827.md.

# Retail US — qualification sonde resolve double-target (27 août 2026)

- QUALIFIÉ : `RequestSwapTexture` et les loads hôte restent sur la clé
  `0x1AB60000`, invalidée/rechargée avec succès ; le contenu de cette clé
  tombe à `3448` octets non nuls au HUD noir.
- PRÉPARÉ : la sonde read-only couvre désormais aussi `0x1B9C0000`, l'autre
  destination 1280×720 observée dans les `ResolveInfo` alternés.
- PROCHAIN : un seul relink/rebuild sous cgroup, validation statique, puis un
  runtime séparé pour comparer les deux buffers. Gameplay US non validé et
  PAL fermé.

Preuve : reports/retail-us-resolve-content-dual-target-static-20260827.md.

# Retail US — build sonde resolve double-target (27 août 2026)

- PROUVÉ : build/relink ciblé sous cgroup, statut 0, avec réutilisation de
  `generated/sources.cmake` et sans nouvelle génération C++.
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `9cf04b15…e06e9`, sonde read-only couvrant `0x1AB60000` et `0x1B9C0000`.
- PROCHAIN : une seule session runtime fraîche pour comparer les deux buffers;
  gameplay US non validé et PAL fermé.

Preuve : reports/retail-us-resolve-content-dual-target-build-20260827.md.

# Retail US — runtime sonde resolve double-target (27 août 2026)

- PROUVÉ : runtime séparé terminé avec `exit=0`, arrêt propre, `85/85`, 24
  captures, `game_status=0`, zéro fatal/trap, binaire
  `9cf04b15…e06e9`.
- PROUVÉ : `step-77` conserve la 3D ; les étapes 79/82/85 restent HUD/radar
  vert sur monde noir, sans panneau hôte.
- QUALIFIÉ : les deux plages `0x1AB60000` et `0x1B9C0000` contiennent des
  données pendant la transition ; le fetch et les loads de présentation
  restent fixés sur `0x1AB60000`.
- PREMIÈRE COUTURE STATIQUE : `kComputeWrite` déclare encore
  `VK_ACCESS_SHADER_READ_BIT` alors que le resolve compute écrit le storage
  buffer. Correction minimale à qualifier avant tout nouveau runtime.
- PROCHAIN : patch source miroir upstream/build, unique rebuild/relink sous
  cgroup, validation statique, puis runtime séparé. PAL reste bloqué.

Preuve : reports/retail-us-resolve-content-dual-target-runtime-20260827.md.

# Retail US — rebuild compute-write barrier (27 août 2026)

- PROUVÉ : rebuild/relink ciblé sous cgroup, statut 0, sans nouvelle
  génération C++ (`generated/sources.cmake` réutilisé).
- PROUVÉ : validation statique pass, ctest 16/16, Vulkan/ReXGlue, SDL dummy,
  zéro D3D12 et `bin/bin` absent.
- PROUVÉ : binaire installé
  `c159cbed…a1496e1`, avec `Usage::kComputeWrite` déclaré en
  `VK_ACCESS_SHADER_WRITE_BIT` dans upstream et copie build.
- PROCHAIN : runtime unique séparé avec la même route et les 24 captures pour
  vérifier la visibilité du monde ; PAL reste bloqué.

Preuve : reports/retail-us-compute-write-barrier-build-20260827.md.

# Retail US — runtime après correction compute-write (27 août 2026)

- PROUVÉ : session runtime unique terminée après `85/85` et 24 captures ;
  `xvfb_status=0`, aucune fatal/trap et journal complet jusqu'à
  `Execution complete`.
- QUALIFIÉ : `game_status=-9` et `clean_shutdown=false` sont la course d'arrêt
  du processus pendant la fermeture, pas un crash invité ; les artefacts sont
  exploitables et aucun retry de teardown n'est autorisé.
- RÉFUTÉ : le passage de `VK_ACCESS_SHADER_READ_BIT` à
  `VK_ACCESS_SHADER_WRITE_BIT` pour `kComputeWrite` ne rétablit pas le monde.
- QUALIFIÉ : `0x1B9C0000` est intermédiaire ; le dernier resolve et le swap
  restent sur `0x1AB60000`. Le propriétaire final est `0x8234F598` avec
  destination `0x828C849C`, tandis que le gestionnaire monde s'exécute.
- NON RÉSOLU : étapes 79/82/85 restent HUD vert sur monde noir ; gameplay US
  non validé et PAL fermé.
- PROCHAIN : qualification statique bornée de `DumpRenderTargets`/
  resolve-copy et de l'état shader monde autour de `0x8234F598`/
  `0x8234D5F4`, avant toute nouvelle session lourde.

Preuve : reports/retail-us-compute-write-barrier-runtime-20260827.md.

# Retail US — runtime sonde ownership MSAA (27 août 2026)

- PROUVÉ : relink/rebuild ciblé sous cgroup sans nouvelle génération C++ ;
  validation statique pass, ctest 16/16, suite Python 33/33, Vulkan/ReXGlue,
  SDL dummy, zéro D3D12 et `bin/bin` absent. Binaire installé
  `a0e9ad82…d5fbd64`.
- PROUVÉ : `PerformTransfersAndResolveClears` a émis 56 marqueurs bornés,
  séquences échantillonnées 1…10240, principalement des transferts couleur
  base 0 `k1X`↔`k4X` (valeurs 0↔2) avant la transition de campagne; aucun
  `k2X`→`k1X` du rendu monde n'a été observé.
- QUALIFIÉ : la session s'est arrêtée contrôlément à 55/85 après la capture
  `campaign-intro`, sans `PRESENT` ni `[ac6-campaign-transition]` ;
  `game_status=0`, `xvfb_status=0`, zéro fatal/trap. Ce n'est pas une
  explosion du runtime et aucune preuve Mission 01 n'est disponible dans
  cette exécution.
- DÉCISION : la sonde ownership ne justifie aucun correctif et ne déplace pas
  la couture monde noir. US gameplay non validé, PAL fermé ; reprendre en
  statique autour de `0x8234F598`/`0x8234D5F4` sans nouveau runtime, A/B,
  codegen ou relink.

Preuve : reports/retail-us-rt-transfer-probe-runtime-20260827.md.

# Retail US — expérience sauvegarde/chargement (27 août 2026)

- PROUVÉ : le scénario séparé `run_save_experiment.py` crée le conteneur
  `SAVE` dans un stockage isolé puis, dans un processus neuf, atteint
  `selector44=3 → type28=6 → type28=8 → type28=10`.
- PROUVÉ : le reçu r5 est `pass`, 18/18 opérations, quatre captures, arrêt
  propre (`game_status=0`, `xvfb_status=0`) et zéro fatal/trap.
- CONSERVÉ : `save.dat` et `not_00000000.dat` sous l'artefact r5 ; le profil
  porte un temps de campagne dans FILE 01 mais pas encore de mission, donc pas
  de checkpoint Mission 01 ni de free-mission.
- DÉCISION : cette preuve valide le mécanisme save/load sans modifier le gate
  Vulkan ; le monde 3D et PAL restent bloqués par le gate graphique.

Preuve : reports/retail-us-save-roundtrip-20260827.md.

# Retail US 2026-08-27 — capture RenderDoc D5B4 fermée sans `.rdc`

- ÉCHEC FERMÉ : la tentative unique `ac6-retail-us-renderdoc-cinematic-d5b4-20260827`
  est restée avant `type28=30`; le worker vidéo a bouclé après les premières
  présentations et le superviseur a retourné `124`.
- PROUVÉ : aucun fatal/trap/SIGSEGV, aucune PNG et zéro `.rdc`; la session ne
  qualifie ni D5B4/F556 ni l'attachement/resolve.
- DÉCISION : ne pas répéter la capture identique. Fermer d'abord
  statiquement la frontière d'injection/attente RenderDoc; gameplay US et PAL
  restent fermés.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-20260827.md.

# Retail US 2026-08-27 — injection RenderDoc différée non supportée sous Linux

- ÉCHEC FERMÉ : la variante de lancement normal puis
  `renderdoccmd inject` a atteint `cinematic-view-1` (72/96) mais l'outil
  retourne `4`, avec le message d'injection dans un processus déjà lancé non
  supportée sous Linux.
- PROUVÉ : arrêt contrôlé, zéro fatal/trap et zéro `.rdc`; la ressource D5B4
  et le point-list F556 restent non qualifiés.
- DÉCISION : restaurer le wrapper de lancement RenderDoc et ne répéter aucun
  des deux mécanismes sans outil Linux différent ou hypothèse causale nouvelle.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-injected-20260827.md.

# Retail US 2026-08-27 — RenderDoc vsync désactivé sans `.rdc`

- ÉCHEC FERMÉ : `renderdoccmd capture --wait-for-exit --opt-disallow-vsync`
  atteint `96/96`, produit 27 PNG et quitte proprement, mais `renderdoc/`
  reste vide.
- PROUVÉ : l'hypothèse d'un blocage `vkQueuePresent`/vsync au démarrage est
  réfutée; D5B4/F556 et leur resolve restent non qualifiés.
- DÉCISION : ne pas répéter le wrapper avec ou sans cette option.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-vsync-20260827.md.

# Retail US 2026-08-27 — touche RenderDoc F12 sans `.rdc`

- ÉCHEC FERMÉ : le lancement RenderDoc avec `F12` envoyé à
  `cinematic-view-2` atteint `96/96`, produit 27 PNG, quitte proprement et
  ne crée aucun `.rdc`.
- PROUVÉ : remplacer le keysym `Print` par la touche portable `F12` ne ferme
  pas la frontière de capture; aucun fatal/trap n'est observé.
- DÉCISION : fermer définitivement RenderDoc pour ce gate; toute reprise
  exige un outil Linux de capture différent ou une nouvelle hypothèse causale
  documentée.

Preuve complète : reports/retail-us-renderdoc-cinematic-d5b4-key-20260827.md.

# Retail US 2026-08-27 — sonde ordonnée post-process prête

- PROUVÉ : correctifs ReXGlue génériques SPIR-V/gamma/alpha/resolve déjà
  présents; aucun port amont justifié.
- PROUVÉ : binaire `9890d13f…f0f` relinké sans codegen, validation 16/16 et
  Python 42/42, Vulkan/SDL dummy, installation plate.
- AUTORISÉ : un seul runtime FSI, route qualifiée, sonde read-only bornée à
  30 frames de gameplay pour identifier la première transition 1B9C→1AB6
  riche vers noire. Aucun A/B, RenderDoc, override, rebuild, PAL ou codegen.

Preuve complète : reports/retail-us-postprocess-order-probe-build-20260827.md.

# Retail US 2026-08-27 — runtime post-process arrêté avant sa fenêtre

- ÉCHEC FERMÉ : 2/85, `type28=30` absent, attract cinématique visible,
  superviseur 2, zéro fatal/trap et zéro marqueur post-process.
- QUALIFIÉ : `--mission-render-summary` substituait une ancienne route 85; la
  sonde autonome conserve maintenant la route qualifiée 96/96, tests 42/42.
- PROCHAIN : après rotation imposée par trois gates lourds, un seul runtime
  `--mission-postprocess-order`; aucun rebuild, A/B, override, PAL ou codegen.

Preuve complète : reports/retail-us-postprocess-order-route-boundary-20260827.md.

# Retail US 2026-08-27 — route 96 post-process complète, frontière `2EF/F59F`

- QUALIFIÉ : route scellée `96/96`, 27 captures, 30 frames de sonde, 1045
  draws et 160 resolves qualifiés avec `9890d13f…84f0f`.
- ÉCHEC GATE : les cinq captures de contrôle conservent HUD/radar sur fond
  noir ; teardown `game_status=-9` après fermeture de fenêtre, zéro fatal/trap.
- PROUVÉ PAR ARTEFACT ANTÉRIEUR : le fallback `3ac747eb…ab2a` montre le monde
  3D dans la source `tf0` de `F59F`, mais perd le HUD ; ce n'est pas un fix.
- FRONTIÈRE : collecter `tf19`, vertex `c100/c106` et pixel `c100` du couple
  `2EF/F59F` avant tout override. PAL, codegen et A/B restent fermés.

Preuve complète : reports/retail-us-postprocess-order-route96-runtime-20260827.md.

# Retail US 2026-08-27 — build sonde exposition `2EF/F59F`

- Rebuild incrémental reçu à 0, uniquement `command_processor.cpp`; aucun
  codegen, `sources.cmake` inchangé (`c604796f…f72`).
- Validation reçue : 16/16 natifs, Python 42/42, Vulkan seul, SDL dummy,
  installation plate.
- Binaire installé : `687394ef…a63c`, 37 771 880 octets.
- AUTORISÉ : une route 96 identique, sonde read-only, pour `tf19`, vertex
  `c100/c106` et pixel `c100`; aucun A/B, override, PAL ou codegen.

Preuve complète : reports/retail-us-final-compose-exposure-probe-build-20260827.md.

# Retail US 2026-08-27 — runtime exposition propre, contenu 1×1 restant

- Reçu propre : `96/96`, 27 PNG, 30 frames, superviseur/jeu/Xvfb à 0, zéro
  fatal/trap avec `687394ef…a63c`.
- ÉCHEC GATE : `step-81-gameplay-hud.png` conserve HUD/radar sur fond noir.
- RÉFUTÉ : PS `c100.z/w=(1,1)` sur 31 draws ; les coefficients `F59F` ne
  suppriment pas la scène.
- BORNÉ : `tf19` est valide, 1×1 `k_32_FLOAT`, base `1C152000`; seule sa
  valeur finie/NaN/Inf reste à qualifier par readback exact.
- Trois gates lourds fermés : rotation avant le prochain build/runtime.

Preuve complète : reports/retail-us-final-compose-exposure-probe-runtime-20260827.md.

# Retail US 2026-08-27 — build readback `1C152000`

- Sonde bornée au resolve 1×1 exact, mot brut/`k8in32` et VS/PS producteur ;
  aucune mutation renderer ou invitée.
- Rebuild/validation à 0, 16/16 natifs, Python 42/42, aucun codegen.
- Binaire installé `43fc9068…a5fbf`, Vulkan/SDL dummy, installation plate.
- AUTORISÉ : une route 96 identique avec `--mission-postprocess-order` seul.

Preuve complète : reports/retail-us-exposure-word-readback-build-20260827.md.

# Retail US 2026-08-27 — exposition finie, `2EF/F59F` innocenté

- Premier lancement arrêté avant l'observable : route `80/96`, superviseur 2,
  zéro marqueur post-process et zéro conclusion renderer.
- Reprise à entrée identique reçue : `96/96`, 27 PNG, arrêt propre, binaire
  `43fc9068…a5fbf`, zéro fatal/trap.
- 31/31 valeurs `k8in32` finies et positives, plage `0.0334736…1.148324`,
  producteur unique `EBCCC312/2662DA78`.
- RÉFUTÉ : exposition nulle/non finie et extinction dans `2EF/F59F`.
- ÉCHEC GATE : HUD/radar reste sur monde noir ; prochain observable statique
  et read-only = contenu RGBA8 décodé par producteur des resolves `1B9C`.

Preuve complète : reports/retail-us-exposure-word-readback-runtime-20260827.md.

# Retail US 2026-08-27 — sonde RGBA8 `1B9C` préparée

- PROUVÉ : `readback_resolve=fast` lit potentiellement le slot précédent ; la
  sonde mémorise désormais le vrai frame/draw/VS/PS pour chacun des deux slots.
- DÉCODAGE : grille 32×18, `GetTiledOffset2D`, format/endian du resolve,
  métriques nonzero/somme/min/max par composant ; aucune mutation renderer.
- STATIQUE : source amont et copie de build patchées, `diff --check` propre,
  37/37 tests `test_run_gate.py`.
- PROCHAIN : rotation après trois gates lourds, puis rebuild incrémental et
  validation ; runtime encore fermé.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-static-20260827.md.

# Retail US 2026-08-27 — build sonde RGBA8 validé

- Rebuild incremental cgroup à 0 (`167/167`), pic ~7,6 GiB ;
  `generated/sources.cmake` réutilisé, SHA `c604796f…f72`, aucun codegen.
- Validation cgroup à 0 : binaire `f9cd73ff…25ce1`, 37 786 120 octets,
  ReXGlue/Vulkan, SDL dummy, D3D12 absent, installation plate.
- Suite Python complète : 42/42.
- AUTORISÉ : une seule route 96 avec la sonde RGBA8 `1B9C`, producteur associé
  au slot readback `fast`; runtime sans A/B ni override.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-build-20260827.md.

# Retail US 2026-08-27 — `1B9C` décodé, noir en aval

- Un appel de wrapper pré-créé est rejeté avant lancement (`output exists`,
  superviseur 2) ; aucune session invitée n'est comptée.
- La reprise correcte reçoit `96/96`, 27 captures, arrêt propre et zéro
  fatal/trap avec `f9cd73ff…25ce1`.
- 128 métriques RGBA8 décodées de `1B9C` : les passes `2EF/F59F`, `08DE` et
  `6E59` gardent des RGB non nuls sur 576 échantillons.
- ÉCHEC GATE : `step-81-gameplay-hud.png` reste HUD/radar sur fond noir ; le
  noircissement est postérieur à `1B9C`.
- PROCHAIN : décoder `1AB6` avec son propre couple de slots readback.

Preuve complète : reports/retail-us-1b9c-decoded-content-probe-runtime-20260827.md.

# Retail US 2026-08-27 — sonde finale `1AB6` préparée

- Deux ensembles de métadonnées indépendants sont maintenant attachés aux
  slots `1B9C` et `1AB6` en mode `fast`.
- Même grille 32×18, tiling/endian ReXGlue et métriques par composant ; aucune
  mutation renderer.
- Rebuild/validation requis après rotation ; runtime fermé.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-static-20260827.md.

# Retail US 2026-08-27 — build sonde décodée `1AB6` validé

- Rebuild incrémental cgroup à 0 (`167/167`), pic observé ~7,3 GiB ;
  `generated/sources.cmake` réutilisé (`c604796f…f72`), aucun codegen.
- Validation à 0 : 16/16 tests natifs, ReXGlue/Vulkan, SDL dummy, D3D12 absent,
  installation plate.
- Suite Python retail `pytest -q tests` : 51/51.
- Binaire installé : `603f99da…7cbf1`, 37 786 368 octets ; le marqueur
  `[ac6-postprocess-decoded]` est présent.
- AUTORISÉ : une seule route 96 `--mission-postprocess-order` pour qualifier
  `1AB6`; aucun A/B, override, PAL, codegen ou optimisation.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-build-20260827.md.

# Retail US 2026-08-27 — première route `1AB6` arrêtée avant gameplay

- La build `603f99da…7cbf1` démarre correctement sous Vulkan/SDL dummy et
  produit 22 captures jusqu'à `step-78-cinematic-view-5`.
- ÉCHEC GATE : le prédicat guest `cinematic=0 world=1 hud=1 stable=30` n'est
  pas atteint; le runner termine à 2 (`game_status=-9` au teardown), zéro
  fatal/trap et aucune ligne décodée `1B9C`/`1AB6`.
- Les captures montrent un retard d'un écran (sélection avion/arme puis
  briefing) par rapport à la route précédente; ce run ne qualifie donc pas le
  renderer et ne constitue pas un A/B.
- AUTORISÉ : une reprise strictement identique, avec le cache de cette build
  déjà parcouru, puis qualification de `1AB6` si la fenêtre gameplay s'ouvre.

Preuve complète : reports/retail-us-1ab6-decoded-content-probe-runtime-20260827.md.

# Retail US 2026-08-27 — gate Mission 01 fermé

Le binaire retail NTSC-U/J installé `d8b7b7b73b7fa0d2b98ca3eba6abdb42a77474ef8c375ccd1ff19c0d4e14fab8`
est validé sous ReXGlue/Vulkan (16/16 natifs, 51/51 Python, installation
plate). La route qualifiée `mission01-qualified-96.steps` atteint le prédicat
`cinematic=0 world=1 hud=1 stable=30`, exécute 96/96 opérations et produit
27 captures avec arrêt propre. Le reçu gameplay et l'audit v2 sont `pass`.

La correction produit maintenant la scène en remplaçant, sous garde stricte,
le swap sparse `0x1AB60000` par le fetch de composition `0x1B9C0000` du couple
`2EF9631F6325FA91/F59F21F4A1E7843E`. Le cache vertex retiré reste une correction
de résidence générale ; il n'était pas suffisant seul.

Les défauts visuels signalés restent classés ouverts et non masqués : avions
et after-effects blancs, hautes lumières ciel/eau écrêtées, plans de
cinématique pré-mission dégradés. Les ressources BC3/D5B4 sont valides ; la
prochaine correction devra être une preuve ciblée fragment/éclairage/blend ou
post-process. PAL reste une cible distincte et n'est pas promu par ce gate.

Preuve complète : reports/retail-us-gameplay-final-compose-20260827.md.

# Retail US 2026-08-28 — run M01 autorisé, timeout startup seed complet

- AUTORISÉ : un seul run runtime borné après décision explicite, sous cgroup
  `ac6-retail-us-m01-v2-auth-20260828`, avec `SDL_AUDIODRIVER=dummy`, binaire
  `b004ee70…4d70`, route `543a97ce…c5c7f` et seed complet
  `/tmp/ac6-cache-seed-v2-complete`.
- ÉCHEC FERMÉ : `2/96` après `248,723 s`, erreur
  `log predicate not reached: type28=30`, zéro capture, `game_status=-9`,
  `xvfb_status=0`, zéro fatal/trap.
- OBSERVÉ : Vulkan sélectionné, environ 5,45 GiB et 73 tâches au pic ; deux
  phases visuelles seulement (`world=0`, puis `hud=1`, `stable=0`).
- DÉCISION : route et manifeste non promus ; prochaine exécution interdite sans
  nouvelle hypothèse causale et autorisation distincte. Audit XPSO :
  en-tête/version valides, zéro hash invalide ; 193 pipelines chargés depuis le
  seed contre 164 au run v2 positif.

Preuve complète : reports/retail-us-mission01-gameplay-candidate-20260828.md ;
artefact : artifacts/retail-us-mission01-gameplay-v2-authorized-20260828/output/RESULT.json.

# Retail US 2026-08-28 — XPSO-164 atteint gameplay, contrôles nuls

- AUTORISÉ : seed `/tmp/ac6-cache-seed-v2-xpso164`, seule différence = XPSO
  164 descriptions; XSH/GLCache du seed complet inchangés.
- OBSERVÉ : route v2 `96/96`, 27 captures, `cinematic=0 world=1 hud=1
  stable=30`, centre non noir (`0.232472/0.287361/1.0`), arrêt propre,
  `game_status=0`, zéro fatal/trap.
- ÉCHEC GATE : `control_changed_pixels=0` pour pitch/roll/yaw/throttle/frein;
  reçu non promu. L'écart de 29 pipelines XPSO n'est donc pas causal pour
  l'atteinte de la frontière gameplay.

Preuve : `artifacts/retail-us-mission01-gameplay-v2-xpso164-20260828/output/RESULT.json`.

# Retail US 2026-08-28 — reverse entrée Linux fermé

- STATIQUE : `ac6_kbm_input.cpp` protège l'injection custom par `_WIN32` et
  `GetAsyncKeyState`, avec `ac6_kbm_enabled=false`; Linux reste sur le driver
  MnK stock. Bindings vérifiés: W/S pitch, A/D roulis, Q/F épaules, LMB
  accélération, RMB frein.
- CORRIGÉ : route v2 réalignée sur les bindings stock, puis settle 15 s
  post-cinématique; SHA courant
  `44c7cac85dba7f7dad42453f7232a1e5c3f44798c9f74bc5e404626d27dcc8a9`.
- CONTRÔLE : activer `REX_AC6_KBM_ENABLED=true` force `mnk_mode` off sans
  injection Linux et bloque l'étape 2 (`type28=30` absent); cette voie est
  rejetée. 60 tests Python passent.

Preuve : `artifacts/retail-us-mission01-gameplay-v2-kbm-oldseed-20260828/output/RESULT.json` ;
rapport : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — settle post-cinématique et screenshot

- La route courante attend 15 s après `Space` avant `Escape`; SHA
  `44c7cac85dba7f7dad42453f7232a1e5c3f44798c9f74bc5e404626d27dcc8a9`.
- Le test settle-15 + seed complet a expiré au startup à l'étape 2
  (`type28=30` absent, `257,571 s`, zéro capture, zéro fatal/trap):
  `artifacts/retail-us-mission01-gameplay-v2-stock-settle15-20260828/output/RESULT.json`.
- Le run complet précédent a atteint `96/96`, `stable=30`, monde non noir et
  27 captures; contrôles encore identiques (0 pixel):
  `artifacts/retail-us-mission01-gameplay-v2-stock-complete193-20260828/output/RESULT.json`.
- Capture utilisateur: `artifacts/retail-us-mission01-gameplay-v2-stock-route-20260828/output/step-40-language.png`.
- Gate M01 v2, débrief/save et missions 02–15 restent fermés; prochaine
  frontière = premier consommateur du paquet MnK stock pendant `FlightActive`.

# Retail US 2026-08-28 — consommateur MnK stock qualifié statiquement

- IDENTITÉ : projet `ghidra-projects/ac6-us`, `default.xex`, XEX
  `6eefba42…67cbbbc`; generated source du même target NTSC-U/J.
- PROUVÉ : `MnkInputDriver::GetState` écrit boutons, triggers et quatre axes
  dans `X_INPUT_STATE`; `XamInputGetState_entry` transmet le paquet à
  `InputSystem` sans supprimer les champs analogiques.
- PROUVÉ : le guest US `0x8234CEB8` passe `r31+68` à `0x82390CE0` (LR
  `0x8234CEE0`), puis `0x8234CE40` lit boutons/axes et `0x8234CC38` lit les
  triggers. Le premier consommateur guest n'est donc pas le point de perte.
- BORNÉ : le logger read-only positif atteint 96/96 et observe les LR XAM et
  boutons, mais ne sérialise pas les axes; le record longstart n'a pas finalisé
  son fichier; l'attach GDB est refusé par `ptrace_scope`.
- DÉCISION : ne pas patcher guest/renderer. Le prochain runtime unique doit
  corréler événement X11, paquet stock non nul, heartbeat de vol et phase
  visuelle; ensuite seulement débrief/save et missions 02–15.

Preuve complète : `reports/retail-us-mission01-gameplay-candidate-20260828.md`.

# Retail US 2026-08-28 — limite trace GDB

- Une tentative unique a lancé le même binaire comme enfant GDB pour éviter
  `ptrace_scope`; breakpoint `MnkInputDriver::GetState` installé.
- ÉCHEC OUTIL : `SIGSEGV` guest à `rex_sub_821E4378` avant tout hit, route
  `1/96`, environ 10 s, reçu `artifacts/retail-us-mission01-mnk-gdb-start-20260828/output/RESULT.json`.
- DÉCISION : aucun état MnK n'est déduit de ce run; mode GDB perturbateur
  abandonné. La qualification statique `GetState -> XAM -> 0x8234CEB8 ->
  0x8234CE40/CC38` reste la preuve active.

# Retail US 2026-08-28 — trace packet stock bornée non concluante

- Une seule session read-only a utilisé le wrapper XAM qualifié avec le binaire
  `c225f5e6…fc08923`, la route historique `6ef77bef…14dac4b`, le seed positif
  et `SDL_AUDIODRIVER=dummy`, sous le cgroup
  `ac6-retail-us-m01-inputstate-log-20260828`.
- Reçu `artifacts/retail-us-mission01-stock-input-state-log-positive-route-20260828/output/RESULT.json`:
  `status=fail`, `2/96`, `249,431 s`, `type28=30` absent, une capture
  cinématique, `clean_shutdown=false`, `game_status=-9`, zéro fatal/trap.
- Seulement deux sites XAM (`0x8234CFA4`, `0x8234CEE0`) ont été journalisés;
  zéro paquet `xinput user=... state`, axe, trigger ou heartbeat de vol.
  La fenêtre est fermée; aucune répétition identique, correction renderer ou
  promotion gameplay n'est permise.

# Retail US 2026-08-28 — instrumentation host réfutée, baseline propre échoue

- Le probe host-side (binaire `68090b0494f42de96b4b8a5784216ce0c26df3bcd74741f5c74c15c5356919c7`)
  n'a produit aucun état : `InputSystem::GetState` renvoie `0x48F` sans
  périphérique, puis `0x82390CE0` synthétise le paquet en mémoire guest. Reçu :
  `artifacts/retail-us-mission01-stock-input-host-log-positive-route-20260828/output/RESULT.json`;
  `2/96`, `253,028 s`, `PRESENT=1829`, capture noire 1-bit, zéro fatal/trap.
- Patch host retiré; rebuild/validate propre installé
  `a279d55180226496b4d2b70194e13a71e122a678d87842944db7e7cc50c2c95c`
  (37 803 744 octets), `bin/bin` absent.
- La baseline propre avec même route/seed échoue encore :
  `artifacts/retail-us-mission01-baseline-clean-positive-route-20260828/output/RESULT.json`,
  `2/96`, `250,685 s`, `PRESENT=1863`, phase `world=0/hud=1`, capture
  cinématique, `clean_shutdown=false`, `game_status=-9`, zéro fatal/trap.
- Décision : hypothèse « instrumentation cause le timeout » réfutée; ne pas
  répéter la route. Divergence `stock/cache/état -> type28` reste ouverte et
  doit être réduite statiquement avant tout nouveau runtime.

# Retail US 2026-08-28 — cache chaud XPSO-193 progresse, gate visuel fermé

- Une seule session bornée avec baseline propre `a279d551…c95c`, route
  historique `6ef77bef…dac4b`, seed `stock-complete193` et cgroup
  `ac6-retail-us-m01-warmcache193-a279-20260828` a chargé 193 pipelines,
  ajouté un 194e record et exécuté 96/96 pas (`PRESENT=16046`, `type28=94`).
- OBSERVÉ : phases `cinematic=0 world=1 hud=1 stable=30` puis
  `cinematic=0 world=0 hud=1`; captures 70/73 montrent le monde 3D mais
  fortement écrêté/blanc. Les captures de vol 77/79/80/84/87/90/93/96 sont
  identiques; cinq contrôles à 0 pixel.
- ÉCHEC : reçu
  `artifacts/retail-us-mission01-warmcache193-a279-20260828/output/RESULT.json`,
  `status=fail`, `clean_shutdown=false`, `game_status=-11`, sans fatal/trap.
  Le journal finit après `Window closing`, worker audio arrêté,
  `TerminateTitle` et `Execution complete`; le signal est post-teardown,
  non attribué au guest.
- DÉCISION : cache chaud retenu comme précondition de progression, pas comme
  fix. Aucun run identique. Gate M01 v2, débrief/save et missions 02–15 restent
  fermés; prochaines frontières statiques = événements GTK/X11 -> `has_focus_`,
  ordre de destruction post-`Execution complete`, producteur postprocess blanc.

# Retail US 2026-08-28 — parcours original confirme le seam XAM stock

- Une session diagnostique unique a utilisé `mission01-qualified-96.steps`
  (`771a77a8…b6043`), baseline `a279d551…c95c`, cache chaud XPSO-193 et
  `--mission-stock-input-log`, sous cgroup
  `ac6-retail-us-m01-route-original-inputlog-20260828`.
- Reçu :
  `artifacts/retail-us-mission01-route-original-inputlog-20260828/output/RESULT.json`;
  `diagnostic-capture-ready`, `96/96`, `11717 PRESENT`, `94 type28`,
  `cinematic=0 world=1 hud=1 stable=30`, 27 captures, arrêt propre et zéro
  fatal/trap.
- Le wrapper XAM `0x82390CE0` voit `A=0x1000`, `START=0x0010` et
  `LB=0x0100` sur les deux LR qualifiés; la livraison GTK/X11 -> seam n'est
  donc pas totalement perdue. Ce logger ne couvre pas axes/triggers.
- Les frames de vol varient (avion visible en pitch/roll, noir en yaw, gris en
  brake), mais le parcours reste diagnostic et le HUD de référence est noir;
  aucune causalité contrôle/renderer n'est promue. Ne pas répéter cette
  route/seed; garder `FlightActive`, postprocess et teardown `-11` ouverts.

# US full-native Linux — profil cible et reprise 2026-08-29

- NTSC-U/J devient la priorité produit Linux AMD64 ; PAL reste une identité
  séparée et aucune preuve n'est fusionnée.
- Les identités XDVDFS du XEX, de `DATA.TBL`, `DATA00.PAC`, `DATA01.PAC` et des
  packs médias sont qualifiées dans
  `analysis/oracle/ac6-recomp-ab90b-us/content-identity.json` sans octets
  retail committés.
- Le runtime manuscrit expose désormais `RetailTarget::{Pal,NtscUj}`, policies
  de contenu/média dédiées, mappings sélectionnés par cible et cache US séparé.
- Build Linux/Ninja, CTest ciblé, pytest identité/retail et audit de frontière
  passent; l’audit de cache sélectionne aussi l’identité et les ressources M01
  par cible. Le palier retail ReXGlue reste bloqué sur la preuve M01 visible et
  contrôlable ; aucune optimisation ni promotion gameplay.

# Retail US 2026-08-30 — composition host réfutée, postprocess ouvert

- Le cache qualifié 193 permet `type28=30`, campagne `0→1→2` et route 96/96.
- Composer `1B9C + 1AB6` avant gamma donne `mean=0,996537`, `stddev=0,0197882`
  et HUD vert `0,0` : l'hypothèse presenter est réfutée.
- Le fallback expérimental est retiré ; rollback build/validate passe.
- Aucun reçu gameplay/HUD promu. Gate courant : perte postprocess guest entre
  tone-map, HUD/UI et resolve final `1AB6`.
# 2026-08-30 — `1B9C → 0311`, sampler réfuté

Les traces 1AB6 prouvent que le HUD est conservé après `0311`; la scène est
déjà noire avant HUD. Le cache prépare bien les reloads `1B9C`. F59F et 0311
partagent la même ressource ; l’A/B scoped point/linéaire n’améliore pas
l’image et a été retiré. Gate actif : contenu de l’image Vulkan après upload
`1B9C`, juste avant `0311`. Rapport courant :
`reports/retail-us-hud-layer-compose-20260830.md`.
