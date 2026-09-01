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
12. **r107 a écarté une hypothèse concrète et vérifiable pour la valeur
    mystère de r106, sans deviner de correctif.** Ce projet analyse déjà
    un champ XEX déclaré (`kHeaderDefaultStackSize`, tag `0x00020200`)
    dans `native_xex.cpp` — candidat plausible pour ce qui devrait
    occuper l'adresse trouvée par r106. Vérifié directement (une ligne de
    diagnostic temporaire dans `ac6recomp_main.cpp`, fichier hôte
    maintenu à la main et TRACKÉ, restauré depuis sauvegarde après usage;
    `ctest` 9/9 reconfirmé) : `stack_size=0x40000` (256 Ko). **Écarté sur
    deux points** : (1) ce harnais analyse ce champ mais ne l'utilise
    JAMAIS ailleurs (`grep` complet : aucun hit avant ce cycle) — `r1` est
    codé en dur à `0x8ff00000` sans égard à la valeur déclarée du titre,
    une vraie lacune de fidélité indépendante, notée mais pas corrigée
    ici (ne pas la vendre comme un correctif du plantage r105/r106 sans
    qu'un futur cycle vérifie qu'elle change réellement la valeur à
    `0x8feffd1c`); (2) 256 Ko n'explique pas de façon plausible une
    quantité de 4 Mo/8 Mo — écarté par mesure directe, pas par déduction.
    **Résultat négatif documenté**, même discipline que les cycles
    1111/1113 cités dans CLAUDE.md : élimine proprement une hypothèse
    plausible plutôt que de la laisser comme supposition non testée pour
    un futur cycle. Aucun code natif modifié. **Les deux options de
    r106 restent inchangées** : tracer une séquence de boot noyau/
    chargeur plus large (hors de portée du désassemblage statique seul),
    ou scoper explicitement ceci comme limitation du harnais. Voir
    `reports/ac6-retail-native-codegen-gate2-r107-xex-stack-size-ruled-out-parsed-but-unused-20260901.md`.
13. **r108 a fermé la chaîne de crash GATE2 (r100-r107) — corrige r106
    par son nom.** r106 s'était trompé de cadre : le pointeur mystère
    (`0x8feffd1c`) N'est PAS hors de toute chaîne d'appel tracée — c'est
    une évasion de pointeur de cadre invisible à une recherche
    d'écritures indexée sur `ctx.r1` seul. `Function_821D5F48` passe
    `r1+96` à `sub_821F4820`, qui écrit `r31+12` (= `r1+108`) à travers
    ce pointeur, depuis une valeur obtenue via `__imp__MmQueryStatistics`.
    **Cause racine réelle** : ce stub HLE (`tools/materialize_native_import_stubs.py`)
    n'avait aucun cas dédié et tombait dans le générique
    (`ctx.r3.u64 = kOfflineStatus;`), qui ne touche JAMAIS la mémoire
    invité du tampon de sortie — la valeur "non initialisée" était des
    octets de pile périmés, jamais du territoire noyau/chargeur.
    **Corrigé** : nouveau cas `MmQueryStatistics` écrivant les deux
    champs réellement lus (RAM physique 512 Mo bien documentée de la
    Xbox 360, granularité 4 Ko déjà établie par le `rlwinm`-12 de
    l'appelant : `0x20000` pages totales, `0x18000`=384 Mo disponibles;
    champs non lus ailleurs mis à zéro, pas devinés). **Vérifié en
    direct** (instrumentation temporaire, restaurée) :
    `field+12=0x18000000`, allocation RÉUSSIT désormais
    (`r3=0x16f70000`, non nul, contre échec systématique avant). Sonde
    d'entrée rejouée sans instrumentation : atteint la borne de 30s SANS
    crash, contre SIGSEGV systématique dans `sub_821D6C20` à chaque
    cycle depuis r100. **Prochain cycle** : ré-appliquer l'étape 1 du
    plan en cours (vérification statique de ce qu'attend réellement le
    guest post-GATE2 — NE PAS supposer que c'est encore
    `sub_821E6AC8`, cette chaîne d'appel n'a pas été re-tracée depuis
    que le crash r100 l'a rendue caduque) avant d'implémenter un
    quelconque chemin de signal. Voir
    `reports/ac6-retail-native-codegen-gate2-r108-mmquerystatistics-was-the-uninitialized-source-fixed-20260901.md`.
14. **r109 a réfuté, par mesure directe, une hypothèse plausible pour le
    blocage post-GATE2.** `Function_821D5F48` contient (après GATE2) une
    boucle appelant `sub_821CC508(r29)` tant qu'il retourne 0 — forme
    identique au motif de poll mémoire déjà documenté côté démo
    (r1829-1833, superseded). **Vérifié en direct** (instrumentation
    temporaire à deux tours, restaurée; `ctest` 9/9 reconfirmé) : la
    boucle NE tourne PAS — `sub_821CC508` retourne `-1` dès le premier
    appel (état `3`), prend la sortie de secours partagée, et
    `Function_821D5F48` retourne proprement `r3=0` à `sub_821D7DE0` en
    microsecondes. Aucun code natif modifié. **Le blocage réel de 30s
    reste en aval**, non atteint par ce cycle — prochain candidat :
    tracer `sub_821D7DE0` lui-même (ce qu'il fait du retour `r3=0`),
    avec la même technique d'instrumentation validée plutôt que GDB
    (retiré par r104 pour cette sonde). Voir
    `reports/ac6-retail-native-codegen-gate2-r109-post-gate2-dispatcher-resolves-cleanly-real-stall-still-downstream-20260901.md`.
15. **r110 a trouvé que la sonde est non-déterministe d'une exécution à
    l'autre, même sans GDB — corrige r109 sur une erreur de portée (pas
    sur sa conclusion substantielle).** r109 avait DÉDUIT que
    `sub_821D5F48` retournait proprement à partir de l'absence de
    prints supplémentaires — jamais observé directement. r110 pose des
    marqueurs sur les deux chemins possibles et confirme que la branche
    de sortie EST bien prise (conclusion de r109 sur la boucle
    confirmée). **Mais** cinq exécutions identiques (même binaire,
    mêmes entrées, 20s chacune) donnent des résultats différents : 3/5
    restent bloquées juste après l'entrée de `sub_821D7DE0` (avant même
    la boucle post-GATE2); 2/5 progressent jusqu'à la boucle de
    préchauffe; une exécution isolée a segfault. **Conséquence** : les
    conclusions "GATE2 réussit sans crash" (r108) et "la boucle se
    résout proprement" (r109) décrivaient le résultat d'UNE seule
    exécution chacune, pas le comportement typique. Cause non identifiée
    (deviner refusé, même discipline que 1111/1113). Aucun code natif
    modifié. **Prochain cycle : toute conclusion sur le comportement de
    la sonde doit s'appuyer sur PLUSIEURS exécutions** avant d'être
    rapportée; puis caractériser où (probablement dans GATE1-GATE5,
    avant `loc_821D6358`) la divergence entre exécutions apparaît. Voir
    `reports/ac6-retail-native-codegen-gate2-r110-probe-is-run-to-run-nondeterministic-without-gdb-20260901.md`.
