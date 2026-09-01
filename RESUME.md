# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord:

- `reports/handoff/CURRENT.json`;
- `reports/ac6-retail-native-codegen-gate2-r124-connection-confirmed-sub_821f4e70-directly-dispatches-to-ntreadfile-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r123-object-attributes-resolved-and-the-read-is-fixed-offset-on-a-possibly-absent-hdd-partition-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r122-ntcreatefile-ntreadfile-calling-convention-verified-from-disassembly-implementation-deferred-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r121-real-cause-found-ntreadfile-ntcreatefile-are-unimplemented-status-field-never-updated-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r120-sub_821d4988-posts-an-async-message-not-a-log-string-r119s-speculation-corrected-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r119-real-path-is-a-bounded-retry-loop-checking-per-thread-status-at-ctx-r13-plus-336-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r118-r117s-case3-target-was-wrong-real-bailout-gated-by-a-global-mode-byte-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r117-full-causal-chain-closed-write-site-to-crash-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r116-real-critical-sections-eliminate-the-race-expose-r101s-original-null-global-deterministically-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r115-suspended-thread-creation-implemented-reduces-but-does-not-eliminate-crashes-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r114-root-cause-found-null-guest-function-pointer-plus-excreatethread-ignores-creationflags-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r113-r12-is-unmapped-garbage-and-the-original-main-thread-crash-still-happens-intermittently-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r112-eighteen-threads-spawn-concurrently-crash-is-an-unsynchronized-vtable-read-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r111-nondeterminism-source-is-a-real-background-thread-race-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r110-probe-is-run-to-run-nondeterministic-without-gdb-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r109-post-gate2-dispatcher-resolves-cleanly-real-stall-still-downstream-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r108-mmquerystatistics-was-the-uninitialized-source-fixed-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r107-xex-stack-size-ruled-out-parsed-but-unused-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r106-uninitialized-stack-slot-pinpointed-outside-xstart-own-frame-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r105-crash-root-cause-uninitialized-stack-oversized-allocation-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r104-gdb-live-tracing-unreliable-static-evidence-shows-no-skip-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r103-r102-gate-hypothesis-corrected-real-divergence-still-open-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r102-crash-chain-traced-to-shared-bailout-and-swallowed-failure-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r101-crash-root-cause-uninitialized-service-singleton-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r100-predicate-decode-fixed-new-indirect-call-crash-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r99-predicate-connects-to-interrupt-callback-gap-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r98-opcodes-0x45-0x46-implemented-from-verified-code-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r97-predicate-semantics-need-external-source-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r96-register-count-widened-predicate-gap-named-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r95-decode-error-hex-fixed-register-range-named-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r94-double-endian-swap-fixed-main-thread-unblocked-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r93-threading-avenue-exhausted-76-site-pass-started-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r92-event-callers-settled-dbgprint-added-20260901.md`;
- `reports/ac6-retail-native-codegen-gate2-r91-wait-event-blocks-aggregate-impact-open-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r90-mutant-release-busy-spin-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r89-latch-confirms-r87-bounded-negative-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r88-nested-callback-is-null-by-design-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r86-vd-pipeline-works-generic-wait-is-separate-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r85-wrong-object-corrected-20260831.md`
  (rétracte l'identité d'objet `0x1a0010` utilisée par r77-r84 — leurs
  récits sur cet objet sont superseded, seuls leurs faits de flot de
  contrôle des fonctions restent valables);
- `reports/ac6-retail-native-codegen-gate2-r11-20260831.md`;
- `recompilation/ace-combat-6-retail/build/ntsc-uj/native/codegen-20260831-patched-final-r11/codegen-receipt.json`;
- `recompilation/ace-combat-6-retail/build/ntsc-uj/native/build-receipt.json`;
- `recompilation/ace-combat-6-retail/config/xbox360-toolchain.lock.json`;
- `recompilation/ace-combat-6-retail/native/README.md`;
- `recompilation/ace-combat-6-retail/targets/ntsc-uj.json`;
- `recompilation/ace-combat-6-retail/routes/mission01-qualified-96.steps`;
- `docs/native-recompilation-tools.md` (racine portfolio).

