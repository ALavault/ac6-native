# AC6 retail NTSC-U/J — reprise Gate 2 runtime natif

Reprendre depuis `recompilation/ace-combat-6-retail`.

Lire d'abord:

- `reports/handoff/CURRENT.json`;
- `reports/ac6-retail-native-codegen-gate2-r84-endian-misread-and-timeline-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r83-escalation-is-telemetry-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r82-gate-contradiction-open-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r81-heap-init-ordering-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r80-null-ring-allocation-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r79-generic-allocator-reframe-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r78-actual-wait-frame-20260831.md`;
- `reports/ac6-retail-native-codegen-gate2-r77-fence-frontier-20260831.md`;
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
peuple 19 832 mappings. Le profil natif passe CTest 9/9, pytest 126/126,
l'audit d'installation et `validate.py --target ntsc-uj --runtime native`.

Gate actif: runtime natif encore ouvert. La sonde r75 atteint le renderer Vd
sans ReXGlue installé. Elle établit l'objet `0x10001a00`, son WPTR primaire
`+10952`, le readback exact `state+60`, et la mémoire IB big-endian; le renderer
accepte `PM4_ME_INIT` (19 dwords) puis le lot IB bootstrap (12 dwords). Le guest
reste ensuite sans retour et aucun gameplay visible n'est qualifié.

r84 a exécuté l'expérience à thread unique et corrigé un bug d'endianness
dans la méthode GDB (`x/xw` affiche la mémoire invitée big-endian comme
little-endian; les lectures à zéro restent valables). Chronologie établie :
`sub_821E65B0` initialise `+0x2a9c=3`, un push l'amène à 5, et c'est cet
appel (+0x2a9c=5 réel) qui finit bloqué — mais aucun écrivain statique de
`+0x2a9c` n'écrit jamais zéro, donc comment il redevient zéro reste
ouvert. Prochaine étape : relire `+0x2a9c` À L'INTÉRIEUR du même appel
bloqué (pas juste à la porte), endianness corrigée, sonde à thread unique
(no-op `ExCreateThread` temporaire, ne pas committer). Les stubs restent
build-only;
`IM_LOAD_IMMEDIATE` est borné mais sa traduction Xenos→SPIR-V n'est pas
fermée. Ne pas revendiquer titre, M01, campagne, save/replay ou release. Toute
sonde suivante doit être statique ou bornée au premier import/retour qui
bloque ce thread.

Le patch XenonRecomp utilisé pour r11 reste dans une copie de build ignorée;
le checkout verrouillé et les sources générées ne doivent pas entrer dans le
produit installé. ReXGlue reste oracle read-only seulement. N2 de
`reconstruction/ace-combat-6` reste historique, hors cible active.
La primitive `GuestAddressSpace` réserve l'espace virtuel Xenon 32-bit complet
avec bornes testées et est exigée par `NativeRuntime::boot()`; bindings imports
et appel guest restent ouverts.