17. **r111 a trouvé la source réelle du non-déterminisme de r110 : une
    VRAIE course entre threads, pas un artefact de mesure.** 46 points
    de contrôle dans la région GATE1-5, puis capture directe d'un crash
    via `gdb --batch -ex run -ex bt` (usage passif, distinct du
    pas-à-pas que r103/r104 avaient jugé peu fiable) : deux exécutions
    sur quatre reproduisent un SIGSEGV identique dans
    `__imp__sub_82346428`, appelé via `sub_823453E8`←`sub_821F8008`,
    sur un **VRAI thread OS séparé** (`start_thread`/`__clone3` dans la
    pile). Vérifié directement : `ExCreateThread`
    (`tools/materialize_native_import_stubs.py:271-301`) lance
    réellement un `std::thread` détaché avec seulement 64 Ko de pile
    (`g_next_thread_stack.fetch_sub(0x10000u)`) — pas un stub inerte.
    **Résout r110 sans le contredire** : le point de blocage apparent du
    thread principal variait vraiment d'une exécution à l'autre, mais à
    cause d'une course avec ce second thread qui plante, pas d'un
    non-déterminisme dans `sub_821D5F48` lui-même. Cause exacte du crash
    (offset +414 dans `sub_82346428`) non établie — deviner refusé.
    Aucun code natif modifié. **Prochain cycle** : tracer
    `sub_82346428` statiquement pour l'instruction exacte, puis évaluer
    bug invité réel (façon r108) vs. pile de 64 Ko insuffisante pour ce
    thread. Voir
    `reports/ac6-retail-native-codegen-gate2-r111-nondeterminism-source-is-a-real-background-thread-race-20260901.md`.
