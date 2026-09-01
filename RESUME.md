# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord:

- `reports/handoff/CURRENT.json`;
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
