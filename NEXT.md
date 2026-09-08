# AC6 retail NTSC-U/J — Gate 2 runtime natif

0. **r414 — correctif appliqué et vérifié en direct : `sub_821FA9E0` retourne désormais le bon pointeur (nouveau script `apply_sub_821fa9e0_leave_return_fix.py`, sauvegarde/restauration de `ctx.r3` autour de l'appel `RtlLeaveCriticalSection` de r366). Rejoué SANS MODIFICATION le harnais `r424_return_chain.gdb` : les trois étages (`sub_821FA9E0`/`sub_823857E0`/`sub_8237FA50`) propagent maintenant `0x100015a0` au lieu de `0`, 3/3 croissances capturées. Signal indirect fort : le compteur alloc/free combiné passe de `883` à `67239` sur le même run — le tableau croissant statique C++ grandit désormais normalement au lieu de déborder silencieusement (PAS un blocage qualifié).**
   Processus reconstruit (`ninja ac6recomp`) et testé stable (sortie
   normale, aucun crash introduit). Voir
   `reports/ac6-retail-native-codegen-gate2-r414-return-value-fix-applied-and-verified-live-20260908.md`.
   **Nommé pour r415** : rechercher si un double-octroi équivalent à
   `seq≈874`/`877` (r401-r409) se produit encore ailleurs dans la
   séquence désormais bien plus longue (`67239` événements), ou si ce
   correctif ferme réellement le blocage r399 original.

1. **r413 — le commit manquant de r411 EXPLIQUÉ : le correctif INVENTÉ de r366 (`tools/apply_sub_821fa9e0_leave_fix.py`, fuite de section critique, 2026-09-07) réutilise `ctx.r3` comme registre de travail pour l'appel `RtlLeaveCriticalSection` SANS sauvegarder/restaurer la valeur de retour qu'il vient de recharger deux lignes plus haut — `sub_821FA9E0` retourne donc `0` au lieu du nouveau pointeur `0x100015a0` à chaque fois que le verrou a été pris (systématiquement, pour toute croissance réelle). CE N'EST PAS un bug retail : c'est une régression introduite par ce correctif appliqué à la recompilation elle-même (PAS un blocage qualifié).**
   `r424_return_chain.gdb` capture les trois étages en un seul run :
   `sub_821F9E10` retourne correctement `0x100015a0` ; `sub_821FA9E0`
   (le `realloc()` qui l'englobe) retourne `0x0` ; `sub_823857E0` et
   `sub_8237FA50` propagent fidèlement ce `0` déjà perdu. Désassemblage
   x86 statique (`sub_821FA9E0+2637`..`+2678`) localise l'instruction
   exacte : le rechargement correct (`lwz r3,356(r31)`, `+2637`) est
   immédiatement suivi, si le drapeau "verrou tenu" est mis, par
   `add $0x580,%ebp; ...; mov %rax,(%rbx)` (`+2657`..`+2669`) qui
   écrase `ctx.r3` avec le pointeur de section critique
   (`r27+1408=r27+0x580`, le même offset que `RtlEnterCriticalSection`)
   avant `call RtlLeaveCriticalSection` — jamais restauré ensuite. Voir
   `reports/ac6-retail-native-codegen-gate2-r413-missing-commit-explained-r366-invented-fix-clobbers-its-own-return-value-20260908.md`.
   **Nommé pour r414** : appliquer et vérifier en direct un correctif
   qui sauvegarde `ctx.r3` avant le bloc `if` et le restaure après
   `RtlLeaveCriticalSection`, confirmer par capture live que
   `sub_821FA9E0` retourne désormais le bon pointeur, que
   `sub_8237FA50` commet `begin`, et mesurer si le débordement
   r410/r411/r412 disparaît sur un run complet.

1. **r412 — cause racine confirmée au niveau instruction : `sub_821F90A8` (requête de capacité, sous `sub_82385AF0`) retourne délibérément `-1` (sentinelle "libéré") une fois le tampon rendu au tas ; `sub_8237FA50` compare ce retour au besoin avec `cmplw` (NON SIGNÉ) — `-1` devient `0xFFFFFFFF`, "capacité maximale", et la revérification de capacité est désactivée pour toujours après la première libération. Capturé en direct sur 188 requêtes consécutives : `0x80`(=128, correct) tant que le bloc est marqué en cours d'utilisation, `-1` systématiquement après (PAS un blocage qualifié).**
   `r422_capacity_query.gdb` — lecture CORRIGÉE (la valeur de retour
   de `sub_821F90A8` est stockée dans `ctx.r3` en mémoire, PAS laissée
   dans `%rax` au `ret` x86 ; une première tentative lisant `$rax`
   donnait systématiquement `0`, contredisant la dérivation depuis le
   code source — désassemblage x86 statique a montré la vraie
   convention). C'est ce défaut, pas le saut de commit de `begin`
   documenté par r411, qui rend le débordement PERMANENT : même si le
   commit avait fonctionné et fait pointer le tableau vers le nouveau
   tampon (`0x100015a0`), sa PROCHAINE libération aurait déclenché
   exactement le même défaut de sentinelle. Voir
   `reports/ac6-retail-native-codegen-gate2-r412-root-cause-nailed-negative-one-sentinel-read-as-unsigned-max-20260908.md`.
   **Nommé pour la suite** : relire `loc_8237FAF0` (le saut de commit
   non expliqué de r411) reste ouvert mais n'est plus bloquant pour
   comprendre le débordement lui-même ; déterminer si ce défaut existe
   dans le binaire retail réel (aucun oracle utilisé pour ce cycle) ou
   proposer/qualifier un correctif est la prochaine décision (précédent
   r1111/r1113 : ne pas deviner sans preuve).

1. **r411 — le commit du redimensionnement du tableau croissant ne s'exécute JAMAIS : `begin` (`0x82a5eef0`) n'est écrit qu'UNE SEULE FOIS dans tout le run (`seq=2`, création), jamais après — y compris après une croissance réelle et confirmée en direct (`alloc(256o)->0x100015a0`, `free(0x10000770)`, `seq=5`/`6`). Le tableau continue ensuite d'écrire indéfiniment dans son tampon déjà libéré (`end` avance de 4 octets à chaque appel, 17 déclenchements consécutifs captés, `seq` figé à `6` — aucune nouvelle allocation ne se produit) : un débordement de tas NON BORNÉ, pas un simple use-after-free ponctuel. CORRIGE/PRÉCISE r410 (la lecture complète de `sub_821FA9E0` réfute l'hypothèse d'un bug dans le `realloc()` lui-même — il est textuellement correct) (PAS un blocage qualifié).**
   `r419_grow_alloc_ret.gdb` capture l'allocation interne en direct
   (entrée ET retour) : `seq=5`, `r5(size)=0x100`, pile
   `sub_821F9E10<-sub_821FA9E0<-sub_823857E0<-sub_8237FA50<-
   sub_8237FB58<-sub_821F7B28<-__xstart`, retour `r3=0x100015a0` — une
   adresse neuve, distincte. `r421_begin_full_history.gdb` (point
   d'observation matériel sur `begin` armé dès l'entrée du
   constructeur, run complet jusqu'à `total-seq=883`) montre que
   `0x100015a0` n'est JAMAIS stocké dans `begin` — le chemin de commit
   `loc_8237FAF0` (texte cité par r410) n'exécute pas malgré un retour
   non nul confirmé. `r420_globals_watch.gdb` montre que `end` continue
   ensuite d'avancer de 4 octets par appel, sans aucun nouvel
   alloc/free, jusqu'à au moins `0x10000834` (17 déclenchements) —
   la capacité n'est jamais revérifiée avec succès. Voir
   `reports/ac6-retail-native-codegen-gate2-r411-grow-commit-never-executes-array-overflows-its-freed-buffer-forever-20260908.md`.
   **Nommé pour r412** : lire `sub_82385AF0` (requête de capacité) en
   entier, ou isoler par point d'arrêt x86 la branche exacte
   `cmplwi r3,0`/`bne` juste après le premier `bl sub_823857E0` de
   `sub_8237FA50` pour capturer en direct `cr0`/`r3` à cet instant
   précis lors de l'épisode `seq=5`/`6`.

1. **r410 — l'écrivain de `0x100007f0..+0xc` n'est PAS un sous-système sans rapport : c'est le tas général lui-même, agissant comme client de sa propre API. `sub_8237FA50` (`push_back` d'un tableau croissant global, appelé depuis la boucle des constructeurs statiques C++, `sub_821F7B28`) libère son propre tampon (`free(0x10000770)`, pile d'appel exacte capturée : `sub_821FA6F8<-sub_821FA9E0<-sub_823857E0<-sub_8237FA50<-sub_8237FB58<-sub_821F7B28<-__xstart`) puis continue d'écrire à travers un pointeur de fin resté périmé — un use-after-free interne à un agrandissement de tampon, pas une corruption externe. CORRIGE la version précédente de r410 elle-même (« sans rapport avec l'allocateur », réfutée par la lecture directe du code PPC de `sub_8237FA50`/`sub_821F7B28`) (PAS un blocage qualifié).**
   `0x10000770` = exactement le pointeur retourné par `seq=2` (r409,
   128 octets) ; le tampon du tableau croissant global (globales de
   contrôle invité `0x82a5eef0`/`0x82a5eeec`) EST ce bloc. `end` du
   tableau (`0x82a5eeec`) atteint la borne de capacité exacte
   (`0x10000770+0x80=0x100007f0`) puis la dépasse d'un mot à chaque
   appel, tandis que `begin` (`0x82a5eef0`) reste bloqué à `0x10000770`
   sur les quatre écritures capturées — alors qu'un `free()` de ce même
   pointeur vient d'avoir lieu dans le MÊME appel. Ceci ferme la boucle
   avec r409 : `loc_821FA288` ne fait que propager une valeur déjà
   écrite par ce use-after-free. Voir
   `reports/ac6-retail-native-codegen-gate2-r410-mechanism-traced-to-x86-neighbor-coalesce-merges-live-block-20260908.md`.
   **Nommé pour r411** : lire directement le code PPC de
   `sub_823857E0`/`sub_821FA9E0` pour trancher entre registre `r30`
   périmé après l'appel, ou `sub_823857E0` qui échoue à agrandir
   réellement le tampon avant de libérer l'ancien.

1. **r409 — origine réelle localisée en direct : une découpe à `seq=8` (alloc de 80 octets) écrit un reliquat de `0x823f` unités là où `3` étaient dues, créant un nœud fantôme de 533 Ko `[0x100007c0,0x10082bb0)` — l'adresse même au centre de r399-r408. Première conséquence concrète confirmée : chevauchement mémoire à `seq=284`, 590 appels avant la chaîne r399. CORRIGE r408 (`seq=348` était une écriture de comptabilité intermédiaire pendant un free ordinaire, pas un free anormal) (PAS un blocage qualifié).**
   **CORRIGÉ PAR r410 CI-DESSUS** : l'affirmation « l'erreur naît
   pendant le traitement de la découpe elle-même, pas d'un en-tête déjà
   corrompu » ci-dessous est fausse — r410 (`r414_header_trace.gdb`) a
   montré que l'en-tête était DÉJÀ garbage à `seq=8`-pre, avant même que
   `sub_821F9E10` n'exécute `loc_821FA288`. La cause est le
   use-after-free documenté par r410, pas la découpe elle-même.
   En-tête du bloc trouvé (le bloc alloué `seq=2`/128 octets, libéré
   `seq=6`) confirmé CORRECT (`9` unités) juste avant `seq=8` — l'erreur
   naît pendant le traitement de la découpe elle-même, pas d'un en-tête
   déjà corrompu. Quatre écritures capturées en direct (point
   d'observation matériel armé seulement à l'entrée de l'appel ciblé,
   technique affinée après l'incident r408c), chaîne d'appel jamais vue
   dans ce fil (`sub_821F7A88<-sub_821F59E0<-sub_821D74A8<-
   sub_821DE8D8<-sub_82346B48<-sub_8233E1D8<-sub_8234F2C8`). À
   `seq=284`, la première découpe du nœud fantôme (`r5=0x8c`, retourne
   `0x100007d0`) chevauche physiquement le bloc encore vivant alloué à
   `seq=3` (`[0x100007f0,0x10000d80)`, jamais libéré) — le premier
   chevauchement mémoire confirmé du run entier. À `seq=349`, le
   fragment `[0x10080000,0x10082bb0)` du nœud fantôme porte la MÊME
   signature "lien retour NUL" que r403 avait documentée pour
   `0x10082aa0` — la même anomalie de chaînage, pas un mécanisme
   défensif. Corroboration indépendante : le compteur d'unités libres
   du tas (`heap+0x30`) passe négatif entre `seq=200` et `seq=300`. Voir
   `reports/ac6-retail-native-codegen-gate2-r409-phantom-node-created-at-seq8-writer-pinned-20260908.md`.
   **Nommé pour r410** : convertir les trois adresses hôte des
   écritures du champ de taille en labels PPC (comptage d'instructions,
   technique r403) ; capturer en direct le registre/champ source de
   `0x823f` ; lire la nouvelle chaîne d'appel si nécessaire (précédent
   r1111/r1113).

1. **r408 — CORRIGÉ PAR r409 CI-DESSUS : `seq=348` était une écriture de comptabilité intermédiaire pendant un free ordinaire, pas un free anormal. Recontextualise toute la chaîne r358-r407 : le tas octet-par-octet écrit dans une page RÉSERVÉE-MAIS-NON-COMMISE dès `seq=348` (529 appels avant `seq=877`, le pool). Sur matériel réel ceci lèverait une violation d'accès — le titre s'exécute sur console, donc la divergence recomp/retail précède TOUT ce que r358-r407 ont examiné. Origine non établie (PAS un blocage qualifié).**
   Deux runs combinés par corrélation de `seq` (déterministe, confirmé
   d'un run à l'autre) : (r408b, tous les appels
   `NtAllocateVirtualMemory` du run) aucune commission capturée entre
   `seq=0` (réserve+commet `[0x10000000,0x10010000)`, pile
   `sub_821F9860<-sub_821F7D50<-sub_821F7E28<-__xstart`, l'init du tas
   déjà identifié par r404) et `seq=877` ne couvre `0x10080000` ;
   (r408c, points d'observation matériel sur `0x10080000` et
   `0x10082aa0`, armés dès le début du processus, plafonné à 14
   déclenchements) TOUS les déclenchements portent sur `0x10080000`, le
   premier dès `seq=348` — 529 appels avant `seq=877` — via une
   activité alloc/free/coalescence tout à fait ordinaire du tas
   octet-par-octet (`sub_821F94F8<-sub_821FA6F8`, `sub_821F9E10`,
   `sub_821F85F8`). Le double-octroi bucket-17 documenté par r404-r407
   n'est donc probablement qu'un SYMPTÔME tardif d'un état de tas déjà
   faux dès `seq≈348` ou avant, pas la cause elle-même. Anomalie notée
   sans interprétation : le premier accès capturé est un `free()`, pas
   un `alloc()` — aucune allocation observée n'a émis ce bloc. Voir
   `reports/ac6-retail-native-codegen-gate2-r408-commit-tracking-lags-byte-level-heap-real-overlap-locus-still-open-20260908.md`.
   **Nommé pour r409, dans l'ordre** : (1) lire `sub_821F9860` (init du
   tas) pour son dimensionnement initial ; (2) instantané précoce
   (`seq≈5`) de `heap+384`/du descripteur `0x10000630` ; (3) étendre la
   capture de séquence avec le RETOUR de `sub_821F9E10`
   (`r5_in`/`r3_out`) pour chercher le premier retour `0x1008xxxx`
   avant `seq=348` ; (4) placer `MmAllocatePhysicalMemoryEx` (suspect
   nommé par r389, jamais mis sur cette chronologie) sur le même
   compteur (précédent r1111/r1113).

1. **r407 — `sub_821F92B8` lue en entier : DEUX chemins possibles (table de 64 segments existants / réserve-puis-commit via `NtAllocateVirtualMemory`) ; un seul segment existe dans l'instantané déjà capturé et semble épuisé (`+0x30=12`), mais lequel des deux chemins l'appel `seq=877` a pris N'EST PAS établi (PAS un blocage qualifié).**
   Suite de r406 : `sub_821F92B8` (`ppc_recomp.27.cpp:12734`) commence
   par une recherche dans une table de 64 pointeurs de segments
   (`heap+96`..`+352`). Un seul slot non nul dans l'instantané déjà
   capturé par r401 (`heap2_call1_size1.bin`) : segment
   `[0x10000000, 0x10100000)` (1 Mo), dont le champ comparé à la taille
   demandée (`+0x30`) vaut `12` — bien inférieur aux deux requêtes de ce
   fil (8888 octets, `0x80310` octets), suggérant un segment épuisé.
   Si aucune entrée ne convient, la fonction appelle
   `NtAllocateVirtualMemory` (`0x823D037C`) : RÉSERVE seule
   (`0x60002000`, boucle de repli qui divise par deux) puis COMMIT
   séparé (`0x60001000`) — corrige une première version de ce rapport
   qui décodait `0x60002000` comme `RESERVE|COMMIT` combinés sans avoir
   vérifié les constantes dans `xtypes.h`. Le stub hôte délègue à
   `BaseHeap::Alloc` (`upstream/.../rexglue-sdk/src/system/xmemory.cpp`,
   2070 lignes, non lue), dans le MÊME fichier que
   `MmAllocatePhysicalMemoryEx` — le candidat que r389 avait déjà mis
   en cause pour un chevauchement structurellement identique ; le
   rapprochement est donc plus direct qu'il n'y paraissait, sans être
   confirmé. **Quel chemin `seq=877` a réellement pris N'EST PAS
   capturé ce cycle** (le journal de r406 ne loggue que l'ENTRÉE de
   `sub_821F92B8`, pas ses branches internes) — une première version de
   ce rapport l'affirmait à tort, corrigée avant publication. Voir
   `reports/ac6-retail-native-codegen-gate2-r407-sub821f92b8-read-calls-ntallocatevirtualmemory-not-mmallocatephysical-20260908.md`.
   **Nommé pour r408** : étendre la capture fusionnée avec des points
   d'arrêt sur `sub_821F8368` (chemin segment existant) ET
   `NtAllocateVirtualMemory` (chemin noyau) pour trancher directement
   lequel `seq=877` emprunte ; si noyau, capturer la plage retournée et
   la comparer à `0x10082aa0` (précédent r1111/r1113).

1. **r406 — corrige r404 : ordre exact établi en direct par capture fusionnée (points d'observation matériel + compteur de séquence). Le nœud du bucket 17 (`0x10082aa0`) est un reliquat ORDINAIRE créé par un split normal (`seq=874`) ; le pool de `sub_8236E868` (`seq=877`, trois appels plus tard, zéro libération entre les deux) est obtenu via `sub_821F92B8` (chemin "croissance", jamais lu) et recouvre cette adresse déjà distribuée (PAS un blocage qualifié).**
   Une relecture du journal de libérations déjà capturé par r404
   (`r405_free_check_r5.log`) a montré que 40 des 47 pointeurs libérés
   tombent DANS la plage du pool (`[0x10011c60, 0x10091f80)`) — mais
   TOUS avant sa création (`seq<=838` contre `seq=877` pour le pool,
   aucune libération entre les deux) : réutilisation légitime côté
   petits blocs, qui n'explique PAS le chevauchement. Une capture
   fusionnée (les trois points d'observation matériel de r404 PLUS un
   compteur de séquence partagé sur les entrées de
   `sub_821F9E10`/`sub_821FA6F8`/`sub_821F92B8`, même run) établit
   l'ordre exact : `seq=874` (appel `r5=0x22b8=8888` octets, chaîne
   `sub_821F7A88 <- sub_821F59E0 <- sub_821D74A8 <- sub_823B86D0 <-
   sub_823B8770`, identique à celle déjà relevée par r404) insère
   `0x10082aa0` dans le bucket 17 comme reliquat ordinaire d'un split —
   légitime à ce moment. `seq=876` et `seq=877` (le pool, `r5=0x80310`)
   tombent tous deux dans `sub_821F92B8` (arbre de grands blocs épuisé
   après le split de `874`) ; pendant `877`, `sub_82372128` (le
   formateur du pool) écrase les champs de chaînage retour du nœud
   `0x10082aa0` — même mémoire physique, confirmé indépendamment de
   r404. **Le sens de r404 était inversé** : ce n'est pas une insertion
   tardive dans un bloc déjà remis, c'est `sub_821F92B8` qui, en
   croissant le tas, rend une plage qui recouvre une adresse déjà
   distribuée au niveau octet — structurellement le même type de défaut
   que celui documenté par r389 pour `MmAllocatePhysicalMemoryEx`, mais
   sur un chemin jamais rapproché de celui-là jusqu'ici. Voir
   `reports/ac6-retail-native-codegen-gate2-r406-r404-direction-reversed-remainder-overlaps-freshly-carved-pool-20260908.md`
   (et r405,
   `reports/ac6-retail-native-codegen-gate2-r405-remainder-path-read-not-yet-the-bug-20260908.md`,
   pour la lecture de source de `loc_821FA1D4`/`loc_821FA0BC` qui a
   nommé `sub_821F92B8`).
   **Nommé pour r407** : lire `sub_821F92B8` en entier pour identifier
   le stub hôte qu'elle appelle et comment elle calcule la plage
   retournée ; si elle appelle un stub déjà audité (candidat nommé :
   `MmAllocatePhysicalMemoryEx`, déjà mis en cause par r389), rapprocher
   explicitement les deux fils. Vérifier en direct la plage exacte
   retournée par `sub_821F92B8` à `seq=877` avant tout correctif
   (précédent r1111/r1113).

1. **r404 — une invocation de `sub_821F9E10` insère en direct un nœud "libre" (bucket 17, `0x10082aa0`) depuis l'intérieur d'un bloc de 525 Ko qu'une invocation antérieure de la MÊME fonction, chaîne d'appel différente, avait déjà remis à `sub_8236E868` et jamais libéré (PAS un blocage qualifié). CORRIGÉ PAR r406 CI-DESSUS : l'ordre était inversé.**
   Suite de r403 : point d'observation matériel armé sur `0x10000208`
   (en-tête bucket 17) et `0x10082aa8`/`0x10082aac` (chaînage retour du
   nœud) depuis l'entrée du constructeur `GuestAddressSpace`. Six
   déclenchements : `sub_821F9E10` (chaîne
   `sub_821F7A88 <- ... <- sub_823A5BA0`) insère correctement et
   complètement le nœud `0x10082aa0` en tête du bucket 17 ; `sub_82372128`
   (formateur générique de pool, relu en entier, aucune adresse codée en
   dur) écrase ensuite les deux champs de chaînage retour du nœud, sans
   toucher l'en-tête — d'où l'insertion à moitié effacée que r403 avait
   documentée. Capture de registre confirmant `sub_8236E868` demande
   exactement `0x80310` octets (525 072, la taille exacte de son pool —
   corrige la lecture `+150=784` de r398, une troncature 16 bits) et que
   le pointeur retourné (`0x10011c60`) contient le nœud `0x10082aa0`
   inséré ensuite. Capture filtrée sur `sub_821FA6F8` (fonction `free`)
   confirmant qu'aucun des 47 pointeurs libérés sur l'intégralité du run
   n'est `0x10011c60` — le pool n'a jamais été libéré (une première
   capture avait filtré par erreur sur `ctx.r4` au lieu de `ctx.r5`, le
   registre réel du pointeur libéré selon le prologue de
   `sub_821FA6F8` ; corrigée avant publication). Ni codegen (réfuté
   quatre fois, r401-r404), ni taille mal demandée, ni "chevauchement
   entre deux sous-systèmes indépendants" (hypothèse (a) de r380) — une
   seule fonction, deux invocations, distribue deux fois la même
   mémoire. La branche précise qui traite ce bloc comme disponible n'est
   PAS encore identifiée. Voir
   `reports/ac6-retail-native-codegen-gate2-r404-writer-found-sub82372128-pool-collides-with-live-node-20260908.md`.
   **Nommé pour r405** : lire en entier `loc_821FA1D4` (suite, à partir
   de `bne cr6,loc_821FA2F4`) et `loc_821FA0BC` (jamais lu, chemin
   grand-bloc, `r29>=128`, pertinent pour la requête `0x80310`) dans
   `sub_821F9E10`, pour localiser la branche exacte. Aucun correctif
   tant qu'elle n'est pas confirmée en direct (précédent r1111/r1113).

1. **r403 — mécanisme exact localisé : le nœud `0x10082aa0` a été à moitié inséré dans le bucket 17, ses champs de chaînage retour jamais écrits ; `sub_821F9E10`'s triple-vérification refuse À RAISON de le retirer, donc le vrai défaut est en amont (PAS un blocage qualifié).**
   Lecture complète de `sub_821F9E10` depuis `loc_821F9EC4` jusqu'au
   retour commun `loc_821FA528`. Une première lecture (déchaînement à
   triple vérification lui-même fautif, même famille que r356/r382) a
   été **écrite puis réfutée avant publication** par un contrôle direct :
   les avertissements `Uninitialized memory read` du rejeu microexec de
   r402 (`821fa06c`/`821fa070` pour `call1`, `821f9f04`/`821f9f08` pour
   `call2`), comptés en instructions depuis les labels connus,
   correspondent exactement à `lwz r9,0(r11)`/`lwz r7,4(r10)` — les
   champs retour du nœud trouvé (`0x10082aa8`/`0x10082aac`) sont NULS,
   confirmé en relisant ces deux adresses dans l'instantané déjà
   capturé (les deux valent `0x00000000`). Avec ces liens à zéro, la
   deuxième des trois comparaisons échoue et les deux `stw` qui
   réécriraient l'en-tête du bucket 17 ne s'exécutent jamais — **le
   déchaînement refuse À RAISON de retirer un nœud dont les liens retour
   ne correspondent pas à son bucket.** Le vrai défaut est donc en amont :
   quelque chose a écrit l'en-tête du bucket 17 (`0x10000208`,
   `[0]=[4]=0x10082aa8`) SANS écrire les champs retour du nœud lui-même —
   une insertion à moitié faite, pas encore tracée jusqu'à son écrivain.
   Ceci confirme et affine le verdict r401/r402 (pas de mauvaise
   traduction de codegen dans `sub_821F9E10`) plutôt que de le
   contredire. Voir
   `reports/ac6-retail-native-codegen-gate2-r403-bucket17-header-not-updated-20260908.md`.
   **Nommé pour r404** : point d'observation matériel sur `0x10000208`
   ET sur `0x10082aa8`/`0x10082aac` depuis le début du processus
   (technique r371/r389, déjà nommée par r389 pour la sentinelle et
   jamais exécutée) pour capturer l'écrivain exact de l'en-tête sans les
   champs retour. Deux candidats déjà visibles dans le corps de
   `sub_821F9E10`, ni l'un ni l'autre lu en entier : le chemin de
   réinsertion du reliquat (`loc_821FA1D4`) et le chemin de bloc neuf via
   `NtAllocateVirtualMemory` (`loc_821FA564`). Aucun correctif tant que
   l'écrivain n'est pas confirmé en direct (précédent r1111/r1113).

1. **r402 — les traces microexec de r401 confirmées réelles (pas un chemin d'erreur), écritures concrètes capturées, sémantique exacte encore ouverte (PAS un blocage qualifié).**
   Doute soulevé avant publication de r401 (deux imports host non
   stubés dans le prologue, `KeGetCurrentProcessType`/`KeBugCheckEx`,
   auraient pu faire bifurquer la trace vers un chemin d'erreur avec
   `callee_entries=0` et ~200 pas). Fermé : `heap+20` vaut `0x2` dans
   les deux instantanés (bit testé = 0, le prologue saute PAR-DESSUS
   ces imports) ; rejoué avec les deux imports stubés en plus,
   résultat identique (`steps=212`/`127`, `stubbed_calls=2` inchangé —
   jamais atteints). Les instantanés `dump heap` post-exécution
   montrent des écritures substantielles et cohérentes avec les
   adresses déjà connues de la campagne (`call1` : 9 plages, dont le
   champ sentinelle `0x10000184` et le champ de chaînage propre du nœud
   `0x1009fa10` ; `call2` : seulement 2 octets). **Le verdict de r401
   (concordance, pas de mauvaise traduction de codegen) tient et est
   mieux fondé** — pas rétracté. Reste ouvert : la sémantique exacte des
   champs écrits à `0x10082ac8`/`0x10082acc` (chaînage de freelist vs.
   tags de bornage physiques) et quelle branche exacte `call1` (bucket 2,
   VIDE selon le contrôle direct de cette table) a réellement prise pour
   quand même retourner `0x10082ab0` avec ces écritures. Voir
   `reports/ac6-retail-native-codegen-gate2-r402-microexec-writes-confirm-real-path-20260908.md`.
   **Nommé pour r403** : lire `sub_821F9E10` en entier depuis
   `loc_821F9EC4` à travers `loc_821F9F80` pour trancher la sémantique
   des champs et la branche réellement prise, en utilisant le harnais
   microexec maintenant fonctionnel sur `ac6-us` plutôt que
   l'observation uniquement live.

1. **r400 — arbre stabilisé, gate JF restauré (PAS un blocage qualifié).**
   La situation d'arbre non committé nommée "décision de l'utilisateur,
   inchangée" depuis r239 (~160 cycles) bloquait en pratique le gate
   `mission01-final-gate-v3.json` (`evidence size mismatch`), pas
   seulement en principe. Résolu ce cycle : 57 fichiers racine vestiges
   d'un fil PAL/NDXR/ENTRY9 sans rapport avec la campagne NTSC-US
   supprimés (confirmé : aucune référence dans NEXT.md/RESUME.md/
   EVIDENCE.md/AGENTS.md, aucun équivalent ailleurs dans l'arbre) après
   confirmation explicite de l'utilisateur ; erreur immédiate corrigée
   dans le même cycle (`GLOBAL_OFFLINE_LADDER.md` et
   `XENIA_WINE_ORACLE_HANDOFF.md` restaurés, tous deux vivants malgré
   l'apparence) ; `recompilation/ace-combat-6-demo` réduit à son archive
   d'analyse statique (config/docs/tools), son build/source machinery
   retiré, conforme au texte déjà à jour d'`AGENTS.md` ; trois fichiers
   de travail en cours réel (`retail_session.cpp` : sélection caméra par
   mode de vue + ancre free-flight NTSC-U/J ; `ntxr_texture.h` : lecture
   pack par clé GIDX ; `retail_flight_orientation.h` : extraction
   pitch/yaw/roll) re-pinnés via `refresh_contract_evidence.py` et
   committés. **Les trois gates requis par `CLAUDE.md` sont maintenant
   verts** (`audit_ac6_mission01_native_gate.py --require JF` = pass,
   `audit_ac6_contract_artifacts.py` = pass, `audit_ac6_contract_addresses.py`
   = pass 321/321) — première fois depuis le début de r239. Voir
   `reports/ac6-retail-native-codegen-gate2-r400-tree-stabilization-mission01-gate-restored-20260908.md`.
   **Reste non touché** : 72 fichiers modifiés / 206 non trackés hors du
   chemin qui bloquait le gate (dont la surcouche HUD non tracée
   `native_hud_gpu_overlay.{h,cpp}`, cf. règle HUD nommée ci-dessous) ; le
   blocage runtime r399 (inchangé, voir item 1).

1. **r401 — discriminant microexec exécuté : mauvaise traduction de codegen RÉFUTÉE pour `sub_821F9E10`, la cause est en amont (état du tas), PAS un blocage qualifié.**
   État exact capturé en direct (`ctx.r3=0x10000000`, `r4=0`, `r5=1` puis
   `r5=256`, même `sp`) et instantané du tas invité
   `[0x10000000,0x10200000)` au moment précis des deux appels
   `sub_821F9E10` qui retournent tous deux `0x10082ab0` dans le run natif
   compilé (re-confirmé, identique à r398). Rejoué dans
   `MicroExecuteFunction.java` (interprète p-code Ghidra, indépendant du
   C++ compilé, sur les MÊMES octets d'instruction retail) : **les deux
   cas retournent également `0x10082ab0`**, sortie propre (`exit=return`,
   pas de fault). Deux moteurs d'exécution indépendants s'accordent sur la
   même sortie à partir du même état — une mauvaise traduction de codegen
   n'aurait pas dû survivre à une réimplémentation indépendante.
   **`sub_821F9E10` fait ce que ses instructions disent de faire ; le
   problème est en amont, dans l'état du tas au moment de ces deux
   appels**, pas dans la traduction de la fonction. A nécessité d'élargir
   le gate SHA du harnais (`scripts/MicroExecuteFunction.java`, figé sur
   le seul hash PAL de la suite de calibration) à un `Set` incluant le
   hash NTSC-U/J — prouvé sans effet sur le chemin PAL existant par
   comparaison directe avant/après (sortie identique octet pour octet).
   La calibration automatisée complète n'a pas pu tourner (charge utile
   extraite manquante, préexistante, sans rapport avec ce cycle) ; un
   second constat préexistant (le fichier de référence
   `rotation-822a1e80.ppc.json` ne correspond déjà plus à une exécution
   fraîche, indépendamment de ce cycle) a aussi été trouvé — les deux
   nommés pour r402, pas résolus ici. Voir
   `reports/ac6-retail-native-codegen-gate2-r401-microexec-discriminator-codegen-refuted-20260908.md`.
   **Nommé pour r402** : (1) tracer en arrière depuis l'instantané figé
   du tas (`heap2_call1_size1.bin`) pour trouver quelle fonction a écrit
   la freelist dans cette forme AVANT ces deux appels — continuation de
   la chasse à l'écrivain r378-r398, maintenant avec un instantané
   rejouable au lieu d'une observation uniquement live ; le suspect nommé
   par r389 (`MmAllocatePhysicalMemoryEx`, chevauchement de plage avec la
   freelist vivante) reste jamais suivi ; (2) réparer la lacune de
   calibration (payload manquant + fichier de référence obsolète) sans
   quoi tout futur changement à ce harnais reste invérifiable ; (3) un
   blocage qualifié (budget oracle Xenia, trace des quatre premières
   allocations de `sub_8236E868`) et le choix "un bug à la fois" restent
   nommés pour l'utilisateur si la chasse à l'écrivain ne tranche pas —
   voir le plan approuvé.

2. **r399 (historique, affiné par r401 ci-dessus) — tracer en direct depuis les sites d'appel `+294` (taille 1) et `+343` (taille 256) de `sub_8236E868` jusque dans `sub_821F9E10` pour trouver exactement où ces deux requêtes convergent sur la même adresse retournée (`0x10082ab0`), et quelle instruction/branche échoue à avancer le curseur/pointeur-de-tas responsable ; une fois isolé, appliquer le correctif via le script idempotent établi, reconstruire, et vérifier si `sub_821D5F48` revient enfin et si la boucle par image s'exécute.**
   r398 a désassemblé `__imp__sub_8236E868` directement et armé un
   point d'arrêt à chacun des décalages hôte où les 4 premiers appels
   d'allocation retournent leur valeur (`+150`=784, `+225`=55944,
   `+294`=1, `+343`=256), tous en UNE SEULE exécution -- contournant la
   limitation `gdb print <nom-local>` de r397. Résultat identique sur 3
   lancements indépendants : la requête de 1 octet et la requête de
   256 octets, émises l'une après l'autre SANS libération entre les
   deux, retournent le MÊME pointeur exact, `0x10082ab0`.
   `0x10082ab0 + 0x18 = 0x10082ac8`, précisément le nœud freelist
   corrompu chassé depuis r378, et précisément le décalage `0x18`
   identifié à l'origine par r380 (la découverte de r380 et celle-ci ne
   sont PAS en conflit ; r392 avait réfuté une attribution DIFFÉRENTE).
   L'hypothèse principale de r397 (Write 1 issue de la requête de 55944
   octets, `0x10091f80`) est réfutée. **Ceci n'est plus une corrélation
   plausible : c'est un fait live, déterministe, même-invocation** :
   l'allocateur générique délivre le MÊME bloc vivant à deux requêtes
   différentes sans rapport, sans qu'aucune ne le libère -- un bug de
   double-émission de l'allocateur lui-même, pas un unlink manquant
   côté appelant (r380/r392's thread), pas une collision de
   sous-système non coordonné (r386-r389's thread). Règle la question
   "quel côté viole son contrat de propriété mémoire" tournée en rond
   depuis r386-r397. Aucun correctif appliqué : la branche exacte à
   l'intérieur de `sub_821F9E10` qui échoue à marquer `0x10082ab0`
   consommé avant la seconde requête n'est pas encore isolée -- c'est
   la QUATRIÈME tentative d'attribution dans ce sous-fil, les trois
   premières s'étant effondrées sous un examen plus strict (r380 par
   r392 ; r393 par r395 puis re-confirmé par r396). `sub_821D5F48` n'est
   toujours jamais revenu ; la boucle par image ne s'exécute toujours
   jamais, après SEIZE découvertes du sous-système allocateur
   (r358-r398).

1. **r389 (historique, dépassé par ce qui précède) — r388 a effectué le test décisif nommé par r387 : capturé en direct
   l'adresse de base réellement retournée par l'appel `MmAllocatePhysicalMemoryEx`
   de `sub_821D5F48` (`*__imp__sub_821D5F48+1214`, `ctx.r3` = `0x16f80000`),
   calculé la plage `[0x16f80000, 0x2e780000)` avec la taille `0x17800000`
   capturée par r387, et confirmé que `0x280c0b10` (le nœud empoisonné de
   r386) tombe DEDANS. **Lecture (1) confirmée** : la mémoire de la
   freelist des gros blocs est LA MÊME mémoire que cette réservation de
   375 Mio, pas une collision entre deux régions indépendantes (de toute
   façon structurellement impossible vu le compteur monotone unique
   `allocate_guest`, r387). Lecture (2) réfutée pour ce nœud.

   Ceci ne résout PAS encore l'investigation. r386 avait déjà armé un
   point de surveillance matériel sur `0x280c0b10` lui-même depuis le
   tout début du processus et trouvé UNE SEULE écriture dans toute
   l'exécution -- le memset empoisonnant (`0xFE`). Aucune routine
   n'écrit jamais de pointeur « next » valide dans la mémoire propre de
   ce nœud. Son appartenance apparente à la freelist (atteinte par le
   scanner de `sub_821F8A00` en partant de la sentinelle `0x10000180`)
   doit donc venir de quelque chose qui écrit le CHAÎNAGE DE LA
   SENTINELLE elle-même vers `0x280c0b10` -- une cible de surveillance
   différente de celle déjà vérifiée, pas encore armée.

   Mécanisme plausible mais explicitement NON confirmé : une routine
   d'insertion/découpe de pool qui suppose que la mémoire fraîchement
   découpée est déjà mise à zéro (convention `next=0` = terminateur) et
   qui, de ce fait, n'écrit jamais explicitement de terminateur dans le
   nouveau nœud -- hypothèse brisée ici car cette mémoire a été
   empoisonnée à `0xFE`, pas à zéro, par le memset antérieur et non lié
   de `sub_821D5F48`. Correspondrait à la même forme "écriture manquante
   sur un chemin" que tous les correctifs précédents de cette campagne,
   mais PAS confirmé en direct -- pas de correctif appliqué, conformément
   à la leçon apprise deux fois par r382.

   Gate : `ctest` 10/10 (aucune source modifiée ce cycle, investigation
   seule). `git status` après `ctest` : 523 chemins, identique à
   r385-r387, aucune dérive. Aucun commit -- décision de l'utilisateur
   inchangée.

   **Nommé explicitement pour r389** : armer un point de surveillance
   matériel sur le champ de chaînage propre de la sentinelle
   (`0x10000180`, avec la convention de décalage de champ établie depuis
   r369/r371) depuis le tout début du processus (technique de r371), pour
   capturer l'écriture exacte qui lie `0x280c0b10` dans la chaîne pour la
   première fois, et identifier la fonction invité responsable. Lire
   cette fonction en entier, déterminer si elle suppose une mémoire mise
   à zéro pour son terminateur, et si oui si le correctif doit écrire un
   terminateur explicite dans le nouveau nœud, ou si le memset de
   `sub_821D5F48` ne devrait tout simplement pas toucher cette plage (une
   question d'ORDONNANCEMENT entre les deux mécanismes). C'est la
   NEUVIÈME découverte du sous-système allocateur (r358-r388) sans encore
   atteindre la boucle par image. Le thread principal n'est TOUJOURS
   jamais revenu de `sub_821D5F48` ; la boucle par image ne s'est encore
   jamais exécutée, ce cycle ou tout cycle précédent. Le choix permanent
   de l'utilisateur "continuer un bug à la fois" reste en vigueur -- pas
   d'escalade unilatérale vers un audit exhaustif par lot.

   Voir §4.109 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r388-overlap-decisive/`.

2. **r387/r388 — close, contexte historique.** r387 a
2. **r386/r387 — close, contexte historique.** r386 a
   localisé l'écrivain du septième candidat de r385/r384 : un point de
   surveillance matériel armé sur `0x280c0b10` (l'unique nœud réel de la
   freelist des gros blocs) depuis le tout début du processus a capturé
   UNE SEULE écriture, ~1,6s après l'armement, ancienne valeur `0` :
   `sub_823830F0` (un `memset` générique, confirmé correct par lecture
   complète) appelé depuis `sub_821D5F48` -> `sub_821D7DE0`, remplissant
   de `0xFE` toute une région obtenue via `sub_821F4078(dest=adresse
   fixe, r4=-1, r5=0, r6=0x20000004)` -- structurellement un appel de
   réservation+commit à adresse fixe façon `VirtualAlloc`/`mmap`, PAS
   acheminé par l'allocateur à freelist étudié depuis r311.

   **Ce N'EST PAS une huitième instance du même bug "Leave manquant"**
   (r358, r365/366, r376, r378-380, r383) -- c'est un chevauchement de
   plage d'adresses entre DEUX mécanismes de gestion mémoire
   indépendants : la freelist, et la réservation à adresse fixe de
   `sub_821F4078`. Lequel des deux est fautif n'est pas déterminé (`sub_821F4078`
   lue seulement structurellement, pas en entier, ce cycle). Aucun
   correctif appliqué -- ceci ne correspond plus au patron "ajouter un
   appel Leave/unlink manquant, en miroir d'un frère correct" autorisé
   jusqu'ici.

   **Nommé explicitement pour r387** : (a) lire `sub_821F4078` en
   entier pour déterminer quel côté du chevauchement est réellement en
   tort ; (b) noter explicitement qu'après SEPT découvertes dans le
   sous-système allocateur (r358 à r386) sans jamais atteindre la
   boucle par image, et cette découverte étant de nature architecturale
   différente des six précédentes (un possible conflit de propriété de
   plage d'adresses entre sous-systèmes, pas un correctif d'une ligne),
   ceci peut à nouveau justifier un point de contrôle stratégique avec
   l'utilisateur avant de continuer unilatéralement -- signalé ici
   explicitement, pas décidé seul, dans la continuité du précédent de
   r381.

   Voir §4.107 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r386-corrupted-node-writer/`.

2. **r386 — continuation normale (PAS un blocage qualifié), désormais close.** r385 a
   appliqué le contrôle strict de registre de r369 à l'invocation de
   `sub_821F8A00` atteinte via `sub_821F9150` (r384) et l'a CONFIRMÉE
   comme une vraie boucle infinie figée : trois échantillons SIGINT,
   20s d'écart, montrent **tout le fichier de registres identique
   bit-à-bit** (`rbx=0x10000180`, `r11=0`). La lecture croisée
   désassemblage/source (`ppc_recomp.27.cpp:11468-11640`) PROUVE
   mathématiquement l'auto-perpétuation : avec `r11=0`, `LOAD_U16`
   près de `0xFFFFFFF8` puis `LOAD_U32(0)` retournent tous deux 0, donc
   `r11` ne peut jamais converger vers le sentinel `0x10000180` (la
   MÊME tête de freelist des gros blocs partagée depuis
   r369/r371/r378-383, confirmée vivante et correctement écrite
   ailleurs dans le run).

   `sub_821F9150` (jamais lue avant r384) a été lue en entier ce
   cycle : `NtAllocateVirtualMemory` -> `sub_821F8248` (jamais
   examinée, initialise l'en-tête du nouveau bloc) -> épissage via
   l'un de deux appels internes à `sub_821F8A00`. La relation exacte
   entre les champs que `sub_821F8248` initialise et le pointeur
   « next » que le scanner déréférence ensuite n'a PAS été résolue ce
   cycle -- deviner ici répéterait l'erreur de r382 (corréler au lieu
   de prouver).

   **Confirmé : une SEPTIÈME instance du même défaut de chaînage
   cassé** (r358, r365/366, r376, r378-380, r383). **Pas corrigée** --
   aucun correctif appliqué, l'écrivain exact n'ayant pas été localisé
   en direct.

   **r386 doit** : (a) parcourir la freelist des gros blocs en direct,
   tête-à-queue, juste avant que cette invocation ne commence son scan,
   pour trouver le nœud précis dont le champ « next » lit déjà 0
   (technique de r371) ; (b) armer un point de surveillance matériel
   sur ce champ exact depuis le démarrage du processus (technique de
   r371, PIE-safe, avant tout code invité) pour trouver l'écrivain ;
   (c) lire `sub_821F8248` en entier (jamais examinée) et auditer par
   chemin de sortie la fonction impliquée, selon la même technique qui
   a trouvé chaque défaut précédent ; (d) continuer la discipline « un
   bug à la fois, vérifié en direct avant correctif » explicitement
   choisie par l'utilisateur (r381/r382) -- pas de correctif par lot,
   pas de déduction depuis le seul désassemblage ou la seule
   corrélation. Le thread principal n'est toujours jamais revenu de
   `sub_821D5F48` (r384) -- ceci reste le test ultime une fois (si) ce
   septième défaut corrigé. La situation d'arbre non commité (r239+,
   sur la suppression massive du 2026-09-02) reste la décision de
   l'utilisateur, inchangée.

   Voir §4.106 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r385-sub821f9150-loop/`.

2. **r385 (historique, résolu par r386 ci-dessus)** -- confirme le
   sixième candidat de r384 comme une vraie boucle infinie figée, mais
   ne localise pas l'écrivain. Voir §4.106.

3. **r384 (historique, résolu par r385/r386 ci-dessus) — continuation normale (PAS un blocage qualifié).** r383 a
   construit la technique primaire nommée nécessaire par r382 : une
   trace d'exécution pas-à-pas réelle (`nexti`, sautant par-dessus les
   appels) de l'invocation LIVE `ENTER#545` elle-même, du site d'Enter
   jusqu'à son retour (`FUNC_BASE` calculé en direct sous ASLR, boucle
   de trace sortie du callback `stop()` du point d'arrêt). La trace (122
   instructions hôte uniques, pc de sortie identique à l'adresse de
   retour déjà capturée par r382 pour `ENTER#545`) a révélé un
   TROISIÈME chemin : un `jmp` inconditionnel (`+758`) sautant
   directement vers la queue commune de la fonction, sans passer par
   AUCUN des deux sites d'appel Leave connus -- réfutant la corrélation
   `+758` de r382 comme pure coïncidence.

   Identifié précisément par lecture croisée du source généré : le
   chemin d'allocation « gros bloc » (`r29>=128`) appelle
   `sub_821F92B8` ; en cas d'échec (`r3==0`), `loc_821FA630 ->
   loc_821FA634 -> loc_821FA660 -> loc_821FA664 -> return;` ne passe
   JAMAIS par la vérification Leave gardée par `r22` de `loc_821FA528`
   -- même forme de bug que r358/r365-366/r376, cinquième instance
   indépendante dans le même allocateur.

   **Correctif appliqué et vérifié** (`tools/apply_sub_821f9e10_largeblock_failure_leave_fix.py`,
   nouveau script suivi, idempotent) : insère le Leave gardé par `r22`
   en tête de `loc_821FA664`, utilisant `r27` (pas `r30`) comme pointeur
   d'objet tas, exactement comme `loc_821FA528` elle-même. Un premier
   faux négatif (décalage hôte codé en dur périmé après reconstruction,
   hérité du script de r382) a été capturé et corrigé en réarmant le
   point d'arrêt Leave symboliquement sur `*__imp__RtlLeaveCriticalSection`
   lui-même. Re-vérifié : `ENTER#545 leaves_since_enter=1 LEAK=False`.
   Équilibre agrégé (60s) : `enter=575 leave=575 net=0` -- entièrement
   équilibré. **Cinquième fuite de section critique confirmée en direct,
   corrigée et vérifiée dans cette campagne.**

   **MAIS re-testé la boucle par image** (`sub_821D7AE0`/`sub_821D7CD0`,
   fenêtre de 120s post-correctif) : **zéro exécution, inchangé**. La
   poignée de main avec l'ouvrier se déclenche encore exactement une
   fois (`b5a0=1 a620=1 a610=1`) et le thread principal ne redemande
   jamais -- le motif déjà caractérisé par r367/r377, maintenant
   confirmé persister même avec l'allocateur entièrement et prouvément
   propre (`net=0`). La sous-investigation de l'allocateur (r356-r358,
   r365/366, r376, r378-383) est maintenant close à un point d'arrêt
   honnête et complet -- un vrai progrès, mais pas la réponse à
   « pourquoi `presented_frames` reste à 0 ».

   **r384 doit** : revenir à la direction déjà nommée par r377/r382,
   maintenant sur un allocateur prouvément propre -- instrumenter
   directement la décision de re-demande de la poignée de main
   (génération 2+) du thread principal (le code qui décide de réémettre
   ou non une requête vers l'ouvrier après que la première soit
   complétée), puisque c'est maintenant la seule candidate restante pour
   le vrai blocage de la boucle par image. La situation d'arbre non
   commité (r239+, sur la suppression massive du 2026-09-02) reste la
   décision de l'utilisateur, inchangée.

   Voir §4.104 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r383-enter545-instruction-trace/`.

1. **r383 (historique, PAS un blocage qualifié).** L'utilisateur
   a choisi explicitement « continuer un bug à la fois » sur le point de
   décision stratégique de r381. r382 a vérifié en direct la troisième
   paire Enter/Leave de `sub_821F9E10` et confirmé un déséquilibre
   reproductible (`enter=589 leave=587 net=2` sur 60s, stable). Une
   NOUVELLE technique (suivi par adresse de retour PAR INVOCATION) a
   confirmé en direct qu'`ENTER#545` lui-même retourne à son appelant
   (`sub_823801B8`) sans jamais avoir appelé Leave -- une fuite réelle, à
   cadre unique. Un `jmp` inconditionnel statique (`+758`, correspondant
   au chemin rapide « correspondance exacte de taille », `goto
   loc_821FA528;` ligne ~16519) semblait corréler exactement (un point
   d'arrêt dédié s'y est déclenché une fois, au moment précis où
   `outstanding_before=1`). Un correctif a été appliqué
   (`tools/apply_sub_821f9e10_smallbucket_leave_fix.py`, même motif
   idempotent que r358, patchant aussi le doublon dans `sub_821F9E08`
   comme pour r358) et reconstruit.

   **MAIS la re-vérification par la MÊME technique de suivi de retour,
   sur le binaire reconstruit, PROUVE qu'`ENTER#545` fuit TOUJOURS,
   inchangé** (`leaves_since_enter=0, LEAK=True`). Le nouveau Leave
   inséré s'exécute bien une fois dans la même fenêtre, mais sur une
   invocation DIFFÉRENTE et bien plus tardive (séquence globale 587, pas
   545/546) -- le déséquilibre agrégé reste inchangé (2, avant et après).
   **La corrélation `+758` est explicitement rétractée** comme
   coïncidence probable avec un appel sans rapport empruntant
   légitimement le même chemin rapide avec `r22` déjà à 0, pas un lien
   causal avec `ENTER#545`. Le correctif est CONSERVÉ (réel, correct,
   inoffensif, en miroir d'une logique sœur déjà correcte, passe les
   gates) mais ne résout PAS la fuite reproductible `ENTER#545`/`#546`
   que cette sous-investigation (r378-r382) poursuit depuis.

   **r383 doit** : re-dériver l'instruction de contournement réelle
   d'`ENTER#545` en utilisant le suivi par adresse de retour par
   invocation comme outil PRINCIPAL -- armer le suivi de retour d'abord,
   puis biséquer l'intervalle entre l'Enter et son retour connu avec des
   points d'arrêt conditionnels supplémentaires, plutôt que de deviner
   des branches candidates depuis le désassemblage optimisé et
   réordonné, qui a maintenant produit DEUX fausses pistes de suite pour
   cette investigation précise (`+1949`/`+1970`/`+2063` en première
   tentative, puis `+758` en seconde). La situation d'arbre non commité
   (r239+, sur la suppression massive du 2026-09-02) reste la décision
   de l'utilisateur, inchangée.

   Voir §4.103 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r382-9e10-enter-leave-balance/`.


1. **r380 — continuation normale (PAS un blocage qualifié).** r379 a
   tracé en direct l'écrivain exact du NULL de r378 (technique de r371,
   point d'observation armé avant tout code invité sur
   `0x10082ac8`) : trois écritures seulement avant sortie normale du
   processus -- bootstrap correct (`sub_821F9E10`, `next=head`), puis
   deux initialiseurs légitimes SANS RAPPORT (`sub_82377C00`, table de
   8 handles ; `sub_82372128`/`sub_82372198`, pool de 64 emplacements)
   qui zèrent ce qu'ils croient être leur propre mémoire privée --
   lecture complète des deux corps confirme qu'AUCUNE des deux n'est
   défectueuse en elle-même. L'adresse `0x10082ac8` sert donc à trois
   usages sans rapport en ~10ms de démarrage. Deux hypothèses non
   départagées : (a) un appel d'allocation a distribué ce bloc aux
   écritures 2/3 sans le retirer de la liste que `sub_821F8A00`
   parcourt encore ailleurs (un déchaînement manquant côté allocation,
   même famille de défaut que les quatre déjà trouvés mais dans une
   fonction différente) ; (b) l'écriture 1 elle-même était
   prématurée/erronée. **PAS corrigé** : appliquer un correctif à
   `sub_82377C00`, `sub_82372128` ou `sub_821F9E10` maintenant serait
   deviner sans contrôle en direct -- explicitement refusé (précédent
   r1111/r1113), quatrième fois dans cette chaîne qu'une hypothèse
   plausible sur l'écrivain lui-même est abandonnée au profit d'une
   hypothèse mieux fondée un niveau plus haut. **Nommé pour r380** :
   mettre un point d'arrêt sur les points d'entrée « allocate » de
   l'allocateur général (`sub_821F92B8`/`sub_821F9150`) entre l'écriture
   1 et l'écriture 2, capturer la taille demandée et l'adresse
   retournée, vérifier si le déchaînement a eu lieu ; si l'écriture 1
   s'avère être l'erreur à la place, auditer la logique d'appartenance
   de bucket de `sub_821F9E10` comme r356 l'a fait pour `sub_821FA6F8`.
   Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.100 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r379-null-writer/`.

2. **(contexte, r379 — voir ci-dessus)**

3. **r379 — continuation normale (PAS un blocage qualifié).** r378 a
   appliqué le contrôle plus strict de r369 au candidat que r377 avait
   trouvé (6 échantillons au même décalage dans `sub_821F8A00`) : point
   d'arrêt compté sur `*__imp__sub_821F8A00+480`, capturant `r11`
   (curseur) sur 30 arrêts consécutifs. `r11` atteint `0x00000000` à
   l'arrêt #5 et y reste identique bit à bit sur les 26 arrêts suivants
   -- **confirmé : une VRAIE boucle infinie gelée**, pas un artefact
   d'échantillonnage. Lecture mémoire ciblée : le champ `next` brut de
   l'entrée `0x10082ac8` est un NULL nu (`0x00000000`) que la
   vérification de terminaison de cette boucle (uniquement contre la
   sentinelle `0x10000180`) ne reconnaît jamais -- un QUATRIÈME bug de
   chaînage de freelist indépendant (après r358, r365/366, r376), même
   sous-système allocateur, forme de défaut similaire mais entrée/bucket
   et valeur de corruption différentes (NULL, pas auto-référence). La
   nouvelle chaîne d'appel n'est PAS un nouveau sous-système -- un
   appelant inexaminé du même allocateur étudié depuis r311. **PAS
   corrigé** : le symptôme est confirmé en direct, mais quelle fonction
   en amont a écrit ce zéro n'a pas encore été tracé -- deviner un
   correctif sans cette traçabilité violerait la discipline de preuve du
   projet (précédent r1111/r1113). La vraie boucle par image
   (`sub_821D7AE0`/`sub_821D7CD0`) ne s'exécute toujours pas (attendu,
   aucun code changé ce cycle). **Nommé pour r379** : (1) tracer en
   arrière depuis le champ `next` corrompu de `0x10082ac8` (réutiliser la
   technique de point d'observation-depuis-le-démarrage-du-processus de
   r371) pour trouver quelle fonction a écrit `0x00000000` au lieu d'un
   lien correct ; (2) appliquer et vérifier un correctif via le même
   script de correctif tracé déjà utilisé trois fois, une fois l'écrivain
   identifié ; (3) ne pas supposer que ce sera le dernier bug de ce genre
   -- quatre défauts de chaînage de freelist indépendants ont déjà été
   trouvés dans cet unique allocateur. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.99 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r378-newloop-control/`.

2. **(contexte, r378 — voir ci-dessus)**

3. **r378 — continuation normale (PAS un blocage qualifié).** r377
   (pas de correctif ce cycle) a caractérisé la fenêtre post-r376 avec
   une sonde d'échantillonnage périodique (SIGINT externe vers
   l'inférieur, 6 arrêts sur 95s) : (a) la poignée de main ouvrier
   (`sub_8233B5A0`/`sub_8233A620`) se déclenche désormais UNE FOIS,
   à moins d'1ms du démarrage -- jamais vu depuis r315, mais une seule
   occurrence, pas un régime stationnaire ; (b) les 6 échantillons
   (t≈12s à t≈84s) atterrissent TOUS dans une NOUVELLE boucle de
   recherche interne à `sub_821F8A00` (décalages `+489`/`+494`,
   confirmés par désassemblage), distincte de la boucle déjà corrigée
   de `sub_821F9E10`, se terminant à une sentinelle `+0x180` ; (c) la
   pile d'appel complète menant ici
   (`sub_821F9150 <- sub_821F92B8 <- sub_821F9E10 <- sub_821F7A88 <-
   sub_821F59E0 <- sub_821D74A8 <- sub_823B86D0 <- sub_823B8770 <-
   sub_823B0B48 <- sub_823A65A0 <- sub_8236E618`) est ENTIÈREMENT
   NOUVELLE au-dessus de `sub_821D74A8` -- jamais examinée par
   r311-r376. **Nommé pour r378** : (1) appliquer le contrôle
   registre-à-travers-plusieurs-arrêts de r369 à ce nouveau candidat
   AVANT de le traiter comme confirmé (6 échantillons au même décalage
   sur 72s est une preuve circonstancielle forte, pas encore une
   preuve directe comme celle de r369) ; (2) si confirmé, identifier
   le bucket/liste concerné et lire les fonctions `823A`/`823B` jamais
   examinées pour comprendre le sous-système demandeur ; (3) ne pas
   présumer qu'un correctif similaire (missing-Leave / control-flow
   gate) s'applique sans le vérifier en direct -- chaque bug de cette
   investigation a eu un mécanisme distinct. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.98 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r377-postfix-timeline/`.