19. **r112 a trouvé que dix-huit threads démarrent en parallèle, avec
    deux sites de crash distincts, et a capturé l'instruction de crash
    exacte.** Un lot de dix répétitions `gdb --batch` (technique
    passive de r111) montre 18 `[New Thread ...]` par exécution, pas
    un seul — et deux sites de crash reproductibles, tous deux atteints
    via le même trampoline `sub_821F8008` : `sub_82346428` (via
    `sub_823453E8`, 2/10) et `sub_821D4C20` (direct, 2/10).
    **Instruction capturée en direct** : `call *(%r12,%rax,2)` avec
    `rax=0` — appel indirect via `[r12]`, suivi (si atteint) d'un
    `RtlEnterCriticalSection` sur le même objet — dispatch façon
    vtable AVANT acquisition du verrou censé le protéger, cohérent
    avec une course de lecture non synchronisée (valeur de `r12` non
    capturée — taux de reproduction très variable, 2/10 puis 0/18,
    a empêché une nouvelle capture ce cycle). Aucun code modifié
    (investigation gdb/objdump pure). **Prochain cycle** : recapturer
    `r12`; tracer `sub_821D4C20`; vérifier si `ExCreateThread` devrait
    lancer ces 18 threads avec un ordre/rythme que le stub actuel ne
    modélise pas (threads suspendus jusqu'à reprise, par ex.). Voir
    `reports/ac6-retail-native-codegen-gate2-r112-eighteen-threads-spawn-concurrently-crash-is-an-unsynchronized-vtable-read-20260901.md`.
21. **r113 a confirmé `r12` non mappé au crash `sub_82346428`, ET
    trouvé que le crash ORIGINAL `sub_821D6C20` (r100) se produit
    toujours, par intermittence, sur le THREAD PRINCIPAL, après r108.**
    `r12` capturé = `0x7ffe75980000`, confirmé inaccessible par gdb —
    pas nul, véritablement invalide, cohérent avec une origine non
    initialisée (même classe que `MmQueryStatistics`/r108, champ
    différent non identifié). **Séparément** : une capture a montré
    `sub_821D6C20` planter à nouveau sur le thread principal (`main →
    __xstart → sub_821D7DE0 → sub_821D6C20`), pas sur un thread
    d'arrière-plan — ne contredit pas la découverte spécifique de r108
    (l'allocation GATE2 réussit vraiment) mais montre que "la chaîne de
    crash est fermée" n'a jamais été une affirmation universelle. Non
    reproduit dans 15 tentatives de suivi. Aucun code modifié
    (investigation gdb pure). **Prochain cycle** : recapturer
    `sub_821D6C20` avec désassemblage complet; envisager un balayage
    plus long vu le taux de reproduction ~10-20%. Voir
    `reports/ac6-retail-native-codegen-gate2-r113-r12-is-unmapped-garbage-and-the-original-main-thread-crash-still-happens-intermittently-20260901.md`.
23. **r114 a trouvé la CAUSE RACINE complète : appel via un pointeur de
    fonction invité NUL, et le stub `ExCreateThread` ne lit jamais
    `CreationFlags`.** `sub_821D4C20` (2e site de r112) capturé avec
    désassemblage complet : instruction et valeur de `r12` IDENTIQUES à
    `sub_82346428` — `objdump` confirme `r12` est une CONSTANTE figée à
    la compilation (`movabs $0xffffffff7e980000`), pas une lecture
    mémoire runtime. Correspond exactement à la macro
    `PPC_CALL_INDIRECT_FUNC` (`rex/ppc/context.h:126-131`) : le
    compilateur replie la partie constante dans `r12`, laissant `y*2`
    (`rax*2`) comme seule partie variable. **`rax=0` aux deux crashes
    signifie `y=0`** — appel d'un pointeur de fonction invité NUL, sans
    vérification, provoquant le débordement d'adresse observé.
    **Mécanisme entièrement expliqué.** Cause du pointeur nul :
    `ExCreateThread` (`tools/materialize_native_import_stubs.py:271-301`)
    lit r3/r6/r7/r8 mais JAMAIS r9 (`CreationFlags`, qui porterait
    `CREATE_SUSPENDED` sur le vrai matériel — signature XDK publique, à
    revérifier). Les 18 threads démarrent donc tous immédiatement,
    expliquant le mécanisme ET la sensibilité au timing depuis r110.
    Corrige r112 ("course non synchronisée" → déterministe une fois
    `y=0` connu) et affine r113 (`r12` jamais non initialisé — c'est le
    SLOT de pointeur invité en amont qui est encore nul). Aucun code
    modifié — changement d'infrastructure trop lourd pour ce cycle.
    **Prochain cycle** : implémenter la création suspendue dans
    `ExCreateThread` (lire r9, parquer le thread jusqu'à un vrai
    `NtResumeThread`/`KeResumeThread`), vérifier la valeur réelle de
    `CREATE_SUSPENDED`, puis re-tester si les crashes cessent. Voir
    `reports/ac6-retail-native-codegen-gate2-r114-root-cause-found-null-guest-function-pointer-plus-excreatethread-ignores-creationflags-20260901.md`.
25. **r115 a implémenté l'étape suivante de r114 : `ExCreateThread`
    respecte maintenant `CreationFlags`, avec `NtResumeThread`/
    `KeResumeThread`.** Confirmé d'abord que ces deux imports sont
    RÉELLEMENT utilisés par ce XEX avant d'implémenter. Nouveau
    `park_until_resumed()` (attente NON bornée, distincte du
    `wait_event()` borné 2ms de r91) parque le thread créé avec
    `CREATE_SUSPENDED` (0x4, convention XDK publique à revérifier)
    jusqu'à un vrai resume. 3 nouveaux tests (dont un garde explicite
    contre la réutilisation accidentelle de `wait_for`), suite complète
    134/134. **Résultat vérifié en direct, honnête** : `sub_82346428`
    n'a pas planté sur 15 essais, mais `sub_821D4C20` ET le crash
    ORIGINAL `sub_821D6C20` (r100) ont chacun planté une fois — **PAS
    une correction complète**, réduction directionnelle seulement (pas
    de taux quantifié fiable sur 15 échantillons). Deux explications
    ouvertes non tranchées : tous les 18 threads ne sont peut-être pas
    créés suspendus, OU l'ordonnancement du resume a sa propre course.
    Gates : `ctest` 9/9, pytest 134/134, compteur démo inchangé (185).
    **Prochain cycle** : instrumenter quels appels `ExCreateThread`
    posent réellement `CREATE_SUSPENDED`; tracer la dépendance de
    reprise pour les threads qui plantent encore; refaire un balayage
    plus large avant de conclure. Voir
    `reports/ac6-retail-native-codegen-gate2-r115-suspended-thread-creation-implemented-reduces-but-does-not-eliminate-crashes-20260901.md`.
27. **r116 — les vraies sections critiques éliminent COMPLÈTEMENT les
    crashes r111-r115, exposant le null global ORIGINAL de r101,
    maintenant déterministe.** Trace ajoutée d'abord (motif `DbgPrint`)
    : AUCUN des 18 threads ne demande `CREATE_SUSPENDED` — réfute
    l'hypothèse centrale de r114/r115. Vrai bug trouvé :
    `RtlEnterCriticalSection`/`RtlLeaveCriticalSection` étaient des
    no-ops sous l'hypothèse "un seul thread invité" (réfutée par ce
    projet même). Corrigé avec de vrais `std::recursive_mutex` par
    objet (clés sur l'adresse invité, motif `g_events`). 4 tests,
    suite 135/135. **Vérifié en direct** : 25 exécutions — ZÉRO crash
    de thread d'arrière-plan (élimination complète), MAIS le thread
    principal plante DÉTERMINISTIQUEMENT (25/25) dans `sub_821D6C20`
    avec `rbp = 0x82935d98` — **exactement le global nul de r101**,
    jamais réellement corrigé, masqué jusqu'ici par la course. Gates :
    `ctest` 9/9, pytest 135/135, démo inchangé (185). **Prochain
    cycle** : rouvrir directement `0x82935d98` (r101); vérifier la
    pertinence de commits antérieurs à cette session (DPC/interruption
    graphique) avant de re-dériver; le crash étant maintenant
    déterministe, l'instrumentation build-tree (r105/r108) suffit en
    UN seul run au lieu d'un balayage répété. Voir
    `reports/ac6-retail-native-codegen-gate2-r116-real-critical-sections-eliminate-the-race-expose-r101s-original-null-global-deterministically-20260901.md`.
29. **r117 — chaîne causale COMPLÈTE fermée, du crash au site
    d'écriture manqué.** `FindPpcAddressMaterialization.java`
    (read-only) trouve UNE SEULE matérialisation de `0x82935d98` dans
    tout le XEX, à l'intérieur de `sub_821D5F48` lui-même (la même
    fonction de ~1700 lignes déjà caractérisée par r105-r109),
    ~1100 lignes APRÈS la boucle post-GATE2. **Vérifié en UNE SEULE
    exécution** (crash déterministe depuis r116) : `post-GATE2 dispatch
    ret=-1` → `BAILOUT sub_821D5F48 exits early via loc_821D6138 --
    0x82935d98 NEVER written`. Chaîne complète : `sub_821CC508` retourne
    -1 pour l'état 3 (déjà capturé par r109) → sortie de secours →
    `return;` GENUINE avant l'écriture → `sub_821D7DE0` n'abandonne pas
    (r102) → `sub_821D6C20` lit le nul → crash. **Ce n'est PAS un
    nouveau mécanisme** — c'est le MÊME échec d'état 3 que r109 avait
    déjà trouvé, maintenant compris comme LA cause racine réelle du
    crash de r100. Aperçu statique de `case 3`
    (`loc_821CC5EC`) : routine substantielle d'allocation/init de pool
    (`sub_821F4170`+`sub_821F3BF0`), pas encore tracée jusqu'au retour
    exact. Aucun code modifié. **Prochain cycle** : tracer `case 3`
    jusqu'à son retour exact, vérifier d'abord une lacune du harnais
    (façon r108) avant un bug invité. Voir
    `reports/ac6-retail-native-codegen-gate2-r117-full-causal-chain-closed-write-site-to-crash-20260901.md`.