Identités déjà scellées: XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`, ISO US
`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`, Ghidra
`ghidra-projects/ac6-us`, AC6_recomp
`09144bb092ad871584808aeead69c395edbd5200`, route SHA-256
`771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043`.

Sous-gate codegen/liaison fermé: receipt r11 `pass`, 81 fichiers générés,
62 629 029 octets, zéro diagnostic et aucune instruction non reconnue. Le
guest se lie à `ppc_func_mapping.cpp` et 229 imports offline; `ac6recomp`
peuple 19 832 mappings. Le profil natif passe CTest 9/9, pytest 130/130 (r92),
l'audit d'installation et `validate.py --target ntsc-uj --runtime native`.

Gate actif: runtime natif encore ouvert. La sonde r75 atteint le renderer Vd
sans ReXGlue installé; le renderer accepte `PM4_ME_INIT` (19 dwords) puis
le lot IB bootstrap (12 dwords).

**FERMÉ (r94) : le blocage `sub_821E6AC8`/`0x10001a00` chassé depuis r51
(r77-r93, identité d'objet r85, pipeline sain r86, deux mécanismes de
déblocage réfutés r88/r93, négatif borné r89) est réellement résolu.**
Cause réelle trouvée en r94, avec l'aide d'un avis externe reliant trois
faits déjà connus mais jamais rapprochés : `drain_locked()` faisait passer
les écritures `EVENT_WRITE_SHD` par `gpu_swap()` (émule le swap matériel
GPU, résultat déjà final) PUIS `store_guest_word()` (son propre bswap) —
deux échanges qui composaient au lieu d'annuler une seule transformation.
Corrigé (`store_guest_bytes_raw()`, memcpy brut). Vérifié algébriquement
contre les octets vivants (`05 00 00 00` jamais expliqués depuis r89
devient `00 00 00 05`=5) ET en direct par reconstruction A/B isolée sur un
seul fichier : le thread principal revient de l'ancienne chaîne d'attente
et bloque maintenant via une chaîne ENTIÈREMENT NOUVELLE
(`sub_821F03B0←sub_8234F558←sub_8233E0A8←sub_8233B5A0`, jamais vue avant
ce cycle) — progression réelle confirmée, pas une hypothèse. Aucune
revendication de boot/titre/gameplay — la sonde expire toujours.

**r95** a corrigé le formatage hex du décodeur (artefact `std::to_string()`
décimal, pas un vrai problème d'adresse) et nommé précisément (pas corrigé)
la vraie plage de registres. **r96** a scanné le tampon indirect complet
(dump GDB + parseur Python, logique de décodage exacte du projet, flux
propre 371 TYPE0+281 TYPE3) : registre maximum réel `0x5002`. Corrigé :
`XenosState::kRegisterCount` `0x4000`→`0x8000` (borne dérivée du format
`low_register_of`, pas devinée). Vérifié en direct : rejet disparu,
publication d'anneau avance à 37 dwords (record) avant un nouveau rejet
délibéré : `predicated TYPE3 packets are not supported`. **r97** a
caractérisé ce rejet SANS deviner de correctif : seuls 2/281 paquets TYPE3
portent le bit prédicat (opcodes déjà implémentés); le premier opcode
vraiment inconnu (`0x46`) n'apparaît qu'après, puis `0x45` se répète 257
fois. Aucune infrastructure de prédicat n'existe dans ce code; deviner la
sémantique matérielle risquerait une corruption visuelle silencieuse —
refusé, besoin d'une source externe vérifiée. **r98** a tracé `0x45`
(`sub_821EB8B8`, table 256 entrées via rampe 127/255) et `0x46`
(`sub_821EBB40`, même motif curseur/limite→`sub_821E60A8` que r93/r94,
structure = `INVALIDATE_STATE`) depuis le code réel, et les a implémentés
en suivant des précédents établis (pas devinés). Vérifié par test unitaire
mais sonde vivante INCHANGÉE — le décodage bute toujours sur le prédicat
(r97) avant d'atteindre ces opcodes. **r99** a tracé les deux paquets
prédiqués jusqu'à leur construction réelle : `DRAW_INDX_2` — bit constante
figée; `WAIT_REG_MEM` — bit GENUINEMENT conditionnel, chemin prédiqué lit
`object+0x2a94`/`0xBADF00D` (exactement les champs du callback imbriqué
r87/r88), branche alternative retourne directement dans `sub_821E63F0`
(le gestionnaire d'interruption déjà désassemblé). **Le prédicat rejoint
donc la lacune callback d'interruption déjà documentée (r85-r89), pas une
inconnue indépendante** — ceci renforce le refus r97 de deviner. Toujours
aucun correctif implémenté. Identité du nouvel objet bloqué (r94) toujours
non lue. Le passage sur les 76 sites d'appel de `sub_821E60A8`
(r79/r93) et la piste `sub_821E65B0` sont hors de propos — ils
caractérisaient l'ANCIEN blocage, résolu en r94.

**r100 a retiré le rejet des paquets TYPE3 prédiqués** (`native_xenos.cpp:169-171`)
sur 4 preuves indépendantes (bits captures figés à la compilation r99, aucun
opcode de positionnement de prédicat dans le recensement complet r96, backend
Vulkan déjà indifférent au bit, branche alternative `WAIT_REG_MEM` retourne
proprement dans le gestionnaire d'interruption déjà connu) — politique de
décodage documentée, pas une revendication matérielle. A aussi corrigé un
second bug pré-existant sans rapport (`ring[2]` mal dimensionné dans un test
r95/r96). **Vérifié en direct : record de progression** (`vd publish
write=49`, contre 37 en r96), nouvelles allocations jusqu'à 2 Mo, un swap
commit, quatre incréments de fence — **puis SIGSEGV**, pas l'expiration
habituelle : appel indirect corrompu dans `sub_821D6C20`
(`_xstart→sub_821D7DE0→sub_821D6C20`), `rax` dans la plage d'un pointeur hôte
au lieu d'une adresse invité. Nouvelle frontière nommée, non caractérisée —
c'est la prochaine question, pas une suite du fil prédicat (clos pour le
contenu observé). L'identité de l'ancien objet bloqué `sub_821E6AC8`/
`sub_821F03B0` (r94) est probablement caduque : la sonde ne l'atteint plus
avant le nouveau plantage.

**r101 a tracé le plantage r100 jusqu'à sa cause exacte, sans deviner de
correctif.** `sub_821D6C20` lit un pointeur d'objet global (`0x82935d98`)
et déréférence sa vtable ~24 fois; la valeur RUNTIME (lue directement dans
le core dump du plantage, pas seulement l'image statique) est
`0x00000000` — un vrai pointeur nul. Le site de construction réel existe
(motif singleton paresseux juste avant `sub_821D6C20`, qui réussit un
appel virtuel différent juste après), mais `sub_821D7DE0` (seul appelant
de `sub_821D6C20`) ne l'atteint jamais avant d'appeler `sub_821D6C20` —
vérifié par dump complet. Trois explications restent ouvertes (boot plus
large non atteint, race multi-thread perdue par la sonde mono-thread,
stub d'import encore no-op) — refusé de deviner laquelle, même discipline
que r97. Aucun code modifié ce cycle.

**r102 a complété la chaîne causale du plantage r100/r101.** Le site de
construction du singleton (r101) appartient à `Function_821D5F48`
(`0x821d5f48`-`0x821d6c1b`), qui EST le premier appel de `sub_821D7DE0`.
Cette fonction a exactement 2 sorties : la construction (fin normale) ou
un bailout partagé (`r3=0`) atteignable par 5 gardes internes distinctes
échouant. `sub_821D7DE0` vérifie le retour, appelle un diagnostic
(`sub_821F5B18`) sur échec, mais continue quand même vers
`sub_821D6C20` sans s'arrêter — chaîne mécanique complète, sans deviner
quel garde échoue précisément. Tentative GDB en direct partiellement
infructueuse (rapportée honnêtement, pas cachée) : `finish` sur
`Function_821D5F48` a expiré à 90s, probablement surcoût `ptrace`. Piste
nommée sans l'affirmer : `sub_821F5B18` pourrait être fatal sur le vrai
matériel, un stub natif qui retourne expliquerait tout. Aucun code
modifié.

**r103 a corrigé l'hypothèse r102, elle-même.** Un traçage GDB filtré par
adresse d'appelant (reproductible sur 2 runs) montre que seuls GATE1 et
GATE2 des 5 gardes sont atteints. Mais la valeur de retour RÉELLE de
GATE2 (`0x8feffcb0`, lue proprement via l'offset `PPCContext::r3`) est
NON NULLE — son propre test ne peut donc pas causer le bailout, contrairement
à ce que r102 supposait. Relecture complète du bloc entre GATE2 et GATE3 :
aucune sortie de branche trouvée statiquement, pourtant GATE3 n'est jamais
atteint en direct. Une tentative de casser sans condition sur GATE3 a
expiré à 240s sans résultat — **refusé de l'interpréter comme "jamais
appelé"**, ce projet ayant déjà documenté que les sessions GDB attachées
se comportent différemment en timing sur cette sonde précise. Vraie
divergence toujours ouverte. Aucun code modifié.

**r104 a désassemblé le code HÔTE compilé** (pas seulement le PPC invité)
au point de retour de GATE2 : confirme exactement la même absence de
branche de sortie vers GATE3 que la lecture PPC de r103 — les deux
lectures statiques concordent, aucune sortie de contrôle. Écarté
l'ambiguïté de symbole GDB comme explication (`sub_821CC508` et
`__imp__sub_821CC508` résolvent à la même adresse). **4 sessions GDB en
direct au total (r103+r104), 3 résultats DIFFÉRENTS** — c'est la vraie
découverte : le traçage `ptrace` change mesurablement le comportement de
cette sonde run après run, pas une découverte sur le jeu invité. **Décidé
d'arrêter le traçage GDB en direct** pour cette question précise; la
question "pourquoi GATE3 n'apparaît jamais en direct" de r103 est mieux
expliquée comme un artefact GDB. Le modèle structurel r102 reste valable;
quel garde échoue (si un échoue) reste non établi. Aucun code modifié.

**r105 a trouvé la cause racine du plantage.** Suivant r104, instrumenté
directement le source GÉNÉRÉ (jamais commité, restauré après usage) et
exécuté nativement (zéro GDB). Résultat reproductible : GATE2
(`sub_821F4078`) échoue réellement (rétractant la lecture GDB non fiable
de r103) — son import réel est `MmAllocatePhysicalMemoryEx`, appelé avec
une taille `0xffc00000` (~4,09 Go, un débordement signé de `-4 Mo`).
Tracé jusqu'à sa source : `r11 - 8 Mo`, où `r11` vient d'un slot de pile
(`108(r1)`) que RIEN dans `Function_821D5F48` ni son unique appelant
`sub_821D7DE0` n'écrit jamais — mémoire de pile non initialisée dans ce
harnais (contient 4 Mo au lieu des ≥8 Mo attendus). **Aucun correctif
implémenté** : deux explications restent ouvertes (dépendance cachée à
une séquence d'appels réelle du matériel vs. vraie lacune du harnais
mono-thread), deviner une valeur risquerait de masquer une vraie lacune.
Technique d'instrumentation temporaire du code généré validée comme
alternative fiable à GDB pour cette sonde.

**r106 a précisément localisé le slot de pile non initialisé de r105.**
Instrumentation directe (même technique, restaurée après usage) : adresse
invité `0x8feffd1c`, valeur `0x400000`, confirmées en direct et par calcul
manuel une fois corrigé. **Cette adresse est SOUS le cadre de `_xstart`
lui-même** — hors de toute la chaîne d'appel PPC tracée, puisque
`_xstart` est le point d'entrée XEX sans appelant invité. Une erreur de
calcul manuel a été auto-corrigée avant de tromper le rapport (le premier
calcul, faux, semblait contredire la lecture du core dump; recalculé
correctement, les deux lectures du core dump concordent — différence de
MOMENT dans l'exécution, pas de non-déterminisme). Toujours aucun
correctif : l'origine nécessite soit de tracer une séquence de boot
noyau/chargeur plus large que ce harnais ne reproduit, soit d'accepter
cela comme une limitation de portée du harnais mono-thread. Aucun code
modifié.

**r107 a écarté une hypothèse concrète pour la valeur mystère de r106.**
Ce projet analyse déjà `stack_size` depuis l'en-tête XEX
(`native_xex.cpp`) — vérifié directement (diagnostic temporaire dans
`ac6recomp_main.cpp`, tracké, restauré après usage) : `0x40000` (256 Ko),
JAMAIS consulté ailleurs dans le harnais (`r1` codé en dur), et 256 Ko
n'explique pas une quantité de 4/8 Mo. Résultat négatif documenté,
écartant proprement une hypothèse plausible plutôt que de la laisser non
testée. Aucun code modifié.

**r108 a fermé la chaîne de crash GATE2 (r100-r107) et corrige r106 par
son nom.** r106 s'était trompé de cadre : le pointeur mystère
(`0x8feffd1c`) est écrit par `sub_821F4820`, via une évasion de pointeur
(`Function_821D5F48` lui passe `r1+96`; il écrit `r31+12`) invisible à
une recherche indexée sur `ctx.r1` seul. Cause racine réelle : le stub
HLE `__imp__MmQueryStatistics` (`tools/materialize_native_import_stubs.py`)
n'avait aucun cas dédié et ne touchait jamais la mémoire invité du
tampon de sortie — les octets étaient de la pile périmée, pas du
territoire noyau/chargeur. **Corrigé** : nouveau cas écrivant les deux
champs lus (RAM physique 512 Mo bien documentée de la Xbox 360, page
4 Ko déjà établie par le code invité : `0x20000` pages totales,
`0x18000`=384 Mo disponibles). **Vérifié en direct** :
`field+12=0x18000000`, l'allocation RÉUSSIT désormais (`r3=0x16f70000`,
non nul). Sonde d'entrée rejouée sans instrumentation : atteint la
borne de 30s SANS crash, contre SIGSEGV systématique depuis r100.
Prochain cycle : ré-appliquer l'étape 1 du plan en cours pour ce que le
guest attend réellement post-GATE2 (ne pas supposer que c'est encore
`sub_821E6AC8` sans re-tracer).

**r109 a réfuté par mesure directe l'hypothèse d'une boucle de poll
mémoire dans `Function_821D5F48`.** Sa forme (`loc_821D6358` appelant
`sub_821CC508` tant qu'il retourne 0) ressemblait exactement au motif
déjà documenté côté démo — vérifiée en direct (instrumentation
temporaire à deux tours, restaurée), elle ne tourne PAS : retour `-1`
dès le premier appel, sortie de secours propre, `r3=0` renvoyé à
`sub_821D7DE0` en microsecondes. Aucun code modifié. Le blocage réel de
30s reste en aval, non atteint — prochain candidat : `sub_821D7DE0`
lui-même.

**r110 a trouvé que la sonde est non-déterministe d'une exécution à
l'autre, MÊME sans GDB.** En traçant `sub_821D7DE0`, corrige une erreur
de portée dans r109 (qui avait déduit un retour propre de l'absence de
prints, jamais observé directement) — les marqueurs directs confirment
que la boucle post-GATE2 sort bien proprement, conclusion de r109
maintenue. **Mais** cinq exécutions identiques du même binaire donnent
des résultats différents : 3/5 restent bloquées avant même d'atteindre
la boucle post-GATE2, 2/5 progressent au-delà, une segfault. Les
conclusions "GATE2 sans crash" (r108) et "boucle résolue proprement"
(r109) décrivaient chacune UNE seule exécution, pas le comportement
typique. Cause non identifiée, deviner refusé. Aucun code modifié.
Prochain cycle : toute conclusion doit désormais s'appuyer sur
plusieurs exécutions.

**r111 a trouvé la source réelle du non-déterminisme de r110 : une
VRAIE course entre threads.** Capture directe d'un crash via `gdb
--batch -ex run -ex bt` (deux exécutions sur quatre) : SIGSEGV dans
`__imp__sub_82346428` (via `sub_823453E8`←`sub_821F8008`) sur un VRAI
thread OS séparé (`start_thread`/`__clone3` visibles). Vérifié :
`ExCreateThread` lance réellement un `std::thread` détaché avec
seulement 64 Ko de pile — pas un stub inerte. Le blocage apparent du
thread principal variait donc vraiment d'une exécution à l'autre, mais
à cause de cette course, pas d'un non-déterminisme interne à
`sub_821D5F48`. Cause exacte du crash non établie, deviner refusé.
Aucun code modifié. Prochain cycle : tracer `sub_82346428` (offset
+414) statiquement.

**r112 a trouvé que dix-huit threads démarrent en parallèle** (pas un
seul comme r111 le suggérait), avec deux sites de crash reproductibles
sous le même trampoline `sub_821F8008` : `sub_82346428` et
`sub_821D4C20`. Instruction de crash capturée en direct : `call
*(%r12,%rax,2)` avec `rax=0`, suivi (si atteint) d'un
`RtlEnterCriticalSection` sur le même objet — dispatch vtable avant
verrouillage, cohérent avec une course de lecture non synchronisée
(non prouvé — `r12` non capturé, taux de reproduction très variable).
Aucun code modifié.

**r113 a confirmé `r12` non mappé au crash `sub_82346428`
(`0x7ffe75980000`, inaccessible), ET trouvé que le crash ORIGINAL
`sub_821D6C20` (r100) se produit toujours, par intermittence, sur le
THREAD PRINCIPAL, après r108.** Ne contredit pas la découverte
spécifique de r108 (l'allocation GATE2 réussit vraiment) mais montre
que "la chaîne de crash est fermée" n'a jamais été universelle. Non
reproduit dans 15 tentatives de suivi. Aucun code modifié.

**r114 — CAUSE RACINE TROUVÉE.** Les deux crashes de threads
d'arrière-plan (r112/r113) partagent l'instruction ET la valeur de
`r12` EXACTES — `objdump` confirme `r12` est une constante figée à la
compilation, pas une lecture runtime, correspondant exactement à la
macro `PPC_CALL_INDIRECT_FUNC` (`rex/ppc/context.h:126-131`).
`rax=0` aux deux crashes = appel d'un pointeur de fonction invité NUL,
sans vérification, débordement d'adresse. **Cause du nul** :
`ExCreateThread` ne lit jamais `CreationFlags` (r9) — signature XDK
publique, à revérifier — donc les 18 threads démarrent tous
immédiatement au lieu d'attendre une reprise explicite. Corrige r112
("course non synchronisée" → déterministe) et affine r113. Aucun code
modifié — changement d'infrastructure trop lourd pour ce cycle;
implémentation prévue pour un prochain cycle dédié.

**r115 a implémenté la suspension de création de r114 —
`ExCreateThread` lit maintenant `CreationFlags`, avec
`NtResumeThread`/`KeResumeThread`** (les deux confirmés réellement
importés avant implémentation). `park_until_resumed()` attend SANS
borne (distinct du `wait_event()` borné 2ms), 3 nouveaux tests, suite
134/134. **Résultat honnête** : réduit mais N'ÉLIMINE PAS les crashes —
`sub_821D4C20` et le crash original `sub_821D6C20` ont chacun planté
une fois sur 15 essais. Deux explications ouvertes non tranchées (pas
tous les threads suspendus, ou course de reprise indépendante).

**r116 — vraies sections critiques, élimination complète des crashes
r111-r115, expose le null global ORIGINAL de r101, maintenant
déterministe.** `RtlEnterCriticalSection`/`RtlLeaveCriticalSection`
étaient des no-ops (aucun des 18 threads ne demande
`CREATE_SUSPENDED`, réfutant r114/r115) — corrigé avec de vrais
`std::recursive_mutex` par objet. 25 exécutions : ZÉRO crash
d'arrière-plan, mais le thread principal plante DÉTERMINISTIQUEMENT
(25/25) dans `sub_821D6C20`, `rbp = 0x82935d98` — le global nul EXACT
que r101 avait trouvé au tout début de cet arc, jamais corrigé,
masqué par la course. Amélioration nette réelle; ne ferme pas Gate 2.
Prochain cycle : rouvrir `0x82935d98` directement.

**r117 — chaîne causale COMPLÈTE fermée.** `0x82935d98` n'a qu'un
SEUL site d'écriture dans tout le XEX (`FindPpcAddressMaterialization.java`,
read-only), à l'intérieur de `sub_821D5F48` lui-même, ~1100 lignes
après la boucle post-GATE2 que r105-r109 avaient déjà caractérisée.
Vérifié en UNE SEULE exécution (crash déterministe depuis r116) :
`sub_821CC508` retourne -1 pour l'état 3 (même échec que r109 avait
déjà capturé) → sortie de secours `loc_821D6138` → `return;` GENUINE
avant l'écriture → `sub_821D7DE0` n'abandonne pas → `sub_821D6C20` lit
le nul → crash. Ce n'est pas un nouveau mécanisme : c'est LA cause
racine réelle du crash original de r100. Aucun code modifié. Prochain
cycle : tracer `case 3` de `sub_821CC508` jusqu'à son retour exact.

**r118 corrige r117 par son nom : "case 3" était la mauvaise cible sous
le build actuel.** État réel = 0, pas 3 (jamais revérifié par r117).
Tracé jusqu'au MÊME corps case-3, mais un print dedans n'a jamais
déclenché — non atteint. Résolu : deux portes précèdent le dispatch
par état dans `sub_821CC508`, dont un OCTET GLOBAL qui doit valoir
exactement 1; sinon ça saute vers `loc_821CC800` puis `loc_821CCD4C`,
une troisième région jamais examinée. Aucun code modifié. Prochain
cycle : lire cet octet, tracer `loc_821CCD4C`.

**r119 — l'octet de mode global vaut RÉELLEMENT 2** (ce cycle avait
re-supposé 0 par déduction avant de l'instrumenter — même piège que
r118 venait de corriger). Chemin réel : troisième table de dispatch;
le -1 vient d'un compteur de nouvelles tentatives (`+22896`) qui,
épuisé, log via `sub_821D4988` et abandonne; la condition
succès/échec réutilise le couple `sub_821F4E70`/`sub_821F50A0` déjà lu
par r117 (champ par thread à `ctx.r13+336`). Cause exacte non établie.
Aucun code modifié. Prochain cycle : instrumenter `ctx.r13` et le
compteur, lire la chaîne de `sub_821D4988`.

**r120 corrige une spéculation de r119** : `sub_821D4988` n'est PAS un
appel de log (adresse de chaîne présumée = 128 octets de zéros). C'est
un producteur qui poste dans un tampon circulaire protégé (vraie
section critique depuis r116) puis attend via `sub_821F5988`. Ne
change pas la chaîne causale — le compteur `+22896` décide toujours.
Aucun code modifié. Prochaines étapes inchangées (`ctx.r13`, compteur).

**r121 — CAUSE RÉELLE TROUVÉE.** Auto-correction : r117/r119 avaient lu
à l'envers une branche de `sub_821F75F0` (repéré via un sentinelle
placeholder, corrigé dans le même cycle). Avec la logique corrigée :
`ctx.r13=kProbePcrAddress` (constante délibérée du harnais) →
`indirected=0xC00000BB` = `kOfflineStatus` EXACTEMENT — du code invité
a copié le retour d'un import non implémenté dans ce champ de statut.
Confirmé via `AC6_NATIVE_IMPORT_TRACE` (existant) : `NtReadFile`
(6×)/`NtCreateFile` (4×) tombent dans le stub générique. **La boucle
attend RÉELLEMENT une lecture de fichier jamais implémentée** —
cohérent avec toute la chaîne depuis r109. Aucun code modifié.
Prochain cycle : implémenter `NtCreateFile`/`NtReadFile` contre
l'infrastructure XDVDFS/média existante.

**r122 — convention d'appel de `NtCreateFile`/`NtReadFile` vérifiée par
désassemblage réel** (7 sites d'appel groupés, un seul wrapper CRT-style
désassemblé intégralement) : `r3-r10` seulement, ordre NT/XDK réel.
`ObjectAttributes` partiellement résolu (ANSI_STRING confirmée vers
`"\Device\Harddisk0\Partition1"`, Length=28 vérifié). Implémentation
DÉLIBÉRÉMENT différée — layout complet pas encore résolu, risque du
motif "implémenté avant vérification complète" (r115). Aucun code
modifié. Prochain cycle : finir le layout d'OBJECT_ATTRIBUTES avant
d'implémenter.

**r123 — `OBJECT_ATTRIBUTES` résolu; la lecture est à décalage fixe sur
un disque dur peut-être absent.** `ObjectName` pointe DIRECTEMENT vers
`"\Device\Harddisk0\Partition1"` (pas de nom de fichier par appel).
Lecture 0x400 octets à décalage 0x800, fixes — sonde de structure
système, pas chargeur d'actifs. Titre disque-uniquement → l'échec
pourrait être ATTENDU sur le vrai matériel. Reformule le correctif
potentiel : peut-être juste un vrai statut d'échec NT au lieu de
`kOfflineStatus`. PAS ENCORE connecté à la table de dispatch de
`sub_821F4E70`. Aucun code modifié. Prochain cycle : établir cette
connexion, puis tester l'hypothèse en direct.

**r124 — connexion confirmée EN DIRECT : `sub_821F4E70` dispatche vers
`NtReadFile`.** Instrumentation d'une exécution (crash déterministe) :
target=NtReadFile ×6 = décompte exact de r121. Mapping registres
confirmé (`r7=&IoStatusBlock`, `259`/STATUS_PENDING pré-écrit).
L'appelant reconnaît EXPLICITEMENT STATUS_PENDING comme issue normale
— confirme depuis la logique du jeu toute la lecture "pending, retry"
depuis r109. Reformule le correctif : candidat = retourner 259 au
premier appel puis faire évoluer l'IoStatusBlock. PAS implémenté.
Aucun code modifié. Prochain cycle : tracer `loc_821F4FE4`, concevoir
le stub.

**r90-r92 (infrastructure toujours valable)** : busy-spin `NtReleaseMutant`
mesuré et corrigé (r90, diagnostic permanent `AC6_NATIVE_IMPORT_TRACE`);
`wait_event()` réellement bloquant (r91); appelants événements réglés,
`DbgPrint` disponible (r92, gated `AC6_NATIVE_IMPORT_TRACE`). Les stubs
restent build-only; `IM_LOAD_IMMEDIATE` est borné mais sa traduction
Xenos→SPIR-V n'est pas fermée. Ne pas revendiquer titre, M01, campagne,
save/replay ou release.

Le patch XenonRecomp utilisé pour r11 reste dans une copie de build ignorée;
le checkout verrouillé et les sources générées ne doivent pas entrer dans le
produit installé. ReXGlue reste oracle read-only seulement. N2 de
`reconstruction/ace-combat-6` reste historique, hors cible active.
La primitive `GuestAddressSpace` réserve l'espace virtuel Xenon 32-bit complet
avec bornes testées et est exigée par `NativeRuntime::boot()`; bindings imports
et appel guest restent ouverts.