2. **(contexte, r377 — voir ci-dessus)**

1. **r377 — continuation normale (PAS un blocage qualifié).** r376 a
   CONFIRMÉ EN DIRECT l'inférence de r375 (le nœud `0x1009fa10` est
   déjà la tête de la freelist surdimensionnée, pointée par la
   sentinelle `0x10000180`, AVANT même que le 4e appel à
   `sub_821F8A00` ne commence), puis a tracé les quatre appels
   (`sub_821F92B8 -> sub_821F8368 -> sub_821F85F8 -> sub_821F8A00`) en
   UNE SEULE passe (calibration de la disposition de `PPCContext` :
   `r3@ctx+0x0`, `r4@ctx+0x10`, valable pour toutes les fonctions
   partageant ce type). Résultat : `sub_821F85F8` reçoit un bloc
   FRAÎCHEMENT alloué (`0x100b0000`) mais renvoie une adresse
   complètement différente (`0x1009fa10`, le voisin arrière calculé en
   interne) à l'appelant. **VRAI CORRECTIF appliqué et vérifié EN
   DIRECT** : la 4e des quatre conditions de fusion-arrière de
   `sub_821F85F8` (la vérification de cohérence de r375, déjà prouvée
   CORRECTE) est la SEULE des quatre dont l'échec ne saute PAS vers
   `loc_821F880C` (« pas de fusion, `r30` de l'appelant inchangé ») --
   elle tombe à la place dans la logique « fusion acceptée », qui
   substitue inconditionnellement le voisin invalide via `r30 = r31`.
   Correctif (`tools/apply_sub_821f85f8_return_gate_fix.py`, nouveau,
   tracé, idempotent) : aligne les deux branches d'échec de cette 4e
   condition sur ses trois sœurs. Reconstruit, vérifié EN DIRECT : (1)
   `sub_821F8A00` reçoit maintenant la vraie valeur `0x100b0000`, plus
   un 5e appel authentiquement NOUVEAU (`0x2e780050`) qui n'existait
   pas avant ; (2) la boucle infinie de `sub_821F9E10` a DISPARU -- 20
   passages complets sur une fenêtre de 90 s, contre ~56 000+ passages
   et en croissance non bornée avant ce correctif. **Cependant** : le
   test de r360 sur la vraie boucle par image
   (`sub_821D7AE0`/`sub_821D7CD0`) montre toujours ZÉRO exécution sur
   la même fenêtre de 90 s -- le processus sort par le minuteur propre
   du harnais de sonde, pas par un blocage, mais pas non plus par le
   gameplay. **Nommé pour r377** : caractériser ce que fait maintenant
   le thread principal pendant cette fenêtre de 90 s (un échantillon
   d'état/pile pris à mi-fenêtre montrerait s'il progresse plus loin,
   se bloque ailleurs, ou termine légitimement son travail disponible)
   -- et NE PAS supposer qu'un éventuel nouveau blocage est un autre
   bug d'allocateur sans preuve directe, ce correctif touchant un
   mécanisme (portillon de flux de contrôle) fondamentalement différent
   des trois précédents (appels `Leave` manquants). Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.97 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r376-return-gate-fix/README.md`.

2. **(contexte, r376 — voir ci-dessus)**


1. **r376 — continuation normale (PAS un blocage qualifié).** r375 a
   RÉFUTÉ la prémisse partagée de r371-r374 : capture EN DIRECT
   corrigée (première lecture au mauvais point d'instruction donnait un
   faux positif, corrigée dans le même cycle) montre que la
   vérification de cohérence fusion-arrière de `sub_821F85F8`
   (`r9==r7`) évalue à FALSE de façon CORRECTE (le voisin candidat
   `0x1009fa10` a `back=NULL`, `fwd=self` -- intrinsèquement
   incohérent) et refuse à juste titre de fusionner. `sub_821F85F8`
   n'est PAS le bug. La corruption est isolée entièrement dans la
   propre boucle de recherche de `sub_821F8A00` : son chemin
   grand/surdimensionné (`>=128`) parcourt une freelist triée SANS
   AUCUNE vérification que le candidat trouvé n'est pas le nœud en
   cours d'insertion -- l'explication la plus probable est que
   `0x1009fa10` était DÉJÀ LIÉ dans cette freelist au moment du 4e appel
   à `sub_821F8A00`. **Nommé pour r376** : (a) parcourir la freelist
   surdimensionnée EN DIRECT juste avant le 4e appel à `sub_821F8A00`
   pour confirmer directement que `0x1009fa10`/`0x1009fa18` y est déjà
   lié, plutôt que par inférence depuis la destination de l'écriture
   corrompue ; (b) si confirmé, tracer lequel des trois appels
   antérieurs à `sub_821F8A00` (ou sa propre logique de coalescence/
   allocation en amont) l'y a inséré sans retrait correspondant ; (c)
   n'appliquer un correctif natif qu'une fois cette asymétrie
   précisément vérifiée en direct. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.96 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r375-backward-merge-check/README.md`.

2. **(contexte, r375 — voir ci-dessus)**


1. **(contexte, r374)** r374 a
   testé et RÉFUTÉ deux hypothèses : (a) un chevauchement d'arrondi
   dans le stub natif `NtAllocateVirtualMemory` (trace EN DIRECT
   complète de tous les appels du run -- zéro chevauchement, zéro
   ajustement d'arrondi observé, hypothèse refermée) ; (b) la recherche
   interne secondaire de `sub_821F8368` nommée par r373 (capture EN
   DIRECT montre que sa propre valeur de retour pour l'appel corrompu
   est `0x100b0000`, pas `0x1009fa10` -- le candidat interne
   `0x1009fa10` ne sert qu'à sa comptabilité locale, jamais transmis :
   coïncidence d'adresse, pas un défaut). Relecture statique de
   `sub_821F85F8` contre son garde réel (`r6=0`) montre que le bloc de
   fusion-AVANT entier est sauté inconditionnellement pour cet appel --
   **seul le bloc de fusion-ARRIÈRE (`loc_821F8708`-`loc_821F8768`)
   peut être responsable.** Sa vérification de cohérence en deux
   parties (`r9==r7 && r9==r8`, dérivée de `*(r31+12)`/`*(r31+8)`) est
   maintenant la SEULE branche non auditée restante dans toute
   l'investigation. **PAS corrigé, PAS un blocage qualifié.** **Nommé
   pour r375** : (a) capturer en direct `r31`/`r11`/`r10`/`r9`/`r7`/`r8`
   à `loc_821F8708` de `sub_821F85F8` pour l'appel exact produisant
   `0x1009fa10`, pour déterminer si la vérification de cohérence passe
   (déchaînement exécuté, écartant ce chemin aussi) ou échoue
   (déchaînement sauté alors que la fusion/réinsertion continue quand
   même -- la forme exacte de bug qui expliquerait tout depuis r369) ;
   (b) n'appliquer un correctif natif qu'une fois un défaut précis
   vérifié en direct. Les décalages hôte des deux fonctions sont déjà
   sauvegardés dans
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r374-nav-hint-trace/disas_sub_821F85F8.txt`.
   Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.95 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r374-nav-hint-trace/README.md`.

2. **(contexte, r373/r374 — voir ci-dessus)**


1. **(contexte, r372)** r372 a
   étendu la sonde de r371 avec une capture de registres EN DIRECT à
   l'instruction exacte de l'écriture auto-référentielle
   (`__imp__sub_821F8A00+209`) : `r8` (= `r4+8`, le nœud en cours
   d'épissure) et `r14_be` (= la valeur PPC r11 écrite, le point
   d'insertion trouvé par la recherche) sont identiques bit pour bit
   (`0x1009fa18` == `0x1009fa18`), **`VERDICT self-write-confirmed=True`**
   -- ce n'est plus une hypothèse structurelle mais un fait vérifié en
   direct. `sub_821F8A00` lui-même ne contient AUCUNE logique de
   déchaînement (relu en entier) ; le déchaînement manquant doit donc
   être en amont, dans `sub_821F85F8` (fonction de coalescence de blocs
   appelée par `sub_821F92B8` juste avant `sub_821F8A00`), qui effectue
   deux blocs de déchaînement inline (fusion-avant et fusion-arrière),
   chacun gardé par des conditions pas encore auditées branche par
   branche. **PAS ENCORE une localisation confirmée du déchaînement
   manquant, PAS un blocage qualifié.** **Nommé pour r373** : (a)
   compléter l'audit branche par branche des deux blocs de
   coalescence/déchaînement de `sub_821F85F8` (fusion-avant autour de
   `loc_821F8654`-`loc_821F86BC`-`loc_821F8708` ; fusion-arrière autour
   de `loc_821F8708`-`loc_821F8768`), la même technique que r356 a
   appliquée à `sub_821FA6F8`, pour trouver la condition exacte sous
   laquelle un voisin fusionnable -- ou le bloc lui-même -- n'est pas
   déchaîné avant que `sub_821F8A00` ne réinsère son épissure ; (b)
   vérifier en direct avec un point d'arrêt conditionnel avant de
   proposer tout correctif ; (c) n'appliquer un correctif natif qu'une
   fois le mécanisme vérifié en direct, pas seulement plausible. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.93 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r372-register-capture/README.md`.

2. **(contexte, r372 — voir ci-dessus)**


1. **(contexte, r371)** r371 a
   armé un point d'observation matériel depuis le PLUS TÔT possible
   (juste après le `mmap()` de `GuestAddressSpace::GuestAddressSpace()`,
   avant même ses boucles de pré-touch) plutôt que depuis l'entrée de
   `sub_821D5F48` (r369, trop tardif). Trois écritures EN DIRECT sur le
   champ figé capturées avec pile d'appels et désassemblage :
   `sub_821F9E10` écrit d'abord `0x10000180` (tête, correct), puis
   `0x10082ac8` (toujours plausible), puis **`sub_821F8A00`** — déjà
   nommée dans ce projet comme « chemin des blocs surdimensionnés » de
   `sub_821FA6F8` — écrase avec `0x1009fa18`, l'adresse du champ
   LUI-MÊME : destination et valeur écrite prouvées identiques, la
   création de l'auto-référence est capturée sur le fait. Lecture
   statique de `sub_821F8A00` (`ppc_recomp.27.cpp:11454-11627`) : il
   divise un bloc libre et réinsère le reste via le MÊME motif
   d'épissure doublement chaînée déjà confirmé correct dans
   `sub_821FA6F8` (r356) ; les quatre écritures d'épissure sont
   structurellement saines, donc l'auto-écriture ne peut avoir lieu que
   si la recherche du point d'insertion retourne le nœud en cours de
   division lui-même — probablement parce qu'il n'a jamais été
   déchaîné de la freelist avant réinsertion. Hypothèse structurellement
   fondée, PAS ENCORE vérifiée en direct. **PAS ENCORE un correctif
   confirmé, PAS un blocage qualifié.** **Nommé pour r372** : (a)
   capturer EN DIRECT les valeurs réelles de `r11`/`r9` au moment de
   l'écriture fautive et comparer à l'adresse du bloc divisé pour
   confirmer laquelle coïncide ; (b) remonter pour trouver si/où un
   appel de déchaînement manque avant cette épissure, en auditant la
   même façon que r356 a audité `sub_821FA6F8` ; (c) n'appliquer un
   correctif natif qu'une fois le mécanisme vérifié en direct, pas
   seulement plausible. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.92 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r371-self-ref-write-located/README.md`.

2. **(contexte, r371 — voir ci-dessus)**


2. **(contexte, r370 — voir ci-dessus)**

3. **r370 (historique, PAS un blocage qualifié).** r369 a
   confirmé EN DIRECT, par comptage de déclenchements + capture de
   registres à travers ~56 000 itérations et ~15s (pas de simples
   échantillons PC séparés dans le temps), que le curseur de balayage
   de `sub_821F9E10` (`rsi`) est figé BIT POUR BIT sur toutes les
   observations : une véritable boucle infinie non bornée, confirmée,
   pas un balayage long-mais-fini. Lecture mémoire en direct : l'entrée
   à l'adresse invitée `0x1009fa18` a son propre `next` qui pointe sur
   elle-même au lieu de la tête de liste (`0x10000180`) — la condition
   de terminaison ne peut jamais devenir vraie. Un point d'observation
   matériel armé dès la toute première entrée de `sub_821D5F48` (~2s,
   bien avant le blocage) et maintenu 40s pleines ne s'est JAMAIS
   déclenché : la valeur auto-référentielle était déjà présente avant
   ce chemin d'appel ce cycle — donnée statique/pré-init, pas une
   écriture au runtime sur ce chemin. **PAS ENCORE un blocage
   qualifié** : deux possibilités restent ouvertes — (1) défaut de
   chargement des données statiques propre à cette recompilation
   native, ou (2) donnée fidèlement expédiée par le retail, avec une
   omission en amont dans la logique du jeu original. **Nommé pour
   r370** : (a) extraire les données statiques du XEX retail aux
   adresses `0x1009fa18`/`0x10000180` depuis l'image disque qualifiée
   et comparer octet pour octet avec l'observation en direct ; (b) si
   fidèle, remonter ce qui devrait peupler/relier ce bucket en amont ;
   (c) si défaut de chargement mécaniquement corrigeable, appliquer et
   vérifier via le motif de correctif déjà autorisé — sinon nommer
   précisément un point de décision pour l'utilisateur. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.90 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r369-scan-cursor-check/README.md`.

2. **(contexte, r369 — voir ci-dessus)**

3. **r369 (historique, PAS un blocage qualifié).** r368 a
   revérifié EN DIRECT la revendication d'épuisement de graphe d'appel
   de r341/r349/r350 contre le binaire D'AUJOURD'HUI (deux fuites
   corrigées) et l'a trouvée obsolète dans son cadrage : nouvelle
   technique fiable de `SIGINT` envoyé DIRECTEMENT au PID de
   l'inférieur (pas `gdb.execute("interrupt")` en thread python,
   déjà prouvé non fiable r353), confirmée sur quatre exécutions.
   `sub_821D7DE0` (disas) ne peut sauter sa vraie boucle par image par
   aucune branche une fois atteinte — mais Sonde 1 (60s, neuf sites
   d'appel/retour) montre ZÉRO déclenchement au-delà de l'entrée de
   fonction : l'exécution ne revient JAMAIS de son premier appel,
   `sub_821D5F48`. Sonde 2 : rafale de 293 passages dans la chaîne
   allocateur déjà épuisée (`sub_821D5600`→...→`sub_821FA6F8`) en
   <0,5s, puis silence total 59s — inchangé par les deux fuites
   corrigées. Sonde 3 (backtrace des 25 threads via SIGINT) : le
   thread principal a pris une AUTRE branche du répartiteur
   `sub_82121308` que celle épuisée par r341-350, atteignant
   `sub_8236B3F8`→`sub_8236E868`→`sub_823801B8`→`sub_821F9E10` — et y
   EXÉCUTE EN DIRECT. Sonde 4 (3 échantillons PC espacés de 4s) :
   tous dans une plage de 9 octets (`+2857`/`+2866`/`+2857`) au sein
   d'une boucle de balayage de table à deux étages — preuve directe de
   plusieurs secondes réelles passées ici, mais PAS une preuve de
   boucle infinie (aucun registre comparé entre échantillons). **Ceci
   corrige le CADRAGE de r341/r349/r350 (pas leurs mesures)** :
   `sub_821D5F48` a une branche jamais parcourue, atteinte seulement
   maintenant que les deux fuites sont corrigées et que le processus
   survit assez longtemps. **PAS (encore) un blocage qualifié** — un
   emplacement candidat, pas une dépendance externe confirmée. **Nommé
   pour r369** : (a) breakpoint sur la cible de rebouclage
   (`sub_821F9E10+2996`) et vérifier si le curseur de balayage avance
   réellement entre les déclenchements, lire ce que représentent
   `r8d`/`rdx` (échantillonnés `8093`/`8093`) depuis l'objet invité ;
   (b) si borné et complet mais blocage persiste ensuite, reprendre le
   parcours linéaire sur la suite ; (c) si le balayage cherche une
   correspondance structurellement impossible dans ce bac à sable
   (contenu disque/asset manquant), nommer alors explicitement le
   blocage qualifié — pas avant. Voir
   `reports/ac6-retail-native-codegen-gate2-r280-doc-deadlock-refuted-codec-starves-on-us-tbl-over-pal-pac-20260906.md`
   §4.89 et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r368-e20-loop-second-pass/README.md`.

2. **(contexte, r367-r368 — voir ci-dessus pour la suite)**

3. **r367 (historique, PAS un blocage qualifié).** r367 a
   instrumenté directement les points de décision de boucle de
   l'OUVRIER (`sub_8233A890`) au niveau désassemblage (deux offsets de
   branchement localisés : `+137` vérification initiale, `+343`
   vérification de continuation). Sonde 120s : l'ouvrier entre une
   fois, drapeau initial non nul, vrai travail par image entré une
   fois, mais la vérification de continuation ne se déclenche JAMAIS.
   Sonde 30s (chaque appel de l'itération instrumenté séparément) :
   `sub_8233DF90`/`sub_8233A830`(×2)/`sub_8233E2F0` chacun une fois ;
   `sub_82345CE0` (« attendre occupé ») exactement 3 fois, la 3e étant
   le SECOND appel d'attente de l'ouvrier lui-même juste après la fin
   du vrai travail — puis silence total sur ces 5 sites pour ~29s,
   pendant que `sub_82345C88` (primitive générique partagée) tourne à
   ~2800/s (processus vivant, pas planté). **Conclusion : le thread
   ouvrier exécute complètement sa première itération réelle, puis
   bloque LÉGITIMEMENT en attendant un second signal que le producteur
   (thread principal) n'envoie jamais — le côté OUVRIER est
   complètement EXONÉRÉ.** Ceci affine la découverte de r360/r366 côté
   producteur (le triplet `sub_8233B5A0`/`sub_8233A620`/`sub_8233A610`
   se déclenche UNE FOIS puis silence permanent jusqu'à 900s) : le
   producteur ne repose jamais la question une seconde fois. Question
   ouverte inchangée en nature, affinée en confiance : pourquoi
   `sub_821D7DE0` (boucle principale) n'exécute jamais une seconde
   itération, alors que r341/r349/r350 avaient déjà déclaré son graphe
   d'appel statique exhaustivement tracé et vide, bien avant la
   découverte des deux fuites de section critique (chaîne d'appel
   différente, depuis `sub_821D5F48`). **Nommé pour r368** : (a)
   revérifier la revendication d'épuisement de r341/r349 contre le
   binaire ACTUEL (deux correctifs de fuite déjà appliqués) plutôt que
   de faire confiance à une conclusion antérieure à ces correctifs ;
   (b) chercher si la CONDITION d'entrée de la boucle par image dépend
   d'une valeur de donnée à l'exécution (une cible d'appel indirect
   résolue différemment, ou une comparaison contre une valeur que ce
   bac à sable ne produit jamais) plutôt qu'un chemin de code manquant
   — r356 a déjà trouvé exactement cette forme de bug une fois ; (c) si
   cela aussi revient vide, nommer explicitement que l'instrumentation
   côté hôte approche la limite de ce qu'elle peut résoudre pour ce
   symptôme précis, et que continuer à y investir face à pivoter vers
   le câblage mort de `presented_frames` (r292, toujours non corrigé
   aujourd'hui) est une décision dont l'utilisateur pourrait vouloir
   être informé. Voir
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r367-worker-flag-check/README.md`.

2. **(contexte, r366-r367 — piste de fuite de section critique close ;
   voir ci-dessus pour la suite)**

3. **r366 (historique, PAS un blocage qualifié).** Sur choix
   explicite de l'utilisateur ("inventer un correctif maintenant"),
   r366 a appliqué et vérifié EN DIRECT le correctif INVENTÉ de
   `sub_821FA9E0` (`RtlLeaveCriticalSection` conditionnel sur `r23 & 1`,
   inséré juste avant l'unique retour de la fonction ; dérivé
   directement de la lecture du corps généré, pas deviné). Sonde
   globale de r364 (45s) : `sub_821FA9E0` maintenant **2/2, net 0**
   (était 3/0, net +3) ; résidu total `net=1` (uniquement le +1
   légitime déjà expliqué de `sub_821F9E10`) -- plus aucune fuite non
   identifiée sur ce verrou. **MAIS** re-exécution de la sonde EXACTE
   de r360 sur 300s post-correctif : `sub_821D7AE0`/`sub_821D7CD0` (la
   vraie boucle par image) ne se déclenche TOUJOURS JAMAIS, et le
   triplet de poignée de main (`sub_8233A620`/`sub_8233B5A0`/
   `sub_8233A610`) ne se déclenche qu'UNE SEULE FOIS puis se tait pour
   les ~299s restantes -- motif IDENTIQUE à r360, totalement INCHANGÉ
   par cette seconde correction réelle et vérifiée. **Conclusion
   centrale** : deux corrections de fuite de section critique
   indépendamment confirmées (r358, r366) n'ont fait avancer d'AUCUNE
   itération le test d'entrée dans la boucle par image -- la piste de
   fuite/blocage mutuel tracée depuis r311 se clôt ici à son terminus
   honnête (les deux fuites connues sur ce verrou sont maintenant
   corrigées et vérifiées, sa comptabilité est entièrement expliquée)
   SANS avoir répondu à la question posée par l'utilisateur
   ("pourquoi `presented_frames` reste à 0"). **Nommé pour r367** :
   pivoter vers l'instrumentation directe du côté OUVRIER du triplet de
   poignée de main (qu'est-ce qui change entre le succès de
   l'itération 1 et le silence de l'itération 2), en n'utilisant QUE
   des breakpoints locaux à la fonction ou des fenêtres longues sans
   gdb. **NOUVEAU risque d'instrumentation documenté** : le crochet
   global `RtlEnterCriticalSection`/`RtlLeaveCriticalSection` (utilisé
   depuis r364) a provoqué un SIGSEGV connu (course CREATE_SUSPENDED,
   r114/r280/r327/r339/r340) sur deux exécutions longues (45s et
   180s) ; la trace « verrou tenu, nouvelle pile d'appel » qui en a
   résulté est explicitement RÉTRACTÉE (capturée pendant/après le
   crash, pas en direct) -- confirmée dépendante de la durée par une
   exécution de contrôle à 20s qui sort proprement. **Ne plus utiliser
   ce crochet global au-delà d'environ 45s.** Voir
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r366-invented-fix/README.md`.

2. **(contexte, r365 — épuisé par r366 ci-dessus, décision suivante déjà prise et exécutée)**

3. **r365 (historique) — décision de l'utilisateur nommée
   par r365.** L'utilisateur a choisi l'option 1 de r364 ("continuer à
   remonter"). r365 a remonté la chaîne d'appel au-dessus de
   `sub_821FA9E0` jusqu'à épuisement : **correction à r364** -- les 4
   sites d'appel de `sub_823857E0` se partagent entre DEUX fonctions
   distinctes (`sub_8237FA48` : 2 sites, jamais appelée en direct dans
   ce scénario, confirmé par 225s cumulées de breakpoint EN DIRECT sans
   aucun coup ; `sub_8237FA50` : les 2 autres, confirmée comme le VRAI
   appelant par 3 backtraces identiques en direct sur `sub_821FA9E0`
   lui-même). Chaîne réelle : `sub_821FA9E0` <- `sub_823857E0` <-
   `sub_8237FA50` <- `sub_8237FB58` <- `sub_821F7B28` (marcheur
   GÉNÉRIQUE de table d'initialiseurs statiques du CRT -- deux plages
   de table fixes, appel indirect `bctrl` sur chaque entrée non nulle,
   ZÉRO section critique, zéro logique métier) <- `__imp___xstart`
   (machinerie hôte de création de thread) <- `std::thread` -- un
   thread OUVRIER dédié, pas la pile bloquée du thread principal.
   **Plus aucune chaîne d'appel invité à remonter** : la frontière
   invité/hôte est atteinte. Le candidat « frère » à motif partagé
   (`sub_821FB060`, dont les adresses retail chevauchent la queue de
   `sub_821FA9E0`) est un STUB DE CODEGEN INCOMPLET (`// ERROR
   821FB0B0` puis un `return` nu) -- pas une copie correcte à imiter.
   **L'option 1 est donc close sur ses PROPRES termes** (frontière
   atteinte, pas budget épuisé) : l'hypothèse « aucun niveau ne relâche
   ce verrou » est confirmée avec toute la profondeur possible.
   **Décision affinée nommée pour l'utilisateur** : (1) appliquer
   maintenant un correctif inventé dans `sub_821FA9E0` lui-même (choix
   réfléchi après épuisement de la remontée, pas un raccourci) ; (2)
   explorer un protocole armer/désarmer INTER-appels plutôt qu'une
   paire enter/exit du même appel (piste plus profonde, sans garantie
   de succès) ; ou (3) arrêter la fermeture du blocage par correctif
   natif et rediriger vers le câblage toujours mort de
   `presented_frames` (r292/r359) ou un autre angle. **NE PAS réarmer
   un `gdb.BP_WATCHPOINT` brut sur ce mutex** (crash de GDB, r362).
   Voir `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r365-upward-trace/README.md`.

2. **(contexte, r364 — épuisé par r365 ci-dessus)** Une sonde EN DIRECT globale filtrée par clé (une seule paire
   de breakpoints sur les points d'entrée hôte de
   `__imp__RtlEnterCriticalSection`/`__imp__RtlLeaveCriticalSection`,
   filtrée sur `*(uint32_t*)$rdi == 0x10000610`, comptée par adresse de
   retour de l'appelant -- au lieu du balayage statique des 242 sites
   nommé par r363) a **entièrement expliqué** le résidu `__count=4` de
   r361 : `sub_821F9E10` (575/574, net +1, structurellement incapable
   de fuir -- un appel légitimement en vol sur un autre thread) ;
   `sub_821FA6F8` (208/208, net **0** -- **le correctif r358 est
   confirmé PARFAITEMENT équilibré en direct**) ; `sub_821FA9E0` (3/0,
   net **+3** -- une SECONDE fuite Enter/Leave réelle, jamais
   corrigée). `1+0+3=4`, aucune source inconnue ne subsiste.
   `sub_821FA9E0` (1136 lignes générées) entre conditionnellement
   `*(r27+1408)` (même motif que `sub_821FA6F8` sur `*(r30+1408)`, même
   idiome de drapeau) mais **zéro** appel `RtlLeaveCriticalSection`
   nulle part dans la fonction, ni dans son unique appelant
   (`sub_823857E0`), ni dans les appelants de celui-ci (`sub_8237FA48`,
   tracés ce cycle). **Contrairement à `sub_821FA6F8`, aucun chemin de
   sortie frère déjà correct n'existe dans la même fonction pour servir
   de modèle** -- corriger ici exigerait d'INVENTER une logique de
   relâchement plutôt que d'en copier une déjà correcte, un changement
   d'une nature matériellement différente de celui autorisé pour
   r356/r358. **Décision nommée pour l'utilisateur** : (1) continuer à
   remonter la chaîne d'appel (`sub_8237FA48` et au-delà) pour trouver
   un site de relâchement authentique à imiter -- reste dans le motif
   déjà autorisé, mais peut prendre plusieurs cycles de plus, comme la
   chaîne à 13 fonctions de r354 ; (2) appliquer un correctif inventé
   par analogie (Leave sur le même bit de drapeau, au retour de la
   fonction) sans contrôle local prouvant que c'est le bon site --
   plus rapide, mais une décision d'une nature nouvelle ; ou (3)
   accepter l'état actuel (359943→4, un bug entièrement corrigé et
   vérifié) comme point final de cette piste et rediriger vers autre
   chose (p.ex. le câblage toujours mort de `presented_frames`,
   r292/r359). **NE PAS réarmer un `gdb.BP_WATCHPOINT` brut sur ce
   mutex** -- cela a fait planter GDB LUI-MÊME (r362) ; un breakpoint
   conditionnel simple (utilisé ce cycle) fonctionne bien. Voir
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r364-second-leak-hunt/README.md`.

2. **(contexte, r362-r364, déjà traité ci-dessus)**

3. **r361 — contexte historique.** r361 a
   CORRIGÉ la prémisse de r360 (« r316 jamais exécutée » était FAUX --
   r316-r325 avaient déjà entièrement achevé cette tâche, prouvant en
   direct un blocage circulaire AB-BA de manuel entre le thread
   principal et le worker sur ce verrou exact). r361 a re-exécuté la
   technique BYTE POUR BYTE de r325 sur le binaire POST-correctif r358
   : backtrace du détenteur IDENTIQUE à r325 (thread principal bloqué
   sur un futex en détenant `0x10000610`), **`__count=4` au lieu de
   `359943`** -- le correctif r358 a réduit l'AMPLEUR d'une contribution
   à la récursion détenue, mais PAS fermé le blocage : `__lock=1` tenu
   par un autre thread bloque le worker quel que soit le compte. Deux
   pistes à départager pour r362 : (a) une SECONDE paire Enter/Leave
   déséquilibrée, encore non corrigée, ailleurs dans les ~90 sites
   d'appel de l'allocateur (réutiliser les techniques statiques de
   r326-r333, restreintes maintenant à « qu'est-ce qui entre encore
   `0x10000610` sans Leave correspondant, post-correctif ») ; (b) un
   problème d'ORDONNANCEMENT structurel -- le propre Enter du thread
   principal n'est simplement jamais suivi de son Leave avant qu'il
   n'atteigne l'attente bloquante (`sub_8233B5A0`→`sub_82345CE0`). Ce
   sont deux bugs différents avec deux correctifs différents. Si (b),
   vérifier si le code retail Xbox 360 original emprunte ici un chemin
   plus étroit/différent avant de supposer qu'un correctif natif est
   même nécessaire -- ceci pourrait devenir un nouveau point de
   décision QUALIFIÉ. Ne PAS re-citer `presented_frames` ou le
   comportement de sortie du processus comme preuve que le blocage est
   résolu -- les deux sont maintenant des signaux confirmés non
   fiables. Voir §4.82 du rapport et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r361-postfix-holder-identify/README.md`.

## Historique (r360-r361, contexte précédent conservé)

1. **r360 — re-tester la méthode de r312-r314 (vraie boucle par image
   `loc_821D7E84` dans `sub_821D7DE0`, via `sub_821D7AE0`/
   `sub_821D7CD0`) POST-correctif r358, sur une fenêtre >=600s.**
   r359 a relu la lacune r311-r333 en entier et confirmé (r325/r326
   l'avaient déjà nommé) que le blocage circulaire résolu par r358 EST
   la continuation directe de la question r283-r297 (« pourquoi
   `presented_frames` reste à 0 »), pas un sujet séparé. r359 a aussi
   RECONFIRMÉ que `presented_frames` est câblé sur du code mort depuis
   r292 (`submit_ring()`, un seul appelant = un test unitaire),
   inchangé aujourd'hui -- **ne plus le citer comme signal de
   progrès** ; le signal correct est la trace `AC6_NATIVE_VD_TRACE`
   et/ou un décodage `PresentPacket`/`XE_SWAP`. Une sonde de 600s
   post-correctif a trouvé un NOUVEAU 6e lot d'anneau VD (jamais vu
   avant le correctif à AUCUNE fenêtre testée, 15s-180s) avec du
   contenu réellement neuf (write_index 19→25→31→37) -- preuve directe
   que le correctif a permis un progrès mesurable au-delà de tout ce
   qui était observé avant. Mais toujours zéro `PresentPacket` décodé,
   et le progrès s'arrête de nouveau après ce 6e lot -- question
   ouverte pour r360. r312-r314 avaient trouvé (entièrement AVANT le
   correctif) que le thread principal n'atteint JAMAIS la vraie boucle
   par image en 400s -- ceci n'a PAS encore été re-testé après le
   correctif et doit l'être en premier. Aucun blocage qualifié ce
   cycle. Voir §4.80 du rapport et
   `recompilation/ace-combat-6-retail/artifacts/retail-us-native-r359-postfix-longwindow/README.md`.

## Historique (r359, contexte précédent conservé)

1. **r359 — investiguer pourquoi `presented_frames` reste à 0 après le
   correctif de r358.** L'utilisateur a explicitement choisi
   « Apply the minimal native-side fix » (option 2 du point de
   décision qualifié r356/357). r358 a appliqué ce correctif via un
   NOUVEAU script TRACKÉ `tools/apply_sub_821fa6f8_leave_fix.py` :
   ajout d'un `RtlLeaveCriticalSection(*(r30+1408))` CONDITIONNEL
   (subordonné à `r25 != 0`) à `loc_821FA94C` (sortie du chemin B de
   `sub_821FA6F8`), reflétant la logique déjà correcte de
   `loc_821FA924`. Le bloc cible apparaissait deux fois (duplication
   par XenonRecomp d'un épilogue partagé avec `sub_821FA6F0`, même
   code retail, pas deux bugs) -- les deux occurrences sont patchées
   par cohérence. **VÉRIFIÉ EN DIRECT** via la cascade `A830Watch`
   déjà prouvée : `recursion=4` au lieu de `359943` au MÊME point de
   contrôle -- **la fuite confirmée depuis r324 est éliminée.** Un run
   propre de 90s se termine maintenant NORMALEMENT de lui-même
   (`generated entry terminated its own thread`) au lieu de rester
   bloqué -- le blocage mutuel circulaire confirmé (r315-338) ne se
   produit plus. **`ctest` 10/10, `pytest` 222/1 skip** -- aucune
   régression. **Cependant** : `presented_frames=0`,
   `entry_returned=0` toujours après ce même run de 90s -- le
   correctif résout le blocage SPÉCIFIQUE confirmé, mais ne démontre
   PAS à lui seul que le jeu atteint un gameplay effectivement rendu.
   Étant donné l'historique de ce projet (des centaines de cycles
   antérieurs à travers de nombreux sous-systèmes), il ne serait pas
   surprenant qu'un AUTRE goulot d'étranglement devienne maintenant le
   facteur limitant. Étapes pour r359 : — investiguer pourquoi `presented_frames` reste à 0 après le
   correctif de r358.** L'utilisateur a explicitement choisi
   « Apply the minimal native-side fix » (option 2 du point de
   décision qualifié r356/357). r358 a appliqué ce correctif via un
   NOUVEAU script TRACKÉ `tools/apply_sub_821fa6f8_leave_fix.py` :
   ajout d'un `RtlLeaveCriticalSection(*(r30+1408))` CONDITIONNEL
   (subordonné à `r25 != 0`) à `loc_821FA94C` (sortie du chemin B de
   `sub_821FA6F8`), reflétant la logique déjà correcte de
   `loc_821FA924`. Le bloc cible apparaissait deux fois (duplication
   par XenonRecomp d'un épilogue partagé avec `sub_821FA6F0`, même
   code retail, pas deux bugs) -- les deux occurrences sont patchées
   par cohérence. **VÉRIFIÉ EN DIRECT** via la cascade `A830Watch`
   déjà prouvée : `recursion=4` au lieu de `359943` au MÊME point de
   contrôle -- **la fuite confirmée depuis r324 est éliminée.** Un run
   propre de 90s se termine maintenant NORMALEMENT de lui-même
   (`generated entry terminated its own thread`) au lieu de rester
   bloqué -- le blocage mutuel circulaire confirmé (r315-338) ne se
   produit plus. **`ctest` 10/10, `pytest` 222/1 skip** -- aucune
   régression. **Cependant** : `presented_frames=0`,
   `entry_returned=0` toujours après ce même run de 90s -- le
   correctif résout le blocage SPÉCIFIQUE confirmé, mais ne démontre
   PAS à lui seul que le jeu atteint un gameplay effectivement rendu.
   Étant donné l'historique de ce projet (des centaines de cycles
   antérieurs à travers de nombreux sous-systèmes), il ne serait pas
   surprenant qu'un AUTRE goulot d'étranglement devienne maintenant le
   facteur limitant. Étapes pour r359 :
   (a) investiguer pourquoi `presented_frames` reste à 0 -- fenêtre de
       test plus longue et/ou instrumentation différente pour
       déterminer si un NOUVEAU goulot d'étranglement est devenu le
       facteur limitant, ou si plus de temps/un déclencheur différent
       est simplement nécessaire ;
   (b) envisager d'intégrer le script de correctif au pipeline de
       build habituel (invocation automatique après régénération du
       codegen) plutôt qu'une invocation manuelle après chaque
       `generate_native_guest.py` frais ;
   (c) ceci reste très plausiblement UNE PARTIE de la réponse à la
       question originelle de r283-r297 (« pourquoi `presented_frames`
       reste à 0 ») -- le blocage spécifique tracé depuis r311 est
       résolu, mais la question globale peut nécessiter d'autres
       correctifs encore non identifiés ;
   (d) NE PAS revenir sur le côté hôte déjà vérifié (sondeur VD,
       câblage de présentation, primitives d'attente bornées, ni la
       course `CREATE_SUSPENDED` déjà adressée r114/r280) sans preuve
       nouvelle et directe.
2. **Discipline** : resync `native` → `native-source` avant chaque build ;
   build sous cgroup ; ctest 10/10 ; pytest `tests/` ; pour poser un point
   d'arrêt sur une instruction précise, `break *(NOM_SYMBOLE+DÉCALAGE)` ;
   avant d'ajouter un nouveau verrou global à un site d'appel à haute
   fréquence, vérifier s'il peut être PAR CLÉ plutôt que global (piège
   introduit puis corrigé en r286/r287, sur le modèle déjà établi de
   `critical_section_for`) ; vérifier toute chaîne `\n` ajoutée dans
   `materialize_native_import_stubs.py` en régénérant et en lisant le
   fichier produit (`\\n` à deux caractères — et vérifier qu'aucun `#`
   n'apparaît par erreur en colonne 0 dans le texte C++ généré, ce qui
   casserait la compilation comme directive de préprocesseur invalide —
   piège rencontré et corrigé dans le MÊME cycle avant tout build) ; ne
   pas garder les traces brutes (réduire en extraits représentatifs) ;
   rapports + artefacts par cycle ; pas de commit tant que la gate racine
   échoue pour une cause étrangère au cycle — le dire dans le rapport.

## Contexte r310 (verrouillé) — corrige r309 : PAS un minuteur périodique, une course divergente

- Sur 150s : `sub_8233A610` reste FIXE à 2 occurrences (réfute le
  minuteur périodique) ; `sub_8233B378` continue à ~473/s (mise à
  l'échelle linéaire, PAS une rafale qui plafonne).
- Le compteur `+64` galope indéfiniment (>70 000 en 150s) — pas bloqué.
  La CIBLE de `sub_8233B5A0` doit donc croître plus vite que le
  compteur après un succès précoce — course divergente, pas minuteur.
- `sub_8233A890` reste vivant tout le temps (sondage `/proc` 90s) — pas
  de mort précoce.
- N'invalide PAS le constat de r309 sur r291 (10 minutes déjà testées
  sans effet sur `presented_frames`) — juste le MÉCANISME (course
  divergente, pas minuteur périodique).
- r311 doit lire directement cible et compteur aux invocations de
  `sub_8233B5A0` pour confirmer.

## Contexte r309 (verrouillé) — CLÔTURE r298-r308 ; PIVOT vers la vraie frontière de r297

- `objet+88`/`+96` de `sub_8233B378` sont son verrou PRIVÉ (Mutant/
  Événement générique), pas une ressource externe. La boucle est un
  compteur pur, borné uniquement par la cadence ~2ms des primitives
  hôte — explique exactement les ~459/s de r308.
- La cadence de ~15s de `sub_8233B5A0` (~6900 incréments) ressemble à
  un COMPTEUR/MINUTEUR LOGICIEL DÉLIBÉRÉ, pas un bug. TOUTE la chaîne
  (r283-r308) est maintenant confirmée fonctionner correctement.
- RÉCONCILIATION CRITIQUE avec r291 : 10 minutes déjà testées,
  ~180-420 cycles complétés, ZÉRO effet sur `presented_frames`.
  Répondre à la cadence CPU n'allait JAMAIS répondre à la présentation
  d'image (r294-r297 : le contenu d'anneau n'inclut jamais de vrai
  présent, quel que soit le nombre de cycles).
- r310 doit PIVOTER vers la seconde recommandation de r297 jamais
  achevée : localiser le code invité de CONSTRUCTION d'un
  `PresentPacket`, pas l'appel `VdSwap` lui-même.

## Contexte r308 (verrouillé) — corrige r303 ; la question devient quantitative, pas mécanique

- Relecture COMPLÈTE de `sub_8233B378` (82 lignes, r303 s'était arrêté à
  35) révèle une VRAIE boucle interne jamais vue par la mesure d'entrée
  de r303. Correction explicite nommée.
- Mesure directe (point d'arrêt sur le vrai site d'appel dans la
  boucle, `__imp__sub_8233B378+0xcd`) : ~459 itérations/seconde (13 761
  en 30s), tid=16.
- Malgré ceci, `sub_8233A610` (signal-occupé de `sub_8233B5A0`) ne se
  déclenche que 2 fois/30s — confirmation 1:1 avec les complétions de
  `sub_8233B5A0`.
- Le compteur `+64` avance VITE ; c'est la CIBLE de `sub_8233B5A0` qui
  exige des milliers d'incréments (~6900 estimé) par cycle. AUCUN
  mécanisme cassé ou lent trouvé nulle part — question maintenant
  QUANTITATIVE (combien de travail réel par cycle, pas pourquoi c'est
  lent par appel).
- r309 doit identifier l'unité de travail réelle de la boucle et ce qui
  fixe la cible.

## Contexte r307 (verrouillé) — le thread confirmé sonde légitimement, ni affamé ni bloqué

- TID OS réel de `sub_8233A890` capturé avec certitude via gdb
  (perturbation minimale, un seul arrêt ponctuel).
- Échantillonnage `/proc` de CE thread précis : 94% du temps en
  `hrtimer_nanosleep` LÉGITIME (sondage actif borné de `wait_mutant`,
  r288 — pas `futex_do_wait` comme r301 avait trouvé pour un AUTRE
  mécanisme/thread).
- ÉCARTE DÉFINITIVEMENT la famine OS ET une primitive bloquée pour ce
  thread précis. La question reste celle de r298/r299 : pourquoi la
  condition qu'il sonde prend-elle tant de tentatives (des dizaines de
  milliers à ~200µs chacune) ?
- r308 doit mesurer directement le rythme du signal-occupé de
  `sub_8233B5A0` sur la porte `+152` de `sub_8233A890` (jamais mesuré
  isolément) — même technique que r299.

## Contexte r306 (verrouillé) — corrige la prémisse : aucun réengendrement, retour au cadrage original de r298/r299

- Trace d'engendrement sur 90s IDENTIQUE OCTET POUR OCTET à celle de 30s
  de r305 — les 8 threads ouvriers sont créés UNE SEULE FOIS, tôt, JAMAIS
  réengendrés (même motif que r292-r294 pour l'anneau GPU, r298-r300
  pour la chaîne d'appel invitée).
- `sub_8233A890` n'est PAS réengendrée — c'est l'UNIQUE instance déjà
  caractérisée par r287-r299. L'angle de création de thread (r304-r306)
  est ÉPUISÉ.
- La question revient au cadrage ORIGINAL de r298/r299 avec pleine
  confiance : pourquoi la boucle interne de CETTE instance unique
  prend-elle 5-8s par itération.
- r307 doit identifier le VRAI TID OS de `sub_8233A890` et lui appliquer
  l'échantillonnage `/proc` de r301, ciblant cette fois le bon thread
  confirmé plutôt que deviné.

## Contexte r305 (verrouillé) — PLUS GRANDE CORRECTION JUSQU'ICI : `sub_8233A890` lui-même est engendré fraîchement, pas persistant

- Vrai répartiteur trouvé : `sub_823453E8` charge `task_function` depuis
  `*(descripteur+20)`, `task_argument` depuis `+24`, appelle
  indirectement.
- Sur 30s, 8 threads ont exécuté 6 fonctions de tâche DISTINCTES :
  `sub_8233B378` (confirme r303), `sub_8233B748` (nouvelle),
  **`sub_8233A890` LUI-MÊME**, `sub_823466C0` (×3), `sub_82344050`,
  `sub_8211C7A8`.
- CONFIRME ET AFFINE la correction de r304 : elle s'applique à
  `sub_8233A890`, l'objet central de toute l'investigation, pas
  seulement à `sub_8233B378`. N'annule PAS les mesures internes de
  r287-r299 (boucle propre à l'instance vivante), change seulement le
  CADRE (thread créé fraîchement, pas persistant).
- r306 doit tracer `ExCreateThread` filtré sur
  `task_function==0x8233a890` sur une fenêtre longue et trouver qui
  l'appelle — c'est la cible précise et correcte de « pourquoi 5-8s ».

## Contexte r304 (verrouillé) — `sub_821F8008` n'est PAS un répartiteur ; correction de la description de r283

- `sub_821F8008` : trampoline générique d'entrée de thread à usage
  unique (appelle le pointeur reçu, puis `ExTerminateThread`
  inconditionnellement). Aucune logique de répartition.
- `ExCreateThread` : 21 déclenchements en 15s, dont 8 SÉPARÉS avec
  `routine=0x823453e8` — PAS un pool fixe de 8 persistants créés une
  fois à l'amorçage. CORRECTION EXPLICITE de la description de r283.
- Modèle de création de thread PAR TÂCHE : un argument
  (`routine_argument`, `ctx.r8.u32`) détermine la tâche réelle.
- r305 doit capturer cet argument (variable dédiée, PAS
  `AC6_NATIVE_IMPORT_TRACE`) pour corréler quel argument mène à quelle
  sous-tâche, puis trouver qui décide de demander la tâche
  `sub_8233B378`.

## Contexte r303 (verrouillé) — écrivain du compteur trouvé, encore plus rare que tout le reste

- `sub_8233B378` (l'écrivain de `*(objet+64+16)`) trouvé en cherchant
  tous les appelants de `sub_82345C88`. Incrémente `*(objet+8)` sous
  section critique, écrit la nouvelle valeur via `sub_82345C88`.
- Balayage gdb : `sub_8233B378` — 1 SEULE occurrence en 15s. Appelée via
  `sub_823453E8` (routine ouvrier, r283) → `sub_821F8008`, un chemin
  DIFFÉRENT de celui vers `sub_8233A890`. Sur tid=16 (le thread « bruit »
  de r299 — pas du bruit pour CE mécanisme).
- Le pool d'ouvriers répartit vers des sous-tâches indépendamment
  rares — `sub_8233B378` pourrait être la VRAIE composante limitante,
  pas `sub_8233A890`.
- r304 doit lire `sub_821F8008` (le répartiteur) pour comprendre la
  condition de répartition.

## Contexte r302 (verrouillé) — les « sites jumeaux » attendent le MÊME objet, pas deux ouvriers différents

- `sub_8233B4D0`/`sub_8233B538` lus en entier : vraies boucles de
  relance (motif de `sub_82345CE0`, r298), TOUTES DEUX sur le MÊME objet
  `r31+64` (un troisième objet-porte, distinct de celui de r298/r299).
- Poignée de main en DEUX PHASES autour de `sub_8233E0A8`/VdSwap :
  `>=` cible (B4D0) → travail VdSwap → `>` cible rechargée (B538).
- CORRECTION EXPLICITE de l'hypothèse de travail de r301 : même objet,
  pas des ouvriers différents en séquence.
- r303 doit mesurer chaque phase indépendamment par gdb et trouver ce
  qui écrit `*(cet-objet+16)`.

## Contexte r301 (verrouillé) — échantillonnage OS : famine par l'ordonnanceur écartée comme explication probable

- ~15 des 29 threads : ~93% CPU en continu — probable source, par
  thread, du bruit agrégé de r289 (nombreux threads busy-pollant chacun
  leur propre objet-porte).
- AU MOINS UN thread : `futex_do_wait` sur TOUTE une fenêtre de 20s —
  réellement stationné, pas affamé par l'ordonnanceur.
- Écarte la famine OS comme explication probable pour la poignée de main
  suivie depuis r283 ; un thread stationné dans un vrai futex attend
  d'être RÉVEILLÉ (signal applicatif), pas de temps CPU.
- r302 doit lire `sub_8233B4D0`/`sub_8233B538` (jamais lus en entier) —
  la cadence de 5-8s pourrait être la SOMME de plusieurs attentes
  séquentielles sur différents ouvriers dans un seul appel.

## Contexte r300 (verrouillé) — la remontée dans le code invité est ÉPUISÉE (résultat négatif propre)

- `sub_82331E78` (43 lignes) : triviale, aucune attente propre.
- `sub_821D7DE0` (142 lignes, la vraie boucle de jeu, r283) : boucle
  INCONDITIONNELLE sans aucune attente/minuterie/pause, appelle
  `sub_82331E78` À CHAQUE itération.
- La chaîne ENTIÈRE (boucle principale → poignée de main → ouvrier) a
  été lue de bout en bout : aucun mécanisme de cadence délibéré dans le
  code invité examiné jusqu'ici.
- Candidats restants : famine du thread ouvrier par l'ordonnanceur OS,
  ou dépendance bloquante non examinée dans le vrai travail par image de
  l'ouvrier lui-même (`sub_8233DF90`/`sub_8233A830`/`sub_8233E2F0`,
  jamais lues en entier).
- r301 REDIRIGE vers l'échantillonnage d'état OS (`/proc`, sans gdb) —
  la lecture de code généré supplémentaire n'est plus la bonne méthode.

## Contexte r299 (verrouillé) — le vrai travail par image lui-même est l'événement rare

- Écriture exacte de `*(porte+16)` trouvée dans `sub_82345C88`
  (`PPC_STORE_U64(porte+16, valeur)`, second argument de l'appelant).
- `sub_8233DF90` (premier appel de vrai travail, un hit propre par
  itération d'ouvrier) : EXACTEMENT 2 hits en 15 s (~0,13 Hz).
- `sub_82345C88` : 6670 hits/15s, mais 6664 sur tid=16 (bruit sans
  rapport, jamais examiné — même leçon que r289 sur l'agrégat non
  filtré). Seulement 5 sur tid=2 (principal), 1 sur tid=18 (ouvrier).
- La question remonte d'un niveau : pourquoi `sub_82331E78` n'appelle
  `sub_8233B5A0` qu'environ une fois toutes les 5-8 secondes ? Rien dans
  `sub_8233B5A0`/`sub_82345CE0`/`sub_8233A890`/`sub_82345C88` n'explique
  une cadence de plusieurs secondes — l'écart doit être dans
  `sub_82331E78` lui-même ou un de ses appels non encore lus en entier.

## Contexte r298 (verrouillé) — confirmé et affiné par mesure directe + désassemblage

- Test de ratio 1:1 en direct (`sub_8233B5A0`:`sub_8233E0A8`, points
  d'arrêt simultanés) : chaque entrée dans `sub_8233B5A0` est
  immédiatement suivie d'une entrée dans `sub_8233E0A8`, même thread —
  confirme empiriquement la revendication causale de r297.
- `sub_8233B5A0` est en fait ENTIÈREMENT LINÉAIRE (aucune branche) —
  le blocage se trouve plus loin, dans `sub_82345CE0` (atteinte via
  `sub_8233A620`), qui contient une VRAIE boucle de relance côté INVITÉ :
  `sub_821F4128`/`sub_821F5868` répétés jusqu'à
  `*(porte+16) == cible`.
- Ceci N'EST PAS une correction de r297 — c'est l'identification précise
  d'OÙ et COMMENT le blocage se produit physiquement.
- r299 doit trouver l'instruction exacte, côté ouvrier, qui écrit
  `*(porte+16)`, et mesurer son propre rythme.

## Contexte r297 (verrouillé) — DÉCOUVERTE UNIFICATRICE : la porte CPU EST la chaîne VdSwap

- Pile d'appels gdb en direct : `sub_82331E78` → `sub_8233B5A0` (moitié
  productrice de la porte CPU, r283-r291) → `sub_8233E0A8` →
  `sub_8234F558` → `sub_82347158` → `sub_821F03B0` → `VdSwap`.
- `sub_8233B5A0` n'atteint `VdSwap` qu'APRÈS le succès (rare,
  ~0,3-0,7/s) de son attente-de-terminé sur la porte Mutant/événement.
- Réconcilie TOUT r283-r297 en UNE SEULE cause racine ; corrige la
  portée implicite de r293 (mesure du débit agrégé dominé par les
  échecs, pas les succès — pas de contradiction réelle).
- `presented_frames=0` n'est pas plusieurs blocages indépendants : le
  taux de succès de la porte CPU EST la question à résoudre (r298).

## Contexte r296 (verrouillé) — mécanisme entier expliqué ; `VdSwap` appelé mais périmé

- Trace d'entrée ajoutée à `publish_write_address()` : `VdSwap` EST
  appelé (deux fois/15s, bien câblé), mais ZÉRO commit — no-op périmé
  les deux fois.
- Diagnostic affiné : `discovered=1` et `write_index == already_read`
  EXACTEMENT (19==19, 31==31) — le sondeur autonome a déjà drainé la
  même valeur avant l'appel `VdSwap` de l'invité.
- Le mécanisme entier est maintenant expliqué sans spéculation hôte
  supplémentaire : sondeur correct (r293), câblage présentation correct
  (r295), contenu d'anneau sans jamais de vrai présent (r294), appel
  `VdSwap` réel mais toujours périmé (r296).
- L'investigation bascule ENTIÈREMENT côté INVITÉ pour r297.

## Contexte r295 (verrouillé) — CORRECTIF RÉEL appliqué ; ambiguïté résolue définitivement

- `bind_guest_vd()` construit maintenant un vrai `VulkanDevice` +
  `VulkanOffscreenTarget` et appelle `bind_offscreen()`. Confirmé
  fonctionnel EN DIRECT sur cet hôte (test dédié, exit 0, aucune note de
  repli ; `present_target_configured=1` dans chaque lot sur l'ISO US).
- Malgré le correctif, TOUJOURS zéro `PresentPacket` décodé,
  `presented_frames` toujours 0 — ÉLIMINE définitivement l'hypothèse
  « hôte ignorait silencieusement une présentation ». L'invité n'en émet
  simplement jamais avant l'arrêt de son anneau.
- L'investigation converge maintenant sur UNE SEULE question côté
  INVITÉ, recoupant r283-r295 : pourquoi l'exécution s'arrête-t-elle de
  progresser après un peu de travail initial, dans au moins trois
  sous-systèmes montrant ce motif identique.

## Contexte r294 (verrouillé) — de vrais dessins ont lieu ; second bug de câblage confirmé (`bind_offscreen()`)

- 28 `DrawPacket` réels + 8 `ImmediateShaderPacket` + synchronisation
  dans les lots 2/4/5 de la rafale VD — PAS juste de l'init PM4. Le jeu
  dessine réellement quelque chose.
- `present_target_configured=0` dans chaque lot, zéro `PresentPacket`
  décodé. `bind_offscreen()` (seul point d'écriture de `present_target_`)
  n'a AUCUN appelant réel dans tout `native/` — seul un test unitaire
  l'appelle. Même motif que `submit_ring()` (r292).
- Même si l'invité émettait un `PresentPacket`, le runtime réel ne
  pourrait actuellement ni l'observer ni agir dessus.
- r295 doit câbler `bind_offscreen()` (tâche d'intégration dédiée,
  construire un `VulkanOffscreenTarget` réel, l'appeler au bon moment
  dans `boot()`), PUIS relancer une fenêtre longue pour distinguer
  « l'invité n'émet jamais de présentation » de « l'hôte en ignorait
  une silencieusement ».

## Contexte r293 (verrouillé) — aucun couplage causal étroit entre les deux stalls ; le VD n'est pas bogué

- `discover_write_index_locked` relit directement le champ live de
  l'invité à chaque poll de 1 ms — pas de redécouverte qui pourrait
  manquer une mise à jour. Le silence GPU = l'invité n'écrit simplement
  plus, pas un bug hôte.
- Sonde combinée 30 s (CPU + GPU) : l'anneau GPU s'arrête en ~1 s, la
  porte CPU continue à taux constant (550-715k/s) sur TOUTE la fenêtre,
  sans changement au moment de l'arrêt GPU. Aucun couplage étroit
  observable — n'écarte pas une cause partagée de plus haut niveau
  (progression du jeu bloquée avant les deux), mais aucun lien direct
  entre les deux mécanismes.

## Contexte r292 (verrouillé) — `presented_frames` est câblé sur du code mort ; le vrai chemin GPU vit puis s'arrête, comme la porte CPU

- `NativeRuntime::submit_ring()` (seul écrivain de `presented_frames`)
  n'a AUCUN appelant réel — seul un test unitaire l'appelle. Le
  diagnostic ne peut jamais devenir non nul par le vrai chemin de code.
- Le vrai chemin, `NativeGuestVdService::drain_locked()`, piloté par un
  thread réel, EST vivant : 5 « drain accepted » réels en 15 s, aucun
  rejet. Sur 90 s : trace IDENTIQUE octet pour octet — l'activité GPU
  s'arrête complètement après le 5e drain, sans erreur.
- Motif IDENTIQUE à la porte CPU de r283-r291 (« progrès puis silence »)
  mais dans un sous-système DIFFÉRENT et sans lien causal établi —
  possibilité d'une cause racine partagée, non confirmée.
- `presented_frames=0` n'a jamais été un signal fiable depuis r280 — bug
  de diagnostic réel, séparé du blocage runtime recherché.

## Contexte r291 (verrouillé) — PIVOT : la porte n'est probablement pas la cause ; suivre le chemin GPU

- Fenêtre de 10 min sans gdb/trace : `presented_frames=0` inchangé,
  malgré ~180-420 itérations réussies de la porte estimées au taux de
  succès de r290. Écarte « il suffit d'attendre plus longtemps ».
- `presented_frames` reflète `backend_.present_count()` (Vulkan),
  incrémenté sur un `PresentPacket`/`XE_SWAP` — chemin de soumission GPU
  ENTIÈREMENT SÉPARÉ de la porte CPU (Mutant/événement) investiguée
  depuis r283. Aucun lien causal établi entre les deux.
- r292 doit suivre le chemin GPU (`draw_count`/`resolve_count`/
  `present_count`, puis `submit()`/`present_to_offscreen()`), PAS
  reprendre l'investigation de la porte CPU sans preuve nouvelle.

## Contexte r290 (verrouillé) — les deux hypothèses de r289 sont RÉFUTÉES : c'est un vrai spin avec un succès rare, pas un artefact

- Réutilisation de handle ÉCARTÉE (lecture de code, pas de build) :
  `g_next_handle` est un compteur unique strictement croissant, jamais
  recyclé ; `NtClose` n'efface même pas `g_mutants`. `0x120`/`0x121`
  désignent le MÊME objet pour toute la vie du process.
- Surcharge de gdb ÉCARTÉE (mesure contrôlée, même run) : rejoué le point
  d'arrêt de r286/r287 sur `sub_82345CE0` EN MÊME TEMPS que le heartbeat
  de r289. Résultat : 5 occurrences en 15 s (cohérent avec r286/r287),
  MAIS le débit `gate120`/`gate121` reste à 162 000-196 000/s à CHAQUE
  seconde — indiscernable de la mesure sans gdb. gdb n'est PAS la cause.
- CONCLUSION RÉELLE, confirmée côté moteur : ~190 000 tentatives/s, mais
  seulement ~0,3-0,7 succès PAR SECONDE (pas par 15s) — ratio d'environ 1
  sur plusieurs cent mille. C'est un spin/livelock réel, pas un artefact
  de mesure.
- CORRECTION EXPLICITE DE r287 (par son nom) : sa caractérisation
  « parfois ~100ms, parfois 1,4s+, aucune explication » décrivait
  l'écart entre succès rares comme si c'était la durée d'un appel
  bloquant. Ce n'en est pas un. Les autres conclusions de r287 (correctif
  de verrouillage par clé, capture du cycle en 120ms) restent correctes.

## Contexte r289 (verrouillé) — DÉCOUVERTE INITIALE : la porte ne s'arrête jamais, contredit r283-r287 (hypothèses tranchées en r290 ci-dessus)

- Compteur d'appels 1/s ajouté à `wait_event`/`wait_mutant`, filtré sur
  les handles exacts de la porte (`gate120`=Mutant, `gate121`=événement).
- RÉSULTAT : ~182 000 appels/s SOUTENUS sur ces deux handles, sur toute
  une fenêtre de ~79 s sans gdb, jamais un creux. `presented_frames`
  reste 0.
- CONTREDIT DIRECTEMENT r286/r287 : leur point d'arrêt gdb sur la MÊME
  instruction de comparaison avait mesuré ~4 occurrences/15s — six
  ordres de grandeur d'écart.
- Hypothèse dominante NON TRANCHÉE : la surcharge de gdb sur un point
  d'arrêt touché des centaines de milliers de fois/s pourrait avoir
  ralenti l'exécution réelle au point de faire paraître bloquée une
  boucle qui tourne en fait sans jamais s'arrêter. Alternative non
  écartée : réutilisation de handle. Si l'hypothèse gdb se confirme,
  ceci n'est plus un blocage mais un spin/livelock sans progression, et
  la caractérisation « variance élevée » de r287 doit être corrigée par
  son nom.

## Contexte r288 (verrouillé) — les deux primitives hôte sont innocentées, la variance est ailleurs

- `wait_event()`/`wait_mutant()` instrumentés (bornés, anomalie
  `elapsed_ms>5` seulement) — gardé, sur sa PROPRE variable dédiée
  `AC6_NATIVE_WAIT_TIMING_TRACE` (pas `AC6_NATIVE_IMPORT_TRACE`, qui
  réactive les vieilles traces par-appel r284/r286/r287 et flood — 487
  Mo/4,3 M lignes en <1 min mesuré directement ce cycle).
- RÉSULTAT SUR FENÊTRE PROPRE (75 s, US ISO, sans gdb) : exactement 2
  anomalies (14 ms, 6 ms), toutes `wait_event`, zéro `wait_mutant`. Ces
  deux primitives NE dépassent JAMAIS leur borne d'environ 2 ms.
  `presented_frames` reste 0.
- CONCLUSION : la variance de 1,4 s+ (r283-r287) N'EST PAS dans
  `wait_event`/`wait_mutant`. Reste à déterminer si elle est dans les
  écarts ENTRE appels (site d'appel invité `sub_8233A890` à instrumenter,
  r289) ou dans un sous-système non instrumenté (`NtReadFile`, non
  écarté) ou si `presented_frames` exige un jalon ultérieur distinct.

## Contexte r287 (verrouillé) — correctif réel gardé, symptôme non résolu, mais nouvelle compréhension

- Verrouillage par Mutant (pas un verrou global) — gardé, corrige un
  vrai convoi de verrou (un tiers thread sans rapport monopolisait le
  verrou partagé 6922 fois/15s), mais SANS EFFET sur le symptôme observé.
- DÉCOUVERTE CLÉ : le mécanisme peut compléter un cycle complet en moins
  de 120 ms (capturé directement) — ce n'est PAS un blocage fixe, c'est
  une VARIANCE non expliquée entre cycles rapides et cycles de 1,4 s ou
  plus. 90 s sans débogueur ne suffisent toujours pas à atteindre
  `presented_frames` > 0.

## Contexte r286/r285/r284/r283/r282/r281/r280 (verrouillés)

- Voir les rapports précédents (§4ter à §4septies du rapport r280) — tous
  les correctifs sont réels, gardés et confirmés individuellement, mais
  aucun n'a encore résolu le symptôme de la boucle de jeu.

## Résultat requis

Atteindre visiblement le début du gameplay Mission 01 avec le runtime natif,
sans substitution de frontbuffer, état synthétique, compteur injecté ni
fallback ReXGlue — sur l'identité US scellée (`6eefba42…` / `204c5e64…`),
disponible dans le bac à sable.