31. **r118 corrige r117 par son nom : la cible "case 3" était fausse
    sous le build actuel.** r117 avait repris "état=3" de r109 SANS le
    revérifier sous le build r108/r116-corrigé. Ré-instrumenté état+case3
    dans la MÊME exécution : `state(+324)=0`, pas 3 — la valeur d'état
    n'est pas fixe, elle dépend de l'historique d'exécution que r108/r116
    ont changé. Tracé l'état 0 (case 0 tombe en fallthrough dans le MÊME
    corps "case 3") — un print placé dedans n'a JAMAIS déclenché,
    prouvant qu'il n'est pas atteint. **Résolu** en relisant
    `sub_821CC508` depuis l'entrée : DEUX portes précèdent le dispatch
    par état (déjà visibles dans la transcription de r109, jamais
    suivies) — la seconde, un OCTET GLOBAL (`lis r21,-32108`/`-18120`),
    doit valoir EXACTEMENT 1 pour atteindre le switch par état; sinon
    ça saute vers `loc_821CC800` puis, si ≠2, vers `loc_821CCD4C` — une
    TROISIÈME région jamais examinée. Aucun code modifié (deux tours
    d'instrumentation sur deux fichiers, tous restaurés). **Prochain
    cycle** : lire/instrumenter cet octet global, confirmer
    `loc_821CCD4C` avant de le tracer. Voir
    `reports/ac6-retail-native-codegen-gate2-r118-r117s-case3-target-was-wrong-real-bailout-gated-by-a-global-mode-byte-20260901.md`.
33. **r119 — l'octet de mode global vaut RÉELLEMENT 2 (pas 0 ni 1); le
    -1 vient d'une boucle de nouvelle tentative bornée.** Ce cycle avait
    RE-supposé l'octet à 0 par déduction avant de l'instrumenter — même
    piège que r118 venait de corriger. Instrumentation directe :
    `octet = 2`. Chemin réel : `loc_821CC800` tombe en fallthrough dans
    une TROISIÈME table de dispatch (base `-32227/-14244`), distincte
    des deux précédentes. Origine du -1 tracée précisément : un
    compteur de nouvelles tentatives à `+22896` de l'objet — atteint 0
    → `sub_821D4988` (log/diagnostic) → retourne -1; sinon décrémente
    et boucle. La condition réutilise le MÊME couple
    `sub_821F4E70`/`sub_821F50A0`→`sub_821F75F0` (champ PAR THREAD à
    `ctx.r13+336`) déjà lu par r117 — mécanisme juste, mal attribué au
    switch à l'époque. Cause exacte non établie (contenu réel de
    `ctx.r13+336`, valeur initiale du compteur) — deviner refusé.
    Aucun code modifié. **Prochain cycle** : instrumenter `ctx.r13` et
    le compteur `+22896`; lire la chaîne de `sub_821D4988` pour nommer
    le sous-système en échec directement. Voir
    `reports/ac6-retail-native-codegen-gate2-r119-real-path-is-a-bounded-retry-loop-checking-per-thread-status-at-ctx-r13-plus-336-20260901.md`.
35. **r120 corrige une spéculation non vérifiée de r119 :** `sub_821D4988`
    ne "ressemble" PAS à un log, ce N'EN EST PAS UN. Adresse de chaîne
    présumée lue via `DumpBytes.java` (read-only) : 128 octets de
    zéros. Lecture de la fonction elle-même : elle poste
    `{buffer, flags}` dans un TAMPON CIRCULAIRE protégé par de vraies
    sections critiques (r116), puis attend via `sub_821F5988`
    (résultat famille `258`/`STATUS_TIMEOUT`) — motif producteur
    classique, pas un `printf`. Ne change PAS la chaîne causale de
    r119 (le compteur `+22896` décide toujours) — corrige juste une
    description avant qu'elle n'induise en erreur. Aucun code modifié
    (lecture statique pure). **Prochain cycle** : reprendre les
    étapes de r119 — instrumenter `ctx.r13` et le compteur `+22896`.
    Voir
    `reports/ac6-retail-native-codegen-gate2-r120-sub_821d4988-posts-an-async-message-not-a-log-string-r119s-speculation-corrected-20260901.md`.
