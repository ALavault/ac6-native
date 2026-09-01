# AC6 retail NTSC-U/J — Gate 2 runtime natif

## Résultat requis

Brancher le renderer natif derrière les imports Vd du runtime CPU Xenon généré
depuis le XEX US qualifié. Atteindre boot, titre, menus, cinématique puis début
visible de gameplay M01, sans substitution de frontbuffer, état synthétique,
compteur injecté ou fallback ReXGlue.

## Identité scellée

- cible: retail NTSC-U/J (`default.xex`), XEX
  `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`;
- ISO: `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`;
- Ghidra: `ghidra-projects/ac6-us/default.xex`,
  `PowerPC:BE:64:Xenon`;
- AC6_recomp: `09144bb092ad871584808aeead69c395edbd5200`;
- route: `recompilation/ace-combat-6-retail/routes/mission01-qualified-96.steps`,
  SHA-256 `771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043`;
- renderer oracle: ReXGlue, uniquement hors produit et hors installation.

## Travail courant

1. Codegen direct r11 : receipt `pass`, 81 fichiers, zéro diagnostic, 229
   imports et 19 832 mappings. Le profil `native` compile et installe
   `ac6recomp`; CTest **9/9**, pytest **130/130** (r92), audit d’installation et
   validator ordinaire passent.
2. Le renderer Vd natif consomme le WPTR primaire qualifié `object+10952`
   (index dwords), le readback exact `state+60` et les IB depuis la mémoire
   guest big-endian. La sonde r75 accepte `PM4_ME_INIT` (19 dwords) puis le lot
   IB bootstrap (12 dwords); elle expire encore après cette étape, sans retour
   guest ni gameplay visible.
3. **FERMÉ (r94) : le blocage `sub_821E6AC8`/`0x10001a00` chassé depuis
   r51 est réellement résolu**, pas contourné. r85-r93 avaient établi
   l'identité de l'objet, la santé du pipeline PM4/Vd, réfuté deux
   mécanismes de déblocage candidats (callback d'interruption r87/r88,
   filet de dépassement r93) et accepté un négatif borné. **r94 a trouvé
   et corrigé la cause réelle** : `drain_locked()` faisait passer les
   écritures `EVENT_WRITE_SHD` par `gpu_swap()` PUIS `store_guest_word()`
   — un double échange d'octets qui composait au lieu d'annuler une seule
   transformation voulue, corrompant chaque valeur de fence livrée à
   l'adresse `0x164e0000` déjà tracée depuis r86 (vérifié algébriquement
   contre les octets vivants observés : `05 00 00 00` — jamais expliqué
   en r89/r91/r93 — devient `00 00 00 05`=5 avec le correctif). Corrigé
   via une nouvelle `store_guest_bytes_raw()` (memcpy brut, pas de second
   bswap). **Vérifié en direct par reconstruction A/B isolée** (fichier
   unique stashé/restauré) : sans le correctif, comportement r93 identique;
   avec, le thread principal REVIENT de l'ancienne chaîne d'attente et
   bloque maintenant via une chaîne entièrement nouvelle
   (`sub_821F03B0←sub_8234F558←sub_8233E0A8←sub_8233B5A0`, jamais vue
   avant ce cycle) — progression réelle confirmée, pas une hypothèse.
   Voir
   `reports/ac6-retail-native-codegen-gate2-r94-double-endian-swap-fixed-main-thread-unblocked-20260901.md`.
