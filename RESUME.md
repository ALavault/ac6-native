# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord:

- `reports/handoff/CURRENT.json`;
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

**Nouvelle frontière (r94, pas caractérisée)** : un nouveau rejet de
décodage boucle sans recul (`TYPE0 register range exceeds Xenos state, IB
0x308019200` — adresse >32 bits, suspect); identité du nouvel objet
bloqué non lue. Le passage sur les 76 sites d'appel de `sub_821E60A8`
(r79/r93) et la piste `sub_821E65B0` sont maintenant hors de propos — ils
caractérisaient l'ANCIEN blocage, résolu.

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