37. **r121 — CAUSE RÉELLE TROUVÉE : `NtReadFile`/`NtCreateFile` ne sont
    pas implémentés, le champ de statut attendu n'est jamais mis à
    jour.** Auto-correction en route : r117/r119 avaient lu à l'envers
    la branche de `sub_821F75F0` — repéré via un sentinelle placeholder
    dans le premier tour d'instrumentation, corrigé dans le même
    cycle. Avec la logique corrigée : `ctx.r13=0x0f000000` (=
    `kProbePcrAddress`, constante DÉLIBÉRÉE du harnais,
    `ac6recomp_main.cpp:17-30`, n'initialisant que quelques offsets) →
    `p256=0x0f001000` → `indirected=0xC00000BB` = **`kOfflineStatus`**
    EXACTEMENT, la constante que le stub générique renvoie pour tout
    import sans implémentation dédiée — mais qui n'écrit JAMAIS en
    mémoire invité elle-même, donc sa présence à `0x0f001160` prouve
    que du code invité l'y a copiée depuis un appel antérieur.
    **Confirmé via `AC6_NATIVE_IMPORT_TRACE`** (mécanisme existant,
    aucune nouvelle instrumentation) : `NtReadFile` (6×) ET
    `NtCreateFile` (4×) tombent toutes deux dans le stub générique —
    la boucle attend RÉELLEMENT et CORRECTEMENT une lecture de fichier
    jamais implémentée. Cohérent avec toute la chaîne établie depuis
    r109. Aucun code modifié — implémenter mérite son propre cycle
    (façon r108). **Prochain cycle** : implémenter
    `NtCreateFile`/`NtReadFile` contre l'infrastructure XDVDFS/média
    native existante, vérifier la convention d'appel réelle d'abord,
    puis re-vérifier en direct que `0x82935d98` s'écrit enfin. Voir
    `reports/ac6-retail-native-codegen-gate2-r121-real-cause-found-ntreadfile-ntcreatefile-are-unimplemented-status-field-never-updated-20260901.md`.
39. **r122 — convention d'appel réelle de `NtCreateFile`/`NtReadFile`
    vérifiée par désassemblage; implémentation DÉLIBÉRÉMENT
    DIFFÉRÉE.** Sept sites d'appel réels trouvés (groupés dans une
    seule région — un unique wrapper CRT-style). Désassemblage complet
    de `Function_82390F48` (la plus petite fonction contenant les deux
    appels) confirme directement la convention : `NtCreateFile(r3=&Handle,
    r4=DesiredAccess, r5=&ObjectAttributes, r6=&IoStatusBlock,
    r7=AllocationSize, r8=FileAttributes, r9=ShareAccess,
    r10=CreateDisposition)`; `NtReadFile(r3=Handle, r4=Event,
    r5=ApcRoutine, r6=ApcContext, r7=&IoStatusBlock, r8=Buffer,
    r9=Length, r10=&ByteOffset)` — 8 registres seulement, correspond à
    l'ordre NT/XDK réel. `ObjectAttributes` partiellement résolu : une
    VRAIE `ANSI_STRING` pointant vers `"\Device\Harddisk0\Partition1"`
    (Length=28, confirmé octet par octet). **PAS assez pour
    implémenter en sécurité** — layout complet non résolu (RootDirectory
    vs ObjectName, nom de fichier par appel). Décision délibérée de
    différer plutôt que de risquer le motif "implémenté avant
    vérification complète" (déjà vécu en r115). Aucun code modifié.
    **Prochain cycle** : résoudre le layout complet d'`OBJECT_ATTRIBUTES`
    avant d'implémenter, puis implémenter contre l'infrastructure
    XDVDFS/média existante. Voir
    `reports/ac6-retail-native-codegen-gate2-r122-ntcreatefile-ntreadfile-calling-convention-verified-from-disassembly-implementation-deferred-20260901.md`.
41. **r123 — `OBJECT_ATTRIBUTES` entièrement résolu; la lecture est à
    décalage fixe sur une partition disque dur peut-être absente.**
    Structure Xbox 360 réduite (3 champs, 12 octets) confirmée octet
    par octet : `RootDirectory=0`, `ObjectName`→ANSI_STRING statique
    (MÊME chaîne `"\Device\Harddisk0\Partition1"` de r122),
    `Attributes=0x40`=`OBJ_CASE_INSENSITIVE`. **`ObjectName` pointe
    DIRECTEMENT vers le périphérique — le tampon sprintf du nom de
    fichier par appel n'est JAMAIS référencé.** Lecture à
    décalage/longueur FIXES (0x800/0x400) — reformule l'hypothèse :
    sonde de structure système fixe sur un DISQUE DUR, pas un
    chargeur d'actifs générique. Titre disque-uniquement → l'échec
    pourrait être ATTENDU sur le vrai matériel (pas de HDD). Graphe
    d'appel tracé (`Function_82390F48`←`Function_82391A40`←1 appelant),
    PAS ENCORE connecté à la table de dispatch INDIRECTE de
    `sub_821F4E70`. **Reformule le correctif potentiel** : peut-être
    juste retourner un VRAI statut d'échec NT au lieu de
    `kOfflineStatus`, bien plus petit que "implémenter la lecture" —
    hypothèse concrète, PAS établie. Aucun code modifié. **Prochain
    cycle** : établir la connexion à `sub_821F4E70`; si connecté,
    tester l'hypothèse "statut d'échec" en direct. Voir
    `reports/ac6-retail-native-codegen-gate2-r123-object-attributes-resolved-and-the-read-is-fixed-offset-on-a-possibly-absent-hdd-partition-20260901.md`.
43. **r124 — connexion confirmée EN DIRECT : `sub_821F4E70` dispatche
    vers `NtReadFile`, l'appelant attend `STATUS_PENDING` comme issue
    normale.** Instrumentation d'une seule exécution (crash
    déterministe, r116) sur le `bctrl` : `target=0x823d035c` (×6 =
    décompte exact de r121) — l'adresse d'import de `NtReadFile`
    elle-même, PAS le cluster `Function_82390F48` de r122/r123 (site
    séparé). Mapping complet des registres confirmé, correspond
    exactement à la signature réelle : `r7=r31=&IoStatusBlock`,
    `r31+0` mis à `259`(`STATUS_PENDING`) juste avant l'appel.
    **L'appelant reconnaît EXPLICITEMENT `STATUS_PENDING` comme issue
    normale** (`r3<0` OU `r3==259` → même branche) — confirme depuis
    la LOGIQUE DU JEU (pas une inférence) toute la lecture
    "997/pending, retry" établie depuis r109. `kOfflineStatus` ne
    correspond à AUCUNE issue reconnue. **Reformule le correctif** :
    candidat prometteur = retourner `259` au premier appel puis faire
    évoluer l'IoStatusBlock vers un vrai statut avant épuisement du
    compteur — PAS encore conçu ni implémenté. Aucun code modifié.
    **Prochain cycle** : tracer `loc_821F4FE4`; concevoir un stub
    `NtReadFile` correct. Voir
    `reports/ac6-retail-native-codegen-gate2-r124-connection-confirmed-sub_821f4e70-directly-dispatches-to-ntreadfile-20260901.md`.