4. **r95 a corrigé le formatage hex du décodeur et nommé précisément (pas
   corrigé) la plage de registres.** L'alerte r94 sur une adresse IB
   >32 bits était un artefact de formatage; adresse réelle `0x125c0000`
   (valide), header réel `0x00054800` → TYPE0 base=`0x4800`, count=6.
   **r96 a scanné le tampon indirect complet** (2840 dwords, dump GDB +
   parseur Python utilisant la logique de décodage exacte du projet, flux
   propre : 371 TYPE0 + 281 TYPE3, aucune désynchronisation) : registre
   maximum réellement touché `0x5002`. **Corrigé** :
   `XenosState::kRegisterCount` `0x4000`→`0x8000` — borne dérivée du
   FORMAT (`low_register_of` masque 15 bits, `0x8000` est la plage
   complète adressable par ce champ, pas une constante matérielle
   devinée), couvre le besoin observé avec marge. **Vérifié en direct** :
   rejet de plage de registres disparu; publication d'anneau avance de
   31 à 37 dwords (record de progression) avant un NOUVEAU rejet distinct,
   délibéré : `predicated TYPE3 packets are not supported`. **r97 a
   caractérisé ce rejet sans deviner de correctif** : seuls 2 des 281
   paquets TYPE3 du tampon portent le bit prédicat (`DRAW_INDX_2`,
   `WAIT_REG_MEM` — tous deux déjà implémentés); le premier opcode
   VRAIMENT inconnu (`0x46`) n'apparaît qu'à l'offset 400, après les deux
   prédiqués; `0x45` se répète ensuite 257 fois. Aucune infrastructure de
   prédicat n'existe dans ce code (`grep` : un seul résultat, le message
   de rejet lui-même); un test préexistant valide ce rejet comme
   délibéré. Deviner la sémantique matérielle risquerait une corruption
   visuelle SILENCIEUSE — refusé, besoin d'une source externe vérifiée
   (politique oracle du projet : "non" toute la campagne). **Prochain
   cycle, deux options indépendantes** : (1) obtenir une source Xenos PM4
   vérifiée pour le prédicat, correctif alors étroit
   (`native_xenos.cpp:169-171`); (2) **probablement plus haute valeur** :
   étudier les opcodes `0x45`/`0x46` (257+1 paquets, la majorité du
   contenu) depuis le désassemblage invité réel — sans le risque de
   corruption silencieuse (échec bruyant, pas une exécution incorrecte).
   **r98 a fait exactement ça** : `FindInstructionScalar.java` a tracé
   `0x45` à `sub_821EB8B8` (écrit une forme fixe de 6 dwords 256 fois,
   empaquetant 3 tableaux uint16 fournis par l'appelant, dont le
   producteur `sub_821F00C0` divise un compteur par 127/255 — forme de
   rampe/table de quantification) et `0x46` à `sub_821EBB40` (atteint le
   MÊME motif dépassement curseur/limite → `sub_821E60A8` déjà caractérisé
   r93/r94, structure identique à `INVALIDATE_STATE`/0x3B déjà
   implémenté). **Implémentés** en suivant des précédents établis dans ce
   code (0x45 comme `SET_BIN_MASK`, accepté structurellement sans
   modélisation sémantique; 0x46 identique à `INVALIDATE_STATE`) — pas
   devinés. Vérifié : tests unitaires reproduisant les paquets réels,
   décodage réussi. **Sonde vivante INCHANGÉE** : le décodage bute
   toujours sur le prédicat à l'offset 239 (r97, non résolu), 0x45/0x46
   étant plus loin dans le flux — correctif correct et vérifié, mais sans
   effet observable tant que la question du prédicat n'est pas résolue.
   **Le prédicat (r97) reste donc le SEUL blocage vivant restant** sur ce
   tampon. **r99 a tracé les DEUX paquets prédiqués jusqu'à leur
   construction réelle** : `DRAW_INDX_2` — bit constante figée, jamais
   calculée. `WAIT_REG_MEM` — bit GENUINEMENT conditionnel (bit 2 d'un
   argument), et le chemin prédiqué lit `object+0x2a94`/compare
   `0xBADF00D` — EXACTEMENT les champs du callback imbriqué r87/r88; la
   branche alternative retourne directement dans `sub_821E63F0` (le
   gestionnaire d'interruption déjà désassemblé). **Ce n'est pas deux
   occurrences indépendantes de prédication — le prédicat rejoint
   directement la lacune callback d'interruption déjà documentée sur cinq
   cycles (r85-r89).** Ceci RENFORCE le refus r97 de deviner : un
   correctif "toujours vrai" risquerait d'interagir mal avec cette lacune
   connue, pas seulement de deviner une sémantique isolée. Toujours aucun
   correctif implémenté. **Prochain cycle, options plus précisément
   cadrées** : (1) tracer l'appelant de `Function_821E6280` pour voir ce
   qui détermine le bit 2 de r5, et s'il corrèle avec l'enregistrement du
   callback déjà confirmé (r87) — question statique bornée; (2) un
   correctif étroit spécifique à `DRAW_INDX_2` seul (bit constant, plus
   sûr) — mais n'unbloquerait pas seul le tampon. Voir
   `reports/ac6-retail-native-codegen-gate2-r99-predicate-connects-to-interrupt-callback-gap-20260901.md`.
   L'identité du nouvel objet bloqué `sub_821E6AC8`/`sub_821F03B0` (r94)
   reste aussi non lue. Le passage sur les 76 sites d'appel de
   `sub_821E60A8` (r79, r93) et la piste `sub_821E65B0` sont hors de
   propos — ils caractérisaient l'ANCIEN blocage, résolu en r94. Ne pas y
   revenir sans raison nouvelle. r90-r92 restent valables (busy-spin
   `NtReleaseMutant` corrigé, `wait_event` bloquant, appelants événements
   réglés, `DbgPrint` disponible) — voir leurs rapports pour
   l'infrastructure de sonde toujours en place (`AC6_NATIVE_IMPORT_TRACE`,
   `AC6_NATIVE_VD_TRACE`). Conserver le poll limité au champ WPTR, jamais
   au contenu non publié du ring; ne jamais écrire de valeur synthétique
   pour faire avancer le
   décodeur.