45. **r125 — CHAÎNE COMPLÈTE FERMÉE : deux imports non implémentés
    (`NtReadFile` ET `RtlNtStatusToDosError`) produisent ensemble la
    valeur que la boucle ne peut jamais accepter.** `loc_821F4FE4`
    appelle `sub_821F75B8` — LE setter EXACT correspondant au getter
    déjà tracé par r117/r119 (`sub_821F75F0`, voisin immédiat), écrit
    dans `[[ctx.r13+256]+352]`, l'adresse EXACTE de r119/r121. Avant
    d'écrire, appelle `__imp__RtlNtStatusToDosError` — l'import non
    implémenté LE PLUS appelé de toute la trace (43×, r121) — qui
    retombe dans le MÊME stub générique. **`RtlNtStatusToDosError`
    convertit NTSTATUS→Win32 : `STATUS_PENDING(0x103)`→
    `ERROR_IO_PENDING(997)` — LA constante 997 poursuivie depuis r109 a
    maintenant une source confirmée.** Chaîne complète bout en bout :
    vrai matériel = NtReadFile→PENDING→conversion→997→boucle reconnaît
    "en cours"→continue jusqu'à complétion réelle; CE harnais =
    NtReadFile→kOfflineStatus→conversion inchangée→boucle ne reconnaît
    RIEN→compteur épuisé→abandon -1→`0x82935d98` jamais écrit→crash de
    r100. **Ni l'un ni l'autre import seul ne suffit** — PENDING
    indéfiniment transformerait le crash en boucle INFINIE. Aucun code
    modifié. **Prochain cycle** : implémenter `RtlNtStatusToDosError`
    (petite table, faible risque) + concevoir `NtReadFile` pour une
    complétion RÉELLE — mérite son propre cycle dédié (façon r108/r116).
    Voir
    `reports/ac6-retail-native-codegen-gate2-r125-entire-chain-closed-two-unimplemented-imports-ntreadfile-and-rtlntstatustodoserror-20260901.md`.
47. **r126 — `RtlNtStatusToDosError` implémenté et vérifié en direct;
    NÉCESSAIRE mais PAS suffisant seul, exactement comme prédit par
    r125.** Implémenté la première moitié du correctif en deux parties :
    `STATUS_SUCCESS→ERROR_SUCCESS`, `STATUS_PENDING(0x103)→
    ERROR_IO_PENDING(997)`, défaut = `ERROR_MR_MID_NOT_FOUND(317)`
    (vrai défaut Windows NT documenté, pas deviné), tracé via
    `AC6_NATIVE_IMPORT_TRACE`. 1 nouveau test, suite 136/136.
    **Vérifié en direct** : les 43 appels (décompte exact de r121)
    convertissent `0xc00000bb` (`kOfflineStatus`, toujours depuis
    `NtReadFile` non implémenté) et retombent sur `317` correctement.
    **Comme prédit, n'arrête PAS le crash seul** — site de crash
    INCHANGÉ (`gdb --batch`, `sub_821D6C20`, identique depuis r116).
    Infrastructure réelle, sans risque de masquage. `NtReadFile` reste
    délibérément non implémenté. Gates : `ctest` 9/9, pytest 136/136,
    démo inchangé (185). **Prochain cycle** : tracer l'origine du
    handle de fichier de `sub_821F4E70` avant de concevoir le
    correctif `NtReadFile`. Voir
    `reports/ac6-retail-native-codegen-gate2-r126-rtlntstatustodoserror-implemented-verified-live-necessary-not-sufficient-20260901.md`.
49. **r127 — le handle de fichier de `sub_821F4E70` est TOUJOURS
    `INVALID_HANDLE_VALUE` — `NtCreateFile` n'a jamais produit de vrai
    handle.** Instrumentation d'une seule exécution (crash
    déterministe) sur les deux sites d'appel : le handle vient d'une
    PETITE TABLE GLOBALE indexée par un octet "type" (=0). Capturé :
    `handle=0xFFFFFFFF` (×6, décompte exact de r121) —
    `INVALID_HANDLE_VALUE`, sentinelle standard Win32/NT. **Décisif à
    lui seul** : aucune implémentation correcte de `NtReadFile` ne peut
    lire via un handle jamais valide. Confirmé comme VRAIE écriture
    délibérée (pas zero-fill — `GuestAddressSpace` garantit zéro au
    premier contact). `FindPpcAddressMaterialization.java` ne trouve
    PAS l'écrivain directement (probablement le même registre de base
    `r24`-style, matérialisé ailleurs). **Reformule le correctif** : le
    problème est EN AMONT de `NtReadFile`, au `NtCreateFile` censé
    peupler cette table. Aucun code modifié. **Prochain cycle** :
    localiser l'écrivain de `0x8293b93c` — cluster
    `Function_82390F48` de r122/r123, ou site séparé? Voir
    `reports/ac6-retail-native-codegen-gate2-r127-the-file-handle-is-invalid-handle-value-ntcreatefile-never-produced-a-real-one-20260901.md`.