5. **r100 a retiré le rejet du prédicat TYPE3** sur 4 preuves indépendantes
   (bits captures constants r99, aucun opcode prédicat dans le recensement
   complet r96, backend Vulkan déjà indifférent au bit, branche alternative
   `WAIT_REG_MEM` propre r99) — politique de décodage pour le contenu
   observé, pas une revendication matérielle générale; la lacune (un futur
   contenu avec un vrai opcode de prédicat déciderait pareil) est nommée
   dans le commentaire du code. A aussi corrigé un second bug pré-existant
   sans rapport (`ring[2]` sous-dimensionné dans un test r95/r96, vérifié
   antérieur à ce cycle par `git stash`). **Vérifié en direct** : record de
   progression, `vd publish write=49` (record précédent 37), nouvelles
   allocations jusqu'à 2 Mo jamais vues, un `vd swap commit`, quatre cycles
   de fence incrémentée. **Puis SIGSEGV** (pas l'expiration habituelle du
   probe) : appel indirect corrompu (`call *(%rcx,%rax,1)`) dans
   `sub_821D6C20`, atteint via `_xstart→sub_821D7DE0→sub_821D6C20`, `rax`
   dans la plage d'un pointeur hôte plutôt qu'une adresse invité valide (24
   heures de trace obtenues via un core dump `apport`, pas `gdb` en direct
   — la fenêtre du crash est sensible à l'ordonnancement et un `gdb` attaché
   ne l'a pas reproduit à 60s ni 180s). **Prochaine frontière concrète** :
   caractériser `sub_821D6C20` depuis une passe Ghidra propre — ce qui
   positionne le registre fautif au site d'appel — plutôt qu'une suite du
   fil prédicat, désormais clos pour le contenu observé. L'ancien fil
   d'objet bloqué `sub_821E6AC8`/`sub_821F03B0` (r94) est probablement
   caduc : la sonde ne l'atteint plus avant ce nouveau plantage. Voir
   `reports/ac6-retail-native-codegen-gate2-r100-predicate-decode-fixed-new-indirect-call-crash-20260901.md`.
6. **r101 a tracé le plantage r100 (`sub_821D6C20`) jusqu'à sa cause
   exacte, sans deviner de correctif.** `sub_821D6C20` répète ~24 fois le
   motif charger-un-pointeur-global→déréférencer-sa-vtable→appeler-un-slot
   (`FindInstructionScalar.java 0x5d98` : 26 occurrences dans TOUT
   l'exécutable, toutes des lectures, toutes dans cette seule fonction).
   **Valeur runtime lue directement dans le core dump `apport` du
   plantage** (pas l'image statique) : `0x00000000` — un vrai pointeur
   nul, cause mécanique exacte du `bctrl` sauvage. Le site de construction
   réel existe (`FindPpcAddressMaterialization.java` : motif singleton
   paresseux juste avant le prologue de `sub_821D6C20`, `0x821d6be8` —
   seul site d'écriture dans tout l'exécutable — qui réussit un appel
   virtuel différent juste après, l'objet est réel et constructible), mais
   `sub_821D7DE0` (`FindDirectCallsTo.java` : seul appelant de
   `sub_821D6C20`) ne l'atteint JAMAIS avant d'appeler `sub_821D6C20` —
   vérifié par dump complet de son corps. Trois explications restent
   compatibles avec cette preuve et aucune n'est distinguée : boot plus
   large de `_xstart` non atteint par cette sonde mono-thread, race
   multi-thread que la sonde ne peut pas gagner (un seul thread invité
   déterministe via `initialize_probe_thread`), ou stub d'import/XAM
   encore no-op qui déclencherait la construction. **Refusé de deviner**
   — même discipline que r97 sur le prédicat. Aucun code natif modifié.
   **Prochain cycle** : identifier la fonction contenant le bloc de
   construction (`0x821d6b60`-`0x821d6c1c`, pas encore nommée) et tracer
   SES appelants pour trancher entre ces trois hypothèses — question
   statique bornée, pas une suite du fil prédicat (clos). Voir
   `reports/ac6-retail-native-codegen-gate2-r101-crash-root-cause-uninitialized-service-singleton-20260901.md`.
7. **r102 a complété la chaîne causale du plantage r100/r101, sans
   deviner de correctif.** Le site de construction du singleton (r101)
   appartient à `Function_821D5F48` (`0x821d5f48`-`0x821d6c1b`,
   `GetFuncBounds.java`), qui EST le premier appel de `sub_821D7DE0`
   (`0x821d7dec bl 0x821d5f48`) — r101 était imprécis en disant que
   `sub_821D7DE0` "n'atteint jamais" le site : il atteint bien la
   fonction contenante. Dump complet (822 lignes) : aucun `blr`,
   exactement 2 sorties. Le bailout précoce (`0x821d6138`, `r3=0`) est la
   cible PARTAGÉE de 5 gardes internes distinctes (après
   `bl 0x82338300`, `0x821f4078`, `0x821cc508`, `0x821d28c8`,
   `0x821d5600`) — n'importe laquelle en échec saute au bailout,
   contournant la construction (`0x821d6be8`), atteignable seulement si
   les 5 réussissent. **`sub_821D7DE0` distingue l'échec** (appelle un
   diagnostic `sub_821F5B18` seulement si retour=0) **mais ne s'arrête
   pas** — les deux chemins reconvergent et continuent inconditionnellement
   vers `sub_821D6C20`. Chaîne causale mécanique complète. Tentative GDB
   en direct pour identifier LEQUEL des 5 gardes échoue : partiellement
   infructueuse, rapportée honnêtement plutôt que cachée — casser sur les
   5 fonctions-garde est ambigu (réutilisées ailleurs dans l'exécutable);
   casser sur `Function_821D5F48` elle-même (appelant unique confirmé) +
   `finish` a expiré à 90s (probablement surcoût `ptrace` sur ~800
   instructions et leurs appels imbriqués, pas une preuve de blocage — le
   crash prouve que la fonction retourne bien). Piste nommée SANS
   l'affirmer : si `sub_821F5B18` est un chemin fatal sur le vrai
   matériel (halt/exception plutôt qu'un retour), un stub natif qui
   retourne simplement expliquerait tout — hypothèse à vérifier, pas un
   fait établi. Aucun code natif modifié. **Prochain cycle** : identifier
   précisément quel garde échoue (breakpoint calculé sur l'instruction de
   comparaison côté appelant, pas sur l'entrée de fonction ambiguë) et
   tracer `sub_821F5B18` pour trancher diagnostic-seulement vs fatal. Voir
   `reports/ac6-retail-native-codegen-gate2-r102-crash-chain-traced-to-shared-bailout-and-swallowed-failure-20260901.md`.
8. **r103 a corrigé l'hypothèse r102 (elle-même), sans deviner de
   remplacement.** Traçage GDB filtré par adresse d'appelant (évite le
   problème de callee ambigu de r102), reproductible sur 2 runs
   identiques : seuls GATE1 (`sub_82338300`) et GATE2 (`sub_821F4078`)
   des 5 gardes internes de `Function_821D5F48` sont atteints avant
   `sub_821D6C20`. Mais la valeur de retour RÉELLE de GATE2
   (`r3=0x8feffcb0`, lue via un breakpoint temporaire à l'adresse de
   retour, offset `PPCContext::r3`=8 confirmé depuis les headers, pas
   deviné) est NON NULLE — son propre test (`cmplwi cr6,r31,0; beq
   cr6,bailout`) ne peut donc PAS causer le bailout, contrairement à ce
   que r102 supposait pour "un des 5 gardes". Relu intégralement le bloc
   entre le test de GATE2 et l'appel de GATE3 (~115 instructions, 3
   structures locales convergentes) : **aucune branche ne sort de cette
   plage** par lecture statique — pourtant GATE3 (`sub_821CC508`) n'est
   JAMAIS atteint sur les 2 runs reproductibles. Tentative de casser sans
   condition sur GATE3 seul : expiré à 240s sans le moindre coup ni
   crash. **Refusé d'interpréter ce hang comme "jamais appelé"** — ce
   projet a déjà documenté (r100/r101) que les sessions GDB attachées se
   comportent très différemment en timing des runs natifs sur cette sonde
   précise, et le code a un historique connu de comportement multi-thread
   sensible au timing (r90/r91) — un breakpoint supplémentaire changeant
   quel côté d'une race interne est pris est une hypothèse réelle, pas un
   prétexte. Le modèle structurel (une fonction, deux sorties, un site de
   construction atteignable seulement après les 5 gardes) reste correct
   en tant que fait STATIQUE (r101/r102 inchangés sur ce point) — ce qui
   était faux, c'est de supposer que N'IMPORTE LEQUEL des gardes cause le
   bailout par son PROPRE test; GATE2 réfute ça pour lui-même. Aucun code
   natif modifié. **Prochain cycle** : (a) tracer pas-à-pas (pas par
   breakpoint) le bloc GATE2→GATE3 pour observer directement pourquoi
   GATE3 n'est pas atteint plutôt que de se fier à l'absence de coup de
   breakpoint; (b) réexaminer si un des ~10 appels imbriqués de ce bloc
   (`sub_82221DD0`, `sub_82221F40`, `sub_82222D80`, `sub_823D009C`,
   `sub_821CC288`, `sub_821CC370`, `sub_821CC008`) pourrait lui-même ne
   pas retourner normalement (transfert style longjmp, un stub natif géré
   différemment du vrai matériel). Voir
   `reports/ac6-retail-native-codegen-gate2-r103-r102-gate-hypothesis-corrected-real-divergence-still-open-20260901.md`.
9. **r104 a clos la piste de traçage GDB en direct comme peu fiable pour
   cette sonde, après vérification statique croisée.** Désassemblé le
   code HÔTE compilé (x86, 220 instructions à partir de l'adresse de
   retour de GATE2) : concorde exactement avec la lecture PPC invité de
   r103 — aucune branche ne sort de l'intervalle GATE2→GATE3 dans les
   DEUX représentations, indépendamment. Fait nouveau noté (pas
   poursuivi) : `RtlInitializeCriticalSection` (un vrai stub d'import
   natif) est appelé DEUX fois dans cet intervalle. Écarté l'ambiguïté de
   symbole GDB comme explication (`sub_821CC508` et
   `__imp__sub_821CC508` résolvent à la même adresse). **4 sessions GDB
   en direct au total (r103+r104), 3 résultats DIFFÉRENTS** sur des
   scripts structurellement quasi identiques — c'est la découverte : le
   traçage `ptrace` de GDB change mesurablement le comportement
   d'exécution de cette sonde précise, run après run (au-delà de la
   sensibilité au timing déjà documentée r90/r91/r100/r101). **Décidé
   d'arrêter le traçage GDB en direct pour cette question** — disqualifié
   après 3 résultats différents sur 4 tentatives sans bug de script
   commun à toutes. La preuve STATIQUE (concordante sur les deux
   représentations) reste le signal fiable : aucune sortie de contrôle
   entre GATE2 et GATE3. La question de r103 ("pourquoi GATE3 n'apparaît
   jamais en direct") est mieux expliquée comme un artefact GDB que comme
   un comportement réel. Le modèle structurel r102 (une fonction, deux
   sorties, un bailout partagé) reste valable en tant que fait statique;
   **quel garde échoue, si un échoue du tout, reste non établi** — ni
   confirmé ni réfuté ce cycle. Aucun code natif modifié. **Prochain
   cycle** : ne PAS reprendre le traçage GDB en direct pour cette
   question précise; utiliser soit la méthode fiable déjà établie
   (run natif borné jusqu'à completion + inspection du core dump
   `apport`, comme r100/r101), soit une instrumentation temporaire dans
   le code généré `ppc_recomp` de `Function_821D5F48` (fichier C++
   ordinaire dans l'arbre de build, jamais commité) — pas encore tentée.
   `RtlInitializeCriticalSection` appelé deux fois mérite une vérification
   contre la table des stubs d'import natifs de ce projet. Voir
   `reports/ac6-retail-native-codegen-gate2-r104-gdb-live-tracing-unreliable-static-evidence-shows-no-skip-20260901.md`.
10. **r105 a trouvé la cause racine mécanique du plantage r100-r104, sans
    implémenter de correctif.** Suivant la recommandation r104 (arrêter
    GDB, instrumenter le code généré), instrumenté temporairement
    `ppc_recomp.23.cpp`/`.26.cpp` (source GÉNÉRÉ, jamais maintenu à la
    main, dans l'arbre de build ignoré par git — jamais commité,
    restauré depuis sauvegarde avant tout commit, `ctest` 9/9 reconfirmé
    après) avec des `fprintf` gardés par `AC6_R105_DIAG`, reconstruit,
    exécuté NATIVEMENT (zéro GDB/`ptrace`). **Résultat reproductible sur
    2 runs** : GATE2 (`sub_821F4078`) échoue réellement — **rétractant
    la lecture GDB de r103** (`r3=0x8feffcb0` non nul), exactement
    l'artefact que r104 avait anticipé sans le prouver. Tracé GATE2
    jusqu'à sa vraie cause : son `bl 0x823d012c` est le nom résolu par
    XenonRecomp pour l'import noyau **`MmAllocatePhysicalMemoryEx`**.
    Instrumenté ses arguments : sur 17 appels ce cycle, UN SEUL échoue,
    demandant **`r4=0xffc00000`** (`-4 Mo` signé ≈ 4,09 Go non signés),
    refusé à juste titre par `allocate_guest()`. **Tracé la taille
    jusqu'à sa source** : `r11 - 0x800000` (8 Mo), où `r11` vient de
    `lwz r11,108(r1)` — un slot de pile LOCAL. Grep exhaustif du corps
    entier de `Function_821D5F48` (1700+ lignes) : **aucune écriture** à
    cet offset. Vérifié l'appelant unique confirmé (`sub_821D7DE0`,
    r102/r103) : n'écrit rien non plus avant son `bl`. **C'est de la
    mémoire de pile non initialisée dans ce harnais** (contient 4 Mo au
    lieu des ≥8 Mo attendus). **Décidé de ne PAS deviner de correctif** :
    deux explications restent ouvertes — (1) dépendance cachée à une
    séquence d'appels du VRAI matériel qui laisse une valeur résiduelle
    appropriée à cet endroit physique de la pile, séquence que ce projet
    n'a pas encore tracée; (2) une vraie lacune de CE harnais
    spécifiquement (thread invité unique de `initialize_probe_thread`,
    déjà nommé comme limitation r101/r102). Écrire une valeur synthétique
    pour faire réussir cet appel masquerait potentiellement une vraie
    lacune plutôt que de la fermer — refusé, même discipline que r97.
    Aucun code natif modifié. Technique d'instrumentation temporaire du
    code généré validée comme alternative fiable à GDB pour cette sonde
    (à préférer désormais, per r104). **Prochain cycle** : tracer plus en
    amont dans `_xstart` ce qui pourrait laisser une valeur résiduelle à
    cet emplacement physique de pile sur un boot complet/le vrai matériel,
    ou accepter et scoper cela comme une limitation du harnais mono-thread.
    Voir
    `reports/ac6-retail-native-codegen-gate2-r105-crash-root-cause-uninitialized-stack-oversized-allocation-20260901.md`.
11. **r106 a précisément localisé le slot de pile non initialisé de r105,
    sans deviner de correctif.** Même technique (instrumentation
    temporaire du source généré, jamais commitée, restaurée après usage;
    `ctest` 9/9 reconfirmé) — ajouté un `fprintf` unique au site de
    lecture, imprimant l'adresse invité résolue et la valeur, éliminant
    tout risque d'erreur de calcul manuel. **Résultat direct** :
    `guest_addr=0x8feffd1c value=0x400000 (ctx.r1.u32=0x8feffcb0)` —
    concorde exactement avec le calcul de cadre (r1 initial `0x8ff00000`
    moins les prologues de `_xstart` `-0x1f0`, `sub_821D7DE0` `-0x70`,
    `Function_821D5F48` `-0xf0`, plus 108). **Cette adresse est SOUS
    `0x8feffe10` — hors du cadre de `_xstart` LUI-MÊME**, pas seulement
    de `Function_821D5F48`/`sub_821D7DE0`. `_xstart` étant le point
    d'entrée XEX (aucun appelant invité), rien dans la chaîne d'appel PPC
    tracée jusqu'ici ne possède cette mémoire. **Erreur de calcul manuel
    auto-corrigée avant de tromper le rapport** : un premier calcul (avant
    l'instrumentation directe) oubliait le propre prologue de `_xstart`,
    donnant une adresse fausse lue à zéro dans l'ancien core dump du
    plantage r100/101 — semblait d'abord contredire ce cycle. Recalculé
    correctement et relu la BONNE adresse dans le même core dump : zéro
    AUSSI — cohérent, pas contradictoire, car le core dump capture l'état
    au moment du plantage `sub_821D6C20`, bien plus tardif, après que
    cette région de pile a probablement été réutilisée. Pas de preuve de
    non-déterminisme réel (`GuestAddressSpace` utilise
    `MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE`, garanti zéro par Linux,
    vérifié directement dans `native_guest_memory.cpp`). **Toujours aucun
    correctif implémenté** : l'origine de cette valeur nécessite soit de
    tracer une séquence de boot noyau/chargeur PLUS LARGE que ce que ce
    harnais reproduit (`initialize_probe_thread` est minimal — PCR/TLS de
    base, pas un bring-up noyau complet), soit d'accepter cela comme une
    limitation de portée du harnais mono-thread. Deviner une valeur à
    écrire là masquerait potentiellement une vraie lacune — refusé, même
    discipline que r97/r105. Aucun code natif modifié. **Prochain cycle** :
    soit tracer plus large (documentation externe des conventions de
    bring-up de thread/pile du noyau Xenon, hors de portée d'une simple
    désassemblage statique du titre), soit scoper explicitement ceci
    comme limitation du harnais `initialize_probe_thread` et investiguer
    ce qu'un bring-up plus complet devrait fournir. Voir
    `reports/ac6-retail-native-codegen-gate2-r106-uninitialized-stack-slot-pinpointed-outside-xstart-own-frame-20260901.md`.
12. La traduction `IM_LOAD_IMMEDIATE` Xenos→SPIR-V reste ouverte. Aucun rendu
   présentable, titre, M01, campagne, save/replay ou mode offline n’est promu.
   Ne pas optimiser avant le début visible de gameplay.

## Frontières

Le N2 `reconstruction/ace-combat-6` est abandonné pour cette feuille de route;
sa preuve reste historique et aucune de ses sources n'est fusionnée. Ne pas
qualifier PAL, M02–M15, save/reload ou release avant fermeture Gate 2. Le
catalogue d'architecture local manque; aucune assertion générique n'en est
dérivée. ReXGlue reste oracle hors installation et le checkout XenonRecomp
reste ignoré/verrouillé.