51. **r128 — les watchpoints GDB sont AUSSI peu fiables sur cette sonde
    (étend r104); une piste de première tentative RÉTRACTÉE.** Deux
    techniques statiques épuisées, négatives (matérialisation
    lis+addi/ori absente pour `0x8293b93c`; grep exhaustif des stores
    `-18116` trouve 2 sites mais avec une base différente `-32099`;
    image statique du XEX confirmée à zéro, pas `0xFFFFFFFF`). Nouvelle
    technique tentée : watchpoint GDB passif (pose puis `continue`, PAS
    le pas-à-pas déjà retiré par r104). Première tentative : déclenché
    avec pile traversant `sub_82346428` (le crash de thread
    d'arrière-plan de r111-r116) — semblait une connexion réelle.
    **RÉTRACTÉ après vérification de reproductibilité** : deux
    répétitions déclenchent avec des piles COMPLÈTEMENT DIFFÉRENTES et
    incohérentes, `New value=<unreadable>` à chaque fois — GDB produit
    des déclenchements FANTÔMES sur cette sonde à 18 threads, pas de
    vrais événements d'écriture. Étend r104 à une deuxième
    fonctionnalité GDB. Aucun code modifié. **Prochain cycle** : NE PAS
    utiliser de watchpoints GDB ici; utiliser l'instrumentation
    build-tree fiable sur `sub_82346428` (candidat à vérifier
    proprement) et le cluster `Function_82390F48`. Voir
    `reports/ac6-retail-native-codegen-gate2-r128-gdb-watchpoints-are-also-unreliable-on-this-probe-a-false-lead-retracted-20260901.md`.
53. **r129 — l'écrivain trouvé : un VRAI nom de fichier `game:\DATA00.PAC`,
    et ce fichier existe RÉELLEMENT sur l'ISO retail déjà qualifié de
    ce projet — CE N'EST PAS un blocage de contenu manquant.** Script
    Python précis trouve `sub_821CC370(type, value) = table[type] =
    value` — l'écrivain, appelé UNIQUEMENT deux fois, très tôt dans
    `Function_821D5F48` : `sub_821CC370(0, 0x82067d40)` puis `(1,
    0x82067d54)`. **`0x82067d40` = `"game:\DATA00.PAC"`** (octets
    lus directement) — la table démarre avec des CHAÎNES DE NOM DE
    FICHIER, pas des handles; du code intermédiaire (pas localisé)
    doit les remplacer par un vrai handle ou `INVALID_HANDLE_VALUE` à
    l'échec — expliquant parfaitement la capture `0xFFFFFFFF` de r127.
    **TOUTE l'investigation r105-r128 tournait contre `assets/`**
    (contient SEULEMENT `default.xex`) — **jamais contre le vrai
    média**. L'ISO qualifié (`targets/ntsc-uj.json`, SHA-256
    `204c5e6...`) EXISTE à la racine du workspace, hash confirmé EXACT;
    `strings` trouve littéralement `DATA00.PAC`/`DATA01.PAC` dessus.
    Ferme la question pratique de r122/123/127 : le contenu EXISTE,
    sur un média déjà qualifié. Correctif entièrement déterminé en
    forme : `NtCreateFile`→`read_xdvdfs_file` (chemins `game:\`),
    `NtReadFile`→vrais octets. Aucun code modifié. **Prochain cycle** :
    implémenter les deux, PUIS relancer la sonde contre l'ISO qualifié
    (pas `assets/`) — nouveau prérequis établi ce cycle. Voir
    `reports/ac6-retail-native-codegen-gate2-r129-writer-found-real-content-exists-the-probe-just-never-used-the-qualified-iso-20260901.md`.
54. **r130-r131 : `NtCreateFile`/`NtReadFile` implémentés contre un nouveau
    `NativeGuestMediaService`, vérifiés en direct contre le VRAI ISO
    qualifié pour la première fois de toute l'investigation. Le "not
    found" initial de r130 (les trois fichiers `DATA00.PAC`/
    `DATA01.PAC`/`DATA.TBL`) était un bug du PARAMÈTRE `maximum_size` de
    `read_xdvdfs_file` (rejette tout appel >16MiB, indépendamment de la
    taille réelle du fichier) — pas un bug de recherche dans l'arbre.
    Corrigé par une nouvelle `locate_xdvdfs_file()` (résout offset/taille
    sans copie ni plafond) + lecture STREAMÉE directement depuis l'ISO
    (jamais un préchargement complet — `DATA00.PAC` fait ~2,1GiB).
    Les trois `NtCreateFile` réussissent maintenant en direct.**
    **LE CRASH ORIGINAL DE r100 (`sub_821D6C20`) EST CONFIRMÉ DISPARU** —
    remplacé par un nouveau crash déterministe dans `sub_821F7C80`
    (appelé via `sub_82390B18` <- `sub_821F8008` <- thread
    `ExCreateThread`), forme de déréférencement de pointeur nul
    (`rbp=rdx=r13=r15=0`), mécanisme pas encore établi par
    désassemblage. **Prochain cycle** : désassembler `sub_821F7C80`
    (vérifier complétude `.pdata` d'abord) pour localiser le champ nul
    exact et son lien probable avec le contenu de `DATA.TBL`/
    `DATA00.PAC`/`DATA01.PAC` tout juste lisible. Voir
    `reports/ac6-retail-native-codegen-gate2-r131-xdvdfs-maximum-size-parameter-was-rejecting-every-open-r100s-original-crash-site-is-confirmed-gone-20260901.md`.**
55. **r132 : le nouveau crash `sub_821F7C80` (r131) est causé par une
    LISTE DE NOTIFICATION CORROMPUE — `sub_821F7C80` diffuse un
    appel à tous les nœuds d'une liste chaînée intrusive (sentinelle
    `0x823F0C4C`, un seul enregistrant réel `0x82915FD8` avec un
    pointeur de fonction `0x82389BF8`). Mesuré en direct
    (`AC6_R132_DIAG`, diagnostic temporaire, entièrement annulé) : la
    diffusion réussit proprement ~17 fois, puis le dernier appel avant
    le crash lit `head=0x00009182` — ni la sentinelle ni le nœud
    connu — LA MÉMOIRE A ÉTÉ ÉCRASÉE entre deux appels. `NtReadFile`
    est EXCLU comme écrivain PAR MESURE DIRECTE (un seul appel avant
    le crash, `length=0`, donc zéro octet copié). L'écrivain réel
    n'est PAS localisé. Aucun code source modifié ce cycle.
    **Prochain cycle** : trouver l'écrivain de `0x823F0C4C` par
    bissection (snapshots d'entrée/sortie de fonctions candidates
    entre le dernier appel sain et celui qui crashe) — pas de
    watchpoint GDB (peu fiable sur cette sonde à 18 threads, r128).
    Voir
    `reports/ac6-retail-native-codegen-gate2-r132-sub_821f7c80-crash-is-a-corrupted-notification-list-ntreadfile-ruled-out-as-cause-20260901.md`.**
56. **r133 : L'ÉCRIVAIN DE LA CORRUPTION (r132) EST TROUVÉ — `sub_82234B88`
    parse un tampon `DATA.TBL` que `NtReadFile` n'a JAMAIS rempli.** Un
    watch global ajouté dans les macros `PPC_STORE_*` partagées (au lieu
    de deviner quelle fonction instrumenter) capture 12 écritures
    `STORE_U32` séquentielles depuis `0x823F0C32`, dont les octets
    combinés donnent EXACTEMENT `0x00009182` — la valeur corrompue de
    r132. Pile : `_xstart → sub_821D7DE0 → sub_821D5F48 → sub_821CC508 →
    sub_82234B88` (les deux fonctions du milieu sont la boucle de relance
    déjà tracée depuis r117). `sub_82234B88` parse un en-tête depuis son
    argument source `r4` pour calculer des bases de tableaux à
    échanger-en-place — SANS jamais référencer `0x823F0C30`
    littéralement. Mesuré en direct : `r4 = 0x173a0020`, EXACTEMENT
    l'adresse cible du seul appel `NtReadFile` de la session, qui
    demandait `length=0` et copiait `bytes_read=0` — le tampon contient
    des octets de POISON (`fe fe fe fe`), pas du vrai contenu. Chaîne
    causale complète et déterministe. Question restante (nommée) :
    pourquoi le jeu demande-t-il une lecture de longueur zéro ?
    `NtCreateFile` de ce projet ne renvoie jamais de taille — un titre
    réel l'apprend via `NtQueryInformationFile`/`GetFileSizeEx`, aucun
    implémenté ici. Aucun code source modifié ce cycle (3 diagnostics
    temporaires, tous annulés et vérifiés, ctest 9/9 + 139/139 Python
    après reconstruction propre). **Prochain cycle** : tracer l'appel
    entre `NtCreateFile("DATA.TBL")` et la lecture de longueur zéro pour
    trouver qui détermine la longueur demandée ; implémenter le vrai
    mécanisme de taille (déjà connu du runtime via
    `NativeGuestMediaService`/`locate_xdvdfs_file`) ; relancer la sonde
    et vérifier EN DIRECT que le crash `sub_821F7C80` disparaît. Voir
    `reports/ac6-retail-native-codegen-gate2-r133-writer-found-sub_82234b88-parses-a-zero-length-ntreadfile-buffer-as-a-real-header-20260901.md`.**
57. **r134 : la lecture de longueur zéro (r133) tracée jusqu'à un store
    CONDITIONNEL jamais pris dans `sub_821CC508` — corrige l'hypothèse
    "import de taille manquant" de r133.** Chaîne d'appel (`addr2line`) :
    `_xstart → sub_821D7DE0 → sub_821D5F48 → sub_821CC508 → sub_821F4E70
    → NtReadFile`. Mesuré en direct : `r30(record)=0x00000008` (quasi-nul,
    pas une vraie adresse), calculé via `[r26-18100]` où
    `r26=0x82940000` (MÊME base que la table de r129) —
    `r26-18100=0x8293B94C`, exactement 16 octets après `0x8293B93C` (la
    table de r129). Un SEUL écrivain trouvé (même technique de grep que
    r129) : un store gardé par un drapeau octet à `0x8293B938` (4 octets
    avant la table de r129) — si NON-ZÉRO, écrit `0x8293B94C` ; si ZÉRO
    (état observé), une branche ALTERNATIVE remplit 4 AUTRES champs
    (`0x8293B950/54/58/5C`) mais jamais celui-ci. Ce n'est PAS un import
    manquant — c'est du code déjà exécuté prenant la mauvaise branche.
    Aucun code source modifié ce cycle (3 diagnostics temporaires, tous
    annulés et vérifiés, ctest 9/9 + 139/139 Python après reconstruction
    propre). **Prochain cycle** : trouver ce que représente le drapeau
    `0x8293B938` (grep des stores vers `-18120(r26)`) ; déterminer si son
    état zéro est correct à ce point d'exécution, ou si les champs
    `0x8293B950-5C` (remplis par LA BRANCHE PRISE) sont en fait les vrais
    champs pertinents plutôt que `0x8293B94C`. Voir
    `reports/ac6-retail-native-codegen-gate2-r134-zero-length-read-traced-to-a-boolean-gated-store-that-never-populates-a-descriptor-field-20260901.md`.**
58. La traduction `IM_LOAD_IMMEDIATE` Xenos→SPIR-V reste ouverte. Aucun rendu
   présentable, titre, M01, campagne, save/replay ou mode offline n’est promu.
   Ne pas optimiser avant le début visible de gameplay.

## Frontières

Le N2 `reconstruction/ace-combat-6` est abandonné pour cette feuille de route;
sa preuve reste historique et aucune de ses sources n'est fusionnée. Ne pas
qualifier PAL, M02–M15, save/reload ou release avant fermeture Gate 2. Le
catalogue d'architecture local manque; aucune assertion générique n'en est
dérivée. ReXGlue reste oracle hors installation et le checkout XenonRecomp
reste ignoré/verrouillé.
